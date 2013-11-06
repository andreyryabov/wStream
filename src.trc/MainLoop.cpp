//
//  MainLoop.cpp
//  wStream
//
//  Created by Andrey Ryabov on 10/29/13.
//  Copyright (c) 2013 Andrey Ryabov. All rights reserved.
//

#include "Common.h"
#include "Exception.h"
#include "MainLoop.h"
#include "Json.h"
#include "MPack.h"

using namespace std;
using namespace msgpack;

namespace wStream {

static const string MSG = "_msg_";
static const string CMD = "_cmd_";
static const string RES = "_res_";
static const string SEQ = "_seq_";

static const string FILE_PREFIX  = "file:";
static const char * INPROC_MEDIA = "ipc:///tmp/mediatranscoder";

zmq::message_t toZmsg(Packer & buf) {
    zmq::message_t zmsg(buf.size());
    memcpy(zmsg.data(), buf.data(), buf.size());
    return zmsg;
}

/***************** ZAdddr *********************************************************/
ZAddr::ZAddr(const string & addr) {
    *this = parse(addr);
}

ZAddr::ZAddr(const string & p, const string & a, int pt) : proto(p), address(a), port(pt) {}

ZAddr ZAddr::parse(const string & str) {
    if (str.size() > 256) {
        throw Exception EX("parsing zaddr failed, too long string");
    }
    smatch m;
    if (!regex_match(str, m, RX_ADDR)) {
        throw Exception EX("failed to parse z-address: " + str);
    }
    if (m.size() != 4) {
        throw Exception EX("invalid address: " + str);
    }
    return ZAddr(m[1].str(), m[2].str(), stoi(m[3].str()));
}

ostream & operator << (ostream & out, const ZAddr & z) {
    return out<<z.proto<<"://"<<z.address<<":"<<z.port;
}
  
const std::regex ZAddr::RX_ADDR(R"x((\w{2,8})://([\d\w\./]+|\*):(\d+|\*))x");

ZAddr getEndpoint(zmq::socket_t & sock) {
    char endpoint[256];
    size_t epSize = sizeof(endpoint) - 1;
    sock.getsockopt(ZMQ_LAST_ENDPOINT, endpoint, &epSize);
    endpoint[epSize] = 0;
    return ZAddr::parse(endpoint);
}

/******** class MainLoop *******************************************/
MainLoop::MainLoop() : _context(1),
    _pullSock(_context, ZMQ_PULL),
    _pushSock(_context, ZMQ_PUSH),
    _pubSock (_context, ZMQ_PUB ),
    _subSock (_context, ZMQ_SUB ) {
}

void MainLoop::init(const ZAddr & addr, int64_t uuid) {
    _uuid = uuid;
    _pushSock.connect(toStr(addr).c_str());
    _subSock .connect(INPROC_MEDIA);

    _pullSock.bind("tcp://*:*");
    _pubSock .bind("tcp://*:*");
    
    int cmdPort = getEndpoint(_pullSock).port;
    int pubPort = getEndpoint(_pubSock).port;
    
    message("started", json("command", json("port", cmdPort), "media", json("port", pubPort)));
    
    Log<<"transcoder, commands port: "<<cmdPort<<", media port: "<<pubPort<<endl;
}

void MainLoop::run() {
    zmq::pollitem_t items[2] = {{_pullSock, 0, ZMQ_POLLIN}, {_subSock, 0, ZMQ_POLLIN}};
    try {
        _pingTs = Timer::now();
        while (_running) {
            zmq::poll(items, 2, 5000);
            if (items[0].revents & ZMQ_POLLIN) {
                onCommand();
            }
            if (items[1].revents & ZMQ_POLLIN) {
                onMedia();
            }
            Timer::duration diff = Timer::now() - _pingTs;
            if (diff > chrono::seconds(15)) {
                throw Exception EX("keep alive timeout");
            }
        }
    } catch (const exception & e) {
        Err<<"main loop error: "<<toStr(e)<<endl;
    }
    Log<<"exit main loop"<<endl;
}

void MainLoop::message(const std::string & msg, const Json::Value & value) {
    Json::Value json = value;
    json[MSG] = msg;
    
    Packer pack;
    pack.put(MSG_JSON);
    pack.put(_uuid);
    pack.put(json.toStyledString());
    
    zmq::message_t zmsg = toZmsg(pack);
    _pushSock.send(zmsg);
}

void MainLoop::onCommand() {
    zmq::message_t msg;
    while (_pullSock.recv(&msg, ZMQ_DONTWAIT)) {
        Unpacker up(msg.data(), msg.size());
        int type = int(up.get<mtype::POSITIVE_INTEGER>());
        
        if (type == MSG_HALT) {
            Log<<"got halt"<<endl;
            _running = false;
            return;
        }
        if (type == MSG_PING) {
            int64_t uuid = up.get<mtype::POSITIVE_INTEGER>();
            if (uuid != _uuid) {
                Err<<"got wrong ping uuid: "<<uuid<<" != "<<_uuid<<endl;
            } else {
                Log<<"got ping"<<endl;
                _pingTs = Timer::now();
            }
            continue;
        }
        if (type == MSG_JSON) {
            Json::Value json = parseJson(up.getStr());
            Log<<"got json: "<<json.toStyledString()<<endl;
            handleCommandJson(json);
            continue;
        }
        throw Exception EX("invalid message type: " + toStr(type));
    }
}

void MainLoop::handleCommandJson(const Json::Value & json) {
    if (json.isMember("stream")) {
        string name = json["stream"]["name"].asString();
        int     sid = json["stream"]["sid"].asInt();
        openStream(sid, name);
        return;
    }
    Err<<"invalid json command "<<json.toStyledString()<<endl;
    throw Exception EX("invalid json command");
}

void MainLoop::openStream(int sid, const string & name) {
    auto it = _name2sid.find(name);
    if (it != _name2sid.end() && it->second != sid) {
        throw Exception EX("stream: " + toStr(name) + " already registered");
    }
    _name2sid[name] = sid;

    Log<<"media subscribe: "<<name<<endl;
    Packer nPack;
    nPack.put(name);
    _subSock.setsockopt(ZMQ_SUBSCRIBE, nPack.data(), nPack.size());
    
    if (name.find(FILE_PREFIX) == 0) {
        string fileName = name.substr(FILE_PREFIX.size(), name.length());
        thread t{[=]() {this->fileStream(fileName);}};
        t.detach();
    }
}

void MainLoop::fileStream(const string & fileName) {
    try {
        zmq::socket_t sock(_context, ZMQ_PUB);
        sock.bind(INPROC_MEDIA);

        AVFormatContext * fmtCtx = nullptr;
        if (avformat_open_input(&fmtCtx, fileName.c_str(), nullptr, nullptr) < 0) {
            char buf[1024];
            char * res = getcwd(buf, sizeof(buf));
            Err<<"failed to open video file: "<<fileName;
            if (res) {
                Err<<", being in the directory: "<<res;
            }
            Err<<endl;
            return;
        }

        if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
            Err<<"failed to find stream info in "<<fileName<<endl;
            return;
        }
        
        AVCodec * codec = nullptr;
        int idx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        if (idx < 0) {
            Err<<"failed to find video stream in "<<fileName<<endl;
            return;
        }

        Log<<"stream codec: "<<codec->name<<", long name: "<<codec->long_name<<endl;

        AVPacket packet;
        av_init_packet(&packet);

        packet.data = nullptr;
        packet.size = 0;
        int64_t ts0 = 0;
            
        while (av_read_frame(fmtCtx, &packet) >= 0) {
            if (packet.stream_index != idx) {
                continue;
            }
            Packer pack;
            pack.put(FILE_PREFIX + fileName);
            zmq::message_t msg = toZmsg(pack);
            sock.send(msg, ZMQ_SNDMORE);
            
            pack.clear();
            if (packet.flags & AV_PKT_FLAG_KEY) {
                pack.put(MSG_CODEC);
                pack.put(string(codec->name));
            }
            pack.put(MSG_FRAME);
            pack.putRaw(packet.data, packet.size);
            
            msg = toZmsg(pack);
            sock.send(msg);
            
            if (ts0 == 0) {
                ts0 = packet.pts;
            }
            usleep(unsigned(packet.pts - ts0) * 1000);
                            
            av_free_packet(&packet);
        }
    } catch (const exception & ex) {
        Err<<"exception in fileStream thread: "<<toStr(ex)<<endl;
    }
}

void MainLoop::onMedia() {
    zmq::message_t head;
    if (!_subSock.recv(&head, ZMQ_DONTWAIT)) {
        throw Exception EX("media message header not received");
    }
    
    zmq::message_t body;
    if (!_subSock.recv(&body, ZMQ_DONTWAIT)) {
        throw Exception EX("media message body not received");
    }
    
    Unpacker unpack(head.data(), head.size());
    string stream = unpack.getStr();
    
    
    Log<<"------ got stream frame: "<<stream<<endl;
}

}
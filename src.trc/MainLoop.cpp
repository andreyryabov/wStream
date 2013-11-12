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

static const string FILE_PREFIX   = "file:";
static const string INPROC_PREFIX = "ipc:///tmp/media_";

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

/********** class Stream *****************************************/
class Stream {
  public:
    int             sid;
    Transcoder      trc;
    Timestamp       lastTs;
    Timestamp       lastKey;
    Timer::duration iFrameInterval;
    Timer::duration pFrameInterval;
    
    Stream(int s) : sid(s) {}
};


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
            zmq::poll(items, 2, 50);
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
            transcode();
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
        int type = up.get<int>();
        
        if (type == MSG_HALT) {
            Log<<"got halt"<<endl;
            _running = false;
            return;
        }
        if (type == MSG_PING) {
            int64_t uuid = up.get<int64_t>();
            if (uuid != _uuid) {
                Err<<"got wrong ping uuid: "<<uuid<<" != "<<_uuid<<endl;
            } else {
                Log<<"got ping"<<endl;
                _pingTs = Timer::now();
            }
            continue;
        }
        if (type == MSG_JSON) {
            Json::Value json = parseJson(up.get<std::string>());
            Log<<"got json: "<<json.toStyledString()<<endl;
            handleCommandJson(json);
            continue;
        }
        throw Exception EX("invalid message type: " + toStr(type));
    }
}

void MainLoop::handleCommandJson(const Json::Value & json) {
    if (json.isMember("stream")) {
        const Json::Value & stream = json["stream"];
        string name = stream["name"].asString();
        int     sid = stream["sid"] .asInt();
        StreamConfig cfg;
        cfg.gopSize = 50;
        cfg.width   = stream["width"]  .asInt();
        cfg.height  = stream["height"] .asInt();
        cfg.bitrate = stream["bitrate"].asInt();
        cfg.pFrameInterval = chrono::milliseconds(int(1000 / stream["fps"].asDouble()));
        cfg.iFrameInterval = chrono::milliseconds(stream["keyframeInterval"].asInt());
        openStream(sid, name, cfg);
        return;
    }
    if (json.isMember("keyframe")) {
        keyframe(json["keyframe"].asInt());
        return;
    }
    Err<<"invalid json command "<<json.toStyledString()<<endl;
    throw Exception EX("invalid json command");
}

void MainLoop::keyframe(int sid) {
    for (auto kv : _streams) {
        Ptr<Stream> str = kv.second;
        if (str->sid == sid) {
            str->trc.keyframe();
        }
    }
}

void MainLoop::openStream(int sid, const string & name, const StreamConfig & conf) {
    auto it = _streams.find(name);
    if (it != _streams.end()) {
        if (it->second->sid != sid) {
            throw Exception EX("stream: " + toStr(name) + " already registered");
        }
        return;
    }
    
    _streams[name] = make_shared<Stream>(sid);
    _streams[name]->trc.initEncoder(conf);
    _streams[name]->iFrameInterval = conf.iFrameInterval;
    _streams[name]->pFrameInterval = conf.pFrameInterval;
    
    //_streams[name]->trc.dumpFile("../data/" + toStr(sid) + ".mpg");

    Log<<"media subscribe: "<<name<<endl;
    Packer nPack;
    nPack.put(name);
    _subSock.setsockopt(ZMQ_SUBSCRIBE, nPack.data(), nPack.size());
    
    if (name.find(FILE_PREFIX) == 0) {
        string fileName = name.substr(FILE_PREFIX.size(), name.length());
        string enc = name;
        enc.replace(enc.begin(), enc.end(), '.', 'D');
        enc.replace(enc.begin(), enc.end(), '/', 'D');
        enc.replace(enc.begin(), enc.end(), ' ', 'W');
        enc = INPROC_PREFIX + enc;
        _subSock.connect(enc.c_str());
        thread t{[=]() {this->fileStream(fileName, enc);}};
        t.detach();
    }
}

void MainLoop::fileStream(const string & fileName, const string & channel) {
    AVFormatContext * fmtCtx = nullptr;
    try {
        zmq::socket_t sock(_context, ZMQ_PUB);
        sock.bind(channel.c_str());
  
        if (avformat_open_input(&fmtCtx, fileName.c_str(), nullptr, nullptr) < 0) {
            throw Exception EX("failed to open video file: " + fileName);
        }

        if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
            throw Exception EX("failed to find stream info in " + fileName);
        }
        
        AVCodec * codec = nullptr;
        int idx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
        if (idx < 0) {
            throw Exception EX("failed to find video stream in file: " + fileName);
        }
        AVStream * stream = fmtCtx->streams[idx];
        AVRational timeBase = stream->time_base;
        
        Blob cfg;
        if (stream->codec->extradata) {
            cfg.assign(stream->codec->extradata, stream->codec->extradata + stream->codec->extradata_size);
        }

        Log<<"open file: "<<fileName<<", codec: "<<codec->name<<endl;

        AVPacket packet;
        av_init_packet(&packet);

        packet.data = nullptr;
        packet.size = 0;

        
        for (;;) {
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
                    pack.putRaw(cfg.data(), cfg.size());
                }
                pack.put(MSG_FRAME);
                pack.putRaw(packet.data, packet.size);
                
                msg = toZmsg(pack);
                sock.send(msg);
                //Log<<"file: "<<fileName<<", read frame: "<<pack.size()<<endl;
                
                int64_t ts = (1000 * timeBase.num * packet.pts) / timeBase.den;
                if (ts0 == 0) {
                    ts0 = ts;
                }
                int pause = int(std::max(ts - ts0, 50LL));
                usleep(pause * 1000);
                ts0 = std::max(ts, int64_t(0));
                                
                av_free_packet(&packet);
            }
            sleep(1);
            avformat_seek_file(fmtCtx, idx, 0, 0, 1000, AVSEEK_FLAG_FRAME);
        }
    } catch (const exception & ex) {
        Err<<"exception in fileStream thread: "<<toStr(ex)<<endl;
    }
    avformat_close_input(&fmtCtx);
}

void MainLoop::onMedia() {
    zmq::message_t head;
    if (!_subSock.recv(&head, ZMQ_DONTWAIT)) {
        throw Exception EX("media message header not received");
    }
    if (!head.more()) {
        throw Exception EX("media message has not body part");
    }
    zmq::message_t body;
    if (!_subSock.recv(&body, ZMQ_DONTWAIT)) {
        throw Exception EX("media message body not received");
    }
    
    Unpacker hUnp(head.data(), head.size());
    string stream = hUnp.get<string>();
    
    auto it = _streams.find(stream);
    if (it == _streams.end()) {
        Err<<"stream not found: "<<stream<<endl;
        return;
    }
    
    Unpacker bUnp(body.data(), body.size());
    int msg;
    while (bUnp.next(msg)) {
        if (msg == MSG_CODEC) {
            string codec = bUnp.get<string>();
            Blob conf = bUnp.get<Blob>();
            if (it->second->trc.initDecoder(codec, conf)) {
                Log<<"init decoder: "<<codec<<", config: "<<conf.size()<<", stream: "<<stream<<endl;
            }            
        } else if (msg == MSG_FRAME) {
            object_raw raw = bUnp.get<object_raw>();
            it->second->trc.decode(raw.ptr, raw.size);
            //Log<<"decode frame: "<<raw.size<<", stream: "<<stream<<endl;
        } else {
            Err<<"invalid message type"<<endl;
            return;
        }
    }
}

int MainLoop::transcode() {
    Timer::duration minD = chrono::milliseconds(50);
    
    Timestamp now = Timer::now();
    for (auto it : _streams) {
        bool keyframe = false;
        Timer::duration iDif = now - it.second->lastKey;
        if (iDif >= it.second->iFrameInterval) {
            keyframe = true;
        } else {
            minD = std::min(iDif, minD);
            Timer::duration pDif = now - it.second->lastTs;
            if (pDif < it.second->pFrameInterval) {
                minD = std::min(pDif, minD);
                continue;
            }
        }
        if (keyframe) {
            it.second->trc.keyframe();
            it.second->lastKey = now;
        }
        it.second->lastTs = now;
        
        bool isKey = false;
        const void * data = nullptr;
        size_t size = 0;
        if (it.second->trc.encode(data, size, isKey)) {
            Packer pack;
            pack.put(MSG_FRAME);
            pack.put(it.second->sid);
            pack.put(isKey);
            pack.putRaw(data, size);
            pack.put(size);
            zmq::message_t msg = toZmsg(pack);
            _pubSock.send(msg);
        }
    }
    return int((minD.count() * 1000 * Timer::period::num) / Timer::period::den);
}

}
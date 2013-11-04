//
//  Transcoder.cpp
//  wStream
//
//  Created by Andrey Ryabov on 10/29/13.
//  Copyright (c) 2013 Andrey Ryabov. All rights reserved.
//

#include "Common.h"
#include "Exception.h"
#include "Transcoder.h"
#include "Json.h"
#include "MPack.h"

using namespace std;
using namespace msgpack;

namespace wStream {

static const string MSG = "_msg_";
static const string CMD = "_cmd_";
static const string RES = "_res_";
static const string SEQ = "_seq_";


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

    _pullSock.bind("tcp://*:*");
    _pubSock .bind("tcp://*:*");
    
    int cmdPort = getEndpoint(_pullSock).port;
    int pubPort = getEndpoint(_pubSock).port;
    
    message("started", json("command", json("port", cmdPort), "media", json("port", pubPort)));
    
    Log<<"transcoder, commands port: "<<cmdPort<<", media port: "<<pubPort<<endl;
}

void MainLoop::run() {
    zmq::pollitem_t items[2] = {{_pullSock, ZMQ_POLLIN}, {_subSock, ZMQ_POLLIN}};
    try {
        _pingTs = Timer::now();
        while (_running) {
            zmq::poll(items, 2, 3000);
            if (items[0].revents | ZMQ_POLLIN) {
                handleCommand();
            }
            if (items[1].revents | ZMQ_POLLIN) {
                handleMedia();
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

void MainLoop::handleCommand() {
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
            return;
        }
        if (type == MSG_JSON) {
            Json::Value json = parseJson(up.getStr());
            Log<<"got json: "<<json.toStyledString()<<endl;
            handleCommandJson(json);
            return;
        }
        throw Exception EX("invalid message type: " + toStr(type));
    }
}

void MainLoop::handleCommandJson(const Json::Value & json) {
    zmq::message_t msg;
    while (_pullSock.recv(&msg, ZMQ_DONTWAIT)) {
        if (json.isMember(MSG)) {
         //TODO:........
        }
        
        char dat[5] = {1, 2, 3, 4, 5};

        Packer pack;
        pack.put(MSG_FRAME);
        pack.put(1);
        pack.putRaw(&dat[0], sizeof(dat));

        zmq::message_t zmg = toZmsg(pack);
        _pubSock.send(zmg);
        
        Log<<"------- send media message "<<endl;
    }
}

void MainLoop::handleMedia() {

}

}
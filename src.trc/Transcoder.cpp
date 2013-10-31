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

using namespace std;

namespace wStream {

static const string MSG = "_msg_";
static const string CMD = "_cmd_";
static const string RES = "_res_";
static const string SEQ = "_seq_";


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

void MainLoop::init(const ZAddr & addr, const string & uuid) {
    _uuid = uuid;
    _pushSock.connect(toStr(addr).c_str());

    _pullSock.bind("tcp://*:*");
    int cmdPort = getEndpoint(_pullSock).port;
    
    _pubSock.bind("tcp://*:*");
    int pubPort = getEndpoint(_pubSock).port;
    
    message("started", json("command", json("port", cmdPort), "media", json("port", pubPort)));
}

void MainLoop::run() {
    zmq::pollitem_t items[2] = {{_pullSock, ZMQ_POLLIN}, {_subSock, ZMQ_POLLIN}};
    try {
        while (_running) {
            int cnt = zmq::poll(items, 2, 1000);
            if (cnt == 0) {
                continue;
            }
            if (items[0].revents | ZMQ_POLLIN) {
                handleCommand();
            }
            if (items[0].revents | ZMQ_POLLERR) {
                throw Exception EX("error in command socket");
            }
            if (items[1].revents | ZMQ_POLLIN) {
                handleMedia();
            }
            if (items[1].revents | ZMQ_POLLERR) {
                throw Exception EX("error in media socket");
            }
        }
        Log<<"event loop terminated"<<endl;
    } catch (const exception & e) {
        Err<<"main loop error: "<<toStr(e)<<endl;
    }
}

void MainLoop::message(const std::string & msg, const Json::Value & value) {
    Json::Value json = value;
    json[MSG] = msg;
    json["uuid"] = _uuid;
    string str = json.toStyledString();
    zmq::message_t zmsg(str.size());
    memcpy(zmsg.data(), str.c_str(), str.size());
    _pushSock.send(zmsg);
}

void MainLoop::handleCommand() {
    zmq::message_t msg;
    if (!_pullSock.recv(&msg, ZMQ_DONTWAIT)) {
        return;
    }
    if (msg.size() == 0) {
        Err<<"received zero length command"<<endl;
        return;
    }
    const int8_t * data = (int8_t*) msg.data();
    if (data[0] == MSG_HALT) {
        _running = false;
        return;
    }
    if (data[0] == MSG_JSON) {
        handleCommandJson((const char *)(data + 1), msg.size() - 1);
        return;
    }
    Err<<"unknown command type: "<<toStr(data[0])<<endl;
}

void MainLoop::handleCommandJson(const char * data, size_t size) {
    Json::Value json = parseJson(data, size);
    if (json.isMember(MSG)) {
        
    }
    
}

void MainLoop::handleMedia() {
    
}



}
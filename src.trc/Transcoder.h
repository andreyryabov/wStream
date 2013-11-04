//
//  Transcoder.h
//  wStream
//
//  Created by Andrey Ryabov on 10/29/13.
//  Copyright (c) 2013 Andrey Ryabov. All rights reserved.
//

#ifndef __wStream__Transcoder__
#define __wStream__Transcoder__

#include "zmq.hpp"
#include "Common.h"

namespace Json {
class Value;
}

namespace wStream {

static const int MSG_HALT  = 1;
static const int MSG_JSON  = 2;
static const int MSG_PING  = 3;
static const int MSG_FRAME = 4;

class ZAddr {
  public:
    std::string proto;
    std::string address;
    int         port;
    
    ZAddr(const std::string & zaddr);
    ZAddr(const std::string & proto, const std::string & addr, int port = 0);
    
    friend std::ostream & operator << (std::ostream &, const ZAddr &);
  
    static ZAddr parse(const std::string &);
  private:
    static const std::regex RX_ADDR;
};

class MainLoop {
  public:
    void init(const ZAddr &, int64_t uuid);
    void run();    
    
    MainLoop();
  private:
    void message(const std::string & message, const Json::Value &);
    
    void handleCommand();
    void handleCommandJson(const Json::Value &);
    
    void handleMedia();
  
    zmq::context_t  _context;
    zmq::socket_t   _pullSock;
    zmq::socket_t   _pushSock;
    zmq::socket_t   _pubSock;
    zmq::socket_t   _subSock;
    
    int64_t  _uuid;
    bool     _running{true};
    
    using Timer = std::chrono::steady_clock;
    using Timestamp = Timer::time_point;
    
    Timestamp _pingTs;
};


}

#endif /* defined(__wStream__Transcoder__) */

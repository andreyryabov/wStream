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
#include <iostream>

namespace Json {
class Value;
}

namespace wStream {

static const int8_t MSG_HALT = 1;
static const int8_t MSG_JSON = 2;

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
    void init(const ZAddr &, const std::string & uuid);
    void run();    
    
    MainLoop();
  private:
    void message(const std::string & message, const Json::Value &);
    
    void handleCommand();
    void handleCommandJson(const char *, size_t);
    
    void handleMedia();
  
    zmq::context_t  _context;
    zmq::socket_t   _pullSock;
    zmq::socket_t   _pushSock;
    zmq::socket_t   _pubSock;
    zmq::socket_t   _subSock;
    
    std::string     _uuid;
    bool            _running{true};
};


}

#endif /* defined(__wStream__Transcoder__) */

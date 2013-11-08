//
//  MainLoop.h
//  wStream
//
//  Created by Andrey Ryabov on 10/29/13.
//  Copyright (c) 2013 Andrey Ryabov. All rights reserved.
//

#ifndef __wStream__MainLoop__
#define __wStream__MainLoop__


#include "Common.h"
#include "Transcoder.h"

namespace Json {
class Value;
}

namespace wStream {

static const int MSG_HALT  = 1;
static const int MSG_JSON  = 2;
static const int MSG_PING  = 3;
static const int MSG_FRAME = 4;
static const int MSG_CODEC = 5;

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

class Stream;
class MainLoop {
  public:
    void init(const ZAddr &, int64_t uuid);
    void run();    
    
    MainLoop();
  private:
    void message(const std::string & message, const Json::Value &);
    
    void onCommand();
    void handleCommandJson(const Json::Value &);
    void openStream(int sid, const std::string & name);
    void fileStream(const std::string & name);
    
    void onMedia();
  
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
    std::map<std::string, std::shared_ptr<Stream>> _streams;
};


}

#endif /* defined(__wStream__MainLoop__) */

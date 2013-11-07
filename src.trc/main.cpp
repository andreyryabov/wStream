//
//  main.cpp
//  wStream
//
//  Created by Andrey Ryabov on 10/21/13.
//  Copyright (c) 2013 Andrey Ryabov. All rights reserved.
//

#include "Utils.h"
#include "MainLoop.h"


using namespace std;
using namespace wStream;

void handleSignal(int sig) {
    Err<<"Cought signal: "<<sig<<endl;
    Err<<backtrace()<<endl;
    Err<<flush;
    exit(1);
}

int _test_VideoStream();


int main(int argc, const char * argv[]) {
    av_register_all();

    for (int sig : { SIGSEGV, SIGTERM, SIGINT, SIGHUP, SIGABRT}) {
        signal(sig, handleSignal);
    }
    
return _test_VideoStream();

    if (argc != 4) {
        Err<<"invalid arguments, use: "<<argv[0]<<" tcp://srv:port uuid logFile.log"<<endl;
        return -1;
    }
    
    string  addr = argv[1];
    string  logf = argv[3];
    int64_t uuid = stol(argv[2]);
    
    openLogFile(logf);

    const char * stars = " *\n *\n *\n";
    Log<<stars<<" * Starting id: "<<uuid<<", pushing to: "<<addr<<endl<<stars<<endl;
    
    try {
        MainLoop loop;
        loop.init(ZAddr(addr), uuid);
        loop.run();
        return 0;
    } catch (const exception & ex) {
        Err<<"exception in main: "<<toStr(ex)<<endl;
    } catch (...) {
        Err<<"unknown exception in main"<<endl;
    }    
    return -1;
}



int _test_VideoStream() {

    string fileName = "/Users/RKK2/workspace/wStream/data/testvid.flv";

    AVFormatContext * fmtCtx = nullptr;
    if (avformat_open_input(&fmtCtx, fileName.c_str(), nullptr, nullptr) < 0) {
        char buf[1024];
        char * res = getcwd(buf, sizeof(buf));
        cout<<"failed to open video file: "<<fileName;
        if (res) {
            cout<<", being in directory: "<<res;
        }
        cout<<endl;
        return -1;
    }
    
    if (avformat_find_stream_info(fmtCtx, nullptr) < 0) {
        cout<<"failed to find stream info"<<endl;
        return -1;
    }    
    
    int idx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, nullptr, 0);
    if (idx < 0) {
        cout<<"failed to find video stream"<<endl;
        return -1;
    }
    
    EncoderConfig enCfg;
    enCfg.name    = "mpeg1video";
    enCfg.bitrate = 60000;
    enCfg.width   = 144;
    enCfg.height  = 108;
    enCfg.gopSize = 100;
    
    Transcoder trc;
    trc.initDecoder("flv");
    trc.initEncoder(enCfg);
    
    ofstream mpg("/Users/RKK2/workspace/wStream/out.mpg", ios::binary|ios::trunc);
    
    AVPacket packet;
    apiIntEx(av_new_packet(&packet, 128 * 1024));
   

    while (av_read_frame(fmtCtx, &packet) >= 0) {
        if (packet.stream_index != idx) {
            continue;
        }
        trc.decode(packet.data, packet.size, packet.flags & AV_PKT_FLAG_KEY);
        cout<<"decoded: "<<packet.pts<<", key: "<<(packet.flags & AV_PKT_FLAG_KEY)<<endl;

        const void * data = nullptr;
        size_t size = 0;
        bool key = false;
        
        for (int i = 0; i < 1; i++ ) {
            if (trc.encode(data, size, key)) {
                mpg.write((char*)data, size);
                cout<<"encoded: "<<size_t(data)<<", size: "<<size<<", key: "<<key<<endl;
            }
        }
    }

    return 0;
}




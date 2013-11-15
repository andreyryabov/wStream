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

void ffmpegLog(void * ptr, int level, const char * fmt, va_list args) {
    char buf[1024];
    int pref = 0;
    av_log_format_line(ptr, level, fmt, args, buf, sizeof(buf) - 1, &pref);
    Err<<"ffmpeg: "<<buf<<endl;
}

int main(int argc, const char * argv[]) {
    av_register_all();
    av_log_set_callback(ffmpegLog);

    for (int sig : { SIGSEGV, SIGTERM, SIGINT, SIGHUP, SIGABRT}) {
        signal(sig, handleSignal);
    }    

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



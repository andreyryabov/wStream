//
//  main.cpp
//  wStream
//
//  Created by Andrey Ryabov on 10/21/13.
//  Copyright (c) 2013 Andrey Ryabov. All rights reserved.
//

extern "C" {
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#include "Utils.h"
#include "Transcoder.h"

using namespace std;
using namespace wStream;

void handleSignal(int sig) {
    Err<<"Cought signal: "<<sig<<endl;
    Err<<backtrace()<<endl;
    Err<<flush;
    exit(1);
}

int main(int argc, const char * argv[]) {
    for (int sig : { SIGSEGV, SIGTERM, SIGINT, SIGHUP}) {
        signal(sig, handleSignal);
    }

    if (argc != 4) {
        Err<<"invalid arguments, use: "<<argv[0]<<" tcp://srv:port uuid logFile.log"<<endl;
        return -1;
    }
    
    string addr = argv[1];
    string uuid = argv[2];
    string logf = argv[3];
    
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

/*
    av_register_all();
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
    
    AVCodec * codec = nullptr;
    int idx = av_find_best_stream(fmtCtx, AVMEDIA_TYPE_VIDEO, -1, -1, &codec, 0);
    if (idx < 0) {
        cout<<"failed to find video stream"<<endl;
        return -1;
    }
    
    AVStream * stream = fmtCtx->streams[idx];
    AVRational timeBase = stream->time_base;
    
    int width  = stream->codec->width;
    int height = stream->codec->height;
    
    cout<<"timebase="<<timeBase.num<<"/"<<timeBase.den<<", width=" <<width<<", height="<<height<<endl;
    
    if (avcodec_open2(stream->codec, codec, nullptr) < 0) {
        cout<<"failed to open codec"<<endl;
        return -1;
    }    
    
    AVPacket packet;
    if (av_new_packet(&packet, 128 * 1024) < 0) {
        cout<<"failed to create packet"<<endl;
        return -1;
    }
    
    AVCodec * encoder = avcodec_find_encoder(AV_CODEC_ID_MPEG1VIDEO);
    if (!encoder) {
        cout<<"failed to find encoder"<<endl;
        return -1;
    }

    AVCodecContext * encCtx = avcodec_alloc_context3(encoder);
    if (!encCtx) {
        cout<<"failed to create encoder context"<<endl;
        return -1;
    }
    
    encCtx->bit_rate  = 100000;
    encCtx->width     = stream->codec->width;
    encCtx->height    = stream->codec->height;
    encCtx->time_base = {1, 70};
    encCtx->gop_size  = 20;
    encCtx->pix_fmt   = AV_PIX_FMT_YUV420P;
    encCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
      
    if (avcodec_open2(encCtx, encoder, nullptr) < 0) {
        cout<<"failed to open codec"<<endl;
        return -1;
    }
    
    if (av_new_packet(&packet, 128 * 1024) < 0) {
        cout<<"failed to create packet"<<endl;
        return -1;
    }
    
    ofstream mpg("/Users/RKK2/workspace/wStream/out.mpg", ios::binary|ios::trunc);
    
    int cnt = 0, sum = 0;
    AVFrame * frame = avcodec_alloc_frame();
    while (av_read_frame(fmtCtx, &packet) >= 0) {
        if (packet.stream_index != idx) {
            continue;
        }
        
        int gotFrame = 0;
        int size = avcodec_decode_video2(stream->codec, frame, &gotFrame, &packet);
        if (size < 0) {
            cout<<"error frame decoder failed"<<endl;
            continue;
        }
        //cout<<"size decoded="<<size<<", packet="<<packet.size<<endl;
        if (!gotFrame) {
            continue;
        }
        //cout<<"ts="<<frame->pkt_pts<<", key frame="<<frame->key_frame<<", size="<<frame->pkt_size<<endl;
        
        
        AVPacket encPacket;
        av_init_packet(&encPacket);
        encPacket.data = nullptr;
        encPacket.size = 0;
        
        int gotPacket = 0;
        if (avcodec_encode_video2(encCtx, &encPacket, frame, &gotPacket) < 0) {
            cout<<"error encoding frame"<<endl;
            continue;
        }
        if (!gotPacket) {
            continue;
        }
        
        cout<<(cnt++)<<" - encoded size="<<encPacket.size<<endl;
        
        mpg.write((char*)encPacket.data, encPacket.size);
        
        sum+= encPacket.size;
        av_free_packet(&encPacket);
        

    }
    cout<<"sum="<<sum<<endl;
    
    
    av_free_packet(&packet);

    //TODO: free all objects
    avcodec_free_frame(&frame);
    
    return 0;
*/
}


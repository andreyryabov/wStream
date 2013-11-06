//
//  Transcoder.cpp
//  wStream
//
//  Created by Andrey Ryabov on 11/6/13.
//  Copyright (c) 2013 Andrey Ryabov. All rights reserved.
//

#include "Exception.h"
#include "Transcoder.h"

using namespace std;

namespace wStream {

Transcoder::Transcoder() {
    _frame = av_frame_alloc();
    av_init_packet(&_packet);
    _packet.data = nullptr;
    _packet.size = 0;
}

Transcoder::~Transcoder() {
    avcodec_close(_decCtx);
    av_free(_decCtx);
    avcodec_close(_encCtx);
    av_free(_encCtx);
    av_free_packet(&_packet);
}
    
void Transcoder::initDecoder(const string & codec) {
    if (_decoder) {
        if (_decoder->name == codec) {
            return;
        }
        avcodec_close(_decCtx);
        av_free(_decCtx);
        _decCtx = nullptr;
    }
    _decoder = avcodec_find_decoder_by_name(codec.c_str());
    if (!_decoder) {
        throw Exception EX("decoder not found: " + codec);
    }
    _decCtx = avcodec_alloc_context3(_decoder);
    if (!_decCtx) {
        throw Exception EX("failed to allocate decoder context");
    }
    if (avcodec_open2(_decCtx, _decoder, nullptr) < 0) {
        throw Exception EX("failed to open decoder context");
    }
}

void Transcoder::decode(const void * data, size_t size, bool isKey) {
    AVPacket packet;
    av_init_packet(&packet);
    packet.data = (uint8_t*)data;
    packet.size = int(size);
    if (isKey) {
        packet.flags |= AV_PKT_FLAG_KEY;
    }
    int gotFrame = 0;
    int res = avcodec_decode_video2(_decCtx, _frame, &gotFrame, &packet);
    if (res < 0) {
        Err<<"failed to decode packet"<<endl;
        return;
    }
    if (gotFrame) {
        _hasFrame = true;
    }    
}

void Transcoder::initEncoder(const EncoderConfig & config) {
    _encoder = nullptr;
    if (_encCtx) {
        avcodec_close(_encCtx);
        av_free(_encCtx);
        _encCtx = nullptr;
    }
    _encoder = avcodec_find_encoder_by_name(config.name);
    if (!_encoder) {
        throw Exception EX("failed to open encoder context");
    }
    _encCtx = avcodec_alloc_context3(_encoder);
    if (!_encCtx) {
        throw Exception EX("failed to open encoder context");
    }    
    _encCtx->bit_rate  = config.bitrate;
    _encCtx->width     = config.width;
    _encCtx->height    = config.height;
    _encCtx->time_base = {1, 25};
    _encCtx->gop_size  = config.gopSize;
    _encCtx->pix_fmt   = AV_PIX_FMT_YUV420P;
    _encCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    
    if (avcodec_open2(_encCtx, _encoder, nullptr) < 0) {
        throw Exception EX("failed to open decoder context");
    }
}

bool Transcoder::encode(const void * & ptr, size_t & size, bool & isKey) {
    if (!_hasFrame) {
        return false;
    }

    av_free_packet(&_packet);
    
    int gotPacket = 0;
    if (avcodec_encode_video2(_encCtx, &_packet, _frame, &gotPacket) < 0) {
        Err<<"failed to encode frame"<<endl;
        return false;
    }
    if (gotPacket) {
        ptr   = _packet.data;
        size  = _packet.size;
        isKey = _packet.flags & AV_PKT_FLAG_KEY;
    }
    return gotPacket;
}
    
}
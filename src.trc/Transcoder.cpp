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

const AVPixelFormat PIXEL_FORMAT = AV_PIX_FMT_YUV420P;

Transcoder::Transcoder() {
    _deFrame = av_frame_alloc();
    _scFrame = av_frame_alloc();
    av_init_packet(&_packet);
    _packet.data = nullptr;
    _packet.size = 0;    
}

Transcoder::~Transcoder() {
    avcodec_close(_deCtx);
    av_free(_deCtx);
    avcodec_close(_enCtx);
    av_free(_enCtx);
    av_free_packet(&_packet);
    avpicture_free ((AVPicture*)_scFrame);    
    av_frame_free(&_scFrame);
    sws_freeContext(_scaler);
}
    
void Transcoder::initDecoder(const string & codec) {
    if (_decoder) {
        if (_decoder->name == codec) {
            return;
        }
        avcodec_close(_deCtx);
        av_free(_deCtx);
        _deCtx = nullptr;
    }
    _decoder = avcodec_find_decoder_by_name(codec.c_str());
    if (!_decoder) {
        throw Exception EX("decoder not found: " + codec);
    }
    _deCtx = avcodec_alloc_context3(_decoder);
    if (!_deCtx) {
        throw Exception EX("failed to allocate decoder context");
    }
    if (avcodec_open2(_deCtx, _decoder, nullptr) < 0) {
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
    int res = avcodec_decode_video2(_deCtx, _deFrame, &gotFrame, &packet);
    if (res < 0) {
        Err<<"failed to decode packet"<<endl;
        return;
    }
    if (gotFrame) {
        _hasFrame = true;
    }    
}

void Transcoder::initEncoder(const EncoderConfig & cfg) {
    _encConfig = cfg;
    _encoder = nullptr;
    if (_enCtx) {
        avcodec_close(_enCtx);
        av_free(_enCtx);
        _enCtx = nullptr;
    }
    _encoder = avcodec_find_encoder_by_name(_encConfig.name);
    if (!_encoder) {
        throw Exception EX("failed to open encoder context");
    }
    _enCtx = avcodec_alloc_context3(_encoder);
    if (!_enCtx) {
        throw Exception EX("failed to open encoder context");
    }
    _enCtx->bit_rate  = _encConfig.bitrate;
    _enCtx->width     = _encConfig.width;
    _enCtx->height    = _encConfig.height;
    _enCtx->time_base = {1, 25};
    _enCtx->gop_size  = _encConfig.gopSize;
    _enCtx->pix_fmt   = PIXEL_FORMAT;
    _enCtx->strict_std_compliance = FF_COMPLIANCE_EXPERIMENTAL;
    
    avpicture_free ((AVPicture*)_scFrame);
    avpicture_alloc((AVPicture*)_scFrame, PIXEL_FORMAT, _encConfig.width, _encConfig.height);
    
    _scFrame->width   = _encConfig.width;
    _scFrame->height  = _encConfig.height;
    
    if (avcodec_open2(_enCtx, _encoder, nullptr) < 0) {
        throw Exception EX("failed to open decoder context");
    }
}

bool Transcoder::encode(const void * & ptr, size_t & size, bool & isKey) {
    if (!_hasFrame) {
        return false;
    }

    _scaler = sws_getCachedContext(_scaler,
            _deFrame->width, _deFrame->height, _deCtx->pix_fmt,
            _encConfig.width, _encConfig.height, PIXEL_FORMAT,
            SWS_FAST_BILINEAR, nullptr, nullptr, nullptr);
    
    sws_scale(_scaler, _deFrame->data, _deFrame->linesize, 0, _deFrame->height, _scFrame->data, _scFrame->linesize);
    _scFrame->pict_type = _deFrame->pict_type;

    av_free_packet(&_packet);
    
    int gotPacket = 0;
    if (avcodec_encode_video2(_enCtx, &_packet, _scFrame, &gotPacket) < 0) {
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
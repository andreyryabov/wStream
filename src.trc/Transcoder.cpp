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
    av_free_packet(&_packet);
    avpicture_free ((AVPicture*)_scFrame);    
    av_frame_free(&_scFrame);
    sws_freeContext(_scaler);
}
    
void Transcoder::initDecoder(const string & codec, const string & bitstream, uint8_t * ex, size_t exSize) {
    if (bitstream.size()) {
        _bitstreamFilter = decltype(_bitstreamFilter) {
            nullEx(av_bitstream_filter_init(bitstream.c_str()), EX("bitstream filter not found: " + bitstream)),
            [](AVBitStreamFilterContext * p) { av_bitstream_filter_close(p);}
        };
    } else {
        _bitstreamFilter = decltype(_bitstreamFilter)();
    }
    _decoder = nullEx(avcodec_find_decoder_by_name(codec.c_str()), EX("decoder not found: " + codec));
    _deCtx   = decltype(_deCtx) {
        nullEx(avcodec_alloc_context3(_decoder), EX("decoder context allocation failed")),
        [](AVCodecContext * c) { avcodec_close(c); av_free(c); }
    };
    _deCtx->extradata = ex;
    _deCtx->extradata_size = int(exSize);
    if (avcodec_open2(_deCtx.get(), _decoder, nullptr) < 0) {
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
    
    Holder<uint8_t> ref;
    if (_bitstreamFilter) {
        uint8_t * buf = nullptr;
        int bufSize = 0;
        int res = av_bitstream_filter_filter(
                    _bitstreamFilter.get(), nullptr, nullptr,
                    &buf, &bufSize, packet.data, packet.size, isKey);
        if (res > 0) {
            ref = Holder<uint8_t>(buf, [](uint8_t * p){ av_free(p);});
        }
        if (res < 0) {
            throw Exception EX("bitstream decoding failed");
        }
        packet.data = buf;
        packet.size = bufSize;
    }
        
    int gotFrame = 0;
    int res = avcodec_decode_video2(_deCtx.get(), _deFrame, &gotFrame, &packet);
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
    _encoder   = nullEx(avcodec_find_encoder_by_name(_encConfig.name), EX("encoder not found: " + toStr(cfg.name)));
    _enCtx     = decltype(_enCtx) {
        nullEx(avcodec_alloc_context3(_encoder), EX("encoder context allocation failed")),
        [](AVCodecContext * c) { avcodec_close(c); av_free(c); }
    };
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
    
    if (avcodec_open2(_enCtx.get(), _encoder, nullptr) < 0) {
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
    if (avcodec_encode_video2(_enCtx.get(), &_packet, _scFrame, &gotPacket) < 0) {
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
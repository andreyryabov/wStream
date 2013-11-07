//
//  Transcoder.h
//  wStream
//
//  Created by Andrey Ryabov on 11/6/13.
//  Copyright (c) 2013 Andrey Ryabov. All rights reserved.
//

#ifndef __wStream__Transcoder__
#define __wStream__Transcoder__

#include "Common.h"

namespace wStream {

struct EncoderConfig {
    const char * name;
    int width   = 144;
    int height  = 108;
    int bitrate = 50000;
    int gopSize = 15;
}; 

class Transcoder {
  public:
    void initDecoder(const std::string & name);
    void initEncoder(const EncoderConfig &);

    void decode(const void * data, size_t,  bool isKey = false);
    bool encode(const void * &, size_t &, bool & isKey);
    
    Transcoder();
    virtual ~Transcoder();
  private:
    EncoderConfig    _encConfig;
    SwsContext     * _scaler  { nullptr };
    AVCodec        * _decoder { nullptr };
    AVCodec        * _encoder { nullptr };
    AVCodecContext * _deCtx   { nullptr };
    AVCodecContext * _enCtx   { nullptr };
    AVFrame        * _deFrame { nullptr };
    AVFrame        * _scFrame { nullptr };
    AVPacket         _packet;    
    bool             _hasFrame{ false   };

};

}

#endif /* defined(__wStream__Transcoder__) */

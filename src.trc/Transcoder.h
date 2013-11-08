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
#include "Utils.h"

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
    void initDecoder(const std::string & name, const std::string & bitstream, uint8_t *, size_t);
    void initEncoder(const EncoderConfig &);

    void decode(const void * data, size_t,  bool isKey = false);
    bool encode(const void * &, size_t &, bool & isKey);
    
    Transcoder();
    virtual ~Transcoder();
  private:
public:// TODO: delete
    EncoderConfig                    _encConfig;
    Holder<AVCodecContext>           _deCtx;
    Holder<AVCodecContext>           _enCtx;
    Holder<AVBitStreamFilterContext> _bitstreamFilter;
    SwsContext     * _scaler  { nullptr };    
    AVCodec        * _decoder { nullptr };
    AVCodec        * _encoder { nullptr };
    AVFrame        * _deFrame { nullptr };
    AVFrame        * _scFrame { nullptr };
    AVPacket         _packet;    
    bool             _hasFrame{ false   };

};

}

#endif /* defined(__wStream__Transcoder__) */

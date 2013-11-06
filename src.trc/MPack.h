//
//  MPack.h
//  wStream
//
//  Created by Andrey Ryabov on 11/4/13.
//  Copyright (c) 2013 Andrey Ryabov. All rights reserved.
//

#ifndef wStream_MPack_h
#define wStream_MPack_h

#include "Common.h"

namespace wStream {

namespace mtype = msgpack::type;


class Packer {
  public:
    template<typename T_>
    Packer & put(const T_ & v) {
        msgpack::pack(_buf, v);
        return * this;
    }
    
    Packer & putRaw(const void * data, size_t size) {
        msgpack::packer<msgpack::sbuffer> b(_buf);
        b.pack_raw(size);
        b.pack_raw_body((const char*)data, size);
        return * this;
    }
    
    void clear() {
        _buf.clear();
    }    
    
    void * data() {
        return _buf.data();
    }
    
    size_t size() {
        return _buf.size();
    }
    
    Packer() {}
    
    explicit Packer(size_t size) : _buf(size) {
    }
      
  private:
    msgpack::sbuffer _buf;
};


/***************** Unpacker ***************************************************************/
template<msgpack::type::object_type>
struct UnionSelector {
};

#define UGET(itype_, ntype_, field_)\
template<>\
struct UnionSelector<msgpack::type::object_type::itype_> {\
    using NT = ntype_;\
    static ntype_ get(msgpack::object::union_type & v) { return v.field_; }\
};

UGET(BOOLEAN, bool, boolean);
UGET(POSITIVE_INTEGER, uint64_t, u64);
UGET(NEGATIVE_INTEGER, int64_t, i64);
UGET(DOUBLE, double, dec);
UGET(ARRAY,  msgpack::object_array, array);
UGET(MAP,    msgpack::object_map, map);
UGET(RAW,    msgpack::object_raw, raw);

#undef UGET

class Unpacker {
  public:
    template<msgpack::type::object_type T_>
    typename UnionSelector<T_>::NT get() {
        msgpack::unpacked res;
        _up.next(&res);        
        if (res.get().type != T_) {
            throw Exception EX("unexpected type: " + toStr(res.get().type) + ", must be: " + toStr(T_));
        }
        return UnionSelector<T_>::get(res.get().via);
    }
    
    std::string getStr() {
        msgpack::object_raw raw = get<msgpack::type::RAW>();
        return std::string(raw.ptr, raw.size);
    }    

    Unpacker(const void * data, size_t size) {
        _up.reserve_buffer(size);
        memcpy(_up.buffer(), data, size);
        _up.buffer_consumed(size);
    }
  private:
    msgpack::unpacker _up;
};
    
}

#endif

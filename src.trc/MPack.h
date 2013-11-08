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
template<typename>
struct UnionSelector {
};

#define UGET(itype_, ntype_, field_)\
template<>\
struct UnionSelector<ntype_> {\
    using NT = ntype_;\
    static msgpack::type::object_type getType() { return msgpack::type::itype_; }\
    static ntype_ getValue(msgpack::object::union_type & v) { return v.field_; }\
};


UGET(BOOLEAN, bool,    boolean);
UGET(POSITIVE_INTEGER, uint64_t, u64);
UGET(NEGATIVE_INTEGER, int64_t, i64);
UGET(DOUBLE, double,   dec);
UGET(ARRAY,  msgpack::object_array, array);
UGET(MAP,    msgpack::object_map, map);
UGET(RAW,    msgpack::object_raw, raw);

#undef UGET

class Unpacker {
  public:
    template<typename T_>
    bool next(T_ & v) {
        msgpack::unpacked res;
        if (!_up.next(&res)) {
            return false;
        }
        using S = UnionSelector<T_>;
        if (res.get().type != S::getType()) {
            throw Exception EX("invalid type: " + toStr(res.get().type) +
                               ", expected: " + toStr(UnionSelector<T_>::getType()));
        }
        v = S::getValue(res.get().via);
        return true;
    }
    
    template<typename C_>
    bool nextRaw(C_ & v) {
        msgpack::unpacked res;
        if (!_up.next(&res)) {
            return false;
        }
        if (res.get().type == msgpack::type::RAW) {
            typename C_::const_pointer ptr = reinterpret_cast<typename C_::const_pointer>(res.get().via.raw.ptr);
            v = C_(ptr, ptr + res.get().via.raw.size);
            return true;
        }
        throw Exception EX("invalid type: " + toStr(res.get().type) + ", raw expected");
    }
    
    bool next(std::vector<uint8_t> & v) {
        return nextRaw(v);
    }
    
    bool next(std::string & v) {
        return nextRaw(v);
    }
    
    bool next(int & v) {
        int64_t vv;
        bool res = next(vv);
        v = int(vv);
        return res;
    }
        
    bool next(int64_t & v) {
        msgpack::unpacked res;
        if (!_up.next(&res)) {
            return false;
        }
        if (res.get().type == msgpack::type::POSITIVE_INTEGER) {
            v = res.get().via.u64;
            return true;
        }
        if (res.get().type == msgpack::type::NEGATIVE_INTEGER) {
            v = int64_t(res.get().via.i64);
            return true;
        }
        throw Exception EX("invalid type: " + toStr(res.get().type) + ", integer expected");
    }
            
    bool next(uint64_t & v) {
        msgpack::unpacked res;
        if (!_up.next(&res)) {
            return false;
        }
        if (res.get().type == msgpack::type::POSITIVE_INTEGER) {
            v = res.get().via.u64;
            return true;
        }
        throw Exception EX("invalid type: " + toStr(res.get().type) + ", positive integer expected");
    }

    template<typename T_>
    T_ get() {
        T_ v;
        if (!next(v)) {
            throw Exception EX("unpack buffer underflow, expected: " + toStr(typeid(T_).name()));
        }
        return v;
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

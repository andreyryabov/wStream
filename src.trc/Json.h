//
//  Json.h
//  mKernel
//
//  Created by Andrey Ryabov on 7/6/13.
//  Copyright (c) 2013 Andrey Ryabov. All rights reserved.
//

#ifndef __mKernel__Json__
#define __mKernel__Json__

#include "Common.h"
#include "Exception.h"
#include "json/json.h"

namespace wStream {

inline std::string toStr(const Json::Value & v) {
    std::stringstream s;
    Json::StyledStreamWriter w("  ");
    w.write(s, v);
    return s.str();
}

/********** json builder ***********************************************/
template<typename V_, typename... ParamValue_>
const V_ & jsonValue(Json::Value & json, const V_ & v, ParamValue_... args);

template<typename K_, typename... ParamValue_>
void jsonKey(Json::Value & json,  const K_ & key, ParamValue_... args) {
    json[key] = jsonValue(json, args...);
}

inline void jsonKey(Json::Value & json) {
}

template<typename V_, typename... ParamValue_>
const V_ & jsonValue(Json::Value & json, const V_ & v, ParamValue_... args) {
    jsonKey(json, args...);
    return v;
}

template<typename... ParamValue_>
Json::Value json(ParamValue_... args) {
    Json::Value res;
    jsonKey(res, args...);
    return res;
}

inline Json::Value parseJson(const char * data, size_t size) {
    Json::Reader reader;
    Json::Value  result;
    if (!reader.parse(data, data + size, result, false)) {
        throw Exception EX("invalid json\n" + reader.getFormatedErrorMessages()
                         + "\n" + std::string(data, data + size));
    }
    return result;
}

inline Json::Value parseJson(const std::string & str) {
    Json::Reader reader;
    Json::Value result;
    if (!reader.parse(str, result)) {
        throw Exception EX("invalid json\n" + reader.getFormatedErrorMessages() + "\n" + str);
    }
    return result;
}

inline Json::Value parseJson(std::istream & in) {
    std::stringstream out;
    bool hasObj = false;
    int balance = 0;

    while (in) {
        char c;
        if (hasObj && !balance) {
            break;
        }
        in>>c;
        if (c == '{') {
            hasObj = true;
            balance++;
        }
        if (c == '}') {
            balance--;
        }
        if (hasObj) {
            out<<c;
        }        
    }
    return parseJson(out.str());    
}

//TODO: optimize
inline size_t serialize(const Json::Value & json, void * data, size_t maxSize) {
    std::string str = toStr(json);
    if (str.size() > maxSize) {
        throw Exception EX("failed to serialize json - buffer overflow");
    }
    memcpy(data, str.c_str(), str.length());
    return str.length();
}

}



#endif /* defined(__mKernel__Json__) */

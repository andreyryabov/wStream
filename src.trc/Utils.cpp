//
//  Utils.cpp
//  mKernel
//
//  Created by Andrey Ryabov on 7/20/13.
//  Copyright (c) 2013 Andrey Ryabov. All rights reserved.
//

#include "Utils.h"
#include <cxxabi.h>
#include <execinfo.h>


using namespace std;

namespace wStream {

std::ostream * _clog_ = &cout;
std::ofstream  _cFile_;

void openLogFile(const string & file) {
    _cFile_.open(file, ios_base::app|ios_base::out);
    _clog_ = &_cFile_;
}

string toHexStr(char byte) {
    return toHexStr((unsigned char) (byte));
}

string toHexStr(unsigned char byte) {
    stringstream s;
    s << hex << (unsigned int) (byte);
    string result = s.str();
    return (result.length() == 1) ? ("0x0" + result) : ("0x" + result);
}

string toHexStr(const void * ptr, size_t size, size_t wrap) {
    const unsigned char * src = static_cast<const unsigned char *> (ptr);
    stringstream s;
    for (size_t i = 0; i < size; i++) {
        if (i) {
            s<<" ";
            if (wrap && !(i % wrap)) {
                s<<endl;
            }
        }
        s<<toHexStr(src[i]);
    }
    return s.str();
}

string dumpStr(const void * ptr, size_t size, size_t limit) {
    const unsigned char * cPtr = (const unsigned char*) ptr;
    stringstream str;
    if (limit) {
        size = min(size, limit + 1);
    }
    if (size == 0) {
        str<<"empty";
    }
    for (int i = 0; i < size; ++i ) {
        if (32 <= cPtr[i] && cPtr[i] < 127) {
            str<<cPtr[i];
        } else {
            str<<" "<<toHexStr(cPtr[i]);
        }
    }
    const string & res = str.str();
    if (int(res.size()) > limit) {
        return res.substr(0, limit - 3) + "...";
    }
    return res;
}

string toLower(const string & s) {
    string ss = s;
    transform(ss.begin(), ss.end(), ss.begin(), [](char c){return tolower(c);});
    return ss;
}

string demangled(const string & sym) {
    int status = 0;
    unique_ptr<char, void(*)(void*)> name(abi::__cxa_demangle(sym.c_str(), nullptr, nullptr, &status), std::free);
    return status ? sym : name.get();
}

string backtrace() {
    stringstream result;
    void *array[256];
    int size = ::backtrace(array, 100);
    char ** symbols = backtrace_symbols(array, size);

    for (int i = 0; i < size; i++ ) {
        string line = symbols[i];
        stringstream ss(line);
        string tmp;
        ss>>tmp>>tmp>>tmp>>tmp;
        if (ss && tmp != "0x0") {
            string dem = demangled(tmp);
            size_t pos = line.find(tmp);
            if (pos != string::npos) {
                line.replace(pos, tmp.size(), dem);
            }
        }
        result<<line<<endl;
    }
    free(symbols);
    return result.str();
}

}

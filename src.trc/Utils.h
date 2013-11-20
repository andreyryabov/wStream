//
//  Utils.h
//  mKernel
//
//  Created by Andrey Ryabov on 6/28/13.
//  Copyright (c) 2013 Andrey Ryabov. All rights reserved.
//

#ifndef mKernel_Utils_h
#define mKernel_Utils_h

#include "Common.h"
#include "Exception.h"

#define EX(str) ((toStr(__FILE__) + ":" + toStr(__LINE__) + " [" + __PRETTY_FUNCTION__ + "] " + wStream::toStr(str)) + "\n" + wStream::backtrace())

#define LOC ((toStr(__FILE__) + ":" + toStr(__LINE__) + " [" + toStr(__PRETTY_FUNCTION__) + "]\n> "))

#define Err ((*wStream::_clog_)<<"ERROR ["<<wStream::timeNowStr()<<"] at "<<LOC)
#define Log ((*wStream::_clog_)<<"INFO  ["<<wStream::timeNowStr()<<"] at "<<LOC)

#define apiVarEx(varName, invocation) \
    decltype(invocation) varName = invocation;\
    if (!varName) {\
        throw Exception EX(#invocation" - failed");\
    }

#define apiIntEx(invocation) {\
    int res = invocation;\
    if (res < 0) {\
        throw Exception EX(#invocation" - failed");\
    }\
}


namespace wStream {

template<typename T_>
T_ * nullEx(T_ * v, const std::string & msg) {
    if (!v) {
        throw Exception(msg);
    }
    return v;
}

void openLogFile(const std::string &);

extern std::ostream * _clog_;

class NonCopyable {
  public:
    NonCopyable() {}
  private:
    NonCopyable(const NonCopyable &);
    NonCopyable & operator = (const NonCopyable &);
};


inline std::string timeNowStr() {
    using namespace std;
    using namespace chrono;
    time_t now_c = system_clock::to_time_t(system_clock::now());
    stringstream s;
    s<<put_time(localtime(&now_c), "%F %T");
    return s.str();
}

/*************** Holder *********************************************/
template<typename T_>
class Holder : std::unique_ptr<T_, std::function<void(T_*)>> {
    typedef std::unique_ptr<T_, std::function<void(T_*)>> Base;
  public:
    using Type = T_;
  
    T_ * operator -> () const {
        return Base::operator->();
    }
    
    T_ * get() const {
        return Base::get();
    }
    
    operator bool() const {
        return Base::operator bool();
    }
    
    T_ * release() {
        return Base::release();
    }
    
    Holder(T_ * h = nullptr) : Holder<T_>(h, std::default_delete<T_>()) {
    }
    
    Holder(T_ * h, const std::function<void(T_*)> & f) : Base(h, f) {
    }
};

template<typename T_>
Holder<T_> holder(T_ * h) {
    return Holder<T_>(h);
}


template<typename T_, typename D_>
Holder<T_> holder(T_ * h, D_ deleter) {
    return Holder<T_>(h, deleter);
}

template<typename T_>
using Ptr = std::shared_ptr<T_>;


/*************** callbacks ******************************************/
template<typename T_>
using param = std::conditional<
    std::is_trivially_copyable<T_>::value,
        T_,
        typename std::add_lvalue_reference<const T_>::type>;

template<typename P_>
struct CallbackT {
    typedef typename std::function<void(P_)> type;
};

template<>
struct CallbackT<void> {
    typedef typename std::function<void()> type;
};

template <typename P_=void>
using Callback = typename CallbackT<P_>::type;


/*************** toStr ******************************************/
template<typename T>
std::string toStr(const T & t) {
    std::stringstream s;
    s<<t;
    return s.str();
}

inline std::string toStr(int8_t b) {
    return toStr(int(b));
}

inline std::string toStr(uint8_t b) {
    return toStr(int(b));
}

inline std::string toStr(const std::exception & e) {
    return toStr(e.what());
}

inline std::string toStr(const char * msg) {
    return msg;
}

template<typename T>
std::string toStr(const T * v) {
    if (!v) {
        return "null";
    }
    std::stringstream s;
    s<<std::hex<<(unsigned long)(v);
    return s.str();
}

template<typename T_>
std::string toStr(const std::vector<T_> & v) {
    std::stringstream s;
    s<<v.size()<<"={";
    auto it = v.begin(), end = v.end();
    bool coma = false;
    for (; it != end; ++it ) {
        if (coma) {
            s<<", ";
        } else {
            coma = true;
        }
        s<<toStr(*it);
    }
    s<<"}";
    return s.str();
}

template<typename T_, int N_>
std::string toStr(const T_ (&array)[N_]) {
    std::stringstream s;
    s<<N_<<"=[";
    bool coma = false;
    for (auto & v : array) {
        if (coma) {
            s<<", ";
        } else {
            coma = true;
        }
        s<<toStr(v);
    }
    s<<"]";
    return s.str();
}

/*********** byte array printing functions *********************************/
std::string toHexStr(char byte);
std::string toHexStr(unsigned char byte);
std::string toHexStr(const void * ptr, size_t size, size_t wrap = 80);
std::string dumpStr (const void * ptr, size_t size, size_t limit = 0);


template<typename T_>
std::string typeName(const T_ & t) {
    return typeid(t).name();
}


/*************** Signal/Slot ******************************************/
template<typename... Args>
class Signal;

template<typename... Args>
class Slot {
  public:
    typedef Signal<Args...> Peer;
        
    Slot(const std::function<void(Args...)> & f, const std::string & name = "")
        : _name(name), _callback(f) {
    }
    
    template<typename Class>
    Slot(Class * this_, void(Class::*method)(Args...), const std::string & name = "")
        : _name(name), _callback([=](Args... args) { (this_->*method)(args...); }) {
    }
    
    virtual ~Slot() {
        if (_signal) {
            _signal->remove(*this);
        }
    }
  protected:
    std::string _name;
    
  private:
    friend class Signal<Args...>;
  
    Slot(const Slot &) = delete;
    Slot & operator = (const Slot &) = delete;
    
    Signal<Args...> * _signal = nullptr;
    std::function<void(Args...)> _callback;
};


template<typename... Args>
class Signal : public Slot<Args...> {
  public:
    typedef Slot<Args...> Peer;

    bool enabled() const {
        return _enabled;
    }
    
    void enable(bool val) {
        _enabled = val;
    }
    
    void emit(Args... args) {
        if (!_enabled) {
            return;
        }
        for (auto p : _slots) {
            if (p) {
                p->_callback(args...);
            }
        }
    }
    
    void join(Peer & s) {
        assert(&s != this);
        std::cerr<<"signal: "<<Peer::_name<<", joins slot: "<<s._name<<std::endl;
        assert(!s._signal);
        _slots.push_back(&s);
        s._signal = this;
    }
        
    void remove(Peer & s) {
        std::cerr<<"signal: "<<Peer::_name<<", removes slot: "<<s._name<<std::endl;
        auto it = std::find(_slots.begin(), _slots.end(), &s);
        assert(it != _slots.end());
        _slots.erase(it);
        s._signal = nullptr;
    }
    
    Signal(const std::string & name = "") : Slot<Args...>(this, &Signal::emit, name) {
    }
    
    ~Signal() {
        auto slots = _slots;
        for (auto s : _slots) {
            remove(*s);
        }
    }
  private:
    bool _enabled = true;
    std::vector<Slot<Args...>*> _slots;
};

template<typename... Args>
void connect(Signal<Args...> & signal, Slot<Args...> & slot) {
    signal.join(slot);
}

template<typename... Args>
void connect(Slot<Args...> & signal, Signal<Args...> & slot) {
    signal.join(slot);
}

/*************** SignalState ******************************************/
template<typename T_>
class SignalState : public Signal<T_> {
  public:
    operator const T_ & () const {
        return _value;
    }
    
    SignalState & operator = (const T_ & t) {
        if (_value != t) {
            _value = t;
            Signal<T_>::emit(_value);
        }
        return * this;
    }
    
    SignalState(const T_ & v = T_(), const std::string & name = "")
        : Signal<T_>(name), _value(v) {
    }
  private:
    NonCopyable _nonCopy;    
    T_          _value;
};



/**********************************************************************/
std::string toLower(const std::string &);
std::string backtrace();

}

#endif

//
//  Exception.h
//  mKernel
//
//  Created by Andrey Ryabov on 6/28/13.
//  Copyright (c) 2013 Andrey Ryabov. All rights reserved.
//

#ifndef mKernel_Exception_h
#define mKernel_Exception_h

#include <stdexcept>
#include "Utils.h"


#define EX(str) ((toStr(__FILE__) + ":" + toStr(__LINE__) + " [" + __PRETTY_FUNCTION__ + "] " + wStream::toStr(str)))

namespace wStream {

class Exception : public std::runtime_error {
  public:
    Exception(const std::string & message) : std::runtime_error(message) {}
};

}

#endif

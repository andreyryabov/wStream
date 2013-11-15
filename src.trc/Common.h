//
//  Common.h
//  mKernel
//
//  Created by Andrey Ryabov on 8/6/13.
//  Copyright (c) 2013 Andrey Ryabov. All rights reserved.
//

#ifndef mKernel_Common_h
#define mKernel_Common_h

#include <set>
#include <queue>
#include <regex>
#include <ctime>
#include <thread>
#include <chrono>
#include <vector>
#include <fstream>
#include <iomanip>
#include <cstring>
#include <cstdlib>
#include <sstream>
#include <cassert>
#include <unistd.h>
#include <iostream>
#include <algorithm>
#include <msgpack.hpp>
#include "zmq.hpp"

extern "C" {
#include <libswscale/swscale.h>
#include <libavcodec/avcodec.h>
#include <libavformat/avformat.h>
}

#endif

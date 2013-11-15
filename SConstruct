import os

env = Environment(
        ENV      = os.environ,
        CC       = 'clang',
        CCFLAGS  = '-O3 -g',
        CXX      = 'clang++', 
        CXXFLAGS = '-std=c++11 -stdlib=libc++',
        LD       = 'clang++',
)

if env["PLATFORM"] == "posix":
    env.Append(CCFLAGS = ' -D__STDC_LIMIT_MACROS -D__STDC_CONSTANT_MACROS')

libs = Split("""
    c++
    zmq
    json
    avutil
    swscale
    msgpack
    avcodec
    avdevice
    avfilter
    avformat
""")

libDirs = []
if env["PLATFORM"] == "darwin":
    libDirs += ['externals/lib/osx64']
    
if env["PLATFORM"] == "posix":
    libDirs += ['externals/lib/lin64']

srcs     = Glob('src.trc/*.cpp') + Glob('externals/src/*.cpp')
includes = ['externals/include']

env.Program('wstream', 
    srcs,
    LIBS    = libs,
    LIBPATH = libDirs,
    CPPPATH = includes,
)

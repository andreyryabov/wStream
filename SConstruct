import os

env = Environment(
        ENV      = os.environ,
        CC       = 'clang',
        CCFLAGS  = '-O3 -g',
        CXX      = 'clang++', 
        CXXFLAGS = '-std=c++11 -stdlib=libc++',
        LD       = 'clang++',
)

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

libDirs  = ['externals/lib/osx64']
srcs     = Glob('src.trc/*.cpp') + Glob('externals/src/*.cpp')
includes = ['externals/include']

env.Program('wstream', 
    srcs,
    LIBS    = libs,
    LIBPATH = libDirs,
    CPPPATH = includes,
)

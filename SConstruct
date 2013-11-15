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
    libzmq.a
    libjson.a
    libavutil.a
    libswscale.a
    libmsgpack.a
    libavcodec.a
    libavdevice.a
    libavfilter.a
    libavformat.a
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

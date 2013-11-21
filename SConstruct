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

sLibs = Split("""
    zmq
    avutil
    swscale
    msgpack
    avcodec
    avdevice
    avfilter
    avformat
""")

sLibDir = "/usr/local/lib";
libs = [File(sLibDir + "/lib" + n + ".a") for n in sLibs]

if env["PLATFORM"] == "darwin":
    libs += [File("externals/lib/osx64" + "/libjson.a")]
    
if env["PLATFORM"] == "posix":
    libs += [File("externals/lib/lin64" + "/libjson.a")]

libs += ["c++", "z", "bz2"]

srcs     = Glob('src.trc/*.cpp') + Glob('externals/src/*.cpp')
includes = ['externals/include']

env.Program('wstream', 
    srcs,
    LIBS    = libs,
    CPPPATH = includes,
)

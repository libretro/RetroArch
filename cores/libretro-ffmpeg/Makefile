LIBRETRO_SWITCH        = 0

INTERNAL_LIBAVCODEC    = 0
INTERNAL_LIBAVFORMAT   = 0
INTERNAL_LIBAVUTIL     = 0
INTERNAL_LIBSWRESAMPLE = 0
INTERNAL_LIBSWSCALE    = 0

INTERNAL_LIBFLAC       = 0
INTERNAL_LIBOGG        = 0
INTERNAL_LIBLAME       = 0
INTERNAL_LIBFAAC       = 0
INTERNAL_LIBSPEEX      = 0
INTERNAL_LIBTHEORA     = 0
INTERNAL_LIBOPUS       = 0  
INTERNAL_LIBVORBIS     = 0
INTERNAL_WEBP          = 0
INTERNAL_ZLIB          = 0
INTERNAL_WAVPACK       = 0
INTERNAL_XVIDCORE      = 0
INTERNAL_VPX           = 0


BAKE_IN_FFMPEG := 0

#CPU Optimization flags
HAVE_SSE2 = 0
HAVE_MMX = 0
HAVE_THREADS = 1

#Additional codec support for internal FFmpeg
HAVE_WAVPACK   = 0
HAVE_LIBSPEEX  = 0
HAVE_LIBWEBP   = 0
HAVE_LIBOGG    = 0
HAVE_LIBX264   = 0
HAVE_LIBX265   = 0
HAVE_LIBFAAC   = 0
HAVE_LIBFLAC   = 0
HAVE_LIBLAME   = 0
HAVE_LIBTWOLAME = 0
HAVE_LIBVORBIS = 0
HAVE_LIBTHEORA = 0
HAVE_LIBOPUS   = 0
HAVE_ZLIB      = 0

#Video support
OPENGL = 1
HAVE_CODEC_HW = 0

CORE_DIR := .

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
else ifneq ($(findstring win,$(shell uname -a)),)
   platform = win
endif
endif

system = $(shell uname -m)

ifeq ($(system),x86_64)
ARCH_X86 = 1
ARCH_X86_64 = 1
endif

TARGET_NAME := ffmpeg

ifneq (,$(findstring unix,$(platform)))
	ARCH_X86 = 1
   TARGET := $(TARGET_NAME)_libretro.so
   fpic := -fPIC
   SHARED := -shared -Wl,--version-script=link.T -Wl,--no-undefined -fPIC
ifeq ($(OPENGL),1)
   GL_LIB := -lGL
   CFLAGS += -DHAVE_OPENGL
   HAVE_GL := 1
	HAVE_GL_FFT := 1
endif
   HAVE_SSA := 1

   LIBS = $(shell pkg-config libass --libs) -pthread
   CFLAGS += $(shell pkg-config libass --cflags) -pthread
	CFLAGS += -DHAVE_STRUCT_SOCKADDR_STORAGE -DHAVE_STRUCT_ADDRINFO -DHAVE_FMINF
	HAVE_POLL_H = 1
	HAVE_GETADDRINFO = 1
	HAVE_NETWORK = 1
	HAVE_SOCKLEN = 1
	HAVE_PTHREADS = 1
ifneq ($(INTERNAL_LIBAVCODEC),1)
   LIBS += $(shell pkg-config libavcodec --libs)
   CFLAGS += $(shell pkg-config libavcodec --cflags)
endif
ifneq ($(INTERNAL_LIBAVFORMAT),1)
   LIBS += $(shell pkg-config libavformat --libs)
   CFLAGS += $(shell pkg-config libavformat --cflags)
endif
ifneq ($(INTERNAL_LIBSWRESAMPLE),1)
   LIBS += $(shell pkg-config libswresample --libs)
   CFLAGS += $(shell pkg-config libswresample --cflags)
endif
ifneq ($(INTERNAL_LIBAVUTIL),1)
   LIBS += $(shell pkg-config libavutil --libs)
   CFLAGS += $(shell pkg-config libavutil --cflags)
endif
ifneq ($(INTERNAL_LIBSWSCALE),1)
   LIBS += $(shell pkg-config libswscale --libs)
   CFLAGS += $(shell pkg-config libswscale --cflags)
endif
else ifneq (,$(findstring osx,$(platform)))
	ARCH_X86 = 1
   LIBRETRO_SWITCH = 1
   TARGET := $(TARGET_NAME)_libretro.dylib
   fpic := -fPIC
   SHARED := -dynamiclib
   CFLAGS += -I. -I.. -DHAVE_FMINF
ifeq ($(OPENGL),1)
   GL_LIB := -framework OpenGL
   HAVE_GL := 1
   HAVE_GL_FFT := 1
   CFLAGS += -DHAVE_GL
endif
HAVE_PTHREADS = 1
   OSXVER = `sw_vers -productVersion | cut -d. -f 2`
   OSX_LT_MAVERICKS = `(( $(OSXVER) <= 9)) && echo "YES"`
   fpic += -mmacosx-version-min=10.1
	CFLAGS += -DHAVE_STRUCT_SOCKADDR_STORAGE -DHAVE_STRUCT_ADDRINFO
	HAVE_POLL_H = 1
	HAVE_GETADDRINFO = 1
	HAVE_NETWORK = 1
	HAVE_SOCKLEN = 1

   LIBS = $(shell pkg-config libass --libs) -pthread
   CFLAGS += $(shell pkg-config libass --cflags) -pthread
ifneq ($(INTERNAL_LIBAVCODEC),1)
   LIBS += $(shell pkg-config libavcodec --libs)
   CFLAGS += $(shell pkg-config libavcodec --cflags)
endif
ifneq ($(INTERNAL_LIBAVFORMAT),1)
   LIBS += $(shell pkg-config libavformat --libs)
   CFLAGS += $(shell pkg-config libavformat --cflags)
endif
ifneq ($(INTERNAL_LIBSWRESAMPLE),1)
   LIBS += $(shell pkg-config libswresample --libs)
   CFLAGS += $(shell pkg-config libswresample --cflags)
endif
ifneq ($(INTERNAL_LIBAVUTIL),1)
   LIBS += $(shell pkg-config libavutil --libs)
   CFLAGS += $(shell pkg-config libavutil --cflags)
endif
ifneq ($(INTERNAL_LIBSWSCALE),1)
   LIBS += $(shell pkg-config libswscale --libs)
   CFLAGS += $(shell pkg-config libswscale --cflags)
endif
else ifneq (,$(findstring win,$(platform)))
	ARCH_X86 = 1
   CC = gcc
	CXX = g++
   TARGET := $(TARGET_NAME)_libretro.dll
   SHARED := -shared -static-libgcc -static-libstdc++ -s -Wl,--version-script=link.T -Wl,--no-undefined
   CFLAGS += -Iffmpeg
ifeq ($(OPENGL),1)
   GL_LIB := -lopengl32
   CFLAGS += -DHAVE_OPENGL
   HAVE_GL := 1
	HAVE_GL_FFT := 1
	WIN32_PLATFORM := 1
endif
   LIBS += -L.
ifneq ($(INTERNAL_LIBAVCODEC),1)
   LIBS += -lavcodec
endif
ifneq ($(INTERNAL_LIBAVFORMAT),1)
   LIBS += -lavformat
endif
ifneq ($(INTERNAL_LIBSWRESAMPLE),1)
   LIBS += -lswresample
endif
ifneq ($(INTERNAL_LIBAVUTIL),1)
   LIBS += -lavutil
endif
ifneq ($(INTERNAL_LIBSWSCALE),1)
	LIBS += -lswscale
endif
endif

ifeq ($(LIBRETRO_SWITCH),1)
CFLAGS += -DLIBRETRO_SWITCH=1
INTERNAL_LIBAVCODEC    = 1
INTERNAL_LIBAVFORMAT   = 1
INTERNAL_LIBAVUTIL     = 1
INTERNAL_LIBSWRESAMPLE = 1
INTERNAL_LIBSWSCALE    = 1

INTERNAL_LIBFLAC       = 1
INTERNAL_LIBOGG        = 1
INTERNAL_LIBLAME       = 1
INTERNAL_LIBFAAC       = 1
INTERNAL_LIBVORBIS     = 1
INTERNAL_ZLIB          = 1
endif

ifeq ($(INTERNAL_LIBAVCODEC),1)
	BAKE_IN_FFMPEG += 1
endif

ifeq ($(INTERNAL_LIBAVFORMAT),1)
	BAKE_IN_FFMPEG += 1
endif

ifeq ($(INTERNAL_LIBSWRESAMPLE),1)
	BAKE_IN_FFMPEG += 1
endif

ifeq ($(INTERNAL_LIBWSCALE),1)
	BAKE_IN_FFMPEG += 1
endif

ifeq ($(INTERNAL_LIBAVUTIL),1)
	BAKE_IN_FFMPEG += 1
endif

CFLAGS += -D__LIBRETRO__

include Makefile.common

CFLAGS += $(DEFINES) $(INCFLAGS)
CFLAGS += -Wall $(fpic)

ifeq ($(DEBUG), 1)
   CFLAGS += -O0 -g
else
   CFLAGS += -O3
endif

ifeq ($(HAVE_NEON),1)
	CFLAGS += -DHAVE_INTRINSICS_NEON=1
else
	CFLAGS += -DHAVE_INTRINSICS_NEON=0
endif

OBJECTS := $(SOURCES_C:.c=.o) $(SOURCES_CXX:.cpp=.o)

all: $(TARGET)

%.o: %.c
	$(CC) -c -std=gnu99 -o $@ $< $(CFLAGS)

%.o: %.cpp
	$(CXX) -c -fno-strict-aliasing -o $@ $< $(CFLAGS)

$(TARGET): $(OBJECTS)
	$(CXX) -o $@ $^ $(LIBS) $(SHARED)

clean:
	rm -f $(OBJECTS)
	rm -f $(TARGET)

.PHONY: clean


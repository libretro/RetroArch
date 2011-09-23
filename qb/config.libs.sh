. qb/qb.libs.sh

check_switch_c C99 -std=gnu99
check_critical C99 "Cannot find C99 compatible compiler."

# There are still broken 64-bit Linux distros out there. :)
if [ -d /usr/lib64 ]; then
   add_library_dirs /usr/lib64
fi

if [ -d /opt/local/lib ]; then
   add_library_dirs /opt/local/lib
fi

if [ "$OS" = BSD ]; then
   DYLIB=-lc
else
   DYLIB=-ldl
fi

if [ "$HAVE_DYNAMIC" = "yes" ] && [ "$HAVE_CONFIGFILE" = "no" ]; then
   echo "Cannot have dynamic loading of libsnes and no configfile support."
   echo "Dynamic loading requires config file support."
   exit 1
fi

if [ $HAVE_DYNAMIC != yes ]; then
   check_lib_cxx SNES $LIBSNES snes_init $DYLIB
   check_critical SNES "Cannot find libsnes."
   add_define_make libsnes $LIBSNES
fi

check_lib DYLIB $DYLIB dlopen
check_lib NETPLAY -lc socket

check_pkgconf ALSA alsa
check_header OSS sys/soundcard.h
check_header OSS_BSD soundcard.h
check_lib OSS_LIB -lossaudio

if [ "$OS" = "Darwin" ]; then
   check_lib AL "-framework OpenAL" alcOpenDevice
else
   check_lib AL -lopenal alcOpenDevice
fi

if [ "$OS" = "Darwin" ]; then
   check_lib FBO "-framework OpenGL" glFramebufferTexture2D
else
   check_lib FBO -lGL glFramebufferTexture2D
fi

check_pkgconf RSOUND rsound 1.1
check_pkgconf ROAR libroar
check_pkgconf JACK jack 0.120.1
check_pkgconf PULSE libpulse

check_lib COREAUDIO "-framework AudioUnit" AudioUnitInitialize

check_pkgconf SDL sdl 1.2.10
check_critical SDL "Cannot find SDL library."

# On some distros, -lCg doesn't link against -lstdc++ it seems ...
check_lib_cxx CG -lCg cgCreateContext

check_pkgconf XML libxml-2.0
check_pkgconf SDL_IMAGE SDL_image

if [ $HAVE_FFMPEG != no ]; then
   check_pkgconf AVCODEC libavcodec
   check_pkgconf AVFORMAT libavformat
   check_pkgconf AVUTIL libavutil
   check_pkgconf SWSCALE libswscale

   ( [ $HAVE_FFMPEG = auto ] && ( [ $HAVE_AVCODEC = no ] || [ $HAVE_AVFORMAT = no ] || [ $HAVE_AVUTIL = no ] || [ $HAVE_SWSCALE = no ] ) && HAVE_FFMPEG=no ) || HAVE_FFMPEG=yes
fi

if [ $HAVE_FFMPEG = yes ]; then
   check_lib FFMPEG_ALLOC_CONTEXT3 "$AVCODEC_LIBS" avcodec_alloc_context3
   check_lib FFMPEG_AVCODEC_OPEN2 "$AVCODEC_LIBS" avcodec_open2
   check_lib FFMPEG_AVIO_OPEN "$AVFORMAT_LIBS" avio_open
   check_lib FFMPEG_AVFORMAT_WRITE_HEADER "$AVFORMAT_LIBS" avformat_write_header
fi

check_pkgconf SRC samplerate

check_lib DYNAMIC $DYLIB dlopen

check_pkgconf FREETYPE freetype2
check_pkgconf XVIDEO xv

check_lib STRL -lc strlcpy

check_pkgconf PYTHON python3

add_define_make OS $OS

# Creates config.mk and config.h.
VARS="ALSA OSS OSS_BSD OSS_LIB AL RSOUND ROAR JACK COREAUDIO PULSE SDL DYLIB CG XML SDL_IMAGE DYNAMIC FFMPEG AVCODEC AVFORMAT AVUTIL SWSCALE SRC CONFIGFILE FREETYPE XVIDEO NETPLAY FBO STRL PYTHON FFMPEG_ALLOC_CONTEXT3 FFMPEG_AVCODEC_OPEN2 FFMPEG_AVIO_OPEN FFMPEG_AVFORMAT_WRITE_HEADER"
create_config_make config.mk $VARS
create_config_header config.h $VARS


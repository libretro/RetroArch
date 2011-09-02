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
check_critical SDL "Cannot find SDL 1.2 library."
check_pkgconf SDL_NEW sdl 1.3
if [ $HAVE_SDL_NEW = yes ]; then
   echo "SSNES is not compatible with SDL 1.3 API, please use a recent 1.2 release."
   exit 1
fi

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

check_pkgconf SRC samplerate

check_lib DYNAMIC $DYLIB dlopen

check_pkgconf FREETYPE freetype2
check_pkgconf XVIDEO xv

check_lib STRL -lc strlcpy

check_pkgconf PYTHON python3

add_define_make OS $OS

# Creates config.mk and config.h.
VARS="ALSA OSS OSS_BSD OSS_LIB AL RSOUND ROAR JACK COREAUDIO PULSE SDL DYLIB CG XML SDL_IMAGE DYNAMIC FFMPEG AVCODEC AVFORMAT AVUTIL SWSCALE SRC CONFIGFILE FREETYPE XVIDEO NETPLAY FBO STRL PYTHON"
create_config_make config.mk $VARS
create_config_header config.h $VARS


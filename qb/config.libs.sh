check_switch_c C99 -std=gnu99  "Cannot find C99 compatible compiler."

check_switch_c NOUNUSED -Wno-unused-result
add_define_make NOUNUSED "$HAVE_NOUNUSED"

# There are still broken 64-bit Linux distros out there. :)
[ -d /usr/lib64 ] && add_library_dirs /usr/lib64

[ -d /opt/local/lib ] && add_library_dirs /opt/local/lib

if [ "$OS" = 'BSD' ]; then DYLIB=-lc; else DYLIB=-ldl; fi
[ "$OS" = 'OSX' ] && HAVE_X11=no # X11 breaks on recent OSXes even if present.

[ -d /opt/vc/lib ] && add_library_dirs /opt/vc/lib
check_lib VIDEOCORE -lbcm_host bcm_host_init "-lvcos -lvchiq_arm" 

if [ "$HAVE_VIDEOCORE" = 'yes' ]; then
   [ -d /opt/vc/include ] && add_include_dirs /opt/vc/include
   [ -d /opt/vc/include/interface/vcos/pthreads ] && add_include_dirs /opt/vc/include/interface/vcos/pthreads
   HAVE_GLES='yes'
   HAVE_VG='yes'
fi

if [ "$LIBRETRO" ]; then
   echo "Explicit libretro used, disabling dynamic libretro loading ..."
   HAVE_DYNAMIC='no'
else LIBRETRO="-lretro"
fi

[ "$HAVE_DYNAMIC" = 'yes' ] || {
   check_lib_cxx RETRO "$LIBRETRO" retro_init "$DYLIB" "Cannot find libretro."
   add_define_make libretro "$LIBRETRO"
}

check_lib THREADS -lpthread pthread_create
check_lib DYLIB "$DYLIB" dlopen

check_lib NETPLAY -lc socket
if [ "$HAVE_NETPLAY" = 'yes' ]; then
   HAVE_GETADDRINFO=auto
   check_lib GETADDRINFO -lc getaddrinfo
   if [ "$HAVE_GETADDRINFO" = 'yes' ]; then
      HAVE_SOCKET_LEGACY='no'
   else
      HAVE_SOCKET_LEGACY='yes'
   fi
   HAVE_NETWORK_CMD='yes'
else
   HAVE_NETWORK_CMD='no'
fi

check_lib STDIN_CMD -lc fcntl

if [ "$HAVE_NETWORK_CMD" = "yes" ] || [ "$HAVE_STDIN_CMD" = "yes" ]; then
   HAVE_COMMAND='yes'
else
   HAVE_COMMAND='no'
fi

check_lib GETOPT_LONG -lc getopt_long

if [ "$HAVE_DYLIB" = 'no' ] && [ "$HAVE_DYNAMIC" = 'yes' ]; then
   echo "Dynamic loading of libretro is enabled, but your platform does not appear to have dlopen(), use --disable-dynamic or --with-libretro=\"-lretro\"".
   exit 1
fi

check_pkgconf ALSA alsa
check_header OSS sys/soundcard.h
check_header OSS_BSD soundcard.h
check_lib OSS_LIB -lossaudio

if [ "$OS" = Darwin ]; then
   check_lib AL "-framework OpenAL" alcOpenDevice
else
   check_lib AL -lopenal alcOpenDevice
fi

if [ "$OS" = Darwin ]; then
   check_lib FBO "-framework OpenGL" glFramebufferTexture2D
   check_lib PBO "-framework OpenGL" glMapBuffer
else
   check_lib FBO -lGL glFramebufferTexture2D
   check_lib PBO -lGL glMapBuffer
fi

check_pkgconf RSOUND rsound 1.1
check_pkgconf ROAR libroar
check_pkgconf JACK jack 0.120.1
check_pkgconf PULSE libpulse

check_lib COREAUDIO "-framework AudioUnit" AudioUnitInitialize

check_pkgconf SDL sdl 1.2.10

# On some distros, -lCg doesn't link against -lstdc++ it seems ...
if [ "$HAVE_OPENGL" != 'no' ]; then
   check_lib_cxx CG -lCg cgCreateContext
else
   echo "Ignoring Cg. OpenGL is not enabled."
   HAVE_CG='no'
fi

[ "$HAVE_SDL" = "no" ] && HAVE_SDL_IMAGE=no
check_pkgconf SDL_IMAGE SDL_image

check_pkgconf LIBPNG libpng 1.5

if [ "$HAVE_THREADS" != 'no' ]; then
   if [ "$HAVE_FFMPEG" != 'no' ]; then
      check_pkgconf AVCODEC libavcodec
      check_pkgconf AVFORMAT libavformat
      check_pkgconf AVUTIL libavutil
      ( [ "$HAVE_FFMPEG" = 'auto' ] && ( [ "$HAVE_AVCODEC" = 'no' ] || [ "$HAVE_AVFORMAT" = 'no' ] || [ "$HAVE_AVUTIL" = 'no' ] ) && HAVE_FFMPEG='no' ) || HAVE_FFMPEG='yes'
   fi

   if [ "$HAVE_FFMPEG" = 'yes' ]; then
      check_lib FFMPEG_ALLOC_CONTEXT3 "$AVCODEC_LIBS" avcodec_alloc_context3
      check_lib FFMPEG_AVCODEC_OPEN2 "$AVCODEC_LIBS" avcodec_open2
      check_lib FFMPEG_AVCODEC_ENCODE_AUDIO2 "$AVCODEC_LIBS" avcodec_encode_audio2
      check_lib FFMPEG_AVIO_OPEN "$AVFORMAT_LIBS" avio_open
      check_lib FFMPEG_AVFORMAT_WRITE_HEADER "$AVFORMAT_LIBS" avformat_write_header
      check_lib FFMPEG_AVFORMAT_NEW_STREAM "$AVFORMAT_LIBS" avformat_new_stream
      check_lib FFMPEG_AVCODEC_ENCODE_VIDEO2 "$AVCODEC_LIBS" avcodec_encode_video2
   fi
else
   echo "Not building with threading support. Will skip FFmpeg."
   HAVE_FFMPEG='no'
fi

check_lib DYNAMIC "$DYLIB" dlopen

if [ "$HAVE_KMS" != "no" ]; then
   check_pkgconf GBM gbm 9.1.0
   check_pkgconf DRM libdrm
   if [ "$HAVE_GBM" = "yes" ] && [ "$HAVE_DRM" = "yes" ]; then
      HAVE_KMS=yes
      HAVE_EGL=yes # Required
   elif [ "$HAVE_KMS" = "yes" ]; then
      echo "Cannot find libgbm and/or libdrm libraries required for KMS. Compile without --enable-kms."
      exit 1
   else
      echo "Cannot find libgbm and/or libdrm libraries required for KMS."
      HAVE_KMS=no
   fi
fi

# On videocore, these libraries will exist without proper pkg-config.
if [ "$HAVE_VIDEOCORE" != "yes" ]; then
   check_pkgconf GLES glesv2
   check_pkgconf VG vg

   # GLES or VG requires EGL to be present.
   # GLES requires XML shaders.
   [ "$HAVE_GLES" = "yes" ] && HAVE_EGL=yes && HAVE_XML=yes
   [ "$HAVE_VG" = "yes" ] && HAVE_EGL=yes

   check_pkgconf EGL egl
fi

check_pkgconf XML libxml-2.0
check_pkgconf FREETYPE freetype2
check_pkgconf X11 x11
[ "$HAVE_X11" = "no" ] && HAVE_XEXT=no

check_pkgconf XEXT xext
if [ "$HAVE_X11" = 'yes' ] && [ "$HAVE_XEXT" = 'yes' ]; then
   check_pkgconf XVIDEO xv
else
   echo "X11 or Xext not present. Skipping XVideo."
   HAVE_XVIDEO='no'
fi

check_lib STRL -lc strlcpy

check_pkgconf PYTHON python3

add_define_make OS "$OS"

# Creates config.mk and config.h.
VARS="ALSA OSS OSS_BSD OSS_LIB AL RSOUND ROAR JACK COREAUDIO PULSE SDL OPENGL GLES VG EGL KMS GBM DRM DYLIB GETOPT_LONG THREADS CG XML SDL_IMAGE LIBPNG DYNAMIC FFMPEG AVCODEC AVFORMAT AVUTIL CONFIGFILE FREETYPE XVIDEO X11 XEXT NETPLAY NETWORK_CMD STDIN_CMD COMMAND SOCKET_LEGACY FBO PBO STRL PYTHON FFMPEG_ALLOC_CONTEXT3 FFMPEG_AVCODEC_OPEN2 FFMPEG_AVIO_OPEN FFMPEG_AVFORMAT_WRITE_HEADER FFMPEG_AVFORMAT_NEW_STREAM FFMPEG_AVCODEC_ENCODE_AUDIO2 FFMPEG_AVCODEC_ENCODE_VIDEO2 SINC FIXED_POINT BSV_MOVIE VIDEOCORE"
create_config_make config.mk $VARS
create_config_header config.h $VARS

check_switch_c C99 -std=gnu99  "Cannot find C99 compatible compiler."

check_switch_c NOUNUSED -Wno-unused-result
add_define_make NOUNUSED "$HAVE_NOUNUSED"
check_switch_c NOUNUSED_VARIABLE -Wno-unused-variable
add_define_make NOUNUSED_VARIABLE "$HAVE_NOUNUSED_VARIABLE"

# There are still broken 64-bit Linux distros out there. :)
[ -z "$CROSS_COMPILE" ] && [ -d /usr/lib64 ] && add_library_dirs /usr/lib64

[ -z "$CROSS_COMPILE" ] && [ -d /opt/local/lib ] && add_library_dirs /opt/local/lib

if [ "$OS" = 'BSD' ]; then
   DYLIB=-lc;
elif [ "$OS" = 'Haiku' ]; then
   DYLIB="";
else
   DYLIB=-ldl;
fi

add_define_make DYLIB_LIB "$DYLIB"

[ "$OS" = 'Darwin' ] && HAVE_X11=no # X11 breaks on recent OSXes even if present.

[ -d /opt/vc/lib ] && add_library_dirs /opt/vc/lib
check_lib VIDEOCORE -lbcm_host bcm_host_init "-lvcos -lvchiq_arm" 

if [ "$HAVE_VIDEOCORE" = 'yes' ]; then
   [ -d /opt/vc/include ] && add_include_dirs /opt/vc/include
   [ -d /opt/vc/include/interface/vcos/pthreads ] && add_include_dirs /opt/vc/include/interface/vcos/pthreads
   [ -d /opt/vc/include/interface/vmcs_host/linux ] && add_include_dirs /opt/vc/include/interface/vmcs_host/linux
   HAVE_GLES='auto'
   EXTRA_GL_LIBS="-lGLESv2 -lbcm_host -lvcos -lvchiq_arm"
fi

if [ "$HAVE_FLOATHARD" = "yes" ]; then
   CFLAGS="$CFLAGS -mfloat-abi=hard"
   CXXFLAGS="$CFLAGS -mfloat-abi=hard"
   ASFLAGS="$CFLAGS -mfloat-abi=hard"
fi

if [ "$HAVE_FLOATSOFTFP" = "yes" ]; then
   CFLAGS="$CFLAGS -mfloat-abi=softfp"
   CXXFLAGS="$CFLAGS -mfloat-abi=softfp"
   ASFLAGS="$CFLAGS -mfloat-abi=softfp"
fi

if [ "$HAVE_NEON" = "yes" ]; then
   CFLAGS="$CFLAGS -mfpu=neon"
   CXXFLAGS="$CXXFLAGS -mfpu=neon"
   ASFLAGS="$ASFLAGS -mfpu=neon"
fi

if [ "$HAVE_SSE" = "yes" ]; then
   CFLAGS="$CFLAGS -msse -msse2"
   CXXFLAGS="$CXXFLAGS -msse -msse2"
fi

if [ "$HAVE_EGL" != "no" ]; then
   check_pkgconf EGL egl
   # some systems have EGL libs, but no pkgconfig
   if [ "$HAVE_EGL" = "no" ]; then
      HAVE_EGL=auto && check_lib EGL "-lEGL $EXTRA_GL_LIBS"
      [ "$HAVE_EGL" = "yes" ] && EGL_LIBS=-lEGL
   fi
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

if [ "$MAN_DIR" ]; then
   add_define_make MAN_DIR "$MAN_DIR"
else
   add_define_make MAN_DIR "${PREFIX}/share/man/man1"
fi

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

if [ "$OS" = 'Darwin' ]; then
   check_lib AL "-framework OpenAL" alcOpenDevice
else
   check_lib AL -lopenal alcOpenDevice
fi

check_pkgconf RSOUND rsound 1.1
check_pkgconf ROAR libroar
check_pkgconf JACK jack 0.120.1
check_pkgconf PULSE libpulse

check_lib COREAUDIO "-framework AudioUnit" AudioUnitInitialize

check_pkgconf SDL sdl 1.2.10

if [ "$HAVE_OPENGL" != 'no' ]; then
   if [ "$OS" = 'Darwin' ]; then
      check_lib CG "-framework Cg" cgCreateContext
   else
      # On some distros, -lCg doesn't link against -lstdc++ it seems ...
      check_lib_cxx CG -lCg cgCreateContext
   fi
else
   echo "Ignoring Cg. OpenGL is not enabled."
   HAVE_CG='no'
fi

if [ "$HAVE_SDL" = "no" ]; then
   echo "SDL is disabled. Disabling SDL_image."
   HAVE_SDL_IMAGE=no
fi
check_pkgconf SDL_IMAGE SDL_image

check_pkgconf ZLIB zlib

if [ "$HAVE_THREADS" != 'no' ]; then
   if [ "$HAVE_FFMPEG" != 'no' ]; then
      check_pkgconf AVCODEC libavcodec 54
      check_pkgconf AVFORMAT libavformat 54
      check_pkgconf AVUTIL libavutil 51
      check_pkgconf SWSCALE libswscale 2.1
      ( [ "$HAVE_FFMPEG" = 'auto' ] && ( [ "$HAVE_AVCODEC" = 'no' ] || [ "$HAVE_AVFORMAT" = 'no' ] || [ "$HAVE_AVUTIL" = 'no' ] || [ "$HAVE_SWSCALE" = 'no' ] ) && HAVE_FFMPEG='no' ) || HAVE_FFMPEG='yes'
   fi
else
   echo "Not building with threading support. Will skip FFmpeg."
   HAVE_FFMPEG='no'
fi

check_lib DYNAMIC "$DYLIB" dlopen

if [ "$HAVE_KMS" != "no" ]; then
   check_pkgconf GBM gbm 9.0
   check_pkgconf DRM libdrm
   if [ "$HAVE_GBM" = "yes" ] && [ "$HAVE_DRM" = "yes" ] && [ "$HAVE_EGL" = "yes" ]; then
      HAVE_KMS=yes
   elif [ "$HAVE_KMS" = "yes" ]; then
      echo "Cannot find libgbm, libdrm and EGL libraries required for KMS. Compile without --enable-kms."
      exit 1
   else
      HAVE_KMS=no
   fi
fi

check_pkgconf LIBXML2 libxml-2.0

if [ "$HAVE_EGL" = "yes" ]; then
   if [ "$HAVE_GLES" != "no" ]; then
      HAVE_GLES=auto check_pkgconf GLES glesv2
      [ "$HAVE_GLES" = "no" ] && HAVE_GLES=auto check_lib GLES "-lGLESv2 $EXTRA_GL_LIBS"
   fi
   if [ "$HAVE_VG" != "no" ]; then
      check_pkgconf VG vg
      if [ "$HAVE_VG" = "no" ]; then
         HAVE_VG=auto check_lib VG "-lOpenVG $EXTRA_GL_LIBS"
         [ "$HAVE_VG" = "yes" ] && VG_LIBS=-lOpenVG
      fi
   fi
else
   HAVE_VG=no
   HAVE_GLES=no
fi

if [ "$OS" = 'Darwin' ]; then
   check_lib FBO "-framework OpenGL" glFramebufferTexture2D
else
   if [ "$HAVE_GLES" = "yes" ]; then
      [ $HAVE_FBO != "no" ] && HAVE_FBO=yes
   else
      check_lib FBO -lGL glFramebufferTexture2D
   fi
fi

check_pkgconf FREETYPE freetype2
check_pkgconf X11 x11
[ "$HAVE_X11" = "no" ] && HAVE_XEXT=no && HAVE_XF86VM=no && HAVE_XINERAMA=no

check_pkgconf XEXT xext
check_pkgconf XF86VM xxf86vm
check_pkgconf XINERAMA xinerama
if [ "$HAVE_X11" = 'yes' ] && [ "$HAVE_XEXT" = 'yes' ] && [ "$HAVE_XF86VM" = 'yes' ]; then
   check_pkgconf XVIDEO xv
else
   echo "X11, Xext or xf86vm not present. Skipping X11 code paths."
   HAVE_X11='no'
   HAVE_XVIDEO='no'
fi

check_lib STRL -lc strlcpy

check_pkgconf PYTHON python3

check_macro NEON __ARM_NEON__

add_define_make OS "$OS"

# Creates config.mk and config.h.
add_define_make GLOBAL_CONFIG_DIR "$GLOBAL_CONFIG_DIR"
VARS="RGUI ALSA OSS OSS_BSD OSS_LIB AL RSOUND ROAR JACK COREAUDIO PULSE SDL OPENGL GLES VG EGL KMS GBM DRM DYLIB GETOPT_LONG THREADS CG LIBXML2 SDL_IMAGE ZLIB DYNAMIC FFMPEG AVCODEC AVFORMAT AVUTIL SWSCALE FREETYPE XVIDEO X11 XEXT XF86VM XINERAMA NETPLAY NETWORK_CMD STDIN_CMD COMMAND SOCKET_LEGACY FBO STRL PYTHON FFMPEG_ALLOC_CONTEXT3 FFMPEG_AVCODEC_OPEN2 FFMPEG_AVIO_OPEN FFMPEG_AVFORMAT_WRITE_HEADER FFMPEG_AVFORMAT_NEW_STREAM FFMPEG_AVCODEC_ENCODE_AUDIO2 FFMPEG_AVCODEC_ENCODE_VIDEO2 BSV_MOVIE VIDEOCORE NEON FLOATHARD FLOATSOFTFP"
create_config_make config.mk $VARS
create_config_header config.h $VARS

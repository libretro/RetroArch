. qb/qb.libs.sh

check_switch_c C99 -std=gnu99
check_critical C99 "Cannot find C99 compatible compiler."

if [ "$HAVE_DYNAMIC" = "yes" ] && [ "$HAVE_CONFIGFILE" = "no" ]; then
   echo "Cannot have dynamic loading of libsnes and no configfile support."
   echo "Dynamic loading requires config file support."
   exit 1
fi

if [ $HAVE_DYNAMIC != yes ]; then
   check_lib_cxx SNES $LIBSNES snes_init -ldl
   check_critical SNES "Cannot find libsnes."
   add_define_make libsnes $LIBSNES
fi

check_lib ALSA -lasound snd_pcm_open
check_header OSS sys/soundcard.h
check_lib AL -lopenal alcOpenDevice
check_lib RSOUND -lrsound rsd_init
check_lib ROAR -lroar roar_vs_new
check_lib JACK -ljack jack_client_open

check_pkgconf SDL sdl 1.2.10
check_critical SDL "Cannot find SDL library."

check_lib CG -lCg cgCreateContext
check_pkgconf XML libxml-2.0

if [ $HAVE_FFMPEG != no ]; then
   check_pkgconf AVCODEC libavcodec
   check_pkgconf AVFORMAT libavformat
   check_pkgconf AVCORE libavcore
   check_pkgconf AVUTIL libavutil
   check_pkgconf SWSCALE libswscale

   ( [ $HAVE_FFMPEG = auto ] && ( [ $HAVE_AVCODEC = no ] || [ $HAVE_AVFORMAT = no ] || [ $HAVE_AVCORE = no ] || [ $HAVE_AVUTIL = no ] || [ $HAVE_SWSCALE = no ] ) && HAVE_FFMPEG=no ) || HAVE_FFMPEG=yes
fi

check_pkgconf SRC samplerate
check_critical SRC "Cannot find libsamplerate."

check_lib DYNAMIC -ldl dlopen

check_pkgconf FREETYPE freetype2

# Creates config.mk and config.h.
VARS="ALSA OSS AL RSOUND ROAR JACK SDL FILTER CG XML DYNAMIC FFMPEG AVCODEC AVFORMAT AVCORE AVUTIL SWSCALE SRC CONFIGFILE FREETYPE"
create_config_make config.mk $VARS
create_config_header config.h $VARS


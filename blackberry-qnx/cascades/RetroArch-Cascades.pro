APP_NAME = RetroArch-Cascades

CONFIG += qt warn_on cascades10

LIBS += -lscreen -lbps -lOpenAL -lpng -lEGL -lGLESv2
LIBS += -lbbcascadespickers -lbbdata -lbbdevice

DEFINES += HAVE_RGUI HAVE_MENU RARCH_MOBILE \
           SINC_LOWER_QUALITY RARCH_INTERNAL \
           HAVE_FBO HAVE_GRIFFIN __LIBRETRO__ \
           HAVE_DYNAMIC HAVE_ZLIB HAVE_OPENGLES \
           HAVE_OPENGLES2 HAVE_BUILTIN_FILTERS \
           HAVE_AL HAVE_THREADS WANT_MINIZ HAVE_OVERLAY HAVE_GLSL \
           USING_GL20 HAVE_OPENGL __STDC_CONSTANT_MACROS HAVE_BB10

INCLUDEPATH += ../../../../RetroArch

QMAKE_CXXFLAGS +=
QMAKE_CFLAGS += -Wc,-std=gnu99 -marm -mfpu=neon

SOURCES += ../../../griffin/griffin.c \
           ../../../audio/sinc_neon.S \
           ../../../audio/utils_neon.S

include(config.pri)

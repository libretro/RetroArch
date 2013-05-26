APP_NAME = RetroArch-Cascades

CONFIG += qt warn_on cascades10

LIBS += -lscreen -lbps -lOpenAL -lpng -lEGL -lGLESv2
LIBS += -lbbcascadespickers -lbbdata -lbbdevice

DEFINES += HAVE_RGUI HAVE_NEON RARCH_MOBILE \
           SINC_LOWER_QUALITY HAVE_RARCH_MAIN_IMPLEMENTATION \
           HAVE_VID_CONTEXT HAVE_FBO HAVE_GRIFFIN __LIBRETRO__ \
           HAVE_DYNAMIC HAVE_ZLIB __BLACKBERRY_QNX__ HAVE_OPENGLES \
           PACKAGE_VERSION=\"0.9.9\" HAVE_OPENGLES2 HAVE_NULLINPUT \
           HAVE_AL HAVE_THREADS WANT_MINIZ HAVE_OVERLAY HAVE_GLSL \
           USING_GL20 HAVE_OPENGL __STDC_CONSTANT_MACROS HAVE_BB10

INCLUDEPATH += ../../../../RetroArch

QMAKE_CXXFLAGS +=
QMAKE_CFLAGS += -Wc,-std=gnu99 -marm -mfpu=neon

SOURCES += ../../../griffin/griffin.c \
           ../../../audio/sinc_neon.S \
           ../../../audio/utils_neon.S

include(config.pri)

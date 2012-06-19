RARCH_VERSION		= "0.9.6"
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)


LOCAL_MODULE    := retroarch
LOCAL_SRC_FILES    = ../../console/griffin/griffin.c ../../console/rzlib/rzlib.c

LOCAL_CFLAGS = -DINLINE=inline -DHAVE_OPENGL -DRARCH_CONSOLE -DHAVE_OPENGLES -DHAVE_OPENGLES20 -DHAVE_GLSL -DLSB_FIRST -D__LIBRETRO__ -DHAVE_CONFIGFILE=1 -DHAVE_GRIFFIN=1 -DPACKAGE_VERSION=\"$(RARCH_VERSION)\" -Dmain=rarch_main -std=gnu99

include $(BUILD_SHARED_LIBRARY)

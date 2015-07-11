LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := retro-test-gl

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -DANDROID_ARM
LOCAL_ARM_MODE := arm
endif

ifeq ($(TARGET_ARCH),x86)
LOCAL_CFLAGS += -DANDROID_X86
endif

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DANDROID_MIPS
endif

ifeq ($(GLES), 3)
   LOCAL_CFLAGS += -DHAVE_OPENGLES3 -DGLES3
   GLES_LIB := -lGLESv3
else
   GLES_LIB := -lGLESv2
endif

LOCAL_SRC_FILES += $(addprefix ../,$(wildcard *.c) ../../../libretro-common/glsym/rglgen.c ../../../libretro-common/glsym/glsym_es2.c)
LOCAL_CFLAGS += -O2 -Wall -std=gnu99 -ffast-math -DGLES -DHAVE_OPENGLES2 -I../../../libretro-common/include
LOCAL_LDLIBS += $(GLES_LIB)

include $(BUILD_SHARED_LIBRARY)


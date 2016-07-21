LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

LOCAL_MODULE := retro

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

LOCAL_SRC_FILES += ../libretro-test.c \
						 ../../../libretro-common/vulkan/vulkan_symbol_wrapper.c
LOCAL_CFLAGS += -O2 -Wall -std=gnu99 -ffast-math -DGLES -DHAVE_OPENGLES2 -I../../../libretro-common/include -I../../../gfx/include

include $(BUILD_SHARED_LIBRARY)


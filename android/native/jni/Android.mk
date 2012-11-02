RARCH_VERSION		= "0.9.7"
LOCAL_PATH := $(call my-dir)
PERF_TEST := 0
HAVE_OPENSL     := 1

include $(CLEAR_VARS)

ifeq ($(TARGET_ARCH),arm)
LOCAL_CFLAGS += -DANDROID_ARM -marm
LOCAL_ARM_MODE := arm
endif

ifeq ($(TARGET_ARCH),x86)
LOCAL_CFLAGS +=  -DANDROID_X86
endif

ifeq ($(TARGET_ARCH),mips)
LOCAL_CFLAGS += -DANDROID_MIPS -D__mips__ -D__MIPSEL__
endif

LOCAL_MODULE    := retroarch-activity

RARCH_PATH  := ../../..
LOCAL_SRC_FILES    =	$(RARCH_PATH)/retroarch.c \
			$(RARCH_PATH)/file.c \
			$(RARCH_PATH)/file_path.c \
			$(RARCH_PATH)/hash.c \
			$(RARCH_PATH)/driver.c \
			$(RARCH_PATH)/settings.c \
			$(RARCH_PATH)/dynamic.c \
			$(RARCH_PATH)/message.c \
			$(RARCH_PATH)/rewind.c \
			$(RARCH_PATH)/gfx/gfx_common.c \
			$(RARCH_PATH)/input/input_common.c \
			$(RARCH_PATH)/patch.c \
			$(RARCH_PATH)/fifo_buffer.c \
			$(RARCH_PATH)/compat/compat.c \
			$(RARCH_PATH)/audio/null.c \
			$(RARCH_PATH)/audio/utils.c \
			$(RARCH_PATH)/audio/hermite.c \
			$(RARCH_PATH)/gfx/null.c \
			$(RARCH_PATH)/gfx/gl.c \
			$(RARCH_PATH)/gfx/scaler/scaler.c \
			$(RARCH_PATH)/gfx/scaler/pixconv.c \
			$(RARCH_PATH)/gfx/scaler/scaler_int.c \
			$(RARCH_PATH)/gfx/scaler/filter.c \
			$(RARCH_PATH)/gfx/gfx_context.c \
			$(RARCH_PATH)/gfx/context/androidegl_ctx.c \
			$(RARCH_PATH)/gfx/math/matrix.c \
			$(RARCH_PATH)/gfx/shader_glsl.c \
			$(RARCH_PATH)/gfx/fonts/null_fonts.c \
			$(RARCH_PATH)/gfx/state_tracker.c \
			$(RARCH_PATH)/input/null.c \
			input_android.c \
			$(RARCH_PATH)/conf/config_file.c \
			$(RARCH_PATH)/autosave.c \
			$(RARCH_PATH)/thread.c \
			$(RARCH_PATH)/performance.c \
			main.c

ifeq ($(PERF_TEST), 1)
LOCAL_CFLAGS += -DPERF_TEST
endif

LOCAL_CFLAGS += -O3 -funroll-loops -DNDEBUG -DANDROID -DHAVE_DYNAMIC -DHAVE_OPENGL -DHAVE_OPENGLES -DHAVE_OPENGLES2 -DGLSL_DEBUG -DHAVE_GLSL -DHAVE_ZLIB -DINLINE=inline -DLSB_FIRST -D__LIBRETRO__ -DHAVE_CONFIGFILE=1 -DPACKAGE_VERSION=\"$(RARCH_VERSION)\" -std=gnu99


LOCAL_LDLIBS	:= -L$(SYSROOT)/usr/lib -landroid -lEGL -lGLESv2 -llog -ldl -lz

ifeq ($(HAVE_OPENSL), 1)
LOCAL_SRC_FILES += $(RARCH_PATH)/audio/opensl.c
LOCAL_CFLAGS += -DHAVE_SL
LOCAL_LDLIBS += -lOpenSLES
endif

include $(BUILD_SHARED_LIBRARY)

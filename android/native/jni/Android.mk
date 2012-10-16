RARCH_VERSION		= "0.9.7"
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

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
			$(RARCH_PATH)/screenshot.c \
			$(RARCH_PATH)/conf/config_file.c \
			$(RARCH_PATH)/autosave.c \
			$(RARCH_PATH)/thread.c \
			main.c

LOCAL_CFLAGS = -marm -DANDROID -DHAVE_DYNAMIC -DHAVE_OPENGL -DHAVE_OPENGLES -DHAVE_OPENGLES2 -DGLSL_DEBUG -DHAVE_GLSL -DHAVE_ZLIB -DINLINE=inline -DLSB_FIRST -D__LIBRETRO__ -DHAVE_CONFIGFILE=1 -DPACKAGE_VERSION=\"$(RARCH_VERSION)\" -std=gnu99

LOCAL_LDLIBS	:= -L$(SYSROOT)/usr/lib -landroid -lEGL -lGLESv2 -llog -ldl -lz
LOCAL_STATIC_LIBRARIES := android_native_app_glue

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)

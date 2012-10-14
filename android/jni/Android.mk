RARCH_VERSION		= "0.9.7"
LOCAL_PATH := $(call my-dir)

include $(CLEAR_VARS)

APP_OPTIM := debug

LOCAL_MODULE    := retroarch
LOCAL_SRC_FILES    =	../../retroarch.c \
			../../file.c \
			../../file_path.c \
			../../hash.c \
			../../driver.c \
			../../settings.c \
			../../dynamic.c \
			../../message.c \
			../../rewind.c \
			../../gfx/gfx_common.c \
			../../input/input_common.c \
			../../patch.c \
			../../fifo_buffer.c \
			../../compat/compat.c \
			../../audio/null.c \
			../../audio/utils.c \
			../../audio/hermite.c \
			../../gfx/null.c \
			../../gfx/gl.c \
			../../gfx/scaler/scaler.c \
			../../gfx/scaler/pixconv.c \
			../../gfx/scaler/scaler_int.c \
			../../gfx/scaler/filter.c \
			../../gfx/gfx_context.c \
			../../gfx/context/androidegl_ctx.c \
			../../gfx/math/matrix.c \
			../../gfx/shader_glsl.c \
			../../gfx/fonts/null_fonts.c \
			../../gfx/state_tracker.c \
			../../input/null.c \
			../../screenshot.c \
			../../conf/config_file.c \
			../../autosave.c \
			../../thread.c \
			../../console/rzlib/rzlib.c \
			main.c

LOCAL_CFLAGS = -marm -DANDROID -DHAVE_DYNAMIC -DHAVE_OPENGL -DHAVE_OPENGLES -DHAVE_OPENGLES2 -DHAVE_GLSL -DHAVE_ZLIB -DHAVE_RARCH_MAIN_WRAP -DINLINE=inline -DLSB_FIRST -D__LIBRETRO__ -DHAVE_CONFIGFILE=1 -DPACKAGE_VERSION=\"$(RARCH_VERSION)\" -std=gnu99

LOCAL_LDLIBS	:= -L$(SYSROOT)/usr/lib -landroid -lEGL -lGLESv2 -llog -ldl -lz
LOCAL_STATIC_LIBRARIES := android_native_app_glue

include $(BUILD_SHARED_LIBRARY)

$(call import-module,android/native_app_glue)

# The shared OpenXR units (gfx/common/openxr_*.c, vulkan_openxr.c)
# against a stand-in for the runtime: the host's compiler, the vendored
# OpenXR and Vulkan headers.
TARGET := openxr_core_test
ROOT   := ../../..
CFLAGS += -std=gnu99 -O1 -g1 -Wall -Wno-unused-function -Wno-unused-parameter \
          -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_OPENXR \
          -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/deps \
          -I$(ROOT)/deps/openxr/include -I$(ROOT)/gfx/include

# The desktop GL binding's native half, on a Linux target with the
# EGL, X11 and GL headers; the common tests build without it elsewhere.
ifneq ($(findstring linux,$(shell $(CC) -dumpmachine 2>/dev/null)),)
ifeq ($(shell pkg-config --exists egl x11 gl 2>/dev/null && echo y),y)
   CFLAGS += -DHAVE_OPENXR_GL -DHAVE_EGL -DHAVE_X11 \
             $(shell pkg-config --cflags egl x11 gl)
endif
endif

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

SRCS := openxr_core_test.c \
        $(ROOT)/libretro-common/compat/compat_strl.c \
        $(ROOT)/libretro-common/string/stdstring.c \
        $(ROOT)/libretro-common/encodings/encoding_utf.c \
        $(ROOT)/gfx/video_xr.c \
        $(ROOT)/gfx/video_views.c \
        $(ROOT)/libretro-common/rthreads/rthreads.c \
        $(ROOT)/libretro-common/rthreads/retro_eventcount.c \
        $(ROOT)/libretro-common/time/rtime.c \
        $(ROOT)/libretro-common/features/features_cpu.c

all: $(TARGET)

$(TARGET): $(SRCS) $(wildcard $(ROOT)/gfx/common/openxr_*.c) \
           $(ROOT)/gfx/common/vulkan_openxr.c $(ROOT)/gfx/common/openxr_ring.h \
           $(wildcard $(ROOT)/gfx/common/gl_openxr.[ch])
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS) -lm -lpthread

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

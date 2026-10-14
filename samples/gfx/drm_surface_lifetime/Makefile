# gfx/drivers/drm_gfx.c's surfaces against a fake kernel: every dumb
# buffer, framebuffer and mapping a surface makes is taken down when
# the surface goes, and the one the plane scans out is never removed
# from under it; and a core's views composed into a surface of their
# own for the plane.

TARGET    := drm_surface_lifetime_test
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
SOURCES   := drm_surface_lifetime_test.c \
             $(LRC)/rthreads/rthreads.c \
             $(LRC)/compat/compat_strl.c \
             $(LRC)/string/stdstring.c \
             $(LRC)/encodings/encoding_utf.c \
             $(REPO_ROOT)/gfx/video_views.c \
             $(LRC)/gfx/scaler/scaler.c \
             $(LRC)/gfx/scaler/scaler_filter.c \
             $(LRC)/gfx/scaler/scaler_int.c \
             $(LRC)/gfx/scaler/pixconv.c
CFLAGS    += -Wall -std=gnu99 -g1 -O0 -I$(REPO_ROOT) -I$(LRC)/include \
             -DHAVE_THREADS $(shell pkg-config --cflags libdrm)
LDFLAGS   += -lpthread -lm -Wl,--wrap=mmap,--wrap=munmap
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
$(TARGET): $(SOURCES) $(REPO_ROOT)/gfx/drivers/drm_gfx.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET)
.PHONY: all check clean

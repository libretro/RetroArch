# gfx/drivers/drm_gfx.c's surfaces against a fake kernel: every dumb
# buffer, framebuffer and mapping a surface makes is taken down when
# the surface goes, and the one the plane scans out is never removed
# from under it; a core's views composed into a surface of their
# own for the plane; padded rows, lent pages and dupes; the menu in
# RGUI's own format, lent, and resized; the input overlay on a plane of
# its own, or blended into the frame; the frame turned by the plane, or
# by the copy, and read back as it is shown; without vsync, flips that
# do not wait and never write a page in use; the threaded wrapper's
# frames scanned out where they lie; CRT modelines and HDMI timings;
# content under the menu kept under it; on-screen messages. Built with
# sign-compare as an error and without rthreads: the driver takes no
# locks.

TARGET    := drm_surface_lifetime_test
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
SOURCES   := drm_surface_lifetime_test.c \
             $(LRC)/compat/compat_strl.c \
             $(LRC)/string/stdstring.c \
             $(LRC)/encodings/encoding_utf.c \
             $(REPO_ROOT)/gfx/video_views.c \
             $(LRC)/gfx/scaler/scaler.c \
             $(LRC)/gfx/scaler/scaler_filter.c \
             $(LRC)/gfx/scaler/scaler_int.c \
             $(LRC)/gfx/scaler/pixconv.c
CFLAGS    += -Wall -Werror=sign-compare -std=gnu99 -g1 -O0 -I$(REPO_ROOT) -I$(LRC)/include \
             -DHAVE_THREADS -DHAVE_OVERLAY -DHAVE_MODELINE $(shell pkg-config --cflags libdrm)
LDFLAGS   += -lm -Wl,--wrap=mmap,--wrap=munmap,--wrap=open,--wrap=close,--wrap=poll
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
$(TARGET): $(SOURCES) $(REPO_ROOT)/gfx/drivers/drm_gfx.c $(REPO_ROOT)/gfx/common/drm_common.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET)
.PHONY: all check clean

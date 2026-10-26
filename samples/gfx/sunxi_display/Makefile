# gfx/drivers/sunxi_gfx.c against a fake display engine and framebuffer
# (open, close, ioctl, mmap, munmap and system wrapped), with the NEON
# blits stood in for by C: init fails cleanly off sunxi hardware or
# without a scaler; the pages sit past the console, XRGB8888 for a
# 16-bit console too; a page on the screen is never drawn into; a pitch
# change lands straight; a frame with no room is not drawn past the
# framebuffer; the frame shows where the frontend lays it out, cut where
# it leaves the screen, turned for a rotation; without vsync nothing
# waits, and a framebuffer that cannot wait for one does not spin; the
# menu before any core and past the pages; a lent page shown in place;
# screenshots, messages, and the console put back. Linux only: the
# driver is built against linux/fb.h.

TARGET    := sunxi_display_test
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
SOURCES   := sunxi_display_test.c \
             $(REPO_ROOT)/gfx/video_views.c \
             $(LRC)/rthreads/retro_eventcount.c \
             $(LRC)/rthreads/rthreads.c \
             $(LRC)/compat/compat_strl.c \
             $(LRC)/encodings/encoding_utf.c \
             $(LRC)/gfx/scaler/scaler.c \
             $(LRC)/gfx/scaler/scaler_filter.c \
             $(LRC)/gfx/scaler/scaler_int.c \
             $(LRC)/gfx/scaler/pixconv.c
CFLAGS    += -Wall -Werror=sign-compare -Werror=implicit-function-declaration \
             -std=gnu89 -g1 -O0 -I$(REPO_ROOT) -I$(LRC)/include \
             -DHAVE_SUNXI -DHAVE_THREADS
LDFLAGS   += -lm -lpthread \
             -Wl,--wrap=open,--wrap=close,--wrap=ioctl,--wrap=mmap,--wrap=munmap,--wrap=system \
             -Wl,--wrap=open64,--wrap=mmap64,--wrap=__ioctl_time64
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
$(TARGET): $(SOURCES) $(REPO_ROOT)/gfx/drivers/sunxi_gfx.c \
           $(REPO_ROOT)/gfx/video_turn.h $(REPO_ROOT)/gfx/video_views_cpu.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET)
.PHONY: all check clean

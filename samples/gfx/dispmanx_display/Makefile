# gfx/drivers/dispmanx_gfx.c against a fake VideoCore that keeps each
# resource at its own pitch, as the firmware does: the frame shows
# where the frontend lays it out (aspect, integer scale, custom
# viewport, off the screen) and moves there without being made again;
# a core's rotation, a quarter turned by the CPU and a half by the HVS;
# rows of any pitch land straight and are never read past; the scaler
# kernel follows video_smooth and is put back on exit; the menu blends
# by its own alpha above the frame and stays up while the frame's
# surface is made again.

TARGET    := dispmanx_display_test
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
SOURCES   := dispmanx_display_test.c \
             $(REPO_ROOT)/gfx/video_views.c \
             $(LRC)/rthreads/retro_eventcount.c \
             $(LRC)/rthreads/rthreads.c \
             $(LRC)/memmap/memalign.c \
             $(LRC)/compat/compat_strl.c \
             $(LRC)/encodings/encoding_utf.c \
             $(LRC)/gfx/scaler/scaler.c \
             $(LRC)/gfx/scaler/scaler_filter.c \
             $(LRC)/gfx/scaler/scaler_int.c \
             $(LRC)/gfx/scaler/pixconv.c
CFLAGS    += -Wall -Werror=sign-compare -Werror=implicit-function-declaration \
             -std=gnu89 -g1 -O0 -I$(REPO_ROOT) -I$(LRC)/include \
             -I$(REPO_ROOT)/tools/platform_stubs/videocore \
             -DHAVE_DISPMANX -DHAVE_THREADS -DHAVE_OVERLAY
LDFLAGS   += -lm -lpthread
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
$(TARGET): $(SOURCES) $(REPO_ROOT)/gfx/drivers/dispmanx_gfx.c \
           $(REPO_ROOT)/gfx/video_turn.h \
           $(REPO_ROOT)/tools/platform_stubs/videocore/bcm_host.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET)
.PHONY: all check clean

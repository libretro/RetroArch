# gfx/common/drm_common.c against a fake kernel: an undriven connector
# gets an encoder and a free CRTC, a CRTC that was off is switched off
# again on restore, a missing size falls back to mode 0, which
# hotplugs restart the video driver, and the flip wait that watches
# the hotplug socket in the same poll.

TARGET    := drm_crtc_pick_test
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
SOURCES   := drm_crtc_pick_test.c $(LRC)/compat/compat_strl.c
DRM_CFLAGS ?= $(shell pkg-config --cflags libdrm 2>/dev/null || echo -I/usr/include/libdrm)
CFLAGS    += -Wall -std=gnu99 -g1 -O0 -I$(REPO_ROOT) -I$(LRC)/include \
             -DHAVE_UDEV $(DRM_CFLAGS)
LDFLAGS   += -lm -lpthread -Wl,--wrap=poll
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
$(TARGET): $(SOURCES) $(REPO_ROOT)/gfx/common/drm_common.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET)
.PHONY: all check clean

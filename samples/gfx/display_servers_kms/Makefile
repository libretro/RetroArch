TARGET := display_servers_kms_test

# Path back to the repo root from this sample dir.  Like the
# display_servers sample next door, this one links the real driver
# rather than mirroring its shape: gfx/display_servers/dispserv_kms.c
# reaches for only a handful of RetroArch-side symbols and two globals
# owned by gfx/common/drm_common.c, which the test supplies.
REPO_ROOT := ../../..

# Needs the libdrm headers (xf86drm.h, xf86drmMode.h) but links
# nothing from libdrm: the test fabricates the drmModeConnector the
# driver reads, which is the only way to present it with a connector
# that is absent, empty, or reporting modes in an awkward order.
SOURCES := display_servers_kms_test.c \
           $(REPO_ROOT)/gfx/display_servers/dispserv_kms.c

DRM_CFLAGS ?= $(shell pkg-config --cflags libdrm 2>/dev/null || echo -I/usr/include/libdrm)

# HAVE_MODELINE compiles the modeline ops: set fills the CRT
# consumer's drmModeModeInfo mirror in the video state and asks the
# video driver for a mode set, both of which the test supplies.
CFLAGS  += -Wall -std=gnu99 -g -O0 \
           -DHAVE_KMS -DHAVE_MODELINE \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/libretro-common/include \
           $(DRM_CFLAGS)

LDFLAGS += -lm

OBJS := $(SOURCES:.c=.o)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

# gfx/common/drm_hdr.c: a sink's EDID read for HDR10, and the
# HDR_OUTPUT_METADATA blob built from it, checked without a display;
# the blob's layout against the kernel's own struct.

TARGET    := drm_hdr_test
REPO_ROOT := ../../..
SOURCES   := drm_hdr_test.c $(REPO_ROOT)/gfx/common/drm_hdr.c
CFLAGS    += -Wall -std=gnu99 -g -O0 -I$(REPO_ROOT) -I$(REPO_ROOT)/libretro-common/include \
             $(shell pkg-config --cflags libdrm)
LDFLAGS   += -lm
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET)
.PHONY: all check clean

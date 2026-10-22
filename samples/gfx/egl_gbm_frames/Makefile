# gfx/common/egl_gbm_frames.c against a fake GBM, EGL and GPU: a format
# the GPU samples where the CPU writes it is taken and one mapped through
# staging refused for good; held, in-flight and last buffers are never
# lent; the check's GL state is put back; the free leaves nothing.

TARGET    := egl_gbm_frames_test
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
SOURCES   := egl_gbm_frames_test.c $(REPO_ROOT)/gfx/common/egl_gbm_frames.c
DRM_CFLAGS ?= $(shell pkg-config --cflags libdrm 2>/dev/null || echo -I/usr/include/libdrm)
CFLAGS    += -Wall -std=gnu99 -g1 -O0 -I$(REPO_ROOT) -I$(LRC)/include \
             -DHAVE_KMS -DHAVE_EGL -DHAVE_OPENGLES -DHAVE_OPENGLES2 $(DRM_CFLAGS)
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET)
.PHONY: all check clean

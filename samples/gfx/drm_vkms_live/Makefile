# gfx/drivers/drm_gfx.c against a real kernel: vkms, read back through
# its writeback connector. run_vm.sh boots a kernel with vkms in qemu
# and runs the test there; `make check` does both.

TARGET    := drm_vkms_live_test
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
SOURCES   := drm_vkms_live_test.c \
             $(LRC)/compat/compat_strl.c \
             $(LRC)/string/stdstring.c \
             $(LRC)/encodings/encoding_utf.c \
             $(REPO_ROOT)/gfx/video_views.c \
             $(LRC)/gfx/scaler/scaler.c \
             $(LRC)/gfx/scaler/scaler_filter.c \
             $(LRC)/gfx/scaler/scaler_int.c \
             $(LRC)/gfx/scaler/pixconv.c
CFLAGS    += -Wall -Werror=sign-compare -std=gnu99 -g1 -O1 -I$(REPO_ROOT) \
             -I$(LRC)/include -DHAVE_OVERLAY -DHAVE_MODELINE \
             $(shell pkg-config --cflags libdrm)
LDFLAGS   += -lm $(shell pkg-config --libs libdrm)
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

# gfx/display_servers/dispserv_kms.c's vblank clock, on the same kernel
VBLANK         := kms_vblank_live_test
VBLANK_SOURCES := kms_vblank_live_test.c \
                  $(REPO_ROOT)/gfx/display_servers/dispserv_kms.c \
                  $(LRC)/compat/compat_strl.c

all: $(TARGET) $(VBLANK)
$(TARGET): $(SOURCES) $(REPO_ROOT)/gfx/drivers/drm_gfx.c $(REPO_ROOT)/gfx/common/drm_common.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
$(VBLANK): $(VBLANK_SOURCES)
	$(CC) $(CFLAGS) -DHAVE_KMS -o $@ $(VBLANK_SOURCES) $(LDFLAGS)
check: fetch-check $(TARGET) $(VBLANK)
	./run_vm.sh ./$(TARGET)
	./run_vm.sh ./$(VBLANK)
fetch-check:
	python3 fetch_debs_test.py
clean:
	rm -f $(TARGET) $(VBLANK)
.PHONY: all check fetch-check clean

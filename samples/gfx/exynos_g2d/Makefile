# gfx/drivers/exynos_gfx.c against a fake kernel and a fake G2D that
# draws into the GEM buffers it is handed: the frame at its aspect, the
# core rendering into the buffer the G2D reads, vsync off, messages
# blended over only what they cover, the menu's alpha, screenshots, mode
# switches and CRT modelines, and a free that leaves nothing; then behind
# an IOMMU, frames read from the core's memory and never through a stale
# mapping, the input overlay blended by the G2D, mirrored where its
# quad is, and a core's rotation turned as the frame is copied.

TARGET    := exynos_g2d_test
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
SOURCES   := exynos_g2d_test.c \
             $(LRC)/compat/compat_strl.c \
             $(LRC)/string/stdstring.c \
             $(LRC)/encodings/encoding_utf.c \
             $(REPO_ROOT)/gfx/video_views.c
CFLAGS    += -Wall -Werror=sign-compare -std=gnu99 -g1 -O0 -I$(REPO_ROOT) \
             -I$(LRC)/include -I$(REPO_ROOT)/tools/platform_stubs/exynos \
             -DHAVE_EXYNOS -DHAVE_MODELINE -DHAVE_OVERLAY $(shell pkg-config --cflags libdrm)
LDFLAGS   += -lm -Wl,--wrap=open,--wrap=close,--wrap=poll
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
$(TARGET): $(SOURCES) $(REPO_ROOT)/gfx/drivers/exynos_gfx.c $(REPO_ROOT)/gfx/common/drm_common.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET)
.PHONY: all check clean

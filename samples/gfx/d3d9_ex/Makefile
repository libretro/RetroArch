# The D3D9 drivers' textures and frame memory on a real Direct3D9Ex
# device and a plain Direct3D 9 one (gfx/common/d3d9_tex.h,
# d3d9_sw_fb.h).  Direct3D 9 is Windows-only, so this builds with
# mingw-w64 (cross, or MSYS2 on Windows) and runs there, or under Wine
# with a display (xvfb-run).

ifeq ($(OS),Windows_NT)
CROSS ?=
else
CROSS ?= x86_64-w64-mingw32-
endif
CC := $(CROSS)gcc

CORE_DIR := ../../..
CFLAGS   += -Wall -Wextra -Wno-unused-parameter -Wno-missing-field-initializers \
	-I$(CORE_DIR)/libretro-common/include -I$(CORE_DIR) -I$(CORE_DIR)/deps \
	-I$(CORE_DIR)/gfx/include -std=gnu99 -g1 -O1 \
	-DRARCH_INTERNAL -DHAVE_D3D -DHAVE_D3D9 -DHAVE_DYLIB -DHAVE_HLSL -DHAVE_MENU \
	-DHAVE_THREADS
CFLAGS   += -flto -fwhole-program
LDFLAGS  += -static -flto
THREAD_SRCS := $(CORE_DIR)/libretro-common/rthreads/rthreads.c \
	$(CORE_DIR)/libretro-common/rthreads/retro_eventcount.c

TARGET := d3d9_ex_test.exe

all: $(TARGET)

$(TARGET): d3d9_ex_test.c $(CORE_DIR)/gfx/common/d3d9_tex.h \
		$(CORE_DIR)/gfx/common/d3d9_sw_fb.h $(CORE_DIR)/gfx/common/d3d9_common.c \
		$(CORE_DIR)/gfx/drivers/d3d9hlsl.c $(CORE_DIR)/gfx/video_thread_hw.c \
		$(CORE_DIR)/gfx/video_thread_hw.h $(CORE_DIR)/gfx/video_thread_wrapper.h \
		$(CORE_DIR)/gfx/video_thread_hw_fence.h $(THREAD_SRCS)
	$(CC) $(CFLAGS) -o $@ $< $(THREAD_SRCS) $(LDFLAGS)

ifeq ($(OS),Windows_NT)
RUN :=
else
RUN := WINEDEBUG=-all xvfb-run -a wine
endif

check: $(TARGET)
	$(RUN) ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

# GET_CURRENT_SOFTWARE_FRAMEBUFFER on the D3D9 drivers: the loan in
# gfx/common/d3d9_sw_fb.h against a stand-in texture.  Direct3D 9 is
# Windows-only, so this builds with mingw-w64 (cross, or MSYS2 on
# Windows) and runs there or under Wine.

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
	-DRARCH_INTERNAL -DHAVE_D3D -DHAVE_D3D9
LDFLAGS  += -static

TARGET := d3d9_sw_fb_test.exe

all: $(TARGET)

$(TARGET): d3d9_sw_fb_test.c $(CORE_DIR)/gfx/common/d3d9_sw_fb.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

ifeq ($(OS),Windows_NT)
RUN :=
else
RUN := WINEDEBUG=-all wine
endif

check: $(TARGET)
	$(RUN) ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

TARGET := vblank_clock_ddraw_test.exe

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

# A Windows program: built with MinGW, run on Windows or under wine
# (with a display - xvfb-run on a headless box).
MINGW_CC := $(shell command -v x86_64-w64-mingw32-gcc 2>/dev/null)
WINE     := $(shell command -v wine 2>/dev/null)

CFLAGS += -std=gnu99 -Wall -O1 -I$(RARCH_DIR) -I$(LIBRETRO_COMM_DIR)/include

ifeq ($(MINGW_CC),)
all check:
	@echo "vblank_clock_ddraw_test: no x86_64-w64-mingw32-gcc, skipped"
else
all: $(TARGET)

$(TARGET): vblank_clock_ddraw_test.c $(RARCH_DIR)/gfx/common/vblank_clock.h
	$(MINGW_CC) $(CFLAGS) -o $@ vblank_clock_ddraw_test.c -lm

check: all
	$(WINE) ./$(TARGET)
endif

clean:
	rm -f $(TARGET)

.PHONY: all check clean

# The winmm joypad driver, with the multimedia library's joystick calls
# replaced by the test's. Needs a mingw compiler and wine.
TARGET := winmm_joypad_test.exe
CC     := x86_64-w64-mingw32-gcc
WINE   ?= $(shell command -v wine64 || command -v wine)

ROOT := ../../..
CFLAGS += -std=gnu99 -O1 -Wall -Wno-unused-function \
          -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_WINMM_JOYPAD \
          -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/deps

all: $(TARGET)

$(TARGET): winmm_joypad_test.c $(ROOT)/input/drivers_joypad/winmm_joypad.c
	$(CC) $(CFLAGS) -o $@ winmm_joypad_test.c \
		$(ROOT)/libretro-common/compat/compat_strl.c

check: all
	@test -n "$(WINE)" || { echo "no wine" >&2; exit 1; }
	WINEDEBUG=-all $(WINE) ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

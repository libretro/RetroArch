# The plain Win32 input driver against a real window, under Wine.
# Needs a mingw compiler, wine and an X display (xvfb-run).
TARGET := win32_input_test.exe
CC     := x86_64-w64-mingw32-gcc
WINE   ?= $(shell command -v wine64 || command -v wine)
XVFB   ?= $(shell command -v xvfb-run >/dev/null 2>&1 && echo "xvfb-run -a")

ROOT := ../../..
CFLAGS += -std=gnu99 -O1 -Wall -Wno-unused-function \
          -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_WIN32INPUT \
          -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/deps

all: $(TARGET)

$(TARGET): win32_input_test.c $(ROOT)/input/drivers/win32_input.c $(ROOT)/input/input_keymaps.c
	$(CC) $(CFLAGS) -o $@ win32_input_test.c $(ROOT)/input/input_keymaps.c \
		$(ROOT)/libretro-common/compat/compat_strl.c

check: all
	@test -n "$(WINE)" || { echo "no wine" >&2; exit 1; }
	WINEDEBUG=-all $(XVFB) $(WINE) ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

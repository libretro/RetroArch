# The waveout audio driver against a real wave output device, under
# Wine. Needs a mingw compiler and wine; with no sound device the test
# skips.
TARGET := waveout_test.exe
CC     := x86_64-w64-mingw32-gcc
WINE   ?= $(shell command -v wine64 || command -v wine)

ROOT := ../../..
CFLAGS += -std=gnu99 -O1 -Wall -Wno-unused-function \
          -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_WAVEOUT \
          -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/deps

all: $(TARGET)

$(TARGET): waveout_test.c $(ROOT)/audio/drivers/waveout.c
	$(CC) $(CFLAGS) -o $@ waveout_test.c -lwinmm

check: all
	@test -n "$(WINE)" || { echo "no wine" >&2; exit 1; }
	WINEDEBUG=-all $(WINE) ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

# Built with mingw-w64 and run under Wine (or on Windows).
TARGET    := win32_ime_test.exe
REPO_ROOT := ../../..
CC        := x86_64-w64-mingw32-gcc
CFLAGS    += -Wall -std=gnu99 -O1 -I$(REPO_ROOT) -I$(REPO_ROOT)/libretro-common/include
LDLIBS    += -limm32 -luser32 -lgdi32
# wine goes by several names; the first one found is used
WINE ?= $(shell for w in wine wine64 /usr/lib/wine/wine64; do \
           command -v $$w >/dev/null 2>&1 && { echo $$w; break; }; done)

all: $(TARGET)

$(TARGET): win32_ime_test.c $(REPO_ROOT)/gfx/common/win32_ime.h
	$(CC) $(CFLAGS) -o $@ win32_ime_test.c $(LDLIBS)

check: all
	WINEDEBUG=-all $(WINE) ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

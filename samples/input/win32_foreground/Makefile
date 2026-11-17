# Built with mingw-w64 and run under Wine (or on Windows).
TARGET    := win32_foreground_test.exe
REPO_ROOT := ../../..
CC        := x86_64-w64-mingw32-gcc
CFLAGS    += -Wall -std=gnu99 -O1 -I$(REPO_ROOT) -I$(REPO_ROOT)/libretro-common/include
LDLIBS    += -luser32
# wine goes by several names; the first one found is used
WINE ?= $(shell for w in wine wine64 /usr/lib/wine/wine64; do \
           command -v $$w >/dev/null 2>&1 && { echo $$w; break; }; done)

# Read every frame: these go through the cache, never Windows directly.
# win32_common.c asks Windows itself only to take the foreground back.
PER_FRAME := input/drivers/dinput.c input/drivers/winraw_input.c \
             input/drivers/win32_input.c ui/drivers/ui_win32.c

all: $(TARGET)

$(TARGET): win32_foreground_test.c $(REPO_ROOT)/gfx/common/win32_foreground.h
	$(CC) $(CFLAGS) -o $@ win32_foreground_test.c $(LDLIBS)

check-sources:
	@cd $(REPO_ROOT) && if grep -n 'GetForegroundWindow *(' $(PER_FRAME); then \
	   echo "FAIL a per-frame reader asks GetForegroundWindow() directly"; exit 1; fi
	@cd $(REPO_ROOT) && awk '/^static void win32_foreground_take_back/ {t=1} \
	   t && /^}/ {t=0} !t && /GetForegroundWindow *\(/ {print FILENAME":"FNR": "$$0; bad=1} \
	   END {exit bad}' gfx/common/win32_common.c || { \
	   echo "FAIL win32_common.c asks GetForegroundWindow() outside the take-back"; exit 1; }
	@echo "ok   per-frame readers go through the cache"

check: all check-sources
	WINEDEBUG=-all $(WINE) ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check check-sources clean

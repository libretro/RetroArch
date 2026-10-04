TARGET := xinput_rumble_test.exe

# The real XInput joypad driver, cross-built with mingw-w64 and run
# under Wine: rumble is written by the driver's own thread, so a
# controller slow to answer does not hold the frame. xinput_joypad.c is
# included by the test itself, for its statics; the frontend symbols
# it links against are stood in for in the test.
#    make && make check
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
CC        := x86_64-w64-mingw32-gcc

CFLAGS  += -Wall -Wno-unused -std=gnu99 -g -O1 \
           -DRARCH_INTERNAL -DHAVE_MENU -DHAVE_THREADS -DHAVE_XINPUT \
           -DHAVE_DYLIB -D_WIN32_WINNT=0x0601 \
           -I$(REPO_ROOT) -I$(REPO_ROOT)/input/include \
           -I$(REPO_ROOT)/gfx/include -I$(LRC)/include -I$(REPO_ROOT)/deps

all: $(TARGET)

$(TARGET): xinput_rumble_test.c $(REPO_ROOT)/input/drivers_joypad/xinput_joypad.c \
		$(REPO_ROOT)/input/drivers_joypad/xinput_rumble_writer.h
	$(CC) $(CFLAGS) -o $@ xinput_rumble_test.c \
		$(REPO_ROOT)/input/common/output_writer.c \
		$(LRC)/compat/compat_strl.c $(LRC)/rthreads/rthreads.c

# wine goes by several names; the first one found is used
WINE ?= $(shell for w in wine wine64 /usr/lib/wine/wine64; do \
           command -v $$w >/dev/null 2>&1 && { echo $$w; break; }; done)

check: all
	@test -n "$(WINE)" || { echo "no wine" >&2; exit 1; }
	WINEDEBUG=-all $(WINE) ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

TARGET := winraw_joypad_motion_test.exe

# The real winraw joypad driver, cross-built with mingw-w64 and run
# under Wine. winraw_joypad.c is included by the test itself, with its
# RawInput and HidP calls routed to fakes describing a device; the
# frontend symbols it links against are stubbed in the test.
#    make && make check
REPO_ROOT := ../../..
CC        := x86_64-w64-mingw32-gcc

CFLAGS  += -Wall -std=gnu89 -Wdeclaration-after-statement -g -O1 \
           -DRARCH_INTERNAL -DHAVE_THREADS \
           -I$(REPO_ROOT) -I$(REPO_ROOT)/input/include \
           -I$(REPO_ROOT)/libretro-common/include -I$(REPO_ROOT)/deps

LDLIBS  += -lhid -lsetupapi -luser32

all: $(TARGET)

$(TARGET): winraw_joypad_motion_test.c \
		$(REPO_ROOT)/input/drivers_joypad/winraw_joypad.c
	$(CC) $(CFLAGS) -o $@ winraw_joypad_motion_test.c \
		$(REPO_ROOT)/libretro-common/compat/compat_strl.c \
		$(REPO_ROOT)/input/common/output_writer.c \
		$(REPO_ROOT)/libretro-common/rthreads/rthreads.c \
		$(REPO_ROOT)/libretro-common/encodings/encoding_crc32.c \
		$(REPO_ROOT)/libretro-common/features/features_cpu.c \
		$(REPO_ROOT)/libretro-common/time/rtime.c $(LDLIBS)

# wine goes by several names; the first one found is used
WINE ?= $(shell for w in wine wine64 /usr/lib/wine/wine64; do \
           command -v $$w >/dev/null 2>&1 && { echo $$w; break; }; done)

check: all
	@test -n "$(WINE)" || { echo "no wine" >&2; exit 1; }
	WINEDEBUG=-all $(WINE) ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

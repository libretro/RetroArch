# The WinUSB HID driver with the real pad handlers behind it, against a
# stand-in for winusb.dll. Built with mingw-w64 and run under Wine (or
# on Windows).
TARGET    := winusb_hid_test.exe
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common
CC        := x86_64-w64-mingw32-gcc
CFLAGS    += -Wall -std=gnu99 -O1 -DHAVE_HID -DHAVE_WINUSB -DHAVE_THREADS \
             -I$(REPO_ROOT) -I$(REPO_ROOT)/input -I$(LRC)/include
LDLIBS    += -lsetupapi
SOURCES   := winusb_hid_test.c \
	$(REPO_ROOT)/input/common/input_hid_common.c \
	$(wildcard $(REPO_ROOT)/input/connect/*.c) \
	$(LRC)/compat/compat_strl.c \
	$(LRC)/compat/compat_strcasestr.c \
	$(LRC)/string/stdstring.c \
	$(LRC)/rthreads/rthreads.c \
	$(LRC)/features/features_cpu.c \
	$(LRC)/time/rtime.c \
	$(LRC)/encodings/encoding_utf.c
# wine goes by several names; the first one found is used
WINE ?= $(shell for w in wine wine64 /usr/lib/wine/wine64; do \
           command -v $$w >/dev/null 2>&1 && { echo $$w; break; }; done)

all: $(TARGET)

$(TARGET): $(SOURCES) $(REPO_ROOT)/input/drivers_hid/winusb_hid.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDLIBS)

check: all
	WINEDEBUG=-all $(WINE) ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

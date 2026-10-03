TARGET := winraw_joypad_axes_test.exe

# The real winraw joypad driver, cross-built with mingw-w64 and run
# under Wine. winraw_joypad.c is included by the test itself, with its
# RawInput and HidP calls routed to fakes describing a device; the
# frontend symbols it links against are stubbed in the test.
#    make && make check
REPO_ROOT := ../../..
CC        := x86_64-w64-mingw32-gcc

CFLAGS  += -Wall -std=gnu89 -Wdeclaration-after-statement -g -O1 \
           -DRARCH_INTERNAL \
           -I$(REPO_ROOT) -I$(REPO_ROOT)/input/include \
           -I$(REPO_ROOT)/libretro-common/include -I$(REPO_ROOT)/deps

LDLIBS  += -lhid -lsetupapi -luser32

all: $(TARGET)

$(TARGET): winraw_joypad_axes_test.c \
		$(REPO_ROOT)/input/drivers_joypad/winraw_joypad.c
	$(CC) $(CFLAGS) -o $@ winraw_joypad_axes_test.c \
		$(REPO_ROOT)/libretro-common/compat/compat_strl.c $(LDLIBS)

check: all
	WINEDEBUG=-all wine ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

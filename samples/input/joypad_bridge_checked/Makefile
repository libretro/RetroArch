# Two joypad drivers held to what the snapshot bridge assumes of them:
# udev on the host, DirectInput cross-built and run under Wine. Each
# test includes its driver's source and fills the pad table by hand.
REPO_ROOT := ../../..
MINGW_CC  ?= x86_64-w64-mingw32-gcc
WINE      ?= $(shell command -v wine || command -v wine64 || echo /usr/lib/wine/wine64)

INCS := -I$(REPO_ROOT) -I$(REPO_ROOT)/libretro-common/include \
        -I$(REPO_ROOT)/deps -I$(REPO_ROOT)/input/include -I$(REPO_ROOT)/gfx/include

all: udev_checked_test dinput_checked_test.exe

udev_checked_test: udev_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/udev_joypad.c
	$(CC) -O1 -Wall -Wno-unused -DRARCH_INTERNAL -DHAVE_UDEV $(INCS) \
	   -o $@ udev_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -ludev

dinput_checked_test.exe: dinput_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/dinput_joypad.c
	$(MINGW_CC) -std=gnu99 -O1 -Wall -Wno-unused -DRARCH_INTERNAL -DHAVE_DINPUT \
	   -DHAVE_MENU -DHAVE_THREADS -D_WIN32_WINNT=0x0601 $(INCS) \
	   -o $@ dinput_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -ldinput8 -ldxguid

check: all
	./udev_checked_test
	WINEDEBUG=-all $(WINE) ./dinput_checked_test.exe

clean:
	rm -f udev_checked_test dinput_checked_test.exe

.PHONY: all check clean

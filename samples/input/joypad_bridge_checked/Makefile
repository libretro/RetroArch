# Joypad drivers held to what the snapshot bridge assumes of them: udev,
# linuxraw and SDL2 on the host, DirectInput and the XInput-only driver
# cross-built and run under Wine. Each test includes its driver's
# source and fills the pad table by hand - or, for SDL2, which has
# none, gives SDL virtual joysticks.
REPO_ROOT := ../../..
MINGW_CC  ?= x86_64-w64-mingw32-gcc
WINE      ?= $(shell command -v wine || command -v wine64 || echo /usr/lib/wine/wine64)

INCS := -I$(REPO_ROOT) -I$(REPO_ROOT)/libretro-common/include \
        -I$(REPO_ROOT)/deps -I$(REPO_ROOT)/input/include -I$(REPO_ROOT)/gfx/include

SDL2_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL2_LIBS   := $(shell sdl2-config --libs 2>/dev/null)

all: udev_checked_test linuxraw_checked_test sdl2_checked_test \
     dinput_checked_test.exe xinput_checked_test.exe

linuxraw_checked_test: linuxraw_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/linuxraw_joypad.c
	$(CC) -O1 -Wall -Wno-unused -DRARCH_INTERNAL $(INCS) \
	   -o $@ linuxraw_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c

sdl2_checked_test: sdl2_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/sdl2_joypad.c
	$(CC) -O1 -Wall -Wno-unused -DRARCH_INTERNAL -DHAVE_SDL2 $(SDL2_CFLAGS) $(INCS) \
	   -o $@ sdl2_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c $(SDL2_LIBS)

xinput_checked_test.exe: xinput_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/xinput_joypad.c
	$(MINGW_CC) -std=gnu99 -O1 -Wall -Wno-unused -DRARCH_INTERNAL -DHAVE_XINPUT \
	   -DHAVE_DYLIB -DHAVE_MENU -DHAVE_THREADS -D_WIN32_WINNT=0x0601 $(INCS) \
	   -o $@ xinput_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c

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
	./linuxraw_checked_test
	./sdl2_checked_test
	WINEDEBUG=-all $(WINE) ./dinput_checked_test.exe
	WINEDEBUG=-all $(WINE) ./xinput_checked_test.exe

clean:
	rm -f udev_checked_test linuxraw_checked_test sdl2_checked_test \
	   dinput_checked_test.exe xinput_checked_test.exe

.PHONY: all check clean

# Joypad drivers held to what the snapshot bridge assumes of them: udev,
# linuxraw, SDL2 and SDL3 on the host, DirectInput and the XInput-only driver
# cross-built and run under Wine. Each test includes its driver's
# source and fills the pad table by hand - or, for SDL2 and SDL3, which have
# none, gives SDL virtual joysticks.
REPO_ROOT := ../../..
MINGW_CC  ?= x86_64-w64-mingw32-gcc
WINE      ?= $(shell command -v wine || command -v wine64 || echo /usr/lib/wine/wine64)

INCS := -I$(REPO_ROOT) -I$(REPO_ROOT)/libretro-common/include \
        -I$(REPO_ROOT)/deps -I$(REPO_ROOT)/input/include -I$(REPO_ROOT)/gfx/include

SDL2_CFLAGS := $(shell sdl2-config --cflags 2>/dev/null)
SDL2_LIBS   := $(shell sdl2-config --libs 2>/dev/null)

# SDL3 is not packaged everywhere yet: its test is built and run where
# pkg-config knows it, or where SDL3_CFLAGS and SDL3_LIBS say where a
# build of it is, and skipped otherwise.
SDL3_CFLAGS ?= $(shell pkg-config --cflags sdl3 2>/dev/null)
SDL3_LIBS   ?= $(shell pkg-config --libs sdl3 2>/dev/null)
ifneq ($(strip $(SDL3_LIBS)),)
SDL3_TEST   := sdl3_checked_test
endif

all: udev_checked_test linuxraw_checked_test sdl2_checked_test $(SDL3_TEST) \
     dinput_checked_test.exe xinput_checked_test.exe

sdl3_checked_test: sdl3_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/sdl3_joypad.c
	$(CC) -O1 -Wall -Wno-unused -DRARCH_INTERNAL -DHAVE_SDL3 $(SDL3_CFLAGS) $(INCS) \
	   -o $@ sdl3_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c $(SDL3_LIBS)

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
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c \
	   $(REPO_ROOT)/input/common/output_writer.c \
	   $(REPO_ROOT)/libretro-common/rthreads/rthreads.c

udev_checked_test: udev_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/udev_joypad.c
	$(CC) -O1 -Wall -Wno-unused -DRARCH_INTERNAL -DHAVE_UDEV $(INCS) \
	   -o $@ udev_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c \
	   $(REPO_ROOT)/input/common/output_writer.c -ludev

dinput_checked_test.exe: dinput_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/dinput_joypad.c
	$(MINGW_CC) -std=gnu99 -O1 -Wall -Wno-unused -DRARCH_INTERNAL -DHAVE_DINPUT \
	   -DHAVE_MENU -DHAVE_THREADS -D_WIN32_WINNT=0x0601 $(INCS) \
	   -o $@ dinput_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -ldinput8 -ldxguid

check: all
	./udev_checked_test
	./linuxraw_checked_test
	./sdl2_checked_test
	@if [ -n "$(SDL3_TEST)" ]; then ./sdl3_checked_test; \
	 else echo "[skip] sdl3_checked_test: no SDL3 here (pkg-config sdl3, or SDL3_CFLAGS and SDL3_LIBS)"; fi
	WINEDEBUG=-all $(WINE) ./dinput_checked_test.exe
	WINEDEBUG=-all $(WINE) ./xinput_checked_test.exe

clean:
	rm -f udev_checked_test linuxraw_checked_test sdl2_checked_test sdl3_checked_test \
	   dinput_checked_test.exe xinput_checked_test.exe

.PHONY: all check clean

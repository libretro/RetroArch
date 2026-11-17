# Joypad drivers held to what the snapshot bridge assumes of them: udev,
# linuxraw, parport, SDL 1.2 and its Dingux driver, SDL2, SDL3 and
# Android (their platforms stood in for) and the test driver, run by a
# script, on the host, the web driver built with emcc and run under node (check-web), DirectInput and the XInput-only driver
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
SDL3_TEST   := sdl3_checked_test sdl3_webos_reject_test
endif

all: udev_checked_test linuxraw_checked_test sdl2_checked_test $(SDL3_TEST) \
     android_checked_test parport_checked_test sdl1_checked_test \
     sdl_dingux_checked_test gekko_checked_test gekko_dol_checked_test \
     gx_libogc_checked_test ps2_checked_test ps3_checked_test \
     ps4_checked_test psp_checked_test vita_checked_test \
     switch_checked_test ctr_checked_test qnx_checked_test xdk_checked_test \
     wiiu_checked_test mfi_checked_test dos_checked_test test_checked_test \
     dinput_checked_test.exe xinput_checked_test.exe winmm_checked_test.exe \
     xinput_hybrid_checked_test.exe

sdl3_checked_test: sdl3_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/sdl3_joypad.c
	$(CC) -O1 -Wall -Wno-unused -DRARCH_INTERNAL -DHAVE_SDL3 $(SDL3_CFLAGS) $(INCS) \
	   -o $@ sdl3_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c $(SDL3_LIBS)

sdl3_webos_reject_test: sdl3_webos_reject_test.c stubs.c $(REPO_ROOT)/input/drivers_joypad/sdl3_joypad.c
	$(CC) -O1 -Wall -Wno-unused -DRARCH_INTERNAL -DHAVE_SDL3 $(SDL3_CFLAGS) $(INCS) \
	   -o $@ sdl3_webos_reject_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c $(SDL3_LIBS)

android_checked_test: android_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/android_joypad.c
	$(CC) -O1 -Wall -Wno-unused -DRARCH_INTERNAL $(INCS) \
	   -o $@ android_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c

parport_checked_test: parport_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/parport_joypad.c
	$(CC) -O1 -Wall -Wno-unused -DRARCH_INTERNAL $(INCS) \
	   -o $@ parport_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c

# SDL 1.2's headers; its joystick calls are answered by the tests. Held
# under ASan and UBSan: a port past the slots must not be read, nor a
# button past the bits shifted for.
SDL1_CFLAGS ?= -I/usr/include/SDL

sdl1_checked_test: sdl1_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/sdl1_joypad.c
	$(CC) -O1 -Wall -Wno-unused -DRARCH_INTERNAL $(SDL1_CFLAGS) $(INCS) \
	   -fsanitize=address,undefined -fno-sanitize-recover=all \
	   -o $@ sdl1_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c

sdl_dingux_checked_test: sdl_dingux_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/sdl_dingux_joypad.c
	$(CC) -O1 -Wall -Wno-unused -DRARCH_INTERNAL $(SDL1_CFLAGS) $(INCS) \
	   -fsanitize=address,undefined -fno-sanitize-recover=all \
	   -o $@ sdl_dingux_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c \
	   $(REPO_ROOT)/input/common/output_writer.c

# The consoles' drivers, built on the host against their platforms'
# headers - the tree's own (os/gekko) or the SDK's fetched at a fixed
# release - with the SDK calls their polls make answered by the tests,
# and the host's identity shed so no Linux branch is taken.
HOSTOFF    := -U__linux__ -U__gnu_linux__ -Ulinux -U__unix__ -U__unix -Uunix
CONSOLE_CC := $(CC) -O1 -Wall -Wno-unused $(HOSTOFF) -DRARCH_INTERNAL \
              -fsanitize=address,undefined -fno-sanitize-recover=all
LIBOGC_TAG ?= v3.1.0
LIBOGC_DIR ?= $(or $(TMPDIR),/tmp)/libogc-$(LIBOGC_TAG)

$(LIBOGC_DIR)/gc/gccore.h:
	rm -rf $(LIBOGC_DIR)
	git -c advice.detachedHead=false clone -q --depth 1 \
	   --branch $(LIBOGC_TAG) https://github.com/devkitPro/libogc $(LIBOGC_DIR)

gekko_checked_test: gekko_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/gekko_joypad.c
	$(CONSOLE_CC) -DGEKKO -DHW_RVL -I$(REPO_ROOT)/os/gekko/include $(INCS) \
	   -o $@ gekko_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c

gekko_dol_checked_test: gekko_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/gekko_joypad.c
	$(CONSOLE_CC) -DGEKKO -DHW_DOL -I$(REPO_ROOT)/os/gekko/include $(INCS) \
	   -o $@ gekko_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c

gx_libogc_checked_test: gx_libogc_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/gx_joypad_libogc.c $(LIBOGC_DIR)/gc/gccore.h
	$(CONSOLE_CC) -DGEKKO -DHW_RVL -DRARCH_CONSOLE -DHAVE_LIBOGC \
	   -I$(LIBOGC_DIR)/gc -I$(REPO_ROOT)/tools/platform_stubs/gekko $(INCS) \
	   -o $@ gx_libogc_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -lm

# The Vita's controller headers: vitasdk's, at a fixed commit
VITA_HEADERS_REV ?= c00157c2abd6dfa05c126e7f2f4efb29e8431ba6
VITA_HEADERS     ?= $(or $(TMPDIR),/tmp)/vita-headers-$(VITA_HEADERS_REV)

$(VITA_HEADERS)/include/psp2/ctrl.h:
	rm -rf $(VITA_HEADERS)
	git init -q $(VITA_HEADERS)
	git -C $(VITA_HEADERS) fetch -q --depth 1 \
	   https://github.com/vitasdk/vita-headers $(VITA_HEADERS_REV)
	git -C $(VITA_HEADERS) -c advice.detachedHead=false checkout -q FETCH_HEAD

ps2_checked_test: ps2_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/ps2_joypad.c
	$(CONSOLE_CC) -DRARCH_CONSOLE -DPS2 -D_EE \
	   -I$(REPO_ROOT)/tools/platform_stubs/ps2 $(INCS) \
	   -o $@ ps2_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -lm

ps3_checked_test: ps3_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/ps3_joypad.c
	$(CONSOLE_CC) -DRARCH_CONSOLE -D__PS3__ -D__PSL1GHT__ \
	   -I$(REPO_ROOT)/tools/platform_stubs/ps3 $(INCS) \
	   -o $@ ps3_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -lm

ps4_checked_test: ps4_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/ps4_joypad.c
	$(CONSOLE_CC) -DRARCH_CONSOLE -DORBIS \
	   -I$(REPO_ROOT)/tools/platform_stubs/orbis $(INCS) \
	   -o $@ ps4_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -lm

psp_checked_test: psp_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/psp_joypad.c
	$(CONSOLE_CC) -DRARCH_CONSOLE -DPSP -D_POSIX_C_SOURCE=199309L \
	   -I$(REPO_ROOT)/tools/platform_stubs/psp $(INCS) \
	   -o $@ psp_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -lm

vita_checked_test: psp_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/psp_joypad.c $(VITA_HEADERS)/include/psp2/ctrl.h
	$(CONSOLE_CC) -DRARCH_CONSOLE -DVITA -DSN_TARGET_PSP2 \
	   -I$(VITA_HEADERS)/include $(INCS) \
	   -o $@ psp_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -lm

switch_checked_test: switch_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/switch_joypad.c
	$(CONSOLE_CC) -DRARCH_CONSOLE -DSWITCH -DHAVE_LIBNX -D__SWITCH__ \
	   -I$(REPO_ROOT)/tools/platform_stubs/libnx $(INCS) \
	   -o $@ switch_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -lm

ctr_checked_test: ctr_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/ctr_joypad.c
	$(CONSOLE_CC) -DRARCH_CONSOLE -D_3DS -D__3DS__ -DARM11 \
	   -I$(REPO_ROOT)/tools/platform_stubs/ctr $(INCS) \
	   -o $@ ctr_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -lm

# QNX's input driver, whose devices are its pads, is built with it; the
# SDK's headers are not public, so qnx_sdk/ names what it reaches for
qnx_checked_test: qnx_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/qnx_joypad.c $(REPO_ROOT)/input/drivers/qnx_input.c $(wildcard qnx_sdk/*/*.h)
	$(CONSOLE_CC) -DRARCH_CONSOLE -D__QNX__ -DHAVE_BB10 \
	   -Iqnx_sdk -I$(REPO_ROOT)/tools/platform_stubs/qnx $(INCS) \
	   -o $@ qnx_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -lm

# The original Xbox's: its XDK is not public, so xdk_sdk/ names what the
# driver reaches for
xdk_checked_test: xdk_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/xdk_joypad.c xdk_sdk/xtl.h
	$(CONSOLE_CC) -DRARCH_CONSOLE -D_XBOX -D_XBOX1 -Ixdk_sdk $(INCS) \
	   -o $@ xdk_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -lm

# The Wii U's: its dispatcher and the GamePad, Wiimote and HID drivers
# behind it, against the tree's own Wii U headers
WIIU_DRIVERS := $(REPO_ROOT)/input/drivers_joypad/wiiu_joypad.c \
   $(wildcard $(REPO_ROOT)/input/drivers_joypad/wiiu/*.c)
wiiu_checked_test: wiiu_checked_test.c bridge_view.h stubs.c $(WIIU_DRIVERS)
	$(CONSOLE_CC) -DRARCH_CONSOLE -DWIIU -D__WIIU__ -DHAVE_HID \
	   -I$(REPO_ROOT)/os/wiiu/include -I$(REPO_ROOT)/tools/platform_stubs/wiiu $(INCS) \
	   -o $@ wiiu_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -lm

# Apple's GameController driver is Objective-C, but what the bridge reads
# of it is plain C: those functions are lifted out of mfi_joypad.m as
# they stand and built on the host
MFI_SRC := $(REPO_ROOT)/input/drivers_joypad/mfi_joypad.m
mfi_extract.inc: $(MFI_SRC)
	awk '/^static int32_t apple_gamecontroller_joypad_button\(/{p=1} p{print} \
	   p&&/^static int16_t apple_gamecontroller_joypad_state\(/{s=1} \
	   p&&s&&/^}/{exit}' $(MFI_SRC) > $@
	grep -q apple_gamecontroller_joypad_state $@
mfi_checked_test: mfi_checked_test.c bridge_view.h mfi_extract.inc
	$(CC) -O1 -Wall -Wno-unused -DRARCH_INTERNAL $(INCS) \
	   -fsanitize=address,undefined -fno-sanitize-recover=all \
	   -o $@ mfi_checked_test.c

# DOS's, which reads no pad yet
dos_checked_test: dos_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/dos_joypad.c
	$(CONSOLE_CC) -DRARCH_CONSOLE -DDJGPP $(INCS) \
	   -o $@ dos_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -lm

# The web driver: emcc and node from an emsdk on the PATH
EMCC ?= emcc
NODE ?= node

rwebpad_checked_test.js: rwebpad_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/rwebpad_joypad.c
	$(EMCC) -O1 -Wall -Wno-unused -DRARCH_INTERNAL $(INCS) \
	   -o $@ rwebpad_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c

check-web: rwebpad_checked_test.js
	$(NODE) ./rwebpad_checked_test.js

LC := $(REPO_ROOT)/libretro-common
test_checked_test: test_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/test_joypad.c
	$(CC) -O1 -Wall -Wno-unused -DRARCH_INTERNAL -DHAVE_TEST_DRIVERS $(INCS) \
	   -fsanitize=address,undefined -fno-sanitize-recover=all \
	   -o $@ test_checked_test.c stubs.c \
	   $(LC)/formats/json/rjson.c $(LC)/string/stdstring.c \
	   $(LC)/encodings/encoding_utf.c $(LC)/compat/compat_strl.c \
	   $(LC)/string/rstrtod.c

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

winmm_checked_test.exe: winmm_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/winmm_joypad.c
	$(MINGW_CC) -std=gnu99 -O1 -Wall -Wno-unused -DRARCH_INTERNAL \
	   -DHAVE_MENU -DHAVE_THREADS -D_WIN32_WINNT=0x0601 $(INCS) \
	   -o $@ winmm_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -lwinmm

dinput_checked_test.exe: dinput_checked_test.c bridge_view.h stubs.c $(REPO_ROOT)/input/drivers_joypad/dinput_joypad.c
	$(MINGW_CC) -std=gnu99 -O1 -Wall -Wno-unused -DRARCH_INTERNAL -DHAVE_DINPUT \
	   -DHAVE_MENU -DHAVE_THREADS -D_WIN32_WINNT=0x0601 $(INCS) \
	   -o $@ dinput_checked_test.c stubs.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c -ldinput8 -ldxguid

# The Windows driver with XInput and DirectInput pads side by side:
# DirectInput's pad table from dinput_joypad.c, linked beside it
xinput_hybrid_checked_test.exe: xinput_hybrid_checked_test.c bridge_view.h stubs.c \
      $(REPO_ROOT)/input/drivers_joypad/xinput_hybrid_joypad.c \
      $(REPO_ROOT)/input/drivers_joypad/dinput_joypad.c \
      $(REPO_ROOT)/input/drivers_joypad/dinput_joypad.h
	$(MINGW_CC) -std=gnu99 -O1 -Wall -Wno-unused -DRARCH_INTERNAL -DHAVE_XINPUT \
	   -DHAVE_DINPUT -DHAVE_DYLIB -DHAVE_MENU -DHAVE_THREADS \
	   -D_WIN32_WINNT=0x0601 $(INCS) \
	   -o $@ xinput_hybrid_checked_test.c stubs.c \
	   $(REPO_ROOT)/input/drivers_joypad/dinput_joypad.c \
	   $(REPO_ROOT)/libretro-common/compat/compat_strl.c \
	   $(REPO_ROOT)/input/common/output_writer.c \
	   $(REPO_ROOT)/libretro-common/rthreads/rthreads.c -ldinput8 -ldxguid

check: all
	./udev_checked_test
	./linuxraw_checked_test
	./android_checked_test
	./parport_checked_test
	./sdl1_checked_test
	./sdl_dingux_checked_test
	./gekko_checked_test
	./gekko_dol_checked_test
	./gx_libogc_checked_test
	./ps2_checked_test
	./ps3_checked_test
	./ps4_checked_test
	./psp_checked_test
	./vita_checked_test
	./switch_checked_test
	./ctr_checked_test
	./qnx_checked_test
	./xdk_checked_test
	./wiiu_checked_test
	./mfi_checked_test
	./dos_checked_test
	./test_checked_test
	./sdl2_checked_test
	@if [ -n "$(SDL3_TEST)" ]; then ./sdl3_checked_test && ./sdl3_webos_reject_test; \
	 else echo "[skip] sdl3_checked_test, sdl3_webos_reject_test: no SDL3 here (pkg-config sdl3, or SDL3_CFLAGS and SDL3_LIBS)"; fi
	WINEDEBUG=-all $(WINE) ./dinput_checked_test.exe
	WINEDEBUG=-all $(WINE) ./xinput_checked_test.exe
	WINEDEBUG=-all $(WINE) ./xinput_hybrid_checked_test.exe
	WINEDEBUG=-all $(WINE) ./winmm_checked_test.exe

clean:
	rm -f udev_checked_test linuxraw_checked_test test_checked_test android_checked_test \
	   parport_checked_test sdl1_checked_test sdl_dingux_checked_test \
	   gekko_checked_test gekko_dol_checked_test gx_libogc_checked_test \
	   ps2_checked_test ps3_checked_test ps4_checked_test psp_checked_test \
	   vita_checked_test switch_checked_test ctr_checked_test qnx_checked_test \
	   xdk_checked_test wiiu_checked_test mfi_checked_test mfi_extract.inc \
	   dos_checked_test \
	   rwebpad_checked_test.js rwebpad_checked_test.wasm \
	   sdl2_checked_test sdl3_checked_test \
	   sdl3_webos_reject_test \
	   dinput_checked_test.exe xinput_checked_test.exe winmm_checked_test.exe \
	   xinput_hybrid_checked_test.exe

.PHONY: all check check-web clean

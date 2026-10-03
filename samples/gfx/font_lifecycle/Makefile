# Font lifecycle tests: create, reload in place, free, under sanitizers.
RA      := ../../..
SAN     ?= address,undefined
CFLAGS  := -std=gnu99 -g -O1 -pthread -fsanitize=$(SAN) -DHAVE_STB_FONT -DRARCH_INTERNAL \
           -I$(RA) -I$(RA)/libretro-common/include -I$(RA)/gfx/include \
           -I$(RA)/deps -I$(RA)/deps/stb
SRC     := $(RA)/gfx/font_driver.c \
           $(RA)/libretro-common/file/file_path.c \
           $(RA)/libretro-common/compat/compat_strl.c \
           $(RA)/libretro-common/string/stdstring.c \
           $(RA)/libretro-common/encodings/encoding_utf.c \
           $(RA)/libretro-common/lists/string_list.c \
           $(RA)/libretro-common/time/rtime.c
all: lifecycle lifecycle-threaded create fallback chain frame vg sdl2
lifecycle: font_lifecycle_test.c stubs_retroarch.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ -lm
# Same tests with the marshalling branch compiled in, so a rebuild
# taking a different thread route than its creation is caught.
lifecycle-threaded: font_lifecycle_test.c stubs_retroarch.c $(SRC)
	$(CC) $(CFLAGS) -DHAVE_THREADS -o $@ $^ -lm
create: font_create_test.c stubs_retroarch.c $(SRC) \
        $(RA)/gfx/drivers_font_renderer/stb.c $(RA)/gfx/bitmapfont.c
	$(CC) $(CFLAGS) -DFONT_TEST_REAL_STB -o $@ $^ -lm
# The fallback font read on its own thread, against two fonts asking
# at once: real stb, real rthreads.
fallback: font_fallback_test.c stubs_retroarch.c $(SRC) \
        $(RA)/gfx/drivers_font_renderer/stb.c $(RA)/gfx/bitmapfont.c \
        $(RA)/libretro-common/rthreads/rthreads.c
	$(CC) $(CFLAGS) -DHAVE_THREADS -DFONT_TEST_REAL_STB \
	   -DFONT_TEST_REAL_THREADS -o $@ $^ -lm
# A second fallback face for a range the first lacks
chain: font_fallback_chain_test.c stubs_retroarch.c $(SRC) \
        $(RA)/gfx/drivers_font_renderer/stb.c $(RA)/gfx/bitmapfont.c \
        $(RA)/libretro-common/rthreads/rthreads.c
	$(CC) $(CFLAGS) -DHAVE_THREADS -DFONT_TEST_REAL_STB \
	   -DFONT_TEST_REAL_THREADS -o $@ $^ -lm
# Cells drawn from in a frame are kept for that frame
frame: font_frame_test.c stubs_retroarch.c $(SRC) \
        $(RA)/gfx/drivers_font_renderer/stb.c $(RA)/gfx/bitmapfont.c
	$(CC) $(CFLAGS) -DFONT_TEST_REAL_STB -o $@ $^ -lm
# OpenVG's text path, from the shipping gfx/drivers/vg.c, drawn onto
# the software OpenVG in font_vg_test.c.
# The test includes vg.c, so it is a prerequisite but not compiled on
# its own.
vg: font_vg_test.c stubs_retroarch.c $(SRC) \
        $(RA)/gfx/drivers_font_renderer/stb.c $(RA)/gfx/bitmapfont.c \
        $(RA)/gfx/drivers/vg.c $(RA)/gfx/font_layout.h
	$(CC) $(CFLAGS) -DFONT_TEST_REAL_STB -DHAVE_VG -DHAVE_EGL \
	   -I$(RA)/tools/platform_stubs/openvg -Ivg_mock \
	   -o $@ $(filter %.c,$(filter-out $(RA)/gfx/drivers/vg.c,$^)) -lm
# SDL2's legacy on-screen message path, from the shipping
# gfx/drivers/sdl2_gfx.c, drawn by SDL2's software renderer. The test
# includes the driver and calls only that path, so the frontend the
# rest of it names is left unresolved (GNU ld, non-PIE). Skipped
# without SDL2's development files.
SDL2_CFLAGS := $(shell pkg-config --cflags sdl2 2>/dev/null)
SDL2_LIBS   := $(shell pkg-config --libs sdl2 2>/dev/null)
sdl2: font_sdl2_test.c stubs_retroarch.c $(SRC) \
        $(RA)/gfx/drivers_font_renderer/stb.c $(RA)/gfx/bitmapfont.c \
        $(RA)/gfx/drivers/sdl2_gfx.c $(RA)/gfx/font_layout.h
ifneq ($(SDL2_LIBS),)
	$(CC) $(CFLAGS) -DFONT_TEST_REAL_STB -DHAVE_SDL2 -DHAVE_MENU $(SDL2_CFLAGS) -no-pie \
	   -o $@ $(filter %.c,$(filter-out $(RA)/gfx/drivers/sdl2_gfx.c,$^)) \
	   -Wl,--unresolved-symbols=ignore-all $(SDL2_LIBS) -lm
else
	printf '#!/bin/sh\necho "  skip: no SDL2 development files"\n' > $@
	chmod +x $@
endif
check: all
	ASAN_OPTIONS=detect_leaks=1 ./lifecycle
	ASAN_OPTIONS=detect_leaks=1 ./lifecycle-threaded
	ASAN_OPTIONS=detect_leaks=1 ./create
	ASAN_OPTIONS=detect_leaks=1 ./fallback
	ASAN_OPTIONS=detect_leaks=1 ./chain
	ASAN_OPTIONS=detect_leaks=1 ./frame
	ASAN_OPTIONS=detect_leaks=1 ./vg
	ASAN_OPTIONS=detect_leaks=1 ./sdl2
clean:
	rm -f lifecycle lifecycle-threaded create fallback chain frame vg sdl2
.PHONY: all check clean

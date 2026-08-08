# Font lifecycle tests: create, reload in place, free, under sanitizers.
RA      := ../../..
SAN     ?= address,undefined
CFLAGS  := -std=gnu99 -g -O1 -fsanitize=$(SAN) -DHAVE_STB_FONT -DRARCH_INTERNAL \
           -I$(RA) -I$(RA)/libretro-common/include -I$(RA)/gfx/include \
           -I$(RA)/deps -I$(RA)/deps/stb
SRC     := $(RA)/gfx/font_driver.c \
           $(RA)/libretro-common/file/file_path.c \
           $(RA)/libretro-common/compat/compat_strl.c \
           $(RA)/libretro-common/string/stdstring.c \
           $(RA)/libretro-common/encodings/encoding_utf.c \
           $(RA)/libretro-common/lists/string_list.c \
           $(RA)/libretro-common/time/rtime.c
all: lifecycle create
lifecycle: font_lifecycle_test.c stubs_retroarch.c $(SRC)
	$(CC) $(CFLAGS) -o $@ $^ -lm
create: font_create_test.c stubs_retroarch.c $(SRC) \
        $(RA)/gfx/drivers_font_renderer/stb.c $(RA)/gfx/bitmapfont.c
	$(CC) $(CFLAGS) -DFONT_TEST_REAL_STB -o $@ $^ -lm
check: all
	ASAN_OPTIONS=detect_leaks=1 ./lifecycle
	ASAN_OPTIONS=detect_leaks=1 ./create
clean:
	rm -f lifecycle create
.PHONY: all check clean

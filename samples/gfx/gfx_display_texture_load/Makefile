# gfx_display's texture loads against a stub driver and a simulated
# threaded wrapper: a load into an occupied slot replaces its texture
# only once the new one has landed, and a gfx_surface still owns its
# texture across uploads, decodes and a free in flight. gfx_display.c
# is linked whole; only the load path is called.
RA     := ../../..
CC     ?= gcc
CFLAGS += -Wall -Wextra -g -O1 -std=gnu99 -I$(RA) -I$(RA)/libretro-common/include \
          -I$(RA)/deps -DHAVE_THREADS -DRARCH_INTERNAL $(EXTRA_CFLAGS)
SRC    := gfx_display_texture_load_test.c stubs.c $(RA)/gfx/gfx_display.c \
          $(RA)/gfx/gfx_surface.c $(RA)/gfx/gfx_instrument.c \
          $(RA)/libretro-common/compat/compat_strl.c \
          $(RA)/libretro-common/formats/image_texture.c
TARGET := gfx_display_texture_load_test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm $(LDFLAGS)

check: all
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

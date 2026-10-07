# gfx_surface stills against a stub driver, a simulated threaded
# wrapper and a held task queue: a still owns its texture across
# uploads, decodes and a free in flight.
RA     := ../../..
CC     ?= gcc
CFLAGS += -Wall -Wextra -g -O1 -std=gnu99 -I$(RA) -I$(RA)/libretro-common/include \
          -I$(RA)/deps -DHAVE_THREADS -DRARCH_INTERNAL $(EXTRA_CFLAGS)
SRC    := gfx_surface_still_test.c stubs.c \
          $(RA)/gfx/gfx_surface.c $(RA)/gfx/gfx_instrument.c \
          $(RA)/libretro-common/compat/compat_strl.c \
          $(RA)/libretro-common/formats/image_texture.c
TARGET := gfx_surface_still_test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm $(LDFLAGS)

check: all
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

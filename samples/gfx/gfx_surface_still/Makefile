# gfx_surface stills against a stub driver, a simulated threaded
# wrapper, a held task queue and a stub image decoder under the real
# set decode (image_texture_set.c): a still owns its texture across
# uploads, decodes and a free in flight, and a set of files is decoded
# across threads and goes up in order.
RA     := ../../..
CC     ?= gcc
CFLAGS += -Wall -Wextra -g -O1 -std=gnu99 -I$(RA) -I$(RA)/libretro-common/include \
          -I$(RA)/deps -DHAVE_THREADS -DRARCH_INTERNAL $(EXTRA_CFLAGS)
SRC    := gfx_surface_still_test.c \
          $(RA)/gfx/gfx_surface.c $(RA)/libretro-common/formats/image/image_yuv_blit.c $(RA)/gfx/gfx_instrument.c \
          $(RA)/libretro-common/formats/image_texture_set.c \
          $(RA)/libretro-common/compat/compat_strl.c \
          $(RA)/libretro-common/rthreads/rthreads.c \
          $(RA)/libretro-common/features/features_cpu.c \
          $(RA)/libretro-common/time/rtime.c
TARGET := gfx_surface_still_test

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) -o $@ $(SRC) -lm -lpthread $(LDFLAGS)

check: all
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

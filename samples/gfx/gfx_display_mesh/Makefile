# The mesh facility in gfx/gfx_display.c against a recording display
# driver. gfx_display.c is linked whole and only the mesh calls are
# made, so the frontend the rest of it names is left unresolved (GNU ld,
# non-PIE).
RA     := ../../..
SAN    ?= address,undefined
CFLAGS := -std=gnu99 -g -O1 -fsanitize=$(SAN) -DRARCH_INTERNAL -DHAVE_MENU \
          -DHAVE_SHADERPIPELINE \
          -I$(RA) -I$(RA)/libretro-common/include -I$(RA)/deps
SRC    := gfx_display_mesh_test.c $(RA)/gfx/gfx_display.c \
          $(RA)/libretro-common/compat/compat_strl.c

all: gfx_display_mesh_test

gfx_display_mesh_test: $(SRC) $(RA)/gfx/gfx_display.h
	$(CC) $(CFLAGS) -no-pie -o $@ $(SRC) -no-pie \
	   -Wl,--unresolved-symbols=ignore-all -lm

check: all
	ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=halt_on_error=1 ./gfx_display_mesh_test

clean:
	rm -f gfx_display_mesh_test

.PHONY: all check clean

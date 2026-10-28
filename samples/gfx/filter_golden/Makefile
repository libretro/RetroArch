# Each listed video filter against its recorded output hashes; see
# filter_golden_test.c.
FILTERS := 2xsai super2xsai supereagle phosphor2x blargg_ntsc_snes dedither 2xbr \
	pixel_art_aa normal4x grid4x gameboy4x dot_matrix_4x \
	normal2x normal2x_width normal2x_height scanline2x grid2x grid3x \
	gameboy3x dot_matrix_3x upscale_1_5x upscale_1_66x_fast scale2x
# Blargg NTSC SNES again with each other tvtype, which builds a
# different table (RF also leaves the fields unmerged).
SNES_TV := rf rgb svideo
# The filters with vector code, which also get a build without it.
SIMD_FILTERS := blargg_ntsc_snes dedither 2xbr pixel_art_aa 2xsai \
	super2xsai supereagle scale2x
C_TARGETS := $(addsuffix _c,$(addprefix golden_,$(SIMD_FILTERS)))
TARGETS := $(addprefix golden_,$(FILTERS)) \
	$(addprefix golden_blargg_ntsc_snes_,$(SNES_TV)) \
	$(C_TARGETS)
SNES_NTSC := $(wildcard ../../../gfx/video_filters/snes_ntsc/*)

CFLAGS  += -Wall -std=gnu99 -O2 -I../../../libretro-common/include \
	-I../../../gfx/video_filters
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGETS)

golden_%: filter_golden_test.c golden_%.h ../../../gfx/video_filters/%.c
	$(CC) $(CFLAGS) -DFILTER_C='"../../../gfx/video_filters/$*.c"' \
		-DGOLDEN_H='"golden_$*.h"' -DFILTER_NAME='"golden_$*"' \
		-o $@ filter_golden_test.c $(LDFLAGS)

golden_blargg_ntsc_snes: $(SNES_NTSC)

golden_blargg_ntsc_snes_%: filter_golden_test.c golden_blargg_ntsc_snes_%.h \
		../../../gfx/video_filters/blargg_ntsc_snes.c $(SNES_NTSC)
	$(CC) $(CFLAGS) \
		-DFILTER_C='"../../../gfx/video_filters/blargg_ntsc_snes.c"' \
		-DGOLDEN_H='"golden_blargg_ntsc_snes_$*.h"' -DGOLDEN_STR='"$*"' \
		-DFILTER_NAME='"golden_blargg_ntsc_snes_$*"' \
		-o $@ filter_golden_test.c $(LDFLAGS)

# Each filter with vector code, built with it left out and held to the
# same table.
$(C_TARGETS): golden_%_c: filter_golden_test.c golden_%.h \
		../../../gfx/video_filters/%.c $(SNES_NTSC)
	$(CC) $(CFLAGS) -DSOFTFILTER_NO_SIMD \
		-DFILTER_C='"../../../gfx/video_filters/$*.c"' \
		-DGOLDEN_H='"golden_$*.h"' -DFILTER_NAME='"golden_$*_c"' \
		-o $@ filter_golden_test.c $(LDFLAGS)

# One target a test, so make -j builds and runs them side by side.
RUNS := $(addprefix run_,$(TARGETS))

check: $(RUNS)

$(RUNS): run_%: %
	@./$*

clean:
	rm -f $(TARGETS)

.PHONY: all check clean $(RUNS)

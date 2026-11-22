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
# Recorded floating-point kernels use separate multiply/add rounding.
GOLDEN_FP_FLAGS := -ffp-contract=off
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGETS)

golden_%: filter_golden_test.c golden_%.h ../../../gfx/video_filters/%.c
	$(CC) $(CFLAGS) $(GOLDEN_FP_FLAGS) -DFILTER_C='"../../../gfx/video_filters/$*.c"' \
		-DGOLDEN_H='"golden_$*.h"' -DFILTER_NAME='"golden_$*"' \
		-o $@ filter_golden_test.c $(LDFLAGS)

golden_blargg_ntsc_snes: $(SNES_NTSC)

golden_blargg_ntsc_snes_%: filter_golden_test.c golden_blargg_ntsc_snes_%.h \
		../../../gfx/video_filters/blargg_ntsc_snes.c $(SNES_NTSC)
	$(CC) $(CFLAGS) $(GOLDEN_FP_FLAGS) \
		-DFILTER_C='"../../../gfx/video_filters/blargg_ntsc_snes.c"' \
		-DGOLDEN_H='"golden_blargg_ntsc_snes_$*.h"' -DGOLDEN_STR='"$*"' \
		-DFILTER_NAME='"golden_blargg_ntsc_snes_$*"' \
		-o $@ filter_golden_test.c $(LDFLAGS)

# Each filter with vector code, built with it left out and held to the
# same table.
$(C_TARGETS): golden_%_c: filter_golden_test.c golden_%.h \
		../../../gfx/video_filters/%.c $(SNES_NTSC)
	$(CC) $(CFLAGS) $(GOLDEN_FP_FLAGS) -DSOFTFILTER_NO_SIMD \
		-DFILTER_C='"../../../gfx/video_filters/$*.c"' \
		-DGOLDEN_H='"golden_$*.h"' -DFILTER_NAME='"golden_$*_c"' \
		-o $@ filter_golden_test.c $(LDFLAGS)

# One target a test, so make -j builds and runs them side by side.
RUNS := $(addprefix run_,$(TARGETS))

check: $(RUNS)

$(RUNS): run_%: %
	@$(RUNNER) ./$*

fp_contract_probe_test: fp_contract_probe_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

FMA_TARGETS := golden_blargg_ntsc_snes golden_blargg_ntsc_snes_c \
	$(addprefix golden_blargg_ntsc_snes_,$(SNES_TV))
check-fma: fp_contract_probe_test
	@status=0; $(RUNNER) ./fp_contract_probe_test || status=$$?; \
	if [ $$status -eq 77 ]; then \
		echo "skip: FMA compiler target needs an FMA-capable x86 host"; \
	elif [ $$status -ne 0 ]; then exit $$status; \
	else $(MAKE) -B GOLDEN_FP_FLAGS=-ffp-contract=fast CC="$(CC) -mfma" $(addprefix run_,$(FMA_TARGETS)); fi

clean:
	rm -f $(TARGETS) fp_contract_probe_test snes_fixed_math_test snes_fixed_math_portable_test

.PHONY: all check check-fma clean $(RUNS)

# Native and limb arithmetic, both checked against an independent 128-bit oracle.
snes_fixed_math_test: snes_fixed_math_test.c $(SNES_NTSC)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
snes_fixed_math_portable_test: snes_fixed_math_test.c $(SNES_NTSC)
	$(CC) $(CFLAGS) -DSNES_NTSC_FIXED_PORTABLE -o $@ $< $(LDFLAGS)
check-fixed: snes_fixed_math_test snes_fixed_math_portable_test
	$(RUNNER) ./snes_fixed_math_test
	$(RUNNER) ./snes_fixed_math_portable_test
	CC="$(CC)" RUNNER="$(RUNNER)" COMPARE_CFLAGS="$(CFLAGS)" python3 snes_fixed_compare.py
.PHONY: check-fixed

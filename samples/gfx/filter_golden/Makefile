# Each listed video filter against its recorded output hashes; see
# filter_golden_test.c.
FILTERS := 2xsai super2xsai supereagle phosphor2x blargg_ntsc_snes
# Blargg NTSC SNES again with each other tvtype, which builds a
# different table (RF also leaves the fields unmerged).
SNES_TV := rf rgb svideo
# and with its vector blitters left out, so the plain C ones are held
# to the same table on machines that have the vector ones.
TARGETS := $(addprefix golden_,$(FILTERS)) \
	$(addprefix golden_blargg_ntsc_snes_,$(SNES_TV)) \
	golden_blargg_ntsc_snes_c
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

golden_blargg_ntsc_snes_c: filter_golden_test.c golden_blargg_ntsc_snes.h \
		../../../gfx/video_filters/blargg_ntsc_snes.c $(SNES_NTSC)
	$(CC) $(CFLAGS) -DSNES_NTSC_NO_SIMD \
		-DFILTER_C='"../../../gfx/video_filters/blargg_ntsc_snes.c"' \
		-DGOLDEN_H='"golden_blargg_ntsc_snes.h"' \
		-DFILTER_NAME='"golden_blargg_ntsc_snes_c"' \
		-o $@ filter_golden_test.c $(LDFLAGS)

check: $(TARGETS)
	@set -e; for t in $(TARGETS); do ./$$t; done

clean:
	rm -f $(TARGETS)

.PHONY: all check clean

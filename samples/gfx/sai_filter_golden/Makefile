# Each 2xSaI-family filter against its recorded output hashes; see
# sai_filter_golden_test.c.
FILTERS := 2xsai super2xsai supereagle
TARGETS := $(addprefix sai_golden_,$(FILTERS))

CFLAGS  += -Wall -std=gnu99 -O2 -I../../../libretro-common/include \
	-I../../../gfx/video_filters
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGETS)

sai_golden_%: sai_filter_golden_test.c golden_%.h ../../../gfx/video_filters/%.c
	$(CC) $(CFLAGS) -DFILTER_C='"../../../gfx/video_filters/$*.c"' \
		-DGOLDEN_H='"golden_$*.h"' -DFILTER_NAME='"sai_golden_$*"' \
		-o $@ sai_filter_golden_test.c $(LDFLAGS)

check: $(TARGETS)
	@set -e; for t in $(TARGETS); do ./$$t; done

clean:
	rm -f $(TARGETS)

.PHONY: all check clean

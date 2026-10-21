CC ?= cc
RA := ../../..
CFLAGS += -O1 -g1 -std=gnu89 -Wall -Wextra -Werror -Wframe-larger-than=2048 \
          -DHAVE_RECORD_METAL -DHAVE_THREADS \
          -DHAVE_CONFIGFILE -I$(RA)/libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: capture_test
capture_fragment.c: $(RA)/record/drivers/record_metal.c
	sed -n '/^typedef struct record_metal_context/,/^} record_metal_context_t;/p' $< > $@
	sed -n '/^void record_metal_gpu_capture/,/^void record_metal_stop/{ /^void record_metal_stop/q; p; }' $< >> $@
	sed -n '/^static bool record_metal_audio/,/^const record_driver_t record_metal/{ /^const record_driver_t record_metal/q; p; }' $< >> $@
capture_test: capture_test.c capture_fragment.c
	$(CC) $(CFLAGS) capture_test.c -o $@ $(LDFLAGS) -lm
check: capture_test
	./capture_test
clean:
	rm -f capture_test capture_fragment.c
.PHONY: all check clean

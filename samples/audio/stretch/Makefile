CC = gcc
ROOT = ../../..
CPPFLAGS += -I$(ROOT)/libretro-common/include
# -std=c89 is deliberate: the pipeline headers have to stay C89-clean.
# _POSIX_C_SOURCE is what lets retro_timers.h still see struct timespec
# and nanosleep under it, for the yield in the spin waits.
CFLAGS += -O2 -std=c89 -Wall -Wextra -D_POSIX_C_SOURCE=199309L
WRAP_LDFLAGS = -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free
LDLIBS += -lm
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
endif
SOURCES = stretch_test.c $(ROOT)/audio/audio_stretch.c
all: stretch_filter_test stretch_test stretch_src_test stretch_search_test stretch_canonical_test stretch_epoch_test stretch_pipeline_test resampler_reset_test
stretch_test: $(SOURCES) $(ROOT)/audio/audio_stretch.h $(ROOT)/libretro-common/include/audio/wsola_search.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) $(WRAP_LDFLAGS) $(LDLIBS) -o $@
stretch_src_test: stretch_src_test.c $(SOURCES) $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler_int16.c $(ROOT)/libretro-common/memmap/memalign.c
	$(CC) $(CPPFLAGS) $(CFLAGS) stretch_src_test.c $(ROOT)/audio/audio_stretch.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler_int16.c $(ROOT)/libretro-common/memmap/memalign.c $(LDFLAGS) $(WRAP_LDFLAGS) $(LDLIBS) -o $@
stretch_search_test: stretch_search_test.c $(ROOT)/audio/audio_stretch.c $(ROOT)/audio/audio_stretch.h $(ROOT)/libretro-common/include/audio/wsola_search.h
	$(CC) $(CPPFLAGS) $(CFLAGS) stretch_search_test.c $(LDFLAGS) $(LDLIBS) -o $@
stretch_canonical_test: stretch_canonical_test.c $(SOURCES) $(ROOT)/audio/audio_stretch.h
	$(CC) $(CPPFLAGS) $(CFLAGS) stretch_canonical_test.c $(ROOT)/audio/audio_stretch.c $(LDFLAGS) $(WRAP_LDFLAGS) $(LDLIBS) -o $@
stretch_epoch_test: stretch_epoch_test.c stretch_src_test.c $(SOURCES) $(ROOT)/audio/audio_stretch.h $(ROOT)/audio/audio_pipeline_layout.h $(ROOT)/libretro-common/queues/retro_spsc.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler_int16.c $(ROOT)/libretro-common/memmap/memalign.c
	$(CC) $(CPPFLAGS) $(CFLAGS) stretch_epoch_test.c $(ROOT)/audio/audio_stretch.c $(ROOT)/libretro-common/queues/retro_spsc.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler_int16.c $(ROOT)/libretro-common/memmap/memalign.c $(LDFLAGS) $(WRAP_LDFLAGS) $(LDLIBS) -o $@
stretch_pipeline_test: stretch_pipeline_test.c stretch_epoch_test.c stretch_src_test.c $(SOURCES) $(ROOT)/audio/audio_stretch.h $(ROOT)/audio/audio_pipeline_stretch.c $(ROOT)/audio/audio_speed_lpf.c $(ROOT)/audio/audio_pipeline_stretch.h $(ROOT)/audio/audio_pipeline_layout.h $(ROOT)/libretro-common/queues/retro_spsc.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler_int16.c $(ROOT)/libretro-common/memmap/memalign.c $(ROOT)/libretro-common/rthreads/rthreads.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DHAVE_THREADS stretch_pipeline_test.c $(ROOT)/audio/audio_stretch.c $(ROOT)/audio/audio_pipeline_stretch.c $(ROOT)/audio/audio_speed_lpf.c $(ROOT)/libretro-common/queues/retro_spsc.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler_int16.c $(ROOT)/libretro-common/memmap/memalign.c $(ROOT)/libretro-common/rthreads/rthreads.c $(LDFLAGS) -lpthread $(WRAP_LDFLAGS) $(LDLIBS) -o $@
resampler_reset_test: resampler_reset_test.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler_int16.c $(ROOT)/libretro-common/audio/resampler/drivers/nearest_resampler.c $(ROOT)/libretro-common/audio/resampler/drivers/nearest_resampler_int16.c $(ROOT)/audio/drivers_resampler/cc_resampler.c $(ROOT)/audio/drivers_resampler/cc_resampler_int16.c $(ROOT)/libretro-common/memmap/memalign.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -DHAVE_CC_RESAMPLER resampler_reset_test.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler.c $(ROOT)/libretro-common/audio/resampler/drivers/sinc_resampler_int16.c $(ROOT)/libretro-common/audio/resampler/drivers/nearest_resampler.c $(ROOT)/libretro-common/audio/resampler/drivers/nearest_resampler_int16.c $(ROOT)/audio/drivers_resampler/cc_resampler.c $(ROOT)/audio/drivers_resampler/cc_resampler_int16.c $(ROOT)/libretro-common/memmap/memalign.c $(LDFLAGS) $(WRAP_LDFLAGS) $(LDLIBS) -o $@
stretch_bench: stretch_bench.c $(ROOT)/audio/audio_stretch.c $(ROOT)/audio/audio_stretch.h $(ROOT)/libretro-common/include/audio/wsola_search.h
	$(CC) $(CPPFLAGS) $(CFLAGS) stretch_bench.c $(ROOT)/audio/audio_stretch.c $(LDFLAGS) $(LDLIBS) -o $@
bench: stretch_bench
	./stretch_bench
TESTS = stretch_filter_test stretch_test stretch_src_test stretch_search_test stretch_canonical_test stretch_epoch_test stretch_pipeline_test resampler_reset_test
# Override with an empty TIMEOUT on hosts that lack coreutils timeout.
TIMEOUT ?= timeout 600

check: $(TESTS:%=run-%)

run-%: %
	@echo "=== $* ==="
	$(TIMEOUT) ./$*
stretch_filter_test: stretch_filter_test.c $(ROOT)/audio/audio_pipeline_stretch.c $(ROOT)/audio/audio_stretch.c $(ROOT)/audio/audio_speed_lpf.c $(ROOT)/audio/audio_pipeline_stretch.h $(ROOT)/audio/audio_pipeline_layout.h $(ROOT)/audio/audio_speed_lpf.h $(ROOT)/audio/audio_stretch.h
	$(CC) $(CPPFLAGS) $(CFLAGS) stretch_filter_test.c $(ROOT)/audio/audio_pipeline_stretch.c $(ROOT)/audio/audio_stretch.c $(ROOT)/audio/audio_speed_lpf.c $(ROOT)/libretro-common/queues/retro_spsc.c $(LDFLAGS) $(WRAP_LDFLAGS) $(LDLIBS) -o $@
clean:
	rm -f stretch_filter_test stretch_filter_test.exe stretch_test stretch_test.exe stretch_src_test stretch_src_test.exe stretch_search_test stretch_search_test.exe stretch_bench stretch_bench.exe stretch_canonical_test stretch_canonical_test.exe stretch_epoch_test stretch_epoch_test.exe stretch_pipeline_test stretch_pipeline_test.exe resampler_reset_test resampler_reset_test.exe
.PHONY: all check clean bench

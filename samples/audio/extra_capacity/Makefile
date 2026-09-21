CC = gcc
ROOT = ../../..
COMMON = $(ROOT)/libretro-common
SIMD_MASK ?= 0
CPPFLAGS += -DEXTRA_TEST_SIMD=$(SIMD_MASK)
CPPFLAGS += -DRARCH_INTERNAL -I$(ROOT) -I$(COMMON)/include -I$(ROOT)/deps -I$(ROOT)/deps/stb
CFLAGS += -O2 -std=gnu99 -ffunction-sections -fdata-sections
LDFLAGS += -Wl,--gc-sections
LDLIBS += -lm
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
endif
SOURCES = $(COMMON)/audio/resampler/drivers/sinc_resampler.c \
 $(COMMON)/audio/resampler/drivers/sinc_resampler_int16.c \
 $(COMMON)/audio/conversion/s16_to_float.c $(COMMON)/audio/conversion/float_to_s16.c \
 $(COMMON)/memmap/memalign.c $(ROOT)/audio/audio_upmix.c
all: extra_capacity_test hq_settings_test
# Internalize unrelated frontend entry points in this focused include harness.
extra_capacity_test.o: extra_capacity_test.c $(ROOT)/audio/audio_driver.c $(ROOT)/audio/audio_driver.h $(ROOT)/audio/audio_pipeline_layout.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fwhole-program -c $< -o $@
extra_capacity_test: extra_capacity_test.o $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -Wl,--wrap=free $(LDLIBS) -o $@
hq_settings_test: hq_settings_test.c $(ROOT)/configuration.c $(ROOT)/configuration.h $(ROOT)/settings/settings_def_audio_resampler_quality.h $(ROOT)/settings/settings_def_audio_sync.h
	$(CC) $(CPPFLAGS) $(CFLAGS) -fwhole-program $< $(LDFLAGS) $(LDLIBS) -o $@
check: extra_capacity_test hq_settings_test
	./extra_capacity_test
	./hq_settings_test
clean:
	rm -f extra_capacity_test extra_capacity_test.exe extra_capacity_test.o hq_settings_test hq_settings_test.exe
.PHONY: all check clean

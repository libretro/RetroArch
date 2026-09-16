CC = gcc
ROOT = ../../..
COMMON = $(ROOT)/libretro-common
CPPFLAGS += -I$(COMMON)/include -DHAVE_NEAREST_RESAMPLER
CFLAGS += -O2 -std=c89
LDLIBS += -lm
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
endif
SOURCES = resampler_options_test.c $(COMMON)/audio/resampler/audio_resampler.c \
 $(COMMON)/audio/resampler/drivers/sinc_resampler.c \
 $(COMMON)/audio/resampler/drivers/nearest_resampler.c $(COMMON)/memmap/memalign.c
all: resampler_options_test
resampler_options_test: $(SOURCES) $(COMMON)/include/audio/audio_resampler.h \
 $(COMMON)/audio/resampler/drivers/sinc_resampler_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@
check: resampler_options_test
	./resampler_options_test
clean:
	rm -f resampler_options_test resampler_options_test.exe
.PHONY: all check clean

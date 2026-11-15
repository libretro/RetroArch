CC ?= cc
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -I../../../libretro-common/include
ifneq ($(findstring clang,$(shell $(CC) --version)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
SOURCES = sinc_aligned_test.c ../../../libretro-common/audio/resampler/drivers/sinc_resampler_int16.c ../../../libretro-common/memmap/memalign.c
all: sinc_aligned_test clock_test
sinc_aligned_test: $(SOURCES) ../../../libretro-common/include/audio/sinc_resampler_int16.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS) -lm
clock_test: clock_test.c ../../../libretro-common/audio/resampler/drivers/sinc_resampler_int16.c
	$(CC) $(CFLAGS) clock_test.c ../../../libretro-common/memmap/memalign.c -o $@ $(LDFLAGS) -lm
check: all
	./sinc_aligned_test
	./clock_test
clean:
	rm -f sinc_aligned_test sinc_aligned_test.exe clock_test clock_test.exe
.PHONY: all check clean

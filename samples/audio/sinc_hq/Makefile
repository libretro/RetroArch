CC = gcc
ROOT = ../../..
COMMON = $(ROOT)/libretro-common
CPPFLAGS += -I$(COMMON)/include
# GNU ld wrappers catch allocation and deallocation on the processing path.
ifneq ($(ALLOC_GUARD),0)
CPPFLAGS += -DSINC_TRACK_ALLOCATIONS
LDFLAGS += -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc \
 -Wl,--wrap=free -Wl,--wrap=memalign_alloc -Wl,--wrap=memalign_free
endif
CFLAGS += -O2 -std=c89 -Wall -Wextra
LDLIBS += -lm
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
endif
TARGET = sinc_hq_test
SOURCES = sinc_hq_test.c $(COMMON)/audio/resampler/drivers/sinc_resampler.c \
 $(COMMON)/audio/resampler/drivers/sinc_resampler_int16.c $(COMMON)/memmap/memalign.c
all: $(TARGET)
$(TARGET): $(SOURCES) $(COMMON)/include/audio/sinc_resampler.h \
 $(COMMON)/include/audio/sinc_resampler_int16.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean

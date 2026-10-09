CC = gcc
ROOT = ../../..
COMMON = $(ROOT)/libretro-common
CPPFLAGS += -I$(COMMON)/include
# GNU ld wrappers catch allocation on the processing path.
ifneq ($(ALLOC_GUARD),0)
CPPFLAGS += -DSINC_TRACK_ALLOCATIONS
LDFLAGS += -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc -Wl,--wrap=free
endif
CFLAGS += -O2 -std=c89 -Wall -Wextra
LDLIBS += -lm
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
endif
TARGET = sinc_fastforward_test
RUN ?= ./$(TARGET)
SOURCES = sinc_fastforward_test.c sinc_fastforward_i16.c $(COMMON)/memmap/memalign.c
# The armeabi assembly kernels in place of the NEON intrinsics.
ifeq ($(NEON_ASM),1)
CPPFLAGS += -DHAVE_ARM_NEON_ASM_OPTIMIZATIONS
SOURCES += $(COMMON)/audio/resampler/drivers/sinc_resampler_neon.S
endif
all: $(TARGET)
$(TARGET): $(SOURCES) sinc_fastforward.h \
 $(COMMON)/audio/resampler/drivers/sinc_resampler.c \
 $(COMMON)/audio/resampler/drivers/sinc_resampler_int16.c \
 $(COMMON)/audio/resampler/drivers/sinc_resampler_internal.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@
check: $(TARGET)
	$(RUN)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean

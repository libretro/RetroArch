CC = gcc
ROOT = ../../..
COMMON = $(ROOT)/libretro-common
CPPFLAGS += -I$(COMMON)/include
CFLAGS += -O2 -std=c89 -Wall -Wextra
LDLIBS += -lm
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
endif
TARGET = sinc_dispatch_test
SOURCES = sinc_dispatch_test.c $(COMMON)/memmap/memalign.c
# The armeabi assembly kernels in place of the NEON intrinsics.
ifeq ($(NEON_ASM),1)
CPPFLAGS += -DHAVE_ARM_NEON_ASM_OPTIMIZATIONS
SOURCES += $(COMMON)/audio/resampler/drivers/sinc_resampler_neon.S
endif
all: $(TARGET)
$(TARGET): $(SOURCES) $(COMMON)/audio/resampler/drivers/sinc_resampler.c \
 $(COMMON)/audio/resampler/drivers/sinc_resampler_internal.h \
 $(COMMON)/include/audio/sinc_resampler.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean

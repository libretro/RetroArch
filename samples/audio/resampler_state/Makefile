CC = gcc
ROOT = ../../..
COMMON = $(ROOT)/libretro-common
CPPFLAGS += -I$(COMMON)/include
CFLAGS += -O2 -std=c89 -Wall -Wextra
LDLIBS += -lm
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1
LDFLAGS += -fsanitize=$(SANITIZER)
endif
TARGET = resampler_state_test
RUN ?= ./$(TARGET)
SOURCES = resampler_state_test.c $(COMMON)/audio/resampler/drivers/sinc_resampler.c \
 $(COMMON)/audio/resampler/drivers/nearest_resampler.c $(COMMON)/memmap/memalign.c
ifeq ($(NEON_ASM),1)
CPPFLAGS += -DHAVE_ARM_NEON_ASM_OPTIMIZATIONS
SOURCES += $(COMMON)/audio/resampler/drivers/sinc_resampler_neon.S
endif
all: $(TARGET)
$(TARGET): $(SOURCES) $(COMMON)/include/audio/audio_resampler.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@
check: $(TARGET)
	$(RUN)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean

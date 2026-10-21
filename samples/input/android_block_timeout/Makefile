include ../../sample_objdir.mk

TARGET := android_block_timeout_test

LIBRETRO_COMM_DIR := ../../../libretro-common

# The waits are the driver's own (input/drivers/android_poll_wait.h,
# which takes no NDK types); the looper is reproduced in the test,
# because the driver only builds against the NDK.
SOURCES := android_block_timeout_test.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -std=gnu89 -g1 -O1 -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

android_block_timeout_test.o: ../../../input/drivers/android_poll_wait.h

check: all
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(COMPILE.c) $(OUTPUT_OPTION) $<

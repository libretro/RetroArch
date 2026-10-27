TARGET := android_poll_changes_test

LIBRETRO_COMM_DIR := ../../../libretro-common
DRIVER_DIR        := ../../../input/drivers

# What a poll has changed is the driver's own (android_poll_changes.h,
# which takes no NDK types); the poll's drain is reproduced in the
# harness, because the driver only builds against the NDK.
SOURCES := android_poll_changes_test.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -Wextra -std=c89 -pedantic -g1 -O1 \
          -I$(LIBRETRO_COMM_DIR)/include -I$(DRIVER_DIR)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

android_poll_changes_test.o: $(DRIVER_DIR)/android_poll_changes.h

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

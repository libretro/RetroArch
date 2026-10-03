TARGET := android_kbd_keys_test

LIBRETRO_COMM_DIR := ../../../libretro-common
DRIVER_DIR        := ../../../input/drivers

# The routing decision is the driver's own (android_kbd_route.h, which
# takes no NDK types); the Android side of the pipeline is reproduced
# in the harness, because the driver only builds against the NDK.
SOURCES := android_kbd_keys_test.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -Wextra -std=c89 -pedantic -g -O1 \
          -I$(LIBRETRO_COMM_DIR)/include -I$(DRIVER_DIR)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

android_kbd_keys_test.o: $(DRIVER_DIR)/android_kbd_route.h

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

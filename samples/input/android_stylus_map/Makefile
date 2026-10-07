TARGET := android_stylus_map_test

LIBRETRO_COMM_DIR := ../../../libretro-common
DRIVER_DIR        := ../../../input/drivers

# The mapping decision is the driver's own (android_stylus_map.h, which
# takes no NDK types); the Android side of the pipeline is reproduced
# in the harness, because the driver only builds against the NDK.
SOURCES := android_stylus_map_test.c

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

android_stylus_map_test.o: $(DRIVER_DIR)/android_stylus_map.h

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

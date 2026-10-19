TARGET := device_pool_test

LIBRETRO_COMM_DIR := ../../../libretro-common
INPUT_DIR         := ../../../input

# The device numbers shared between the joypad driver and the sources
# beside it (input_device_pool.h, which takes no driver, settings or
# libretro types); the sources are reproduced in the harness.
SOURCES := device_pool_test.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -Wextra -std=c89 -pedantic -g1 -O1 \
          -I$(LIBRETRO_COMM_DIR)/include -I$(INPUT_DIR)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

device_pool_test.o: $(INPUT_DIR)/input_device_pool.h

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

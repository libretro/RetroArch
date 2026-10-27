TARGET := ignore_devices_test

LIBRETRO_COMM_DIR := ../../../libretro-common
INPUT_DIR         := ../../../input

# SDL_GAMECONTROLLER_IGNORE_DEVICES as the joypad drivers read it
# (common/input_ignore_devices.h).
SOURCES := ignore_devices_test.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -Wextra -std=gnu89 -g1 -O1 \
          -I$(LIBRETRO_COMM_DIR)/include -I$(INPUT_DIR)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

ignore_devices_test.o: $(INPUT_DIR)/common/input_ignore_devices.h

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

TARGET := linux_not_joystick_test

LIBRETRO_COMM_DIR := ../../../libretro-common
COMMON_DIR        := ../../../input/common

# The rule set is the drivers' own (linux_not_joystick.h, which takes no
# libudev or evdev types), so nothing here needs a device or a display.
SOURCES := linux_not_joystick_test.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -Wextra -std=c89 -pedantic -g -O1 \
          -I$(LIBRETRO_COMM_DIR)/include -I$(COMMON_DIR)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

linux_not_joystick_test.o: $(COMMON_DIR)/linux_not_joystick.h

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

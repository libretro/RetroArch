TARGET := led_x11_keyboard_test

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

# The test includes led/drivers/led_x11_keyboard.c itself, against the
# stand-in X11/Xlib.h here: no X server or libX11 needed.
SOURCES := led_x11_keyboard_test.c

CFLAGS  += -Wall -std=gnu89 -g1 -O1 \
           -I. -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(SOURCES) X11/Xlib.h X11/XKBlib.h $(CORE_DIR)/led/drivers/led_x11_keyboard.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe *.o

.PHONY: all check clean

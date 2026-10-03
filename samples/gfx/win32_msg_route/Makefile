TARGET := win32_msg_route_test

LIBRETRO_COMM_DIR := ../../../libretro-common
COMMON_DIR        := ../../../gfx/common

# The routing is the window procedure's own (win32_msg_route.h, which
# takes no Windows types); the three switches it replaced are written
# out in the test, as they stood, to hold it to them.
SOURCES := win32_msg_route_test.c

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

win32_msg_route_test.o: $(COMMON_DIR)/win32_msg_route.h

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

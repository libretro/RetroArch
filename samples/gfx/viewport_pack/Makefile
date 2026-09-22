TARGET := viewport_pack_test

LIBRETRO_COMM_DIR := ../../../libretro-common

# Header-only: the pack layout lives entirely in gfx/video_defines.h,
# so nothing from the tree needs compiling alongside it.
SOURCES := \
	viewport_pack_test.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS +=

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

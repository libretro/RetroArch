# The coordinate array, taken out of gfx/video_driver.c, with each of
# its allocations failed in turn.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

TARGET    := coord_array_test
SOURCES_C := coord_array_test.c
OBJS      := $(SOURCES_C:.c=.o)

CFLAGS += -Wall -Wno-unused-function -std=gnu99 \
          -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)

ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
else
CFLAGS  += -g -O1
endif

all: $(TARGET)

coord_array_driver.h: $(CORE_DIR)/gfx/video_driver.c extract_coord_array.awk
	awk -f extract_coord_array.awk $(CORE_DIR)/gfx/video_driver.c > $@.tmp && mv $@.tmp $@

coord_array_test.o: coord_array_driver.h

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe coord_array_driver.h coord_array_driver.h.tmp

.PHONY: all check clean

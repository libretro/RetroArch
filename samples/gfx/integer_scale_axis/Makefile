# Integer scaling: the viewport the scaler picks, per axis mode.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

TARGET    := integer_scale_axis_test
SOURCES_C := integer_scale_axis_test.c
OBJS      := $(SOURCES_C:.c=.o)

INCDIRS   := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)
CFLAGS   += -Wall $(INCDIRS)

ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
else
CFLAGS  += -g -O1
endif

all: $(TARGET)

# The scaler and its parameter snapshot, taken from the driver.
integer_scale_driver.h: $(CORE_DIR)/gfx/video_driver.c extract_integer.awk
	awk -f extract_integer.awk $(CORE_DIR)/gfx/video_driver.c > $@.tmp && mv $@.tmp $@

integer_scale_axis_test.o: integer_scale_driver.h

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -std=gnu89 -pedantic -D_GNU_SOURCE -Wdeclaration-after-statement -Wno-long-long -c -o $@ $<

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) integer_scale_driver.h integer_scale_driver.h.tmp

.PHONY: all check clean

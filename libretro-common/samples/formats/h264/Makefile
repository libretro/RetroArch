TARGET := rh264_lossless_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rh264_lossless_test.c \
	$(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c \
	$(LIBRETRO_COMM_DIR)/formats/h264/rh264.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -O1 -DHAVE_RMP4 -DHAVE_RH264 -D_GNU_SOURCE \
           -I$(LIBRETRO_COMM_DIR)/include

# The samples workflow passes SANITIZER=address,undefined to every
# sample dir; honour it (see formats/jpeg/Makefile).
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

LDLIBS += -lm

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

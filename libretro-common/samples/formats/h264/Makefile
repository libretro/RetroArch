TARGET := rh264_lossless_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rh264_lossless_test.c \
	$(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c \
	$(LIBRETRO_COMM_DIR)/formats/h264/rh264.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/tpool.c

OBJS := $(SOURCES:.c=.o)

# HAVE_THREADS: the oracle decodes every case on a pool as well, and
# the threaded output has to be the single-thread output to the byte.
CFLAGS  += -Wall -std=gnu99 -g -O1 -DHAVE_RMP4 -DHAVE_RH264 -DHAVE_THREADS -D_GNU_SOURCE \
           -I$(LIBRETRO_COMM_DIR)/include

# The samples workflow passes SANITIZER=address,undefined to every
# sample dir; honour it (see formats/jpeg/Makefile).
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

LDLIBS += -lm -lpthread

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

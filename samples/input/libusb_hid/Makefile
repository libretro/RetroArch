TARGET := libusb_hid_test

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

# The driver against a fake libusb (fake_libusb.c), built on the real
# <libusb.h> for its types: needs libusb-1.0 headers, not the library.
SOURCES := \
	libusb_hid_test.c \
	fake_libusb.c \
	$(RARCH_DIR)/input/drivers_hid/libusb_hid.c \
	$(LIBRETRO_COMM_DIR)/queues/fifo_queue.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -O1 -D_GNU_SOURCE -DHAVE_THREADS -DHAVE_HID \
           -DHAVE_LIBUSB -I$(RARCH_DIR) -I$(LIBRETRO_COMM_DIR)/include -I.
LDFLAGS += -lpthread -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif
ifneq ($(SAN),)
   CFLAGS  := -fsanitize=$(SAN) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SAN) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean

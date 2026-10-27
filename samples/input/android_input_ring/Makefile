TARGET := android_input_ring_test

LIBRETRO_COMM_DIR := ../../../libretro-common
DRIVER_DIR        := ../../../input/drivers

# The input reader's ring is the driver's own (android_input_ring.h,
# which takes no NDK types), over libretro-common's SPSC ring.
SOURCES := android_input_ring_test.c \
           $(LIBRETRO_COMM_DIR)/queues/retro_spsc.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c

CFLAGS += -Wall -Wextra -std=gnu89 -g1 -O1 -DHAVE_THREADS \
          -I$(LIBRETRO_COMM_DIR)/include -I$(DRIVER_DIR)

ifeq ($(findstring mingw,$(CC)),)
LDLIBS += -lpthread
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(SOURCES) $(DRIVER_DIR)/android_input_ring.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS) $(LDLIBS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean

TARGET := android_pad_removed_test

LIBRETRO_COMM_DIR := ../../../libretro-common
DRIVER_DIR        := ../../../input/drivers

# The mailbox and the pad-slot rules are the driver's own
# (android_pad_removed.h, which takes no NDK types); the listener and
# the input thread around them are reproduced in the harness, because
# the driver only builds against the NDK. rthreads.c is the only other
# tree dependency.
SOURCES := \
	android_pad_removed_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include -I$(DRIVER_DIR)
LDFLAGS += -lpthread -lrt

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

android_pad_removed_test.o: $(DRIVER_DIR)/android_pad_removed.h

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

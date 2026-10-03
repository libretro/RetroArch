TARGET := android_lifecycle_flush_test

LIBRETRO_COMM_DIR := ../../../libretro-common

# The command handling is reproduced in the harness (see the file
# header for why); the acknowledgement and its wait are the driver's
# frontend/drivers/android_lifecycle.h, which waits on an eventcount.
SOURCES := android_lifecycle_flush_test.c \
           $(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -std=gnu89 -g -O1 \
          -D_GNU_SOURCE -DHAVE_THREADS \
          -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -pthread

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

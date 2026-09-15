TARGET := retro_procbarrier_test

LIBRETRO_COMM_DIR := ../../..

# The real primitive out of the tree, plus rthreads for the threads and
# condvar the test parks on.
SOURCES := \
	retro_procbarrier_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_procbarrier.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c

CFLAGS  += -Wall -W -std=gnu99 -g -O1 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread -lrt

OBJS := $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

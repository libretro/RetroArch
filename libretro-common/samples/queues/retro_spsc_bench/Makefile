TARGET := retro_spsc_bench

LIBRETRO_COMM_DIR := ../../..

# A benchmark, not a test: not picked up by the sample-discovery CI
# (no test in the name) and built -O2.  Needs two physical cores to
# mean anything.
SOURCES := \
	retro_spsc_bench.c \
	$(LIBRETRO_COMM_DIR)/queues/retro_spsc.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += $(EXTRA_CFLAGS) -Wall -pedantic -std=gnu99 -O2 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread -lrt

# Honour SANITIZER= like the tests do, so the sample CI's build-only
# pass compiles this the same way.  Not meaningful for the numbers.
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

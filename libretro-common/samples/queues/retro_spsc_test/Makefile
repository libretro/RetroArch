TARGET := retro_spsc_test

LIBRETRO_COMM_DIR := ../../..

# retro_spsc.c is the lock-free SPSC byte queue under test; the test
# harness drives a producer and consumer thread through rthreads.c
# (HAVE_THREADS).  retro_atomic.h is header-only and pulled in via
# the queue's transitive includes.
#
# Build under SANITIZER=thread (TSan) for the regression-catching
# CI run; SANITIZER=address,undefined as the default for the
# auto-discovery workflow gives a coarser smoke test.  Both pass on
# correct code.
SOURCES := \
	retro_spsc_test.c \
	$(LIBRETRO_COMM_DIR)/queues/retro_spsc.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -pedantic -std=gnu99 -g -O0 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread
# rthreads.c uses clock_gettime + CLOCK_REALTIME on Linux glibc; on
# older glibc those live in -lrt.  Harmless on newer glibc.
LDFLAGS += -lrt

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

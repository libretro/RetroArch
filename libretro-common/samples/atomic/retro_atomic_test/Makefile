TARGET := retro_atomic_test

LIBRETRO_COMM_DIR := ../../..

# retro_atomic.h is a header-only primitive (no .c counterpart) so the
# test only needs rthreads.c when the SPSC stress check is enabled.
# Build with HAVE_THREADS for real coverage; without HAVE_THREADS the
# header-only checks still run, validating the macros' single-thread
# behaviour on platforms where threading is not available.
HAVE_THREADS ?= 1

SOURCES := retro_atomic_test.c

CFLAGS  += -Wall -pedantic -std=gnu99 -g -O0 -I$(LIBRETRO_COMM_DIR)/include

ifeq ($(HAVE_THREADS),1)
   CFLAGS  += -DHAVE_THREADS
   SOURCES += $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c
   LDFLAGS += -lpthread
   # rthreads.c uses clock_gettime + CLOCK_REALTIME on Linux glibc; on
   # older glibc those live in -lrt.  Harmless on newer glibc.
   LDFLAGS += -lrt
endif

OBJS := $(SOURCES:.c=.o)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: all
	./$(TARGET)
	./check-barriers.sh

# The seqlock's fences cannot be checked by running the test: x86_64
# supplies the ordering they ask for, and qemu-aarch64 does not model
# reordering either, so a build with a fence deleted passes on both.
# The emitted code does change, so check-barriers.sh cross compiles
# and looks for the barriers.  It skips when no aarch64 toolchain is
# installed.
check-barriers:
	./check-barriers.sh

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all check check-barriers clean

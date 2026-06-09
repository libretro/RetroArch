TARGET := gfx_thumbnail_status_atomic_test

# Path back to the repo root from this sample dir.  The test references
# headers from libretro-common/include and (when HAVE_THREADS=1) compiles
# rthreads.c from libretro-common/rthreads.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# This sample mirrors gfx_thumbnail.c's atomic-status synchronisation
# pattern in isolation; the real gfx_thumbnail.c is not linked.  The
# stress test exercises the producer/consumer shape through rthreads
# when HAVE_THREADS is enabled.  Without HAVE_THREADS the test still
# runs the static-assertion and round-trip property checks, which
# validate the type/layout invariants that the C11 stdatomic and C++11
# std::atomic backends rely on.
HAVE_THREADS ?= 1

SOURCES := gfx_thumbnail_status_atomic_test.c

CFLAGS  += -Wall -pedantic -std=gnu99 -g -O0 \
           -I$(LIBRETRO_COMM_DIR)/include

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

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

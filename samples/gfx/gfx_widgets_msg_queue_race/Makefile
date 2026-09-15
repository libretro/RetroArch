TARGET := gfx_widgets_msg_queue_race_test

# Path back to the repo root from this sample dir.  The test references
# headers from libretro-common/include and links against
# libretro-common/queues/fifo_queue.c (the FIFO this regression test
# exercises) and libretro-common/rthreads/rthreads.c (when HAVE_THREADS
# is enabled).
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# This sample mirrors the post-fix locking discipline around
# dispgfx_widget_t::msg_queue in gfx/gfx_widgets.c.  The test uses
# fifo_buffer_t directly (compiled from libretro-common) but does NOT
# pull in gfx_widgets.c itself -- the fix is a locking protocol,
# not a code change to the FIFO primitive.
HAVE_THREADS ?= 1

SOURCES := \
	gfx_widgets_msg_queue_race_test.c \
	$(LIBRETRO_COMM_DIR)/queues/fifo_queue.c

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

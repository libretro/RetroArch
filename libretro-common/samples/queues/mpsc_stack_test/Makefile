TARGET := mpsc_stack_test

LIBRETRO_COMM_DIR := ../../..

# mpsc_stack.h is the header-only many-producer single-consumer stack
# under test; the harness drives producer threads against a draining
# consumer through rthreads.c (HAVE_THREADS).
#
# Build under SANITIZER=thread (TSan) for the regression-catching CI
# run; SANITIZER=address,undefined as the default for the
# auto-discovery workflow gives a coarser smoke test.  Both pass on
# correct code.
SOURCES := \
	mpsc_stack_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -pedantic -std=gnu99 -g -O0 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread
LDFLAGS += -lrt

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c $(LIBRETRO_COMM_DIR)/include/queues/mpsc_stack.h
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all clean

include ../../sample_objdir.mk

TARGET := task_queue_park_test

LIBRETRO_COMM_DIR := ../../..

# Built with HAVE_THREADS: the threaded runner is what parks.  GCD=1
# builds the GCD runner instead, against a libdispatch and blocks
# runtime (DISPATCH_PREFIX for one outside the compiler's paths); off
# Apple the QoS class is mapped onto a global queue priority.
SOURCES := \
	task_queue_park_test.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -pedantic -std=gnu99 -g1 -O1 -fno-omit-frame-pointer \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include

LIBS    += -lpthread

# --wrap=slock_lock so the wait lane can widen the gap between two queue
# locks a runner takes in turn; Apple's linker has no --wrap, and there
# the lane runs without the widening.
ifneq ($(shell uname -s),Darwin)
   CFLAGS  += -DPARK_TEST_WRAP_SLOCK
   LDFLAGS += -Wl,--wrap=slock_lock
endif

ifeq ($(GCD),1)
   CFLAGS  += -fblocks -DHAVE_GCD
   LIBS    += -ldispatch -lBlocksRuntime
   ifneq ($(DISPATCH_PREFIX),)
      CFLAGS  += -I$(DISPATCH_PREFIX)/include
      LDFLAGS += -L$(DISPATCH_PREFIX)/lib -Wl,-rpath,$(DISPATCH_PREFIX)/lib
   endif
   ifneq ($(shell uname -s),Darwin)
      CFLAGS += -DQOS_CLASS_USER_INITIATED=DISPATCH_QUEUE_PRIORITY_HIGH
   endif
   # Under SANITIZER=thread against a libdispatch built without it, run
   # with TSAN_OPTIONS=suppressions=tsan-libdispatch.supp: TSan cannot
   # see libdispatch order its own allocations across its threads.
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean

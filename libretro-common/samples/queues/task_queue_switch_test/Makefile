include ../../sample_objdir.mk

TARGET := task_queue_switch_test

LIBRETRO_COMM_DIR := ../../..

# Built with HAVE_THREADS, since the switch under test is from the
# regular implementation to the threaded one. realloc() is wrapped so
# an absurd request is seen, and refused, rather than attempted,
# slock_lock() so that a waker can be held inside the queue, and
# slock_unlock() and sthread_create() so that the main thread can be
# held where a new runner first runs a task, and sthread_join() and
# sthread_get_current_thread_id() so that a thread can take the
# worker's id while deinit joins it.
SOURCES := \
	task_queue_switch_test.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -pedantic -std=gnu99 -g1 -O1 -fno-omit-frame-pointer \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include
LIBS    += -lpthread

# Apple's linker has no --wrap: there task_queue.c calls the wrappers by
# name, and the test's __real_ calls name the real functions.
ifneq ($(shell uname -s),Darwin)
   LDFLAGS += -Wl,--wrap=realloc -Wl,--wrap=slock_lock \
              -Wl,--wrap=slock_unlock -Wl,--wrap=sthread_create \
              -Wl,--wrap=sthread_join \
              -Wl,--wrap=sthread_get_current_thread_id
else
   TASK_QUEUE_O := $(call sample_objs,$(LIBRETRO_COMM_DIR)/queues/task_queue.o)
   $(TASK_QUEUE_O): CFLAGS += -Drealloc=__wrap_realloc -Dslock_lock=__wrap_slock_lock \
                              -Dslock_unlock=__wrap_slock_unlock \
                              -Dsthread_create=__wrap_sthread_create \
                              -Dsthread_join=__wrap_sthread_join \
                              -Dsthread_get_current_thread_id=__wrap_sthread_get_current_thread_id
   $(TARGET).o: CFLAGS += -D__real_realloc=realloc -D__real_slock_lock=slock_lock \
                          -D__real_slock_unlock=slock_unlock \
                          -D__real_sthread_create=sthread_create \
                          -D__real_sthread_join=sthread_join \
                          -D__real_sthread_get_current_thread_id=sthread_get_current_thread_id
endif

# GCD=1 builds the GCD runner as the threaded one: against libSystem on
# Apple, elsewhere against a libdispatch and blocks runtime
# (DISPATCH_PREFIX for one outside the compiler's paths), with the QoS
# class mapped onto a global queue priority.
ifeq ($(GCD),1)
   CFLAGS  += -fblocks -DHAVE_GCD
   ifneq ($(DISPATCH_PREFIX),)
      CFLAGS  += -I$(DISPATCH_PREFIX)/include
      LDFLAGS += -L$(DISPATCH_PREFIX)/lib -Wl,-rpath,$(DISPATCH_PREFIX)/lib
   endif
   ifneq ($(shell uname -s),Darwin)
      LIBS   += -ldispatch -lBlocksRuntime
      CFLAGS += -DQOS_CLASS_USER_INITIATED=DISPATCH_QUEUE_PRIORITY_HIGH
   endif
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

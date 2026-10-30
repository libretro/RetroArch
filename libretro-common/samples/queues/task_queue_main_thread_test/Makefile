include ../../sample_objdir.mk

TARGET := task_queue_main_thread_test

LIBRETRO_COMM_DIR := ../../..

# Built with HAVE_THREADS: what is under test is where the threaded
# runner runs a main-thread task's handler.
SOURCES := \
	task_queue_main_thread_test.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

# GCD=standin builds it against the stand-in in ../gcd_standin, a
# libdispatch on rthreads, where there is none.
ifeq ($(GCD),standin)
   SOURCES += ../gcd_standin/dispatch_standin.c
   CFLAGS  += -DHAVE_GCD -I../gcd_standin
endif

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -pedantic -std=gnu99 -g1 -O1 -fno-omit-frame-pointer \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include

LIBS    += -lpthread

# GCD=1 builds the GCD runner instead: against libSystem on Apple,
# elsewhere against a libdispatch (DISPATCH_PREFIX for one outside the
# compiler's paths), with the QoS class mapped onto a global queue
# priority.
ifeq ($(GCD),1)
   CFLAGS  += -DHAVE_GCD
   ifneq ($(DISPATCH_PREFIX),)
      CFLAGS  += -I$(DISPATCH_PREFIX)/include
      LDFLAGS += -L$(DISPATCH_PREFIX)/lib -Wl,-rpath,$(DISPATCH_PREFIX)/lib
   endif
   ifneq ($(shell uname -s),Darwin)
      LIBS   += -ldispatch
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

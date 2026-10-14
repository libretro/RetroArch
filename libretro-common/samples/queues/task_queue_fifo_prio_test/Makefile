include ../../sample_objdir.mk

TARGET := task_queue_fifo_prio_test

LIBRETRO_COMM_DIR := ../../..

# Built with HAVE_THREADS: the threaded runner is the one under test.
# task_queue.c takes its Switch priority code with
# TASK_QUEUE_FORCE_SWITCH_PRIO, against the stand-in headers in hostnx/
# and the calls in fifo_svc_shim.c.  FORCE_PRIO=0 builds it without,
# the worker then level with the main thread.  The lanes need
# CAP_SYS_NICE for SCHED_FIFO and skip without it.
SOURCES := \
	task_queue_fifo_prio_test.c \
	fifo_svc_shim.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

FORCE_PRIO ?= 1

CFLAGS  += -Wall -pedantic -std=gnu99 -g1 -O1 -fno-omit-frame-pointer \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include -Ihostnx
ifeq ($(FORCE_PRIO),1)
   CFLAGS += -DTASK_QUEUE_FORCE_SWITCH_PRIO
endif

LIBS    += -lpthread

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

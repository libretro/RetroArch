TARGET := task_progress_push_test

# Drives the shipping libretro-common/queues/task_queue.c directly.
# The subject is task_queue_push_progress()'s contract with msg_push:
# the message is formatted under the property lock and pushed with it
# released, so the push must not reach for a property the lock guards.
# Build with EXTRA_CFLAGS=-DPROGRESS_PUSH_DEFECT to break that half.

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := task_progress_push_test.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -DHAVE_THREADS $(EXTRA_CFLAGS) \
           -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread -lm

# The samples workflow passes SANITIZER=address,undefined; honour it.
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

.PHONY: clean all

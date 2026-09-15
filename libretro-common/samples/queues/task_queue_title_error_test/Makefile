TARGET := task_queue_title_error_test

LIBRETRO_COMM_DIR := ../../..

# task_queue.c (without HAVE_THREADS / HAVE_GCD) is self-contained
# apart from cpu_features_get_time_usec(); the test file provides a
# trivial stub for that symbol so we don't have to drag in
# features_cpu.c (which would pull in a much larger dependency
# graph).  Build task_queue.c without HAVE_THREADS so the slock /
# scond / sthread paths compile out cleanly -- the title/error
# protocol is identical with and without threading; the locks just
# serialise it.
SOURCES := \
	task_queue_title_error_test.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g -O0 -I$(LIBRETRO_COMM_DIR)/include

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

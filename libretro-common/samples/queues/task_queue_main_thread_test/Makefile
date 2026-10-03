TARGET := task_queue_main_thread_test

LIBRETRO_COMM_DIR := ../../..

# Built with HAVE_THREADS: what is under test is where the threaded
# runner runs a main-thread task's handler.
SOURCES := \
	task_queue_main_thread_test.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -pedantic -std=gnu99 -g -O1 -fno-omit-frame-pointer \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include

LIBS    += -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean

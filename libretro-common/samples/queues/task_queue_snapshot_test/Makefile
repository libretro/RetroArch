TARGET := task_queue_snapshot_test
LIBRETRO_COMM_DIR := ../../..
SOURCES := task_queue_snapshot_test.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c
CFLAGS += -std=gnu89 -Wall -Wextra -g -O1 -DHAVE_THREADS \
          -I$(LIBRETRO_COMM_DIR)/include
LDLIBS += -lpthread

ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): $(SOURCES) $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/include/queues/task_queue.h
	$(CC) $(CFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean

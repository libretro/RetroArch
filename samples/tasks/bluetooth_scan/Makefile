TARGET := bluetooth_scan_test

RARCH_DIR         := ../../..
LIBRETRO_COMM_DIR := $(RARCH_DIR)/libretro-common

# tasks/task_bluetooth.c on the real threaded task queue; the driver
# layer and the message strings are stand-ins in the test. The scan
# window is shortened so the test takes a fraction of a second.
SOURCES := \
	bluetooth_scan_test.c \
	$(RARCH_DIR)/tasks/task_bluetooth.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -O1 -D_GNU_SOURCE -DHAVE_THREADS -DRARCH_INTERNAL \
           -DBLUETOOTH_SCAN_WINDOW_US=300000 \
           -I$(RARCH_DIR) -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif
ifneq ($(SAN),)
   CFLAGS  := -fsanitize=$(SAN) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SAN) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean

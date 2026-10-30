TARGET := task_queue_snapshot_test
LIBRETRO_COMM_DIR := ../../..
SOURCES := task_queue_snapshot_test.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c
CFLAGS += -std=gnu89 -Wall -Wextra -g1 -O1 -DHAVE_THREADS \
          -I$(LIBRETRO_COMM_DIR)/include
LDLIBS += -lpthread

# GCD=standin runs the GCD runner on the stand-in in ../gcd_standin, a
# libdispatch on rthreads; GCD=1 on the system's.
ifeq ($(GCD),standin)
SOURCES += ../gcd_standin/dispatch_standin.c
CFLAGS  += -DHAVE_GCD -I../gcd_standin
endif
ifeq ($(GCD),1)
CFLAGS  += -DHAVE_GCD
ifneq ($(DISPATCH_PREFIX),)
CFLAGS  += -I$(DISPATCH_PREFIX)/include
LDFLAGS += -L$(DISPATCH_PREFIX)/lib -Wl,-rpath,$(DISPATCH_PREFIX)/lib
endif
ifneq ($(shell uname -s),Darwin)
LDLIBS  += -ldispatch
CFLAGS  += -DQOS_CLASS_USER_INITIATED=DISPATCH_QUEUE_PRIORITY_HIGH
endif
endif

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

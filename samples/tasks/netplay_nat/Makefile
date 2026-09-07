TARGET := nat_task_serialisation_test

# The unit under test is compiled from the tree - the shipping
# tasks/task_netplay_nat_traversal.c - so the oracle exercises the
# translation unit that ships, against the real task queue.
#
# The natt_* device layer and netplay_driver_ctl are stubbed by the
# test with a deterministic fake, standing in for the UPnP network
# round trips.  That leaves the request serialisation - the queue and
# its flush from the task callbacks - as the thing actually under
# test, including its dependency on the real queue's ordering
# guarantee that a task's callback runs after the task has been
# popped, so a resumed push finds the queue free.
#
# HAVE_THREADS is defined so the queue builds its threaded
# implementation, but the test initialises it unthreaded and drives
# task_queue_check() explicitly: the lanes assert on how far a task
# has progressed after a known number of ticks, which requires the
# handler to run where the test can count it.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := nat_task_serialisation_test.c \
           $(REPO_ROOT)/tasks/task_netplay_nat_traversal.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/net/net_compat.c \
           $(LIBRETRO_COMM_DIR)/net/net_socket.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c

OBJS := $(SOURCES:.c=.o)

DEFINES := -DHAVE_NETWORKING -DHAVE_NETPLAY -DHAVE_THREADS

CFLAGS  += -Wall -std=gnu99 -g $(DEFINES) \
           -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)
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

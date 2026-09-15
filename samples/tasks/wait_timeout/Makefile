TARGET := wait_timeout_test

# Drives the shipping libretro-common/queues/task_queue.c directly.
# The subject is task_queue_wait_timeout(): that a wait which can
# never be satisfied - a network round trip to a server that does not
# answer - gives up instead of hanging the frontend, and that a wait
# which IS satisfied returns immediately rather than sitting out its
# timeout.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := wait_timeout_test.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -DHAVE_THREADS \
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

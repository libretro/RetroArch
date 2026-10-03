TARGET := output_store_test

LIBRETRO_COMM_DIR := ../../../libretro-common
INPUT_DIR         := ../../../input

# The store is the frontend's own (input_output_store.h, which takes no
# driver, settings or libretro types); the core that posts and the
# frame loop that takes are reproduced in the harness. rthreads.c is
# the only other tree dependency.
SOURCES := \
	output_store_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include -I$(INPUT_DIR)
LDFLAGS += -lpthread -lrt

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

output_store_test.o: $(INPUT_DIR)/input_output_store.h

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

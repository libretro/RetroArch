TARGET := device_registry_test

LIBRETRO_COMM_DIR := ../../../libretro-common
INPUT_DIR         := ../../../input

# The registry is the frontend's own (input_registry.h, which takes no
# driver, settings or libretro types); the drivers that report
# controllers are reproduced in the harness.
SOURCES := device_registry_test.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -Wextra -std=c89 -pedantic -g -O1 \
          -I$(LIBRETRO_COMM_DIR)/include -I$(INPUT_DIR)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

device_registry_test.o: $(INPUT_DIR)/input_registry.h

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

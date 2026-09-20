TARGET := android_haptic_defer_test

LIBRETRO_COMM_DIR := ../../../libretro-common

# Self-contained: the protocol under test is reproduced in the harness
# (see the file header for why), so boolean.h is the only dependency and
# it is header-only.
SOURCES := android_haptic_defer_test.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -Wextra -std=c89 -pedantic -g -O1 \
          -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

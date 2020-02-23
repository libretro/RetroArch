TARGET := compat_fnmatch_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	compat_fnmatch_test.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_fnmatch.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g -I$(LIBRETRO_COMM_DIR)/include

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

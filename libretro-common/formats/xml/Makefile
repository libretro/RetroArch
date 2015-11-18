TARGET := rxml

LIBRETRO_COMM_DIR := ../..

SOURCES := \
	$(wildcard *.c) \
	$(LIBRETRO_COMM_DIR)/file/retro_file.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -DRXML_TEST -Wall -pedantic -std=gnu99 -g -I$(LIBRETRO_COMM_DIR)/include

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean


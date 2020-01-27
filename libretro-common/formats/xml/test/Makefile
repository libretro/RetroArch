TARGET := rxml

LIBRETRO_XML_DIR  := ..
LIBRETRO_COMM_DIR := ../../..
LIBRETRO_DEPS_DIR := ../../../../deps

SOURCES := \
	rxml_test.c \
	$(LIBRETRO_XML_DIR)/rxml.c \
	$(LIBRETRO_DEPS_DIR)/yxml/yxml.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c

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

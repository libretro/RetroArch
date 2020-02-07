TARGET := rxml

LIBRETRO_XML_DIR  := ../../../formats/xml
LIBRETRO_COMM_DIR := ../../../
LIBRETRO_DEPS_DIR := ../../../../deps

SOURCES := \
	rxml_test.c \
	$(LIBRETRO_XML_DIR)/rxml.c \
	$(LIBRETRO_DEPS_DIR)/yxml/yxml.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

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

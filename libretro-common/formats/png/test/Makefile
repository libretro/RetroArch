TARGET := rpng

LIBRETRO_PNG_DIR  := ..
LIBRETRO_COMM_DIR := ../../..

HAVE_IMLIB2=1

LDFLAGS +=  -lz

ifeq ($(HAVE_IMLIB2),1)
CFLAGS += -DHAVE_IMLIB2
LDFLAGS += -lImlib2
endif

SOURCES_C := 	\
	rpng_test.c \
	$(LIBRETRO_PNG_DIR)/rpng.c \
	$(LIBRETRO_PNG_DIR)/rpng_encode.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_stdio.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file_zlib.c \
	$(LIBRETRO_COMM_DIR)//file/file_path.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c

OBJS := $(SOURCES_C:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -O0 -g -DHAVE_ZLIB -DHAVE_ZLIB_DEFLATE -DRPNG_TEST -I$(LIBRETRO_COMM_DIR)/include

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean


TARGET := rmsgpack_overflow_test

LIBRETRODB_DIR    := ../..
LIBRETRO_COMM_DIR := ../../../libretro-common

SOURCES := \
	rmsgpack_overflow_test.c \
	$(LIBRETRODB_DIR)/rmsgpack.c \
	$(LIBRETRODB_DIR)/rmsgpack_dom.c \
	$(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -pedantic -std=gnu99 -g \
           -I$(LIBRETRO_COMM_DIR)/include \
           -I$(LIBRETRODB_DIR)
LDFLAGS +=

# Optional ASan/UBSan build: invoke as 'make SANITIZER=address' or
# 'make SANITIZER=address,undefined'.  Matches the convention used
# in samples/tasks/http/Makefile.
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

.PHONY: clean

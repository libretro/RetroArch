TARGET := cdfs_dir_record_test
TARGET_TEST2 := cdfs_pure_api_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := cdfs_dir_record_test.c

# The pure-API test links the real cdfs.c and drives it entirely over
# memory streams, so it needs the intfstream stack (which in turn pulls
# in the file/rzip/deflate backends it can dispatch to at runtime).
TEST2_SOURCES := \
	cdfs_pure_api_test.c \
	$(LIBRETRO_COMM_DIR)/formats/cdfs/cdfs.c \
	$(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/rzip_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file_zlib.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c

OBJS := $(SOURCES:.c=.o)
TEST2_OBJS := $(TEST2_SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g -O0 -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lz

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(TARGET_TEST2)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST2): $(TEST2_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET_TEST2) $(OBJS) $(TEST2_OBJS)

.PHONY: clean

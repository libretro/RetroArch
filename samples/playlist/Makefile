TARGET := playlist_parity_test

CORE_DIR          := ../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

SOURCES_C := \
	$(CORE_DIR)/samples/playlist/playlist_parity_test.c \
	$(CORE_DIR)/playlist.c \
	$(CORE_DIR)/verbosity.c \
	$(LIBRETRO_COMM_DIR)/formats/json/rjson.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/rzip_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_zlib.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file.c \
	$(LIBRETRO_COMM_DIR)/file/archive_file_zlib.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

OBJECTS  := $(SOURCES_C:.c=.o)

DEFINES  := -DHAVE_COMPRESSION -DHAVE_ZLIB -DHAVE_THREADS -DRARCH_INTERNAL
INCDIRS  := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)
CFLAGS   += -Wall -std=gnu99 -g $(DEFINES) $(INCDIRS)
LDFLAGS  += -lz -lm -lpthread

# The samples workflow passes SANITIZER=address,undefined to every
# sample dir; honour it.
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJECTS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJECTS)

.PHONY: clean all

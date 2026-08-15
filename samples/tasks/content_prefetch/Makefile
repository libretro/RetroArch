TARGET := content_prefetch_test

# The unit under test is the shipping tasks/task_content_prefetch.c,
# compiled from the tree against the real task queue and the real
# data_transfer spine.  This is the path a menu content load defers
# to, and delivery is by ownership transfer, so the sanitizer flavors
# matter as much as the assertions.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := content_prefetch_test.c \
           $(REPO_ROOT)/tasks/task_content_prefetch.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/formats/data_transfer.c \
           $(LIBRETRO_COMM_DIR)/memmap/memmap.c \
           $(LIBRETRO_COMM_DIR)/file/archive_file.c \
           $(LIBRETRO_COMM_DIR)/file/archive_file_zlib.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/rzip_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
           $(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
           $(LIBRETRO_COMM_DIR)/streams/trans_stream_zlib.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -DHAVE_THREADS -DHAVE_COMPRESSION \
           -DHAVE_ZLIB -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)
LDFLAGS += -lz -lpthread -lm

# The samples workflow passes SANITIZER=address,undefined; honour it.
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

.PHONY: clean all

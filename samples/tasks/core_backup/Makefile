# Regression tests for the resumable CRC in
# libretro-common/streams/interface_stream.c and the sliced hashing
# loop tasks/task_core_updater.c drives it with.
#
# cpu_features_get_time_usec is wrapped so the tick budget is
# assertable exactly rather than measured against a wall clock; see
# the comment at the top of the test.
#
#   make check
#   make check SANITIZER=address
#   make check SANITIZER=undefined

TARGET  := core_backup_io_test
CORE_DIR := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

SOURCES := \
	core_backup_io_test.c \
	$(CORE_DIR)/tasks/task_core_backup.c \
	$(CORE_DIR)/core_backup.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/lists/dir_list.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(CORE_DIR)/verbosity.c \
	$(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/rzip_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_zlib.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -O1 -fno-omit-frame-pointer \
           -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) -I$(CORE_DIR)/deps \
           -DHAVE_ZLIB -DHAVE_COMPRESSION -DRARCH_INTERNAL -DHAVE_THREADS
LDFLAGS += -Wl,--wrap=cpu_features_get_time_usec \
           -Wl,--wrap=retro_vfs_file_read_impl \
           -Wl,--wrap=retro_vfs_file_write_impl
LIBS    += -lz -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean

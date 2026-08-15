TARGET := pl_manager_budget_test

# The unit under test is the shipping tasks/task_playlist_manager.c,
# compiled from the tree against the real task queue, the real
# playlist reader and the real shared per-frame I/O window
# (tasks/task_nbio_slice.c).
#
# features_cpu.c is deliberately NOT linked: the test supplies
# cpu_features_get_time_usec() itself as a virtual clock advancing a
# fixed step per observation, which is what makes the pacing
# assertions exact - step 0 is a window that never expires, a large
# step is one exhausted on first observation.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := pl_manager_budget_test.c \
           $(REPO_ROOT)/tasks/task_playlist_manager.c \
           $(REPO_ROOT)/tasks/task_nbio_slice.c \
           $(REPO_ROOT)/playlist.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/formats/json/rjson.c \
           $(LIBRETRO_COMM_DIR)/formats/m3u/rm3u.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/file/archive_file.c \
           $(LIBRETRO_COMM_DIR)/file/archive_file_zlib.c \
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
           $(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
           $(REPO_ROOT)/verbosity.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -DHAVE_THREADS -DHAVE_COMPRESSION \
           -DHAVE_ZLIB -DRARCH_INTERNAL \
           -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)
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

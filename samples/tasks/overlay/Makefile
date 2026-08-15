TARGET := overlay_budget_test

# The unit under test is the shipping tasks/task_overlay.c, compiled
# from the tree against the real task queue and the real shared
# per-frame I/O window (tasks/task_nbio_slice.c).
#
# features_cpu.c is deliberately NOT linked: the test supplies
# cpu_features_get_time_usec() itself as a virtual clock advancing a
# fixed step per observation.  That is what makes the pacing
# assertions exact - step 0 is a window that never expires, a large
# step is one exhausted on first observation - instead of dependent
# on how fast the machine running CI happens to be.
#
# RARCH_INTERNAL stays undefined so the loader builds without the
# video driver behind it; image loading is stubbed by the test.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := overlay_budget_test.c \
           $(REPO_ROOT)/tasks/task_overlay.c \
           $(REPO_ROOT)/tasks/task_nbio_slice.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/file/config_file.c \
           $(LIBRETRO_COMM_DIR)/file/config_file_io.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
           $(LIBRETRO_COMM_DIR)/hash/lrc_hash.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -DHAVE_THREADS -DHAVE_OVERLAY \
           -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)
LDFLAGS += -lpthread -lm

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

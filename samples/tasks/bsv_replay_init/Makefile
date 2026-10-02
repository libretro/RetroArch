TARGET := bsv_replay_init_test

# The units under test are compiled from the tree, not copied: the
# shipping tasks/task_movie.c and input/bsv/bsvmovie.c, driven through
# the real task queue, intfstream and VFS.  The core, the settings and
# the runloop's two-line replay protocol are the test's.
#
# HAVE_RZSTD on and HAVE_STATESTREAM off matches the iOS build the bug
# was reported against (pkg/apple/BaseConfig.xcconfig): a checkpoint is
# a RAW-encoded state under Zstandard compression.  HAVE_ZLIB stays off
# so no system zlib is needed.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := bsv_replay_init_test.c \
           $(REPO_ROOT)/tasks/task_movie.c \
           $(REPO_ROOT)/input/bsv/bsvmovie.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_rzstd.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -DHAVE_BSV_MOVIE -DHAVE_RZSTD -DHAVE_THREADS \
           -I$(REPO_ROOT) \
           -I$(LIBRETRO_COMM_DIR)/include

LDFLAGS += -lpthread -lm

# STATESTREAM=1 adds the deduplicating checkpoint encoding, the default
# on desktop builds.
ifeq ($(STATESTREAM),1)
   SOURCES += $(REPO_ROOT)/input/bsv/uint32s_index.c \
              $(REPO_ROOT)/libretro-db/rmsgpack.c \
              $(REPO_ROOT)/libretro-db/rmsgpack_dom.c
   CFLAGS  += -DHAVE_STATESTREAM -I$(REPO_ROOT)/deps
endif

CFLAGS += $(EXTRA_CFLAGS)

OBJS := $(SOURCES:.c=.o)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# allocator_may_return_null lets a too-large allocation fail the way it
# does outside ASan, so the test sees the NULL.
SWEEP_ASAN := ASAN_OPTIONS=detect_leaks=1:allocator_may_return_null=1 \
              UBSAN_OPTIONS=print_stacktrace=1

sweep:
	$(MAKE) clean && $(MAKE) && ./$(TARGET)
	$(MAKE) clean && $(MAKE) SANITIZER=address,undefined && \
	   $(SWEEP_ASAN) ./$(TARGET)
	$(MAKE) clean && $(MAKE) STATESTREAM=1 SANITIZER=address,undefined && \
	   $(SWEEP_ASAN) ./$(TARGET)
	@echo "sweep clean"

clean:
	rm -f $(TARGET) $(OBJS) $(TARGET).replay

.PHONY: all sweep clean

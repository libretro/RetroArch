TARGET := bsv_statestream_test

# The units under test are compiled from the tree, not copied: the
# shipping input/bsv/bsvmovie.c with HAVE_STATESTREAM on, its block
# index (input/bsv/uint32s_index.c) and the rmsgpack reader and writer
# the checkpoints are made of.  tasks/task_movie.c and the rest are
# there because bsvmovie.c links against them.  The core and the
# settings are the test's.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := bsv_statestream_test.c \
           $(REPO_ROOT)/tasks/task_movie.c \
           $(REPO_ROOT)/input/bsv/bsvmovie.c \
           $(REPO_ROOT)/input/bsv/uint32s_index.c \
           $(REPO_ROOT)/libretro-db/rmsgpack.c \
           $(REPO_ROOT)/libretro-db/rmsgpack_dom.c \
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
           -DHAVE_BSV_MOVIE -DHAVE_RZSTD -DHAVE_STATESTREAM -DHAVE_THREADS \
           -I$(REPO_ROOT) -I$(REPO_ROOT)/deps \
           -I$(LIBRETRO_COMM_DIR)/include

LDFLAGS += -lpthread -lm
# The oom lane fails chosen allocations; GNU ld only, as on the CI runner.
LDFLAGS += -Wl,--wrap=malloc -Wl,--wrap=calloc -Wl,--wrap=realloc

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

sweep:
	$(MAKE) clean && $(MAKE) && ./$(TARGET)
	$(MAKE) clean && $(MAKE) SANITIZER=address,undefined && \
	   ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 ./$(TARGET)
	@echo "sweep clean"

clean:
	rm -f $(TARGET) $(OBJS) $(TARGET).replay

.PHONY: all sweep clean

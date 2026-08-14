TARGET := image_decode_slice_test

# Path back to the repo root from this sample dir.  The units under
# test are compiled from the tree rather than copied - the shipping
# tasks/task_image.c and tasks/task_file_transfer.c - so the oracle
# exercises the translation units that ship.  RARCH_INTERNAL stays
# undefined so both build without the frontend behind them.
#
# features/features_cpu.c is deliberately NOT linked: the test
# provides cpu_features_get_time_usec() itself, as a deterministic
# clock that advances a fixed quantum per observation.  That is what
# turns the decode window's time arithmetic into something a test
# can assert exactly.
#
# HAVE_THREADS stays undefined too: the regular (non-threaded)
# gather is the unit under test - the shared window declines the
# threaded queue by design - and without it task_queue.c needs no
# rthreads.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := image_decode_slice_test.c \
           $(REPO_ROOT)/tasks/task_image.c \
           $(REPO_ROOT)/tasks/task_file_transfer.c \
           $(REPO_ROOT)/tasks/task_nbio_slice.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/formats/data_transfer.c \
           $(LIBRETRO_COMM_DIR)/formats/image_texture.c \
           $(LIBRETRO_COMM_DIR)/formats/image_transfer.c \
           $(LIBRETRO_COMM_DIR)/formats/png/rpng.c \
           $(LIBRETRO_COMM_DIR)/formats/png/rpng_apng.c \
           $(LIBRETRO_COMM_DIR)/gfx/scaler/pixconv.c \
           $(LIBRETRO_COMM_DIR)/gfx/scaler/scaler.c \
           $(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_filter.c \
           $(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_int.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/memmap/memmap.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
           $(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g -O0 \
          -DHAVE_RPNG \
          -I$(REPO_ROOT) \
          -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -lm

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

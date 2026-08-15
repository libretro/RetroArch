TARGET := task_file_transfer_test

# Path back to the repo root from this sample dir.  The unit under
# test is compiled from the tree rather than copied, so the oracle
# exercises the shipping translation unit.  RARCH_INTERNAL stays
# undefined so tasks/task_file_transfer.c builds without the frontend
# behind it - that compiles out the taskbar-progress forwarder, which
# is not what this tests.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := task_file_transfer_test.c \
           $(REPO_ROOT)/tasks/task_file_transfer.c \
           $(REPO_ROOT)/tasks/task_nbio_slice.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/formats/data_transfer.c \
           $(LIBRETRO_COMM_DIR)/formats/h264/rh264.c \
           $(LIBRETRO_COMM_DIR)/formats/h265/rh265.c \
           $(LIBRETRO_COMM_DIR)/formats/image/image_hdr_blit.c \
           $(LIBRETRO_COMM_DIR)/formats/image_transfer.c \
           $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c \
           $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4_video.c \
           $(LIBRETRO_COMM_DIR)/formats/vp8/rvp8.c \
           $(LIBRETRO_COMM_DIR)/formats/vp9/rvp9.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/memmap/memmap.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

# HAVE_AUDIOMIXER is defined so the audio arm of the type switch is
# built and the mixer facade can be stubbed: M4A and OPUS were once
# absent from that switch while both push paths set them, so those
# loads finished without the mixer handler ever running.  HAVE_RMP4
# and HAVE_RH264 build the real demuxer for the moov-at-EOF lane.
CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -DHAVE_THREADS -DHAVE_AUDIOMIXER -DHAVE_MMAP \
           -DHAVE_64BIT_OFFSETS -DHAVE_RMP4 -DHAVE_RH264 -D_GNU_SOURCE \
           -I$(REPO_ROOT) \
           -I$(LIBRETRO_COMM_DIR)/include

LDFLAGS += -lpthread -lm

# Extra flags for the caller; CFLAGS= on the command line would replace
# everything set above instead of adding to it.
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

# ASan and TSan are mutually exclusive, hence separate passes.  Each
# pass runs both lanes: the default one and the concurrent-instances
# one, which is where a hidden static in the spine would show up.
sweep:
	$(MAKE) clean && $(MAKE) && ./$(TARGET) && ./$(TARGET) conc
	$(MAKE) clean && $(MAKE) SANITIZER=address,undefined && \
	   ASAN_OPTIONS=detect_leaks=1:detect_stack_use_after_return=1 \
	   UBSAN_OPTIONS=print_stacktrace=1 ./$(TARGET) && \
	   ASAN_OPTIONS=detect_leaks=1 ./$(TARGET) conc
	$(MAKE) clean && $(MAKE) SANITIZER=thread && \
	   TSAN_OPTIONS=halt_on_error=0 ./$(TARGET) && \
	   TSAN_OPTIONS=halt_on_error=0 ./$(TARGET) conc
	@echo "sweep clean"

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all sweep clean

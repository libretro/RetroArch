TARGET := save_state_io_test

# Path back to the repo root from this sample dir.  The unit under
# test is compiled from the tree rather than copied - the shipping
# tasks/task_save.c - so the oracle exercises the translation unit
# that ships.  RARCH_INTERNAL stays undefined so it builds without
# the frontend behind it.
#
# features/features_cpu.c is deliberately NOT linked: the test
# provides cpu_features_get_time_usec() itself, as a deterministic
# clock that advances a fixed step per observation.  That is what
# turns the tick budget into something a test can assert exactly -
# step 0 is a budget that never expires, step >= the budget is one
# that expires on first observation.
#
# HAVE_THREADS is defined so the threaded queue impl is built and the
# "conc" lane can run the handler off the main thread, which is where
# it runs in production.  The default lane still initialises the queue
# unthreaded, so its tick-count assertions are unaffected: the pacing
# protocol is identical either way and the locks only serialise it.
#
# HAVE_COMPRESSION stays undefined, so the plain intfstream path is
# the one under test; the rzip path wraps the same handler loop.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := save_state_io_test.c \
           $(REPO_ROOT)/tasks/task_save.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
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
           -DHAVE_THREADS \
           -I$(REPO_ROOT) \
           -I$(LIBRETRO_COMM_DIR)/include

# --wrap=malloc so the sample's counting allocator sees the allocations
# by tasks/task_save.c, not only its own.  That count is the only way
# to observe the undo snapshot reusing its buffer: the addresses are
# identical either way, because a state-sized block is mmap'd and the
# kernel hands back the same region on the next request.
LDFLAGS += -Wl,--wrap=malloc -lpthread -lm

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

# ASan and TSan are mutually exclusive, hence separate passes.  Leak
# detection is strict: the handler owns the serialized buffer and the
# intfstream, and the terminals this oracle drives (completed, failed
# serialize, failed open, short file) are exactly the ones where an
# early return could drop either.
sweep:
	$(MAKE) clean && $(MAKE) && ./$(TARGET) && ./$(TARGET) conc
	$(MAKE) clean && $(MAKE) SANITIZER=address,undefined && \
	   ASAN_OPTIONS=detect_leaks=1:detect_stack_use_after_return=1 \
	   UBSAN_OPTIONS=print_stacktrace=1 ./$(TARGET) && \
	   ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 \
	   ./$(TARGET) conc
	$(MAKE) clean && $(MAKE) SANITIZER=thread && \
	   TSAN_OPTIONS=halt_on_error=0 ./$(TARGET) && \
	   TSAN_OPTIONS=halt_on_error=0 ./$(TARGET) conc
	@echo "sweep clean"

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all sweep clean

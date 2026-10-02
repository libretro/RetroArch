TARGETS := archive_name_safety_test decompress_window_test

# decompress_window_test compiles the shipping tasks/task_decompress.c
# against the real unthreaded task queue, the real ZIP backend and the
# real shared per-frame I/O window (tasks/task_nbio_slice.c).
# features_cpu.c is deliberately NOT linked: the test supplies
# cpu_features_get_time_usec() as a virtual clock.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

WINDOW_SOURCES := decompress_window_test.c \
           $(REPO_ROOT)/tasks/task_decompress.c \
           $(REPO_ROOT)/tasks/task_nbio_slice.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
           $(LIBRETRO_COMM_DIR)/file/archive_file.c \
           $(LIBRETRO_COMM_DIR)/file/archive_file_zlib.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c

WINDOW_OBJS := $(WINDOW_SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g -O0

WINDOW_CFLAGS := -DHAVE_THREADS -DHAVE_COMPRESSION \
           -DRARCH_INTERNAL \
           -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)
# The fixture archive is written with zlib's deflate either way
WINDOW_LIBS   := -lz

# BUILTIN_DEFLATE=1 builds the ZIP backend against the clean-room
# inflater (encodings/deflate.h) instead of zlib, the decoder builds
# without HAVE_ZLIB ship with; its output-window handling differs.
ifeq ($(BUILTIN_DEFLATE),1)
   WINDOW_CFLAGS  += -DARCHIVE_USE_BUILTIN_DEFLATE
   WINDOW_SOURCES += $(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c
   WINDOW_OBJS    := $(WINDOW_SOURCES:.c=.o)
else
   WINDOW_CFLAGS  += -DHAVE_ZLIB
endif

# The samples workflow passes SANITIZER=address,undefined to every
# sample dir.  Without this block the variable was accepted and
# ignored, so these targets built unsanitized while the workflow
# reported them as checked.
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif


all: $(TARGETS)

archive_name_safety_test.o: archive_name_safety_test.c
	$(CC) -c -o $@ $< $(CFLAGS)

archive_name_safety_test: archive_name_safety_test.o
	$(CC) -o $@ $^ $(LDFLAGS)

$(WINDOW_OBJS): %.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(WINDOW_CFLAGS)

decompress_window_test: $(WINDOW_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -Wl,--wrap=retro_vfs_file_write_impl $(WINDOW_LIBS) -lpthread -lm

check: all
	./archive_name_safety_test
	./decompress_window_test

clean:
	rm -f $(TARGETS) archive_name_safety_test.o $(WINDOW_OBJS) \
	      $(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.o
	rm -rf decompress_window_out decompress_window_fixture.zip

.PHONY: all check clean

include ../../sample_objdir.mk

MANIFEST_TARGET ?= decompress_manifest_test

TARGETS := archive_name_safety_test decompress_window_test $(MANIFEST_TARGET)

# decompress_window_test compiles the shipping tasks/task_decompress.c
# against the real unthreaded task queue, the real ZIP backend and the
# real shared per-frame I/O window (tasks/task_nbio_slice.c).  The
# Zstandard backend is linked for a source whose member has no CRC.
# features_cpu.c is deliberately NOT linked: the test supplies
# cpu_features_get_time_usec() as a virtual clock.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

WINDOW_SOURCES := decompress_window_test.c \
           $(REPO_ROOT)/tasks/task_decompress.c \
           $(REPO_ROOT)/tasks/task_decompress_manifest.c \
           $(REPO_ROOT)/tasks/task_nbio_slice.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
           $(LIBRETRO_COMM_DIR)/file/archive_file.c \
           $(LIBRETRO_COMM_DIR)/file/archive_file_zlib.c \
           $(LIBRETRO_COMM_DIR)/file/archive_file_zstd.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_rzstd.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_xxh64.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c

WINDOW_OBJS := $(call sample_objs,$(WINDOW_SOURCES:.c=.o))

# decompress_manifest_test calls tasks/task_decompress_manifest.c
# directly.  It needs no zlib, no task queue and no threads, so it is
# the one that also builds with mingw-w64 (MANIFEST_TARGET names the
# .exe).  Its objects are built without the window test's defines and
# so carry a suffix of their own.
MANIFEST_SOURCES := decompress_manifest_test.c \
           $(REPO_ROOT)/tasks/task_decompress_manifest.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c

MANIFEST_OBJS := $(call sample_objs,$(MANIFEST_SOURCES:.c=.manifest.o))

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -O0

WINDOW_CFLAGS := -DHAVE_THREADS -DHAVE_COMPRESSION -DHAVE_RZSTD \
           -DRARCH_INTERNAL \
           -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)
MANIFEST_CFLAGS := -DRARCH_INTERNAL -DHAVE_COMPRESSION \
           -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)
# The fixture archive is written with zlib's deflate either way
WINDOW_LIBS   := -lz

# BUILTIN_DEFLATE=1 builds the ZIP backend against the clean-room
# inflater (encodings/deflate.h) instead of zlib, the decoder builds
# without HAVE_ZLIB ship with; its output-window handling differs.
ifeq ($(BUILTIN_DEFLATE),1)
   WINDOW_CFLAGS  += -DARCHIVE_USE_BUILTIN_DEFLATE
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

$(WINDOW_OBJS): %.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS) $(WINDOW_CFLAGS)

decompress_window_test: $(WINDOW_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -Wl,--wrap=retro_vfs_file_write_impl \
	      -Wl,--wrap=retro_vfs_get_mtime_impl \
	      -Wl,--wrap=retro_vfs_file_rename_impl \
	      $(WINDOW_LIBS) -lpthread -lm

$(MANIFEST_OBJS): %.manifest.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS) $(MANIFEST_CFLAGS)

$(MANIFEST_TARGET): $(MANIFEST_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: all
	./archive_name_safety_test
	./decompress_window_test
	./$(MANIFEST_TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGETS) archive_name_safety_test.o $(WINDOW_OBJS) \
	      $(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.o
	rm -f decompress_manifest_test decompress_manifest_test.exe \
	      $(MANIFEST_OBJS)
	rm -rf decompress_window_out decompress_window_fixture.zip
	rm -f decompress_window_fixture.zst
	rm -rf decompress_manifest_out

.PHONY: all check clean

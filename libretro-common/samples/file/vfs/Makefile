TARGET      := vfs_read_overflow_test
TARGET_TEST := vfs_mapped_ptr_test
TARGET_TEST2 := vfs_large_file_test
TARGET_TEST3 := vfs_seek_contract_test
TARGET_TEST4 := vfs_bulk_read_test

LIBRETRO_COMM_DIR := ../../..

COMMON_SOURCES := \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

COMMON_OBJS := $(COMMON_SOURCES:.c=.o)
OBJS        := vfs_read_overflow_test.o vfs_mapped_ptr_test.o \
	vfs_large_file_test.o vfs_seek_contract_test.o \
	vfs_bulk_read_test.o $(COMMON_OBJS)

# -DHAVE_MMAP is required for vfs_read_overflow_test to exercise the
# patched code path; without it the implementation falls back to
# buffered fread and that test becomes a smoke test that can't
# discriminate the overflow case.
#
# vfs_mapped_ptr_test needs BOTH settings to be meaningful, which is
# why this is a variable rather than a fixed flag.  With mappings on,
# filestream_matches_buf() compares against the map and its windowed
# fallback never runs - so the window-boundary cases would be
# asserting on code that was not executed.  Build with MMAP=0 to put
# that path under test; the test reports which one it took.
# Windows has no <sys/mman.h>.  The Win32 file-mapping backend is
# picked up from _WIN32 inside vfs.h and needs no flag here, so
# default this off there rather than leave a plain `make` in an MSYS2
# shell failing on a missing POSIX header.
UNAME_S := $(shell uname -s 2>/dev/null)
ifneq (,$(filter MINGW% MSYS% CYGWIN%,$(UNAME_S)))
   MMAP ?= 0
else
   MMAP ?= 1
endif
ifeq ($(MMAP),1)
   CFLAGS += -DHAVE_MMAP
endif

# vfs_bulk_read_test asserts which I/O path an open lands on, and the
# interesting configuration - descriptor I/O with no file mapping - is
# the one the consoles have and no desktop does.  Build
# 'make MMAP=0 DESCRIPTOR_IO=1' to reproduce it here; MMAP=0 on its
# own gives the other case, a target that is on neither list and must
# keep its buffered behaviour unchanged.
DESCRIPTOR_IO ?=
ifeq ($(DESCRIPTOR_IO),1)
   CFLAGS += -DVFS_HAVE_DESCRIPTOR_IO=1
endif

CFLAGS += -Wall -pedantic -std=gnu99 -g -I$(LIBRETRO_COMM_DIR)/include

# The samples workflow passes SANITIZER=address,undefined to every
# sample dir; without this block the variable was silently ignored
# here, so these two ran unchecked while the rest of the tree did not.
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(TARGET_TEST3) $(TARGET_TEST4)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): vfs_read_overflow_test.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST): vfs_mapped_ptr_test.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# vfs_large_file_test builds sparse fixtures of 3 GiB and 5 GiB.  They
# cost no space on a filesystem with sparse files and are removed
# afterwards; where the filesystem cannot hold them the test skips.
$(TARGET_TEST2): vfs_large_file_test.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# vfs_seek_contract_test runs its contract checks on a 4 KiB fixture
# first, so a machine that cannot hold the 4 GiB one still tests the
# properties instead of skipping the run.
$(TARGET_TEST3): vfs_seek_contract_test.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# vfs_bulk_read_test covers filestream_read_file() and the descriptor
# path RETRO_VFS_FILE_ACCESS_HINT_SEQUENTIAL_BULK selects.  It spawns
# threads to check that whole-file reads hold no shared state, so it
# is the one target here that needs -lpthread; that also makes it the
# one worth running under SANITIZER=thread.
$(TARGET_TEST4): vfs_bulk_read_test.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -lpthread

clean:
	rm -f $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(TARGET_TEST3) \
		$(TARGET_TEST4) $(OBJS)
	rm -f large_3gib.bin large_5gib.bin seek_small.bin seek_4gib.bin
	rm -f bulk_select.bin bulk_select_w.bin bulk_roundtrip.bin \
		bulk_short.bin bulk_shrank.bin bulk_error.bin \
		bulk_binary.bin bulk_threads.bin

.PHONY: clean

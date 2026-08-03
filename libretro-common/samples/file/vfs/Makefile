TARGET      := vfs_read_overflow_test
TARGET_TEST := vfs_mapped_ptr_test

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
OBJS        := vfs_read_overflow_test.o vfs_mapped_ptr_test.o $(COMMON_OBJS)

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
MMAP ?= 1
ifeq ($(MMAP),1)
   CFLAGS += -DHAVE_MMAP
endif

CFLAGS += -Wall -pedantic -std=gnu99 -g -I$(LIBRETRO_COMM_DIR)/include

# The samples workflow passes SANITIZER=address,undefined to every
# sample dir; without this block the variable was silently ignored
# here, so these two ran unchecked while the rest of the tree did not.
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(TARGET_TEST)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): vfs_read_overflow_test.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST): vfs_mapped_ptr_test.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET_TEST) $(OBJS)

.PHONY: clean

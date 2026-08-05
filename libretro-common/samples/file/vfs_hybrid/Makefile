TARGET := vfs_hybrid_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	vfs_hybrid_test.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_hybrid.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

OBJS := $(SOURCES:.c=.o)

# -DHAVE_MMAP so the local branch really produces a mapping; without
# it the zero-copy check has nothing to assert and only reports a skip.
CFLAGS += -Wall -pedantic -std=gnu99 -g -DHAVE_MMAP \
	-I$(LIBRETRO_COMM_DIR)/include

# The wrapper-allocation failure cases exist because that path is
# otherwise unreachable: calloc does not fail on demand, so the branch
# that has to release the already-open backend handle would never be
# executed by a test. --wrap lets the test fail one allocation at a
# chosen moment. Kept out of the sources themselves - nothing in
# vfs_hybrid.c knows this is happening.
LDFLAGS += -Wl,--wrap=calloc

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

.PHONY: clean

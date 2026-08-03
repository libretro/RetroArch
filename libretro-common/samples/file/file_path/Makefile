TARGETS := path_resolve_realpath_test fill_pathname_test

LIBRETRO_COMM_DIR := ../../..

COMMON_SOURCES := \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

COMMON_OBJS := $(COMMON_SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g -DRARCH_INTERNAL -I$(LIBRETRO_COMM_DIR)/include

# The samples workflow passes SANITIZER=address,undefined to every
# sample dir.  Without this block the variable was accepted and
# ignored, so these targets built unsanitized while the workflow
# reported them as checked.
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif


all: $(TARGETS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGETS): %: %.o $(COMMON_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGETS) $(addsuffix .o,$(TARGETS)) $(COMMON_OBJS)

.PHONY: all clean

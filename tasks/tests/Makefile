TARGET   := task_file_transfer_test

RA_DIR            := ../..
LIBRETRO_COMM_DIR := $(RA_DIR)/libretro-common

# The unit under test is compiled from the tree, not copied: the point
# of the oracle is that it exercises the shipping translation unit.
# RARCH_INTERNAL and HAVE_AUDIOMIXER stay undefined so the file builds
# without the frontend behind it - the taskbar-progress forwarder and
# the mixer branch are both compiled out, and neither is what this
# tests.
SOURCES := \
	task_file_transfer_test.c \
	$(RA_DIR)/tasks/task_file_transfer.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/formats/data_transfer.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/memmap/memmap.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -std=gnu99 -g -O1 -DHAVE_THREADS -DHAVE_MMAP \
	-DHAVE_64BIT_OFFSETS -D_GNU_SOURCE \
	-I$(RA_DIR) -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lpthread -lm

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

.PHONY: all clean

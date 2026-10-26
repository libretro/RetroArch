include ../../sample_objdir.mk

TARGET         := chd_meta_overflow_test
TARGET_TEST    := chd_stream_threads_test$(EXE)
TARGET_TEST2   := chd_stream_ahead_test$(EXE)
.DEFAULT_GOAL  := all

LIBRETRO_COMM_DIR := ../../..

SOURCES := chd_meta_overflow_test.c

OBJS := $(SOURCES:.c=.o)

# chd_stream.c over rchd, opened and read on two threads at once, and
# decoding ahead; rchd with the codecs Makefile.common builds it with.
THREADS_SOURCES := \
	$(LIBRETRO_COMM_DIR)/streams/chd_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_prefetch.c \
	$(LIBRETRO_COMM_DIR)/formats/chd/rchd.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_huffman.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
	$(LIBRETRO_COMM_DIR)/formats/7z/r7z_lzma.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

THREADS_OBJS := $(call sample_objs,$(THREADS_SOURCES:.c=.o))

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -O0

THREADS_CFLAGS := -I$(LIBRETRO_COMM_DIR)/include -DHAVE_THREADS \
	-DHAVE_CHD -DHAVE_RCHD -DHAVE_RCHD_DEFLATE -DHAVE_RCHD_LZMA

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

ifeq ($(OS),Windows_NT)
   THREADS_LDLIBS += -lwinmm
else
   THREADS_LDLIBS += -lpthread
endif

all: $(TARGET) $(TARGET_TEST) $(TARGET_TEST2)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS) $(THREADS_CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST): $(SAMPLE_OBJDIR)/chd_stream_threads_test.o $(THREADS_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(THREADS_LDLIBS)

$(TARGET_TEST2): $(SAMPLE_OBJDIR)/chd_stream_ahead_test.o $(THREADS_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(THREADS_LDLIBS)

check: $(TARGET) $(TARGET_TEST) $(TARGET_TEST2)
	./$(TARGET)
	./$(TARGET_TEST)
	./$(TARGET_TEST2)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(OBJS)

.PHONY: all check clean

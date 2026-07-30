TARGET   := gfx_thumbnail_anim_test

RA_DIR            := ../..
LIBRETRO_COMM_DIR := $(RA_DIR)/libretro-common

# The unit under test is the tree's gfx_thumbnail.c.  It is compiled
# -O0 -fno-inline and its static gfx_thumbnail_anim_open is globalized
# with objcopy, so the test can drive the animation open directly -
# upstream of the task queue, downstream of the request plumbing the
# first regression lived in.
UUT_SRC := $(RA_DIR)/gfx/gfx_thumbnail.c
UUT_OBJ := gfx_thumbnail_uut.o

SOURCES := \
	gfx_thumbnail_anim_test.c \
	gfx_thumbnail_stubs.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/formats/data_transfer.c \
	$(LIBRETRO_COMM_DIR)/formats/image_texture.c \
	$(LIBRETRO_COMM_DIR)/formats/image_transfer.c \
	$(LIBRETRO_COMM_DIR)/formats/webp/rwebp.c \
	$(LIBRETRO_COMM_DIR)/formats/vp8/rvp8.c \
	$(LIBRETRO_COMM_DIR)/lists/file_list.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/memmap/memmap.c \
	$(LIBRETRO_COMM_DIR)/memory/mem_stats.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

OBJS := $(SOURCES:.c=.o)

DEFS    := -DHAVE_RWEBP -DHAVE_THREADS -DHAVE_MMAP -D_GNU_SOURCE
CFLAGS  += -Wall -std=gnu99 -g -O1 $(DEFS) \
	-I$(RA_DIR) -I$(LIBRETRO_COMM_DIR)/include
# The UUT additionally needs the frontend defines it ships under.
UUT_CFLAGS := $(CFLAGS) -O0 -fno-inline -DHAVE_MENU -DRARCH_INTERNAL \
	-I$(RA_DIR)/deps -I$(RA_DIR)/deps/stb
LDFLAGS += -lpthread -lm

ifneq ($(SANITIZER),)
   CFLAGS     := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   UUT_CFLAGS := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(UUT_CFLAGS)
   LDFLAGS    := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(UUT_OBJ): $(UUT_SRC)
	$(CC) -c -o $@.tmp.o $< $(UUT_CFLAGS)
	objcopy --globalize-symbol=gfx_thumbnail_anim_open $@.tmp.o $@
	rm -f $@.tmp.o

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS) $(UUT_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS) $(UUT_OBJ)

.PHONY: all clean

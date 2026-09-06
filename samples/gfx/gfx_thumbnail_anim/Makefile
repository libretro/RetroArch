TARGET := gfx_thumbnail_anim_test

# Path back to the repo root from this sample dir.  Links the real
# gfx/gfx_thumbnail.c; the frontend surface it reaches for (video
# driver, config, menu, playlist) is stubbed in stubs_retroarch.c,
# where the texture-load stub doubles as the oracle.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# The unit under test is compiled separately: it needs the frontend
# defines it ships under, and its gfx_thumbnail_anim_open is static,
# so -O0 -fno-inline keeps the symbol and objcopy globalizes it.  That
# lets the test drive the animation open directly - upstream of the
# task queue, downstream of the request plumbing.
UUT_SRC := $(REPO_ROOT)/gfx/gfx_thumbnail.c
ANIM_PREVIEW_SRC := $(REPO_ROOT)/gfx/gfx_anim_preview.c
UUT_OBJ := gfx_thumbnail_uut.o

SOURCES := gfx_thumbnail_anim_test.c \
           stubs_retroarch.c \
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
           $(LIBRETRO_COMM_DIR)/formats/vp8/rvp8.c \
           $(LIBRETRO_COMM_DIR)/formats/webp/rwebp.c \
           $(LIBRETRO_COMM_DIR)/lists/file_list.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/memmap/memmap.c \
           $(LIBRETRO_COMM_DIR)/memory/mem_stats.c \
           $(LIBRETRO_COMM_DIR)/queues/task_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -DHAVE_RWEBP -DHAVE_THREADS -DHAVE_MMAP -D_GNU_SOURCE \
           -I$(REPO_ROOT) \
           -I$(LIBRETRO_COMM_DIR)/include

UUT_CFLAGS := $(CFLAGS) -O0 -fno-inline -DHAVE_MENU -DRARCH_INTERNAL \
              -I$(REPO_ROOT)/deps -I$(REPO_ROOT)/deps/stb

LDFLAGS += -lpthread -lm

# Extra flags for the caller; CFLAGS= on the command line would replace
# everything set above instead of adding to it.
CFLAGS     += $(EXTRA_CFLAGS)
UUT_CFLAGS += $(EXTRA_CFLAGS)

OBJS := $(SOURCES:.c=.o)

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

# The preview session gfx_thumbnail now drives its window feeder and
# audio through; built with the UUT's flags (same stubs surface).
ANIM_PREVIEW_OBJ := gfx_anim_preview_uut.o
$(ANIM_PREVIEW_OBJ): $(ANIM_PREVIEW_SRC)
	$(CC) -c -o $@ $< $(UUT_CFLAGS)

$(TARGET): $(OBJS) $(UUT_OBJ) $(ANIM_PREVIEW_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

# ASan and TSan are mutually exclusive, hence separate passes.
sweep:
	$(MAKE) clean && $(MAKE) && ./$(TARGET)
	$(MAKE) clean && $(MAKE) SANITIZER=address,undefined && \
	   ASAN_OPTIONS=detect_leaks=1:detect_stack_use_after_return=1 \
	   UBSAN_OPTIONS=print_stacktrace=1 ./$(TARGET)
	$(MAKE) clean && $(MAKE) SANITIZER=thread && \
	   TSAN_OPTIONS=halt_on_error=0 ./$(TARGET)
	@echo "sweep clean"

clean:
	rm -f $(TARGET) $(OBJS) $(UUT_OBJ) $(ANIM_PREVIEW_OBJ)

.PHONY: all sweep clean

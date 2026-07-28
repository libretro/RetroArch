TARGET := gfx_thumbnail_sanitize_test

# Path back to the repo root from this sample dir.  Like the
# display_servers and vulkan_validation samples, this one links the
# real code rather than mirroring it: the point is to put
# gfx/gfx_thumbnail.c itself under the sanitizers.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# gfx_thumbnail.c reaches for 97 symbols outside itself.  Eleven of
# them are real libretro-common sources and are linked; the rest are
# in stubs_retroarch.c.
SOURCES := gfx_thumbnail_sanitize_test.c \
           stubs_retroarch.c \
           $(REPO_ROOT)/gfx/gfx_thumbnail.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c

CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -DHAVE_THREADS -DHAVE_MENU -DHAVE_GFX_WIDGETS \
           -I$(REPO_ROOT) \
           -I$(LIBRETRO_COMM_DIR)/include

LDFLAGS += -lpthread

# Extra flags for the caller; passing these through CFLAGS= instead
# would replace everything set above, -DHAVE_THREADS included, and the
# threaded section would silently not be built.
CFLAGS += $(EXTRA_CFLAGS)

OBJS := $(SOURCES:.c=.o)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# The four ways this is meant to be run.  ASan and TSan are mutually
# exclusive, hence two passes.
sweep:
	$(MAKE) clean && $(MAKE) && ./$(TARGET)
	$(MAKE) clean && $(MAKE) SANITIZER=address,undefined && \
	   ASAN_OPTIONS=detect_leaks=1:detect_stack_use_after_return=1 \
	   UBSAN_OPTIONS=print_stacktrace=1 ./$(TARGET)
	$(MAKE) clean && $(MAKE) SANITIZER=thread && \
	   TSAN_OPTIONS=halt_on_error=0 ./$(TARGET)
	@echo "sweep clean"

clean:
	rm -f $(TARGET) $(OBJS) gfx_thumbnail_sanitize_probe.png

.PHONY: all sweep clean

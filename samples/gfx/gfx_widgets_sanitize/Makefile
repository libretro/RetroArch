TARGET := gfx_widgets_sanitize_test

# Path back to the repo root from this sample dir.  Links the real
# gfx/gfx_widgets.c; the point is to put it under the sanitizers, so a
# mirror would prove nothing.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# gfx_widgets.c reaches for 59 symbols outside itself.  Eight resolve
# to real libretro-common sources and are linked; the rest are in
# stubs_retroarch.c, including dummies for the ten gfx_widget_t
# instances that live in gfx/widgets/.
SOURCES := gfx_widgets_sanitize_test.c \
           stubs_retroarch.c \
           $(REPO_ROOT)/gfx/gfx_widgets.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/queues/fifo_queue.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c

CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -DHAVE_THREADS -DHAVE_MENU -DHAVE_GFX_WIDGETS \
           -I$(REPO_ROOT) \
           -I$(LIBRETRO_COMM_DIR)/include

LDFLAGS += -lpthread -lm

# Extra flags for the caller; CFLAGS= on the command line would replace
# everything set above, -DHAVE_GFX_WIDGETS included.
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
	rm -f $(TARGET) $(OBJS)

.PHONY: all sweep clean

TARGET := gfx_animation_sanitize_test

# Path back to the repo root from this sample dir.  Links the real
# gfx/gfx_animation.c, which reaches for only sixteen symbols outside
# itself -- fourteen from libc and libretro-common, two stubbed here.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := gfx_animation_sanitize_test.c \
           stubs_retroarch.c \
           $(REPO_ROOT)/gfx/gfx_animation.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c

CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -I$(REPO_ROOT) \
           -I$(LIBRETRO_COMM_DIR)/include

LDFLAGS += -lm

# Extra flags for the caller; CFLAGS= on the command line would replace
# everything set above instead of adding to it.
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

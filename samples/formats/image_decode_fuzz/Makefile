TARGET := image_decode_fuzz_test

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# Both decoders are self-contained; nothing else is needed.
SOURCES := image_decode_fuzz_test.c \
           $(LIBRETRO_COMM_DIR)/formats/bmp/rbmp.c \
           $(LIBRETRO_COMM_DIR)/formats/tga/rtga.c \
           $(LIBRETRO_COMM_DIR)/formats/dds/rdds.c \
           $(LIBRETRO_COMM_DIR)/formats/webp/rwebp.c \
           $(LIBRETRO_COMM_DIR)/formats/vp8/rvp8.c

CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -I$(LIBRETRO_COMM_DIR)/include

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

sweep:
	$(MAKE) clean && $(MAKE) SANITIZER=address,undefined && \
	   ASAN_OPTIONS=detect_leaks=1 UBSAN_OPTIONS=print_stacktrace=1 \
	   ./$(TARGET)
	@echo "sweep clean"

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all sweep clean

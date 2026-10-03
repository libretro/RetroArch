# The ffmpeg camera driver against lavfi: a synthetic camera, so the
# whole pipeline runs with no device attached.

TARGET := ffmpeg_live_test

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

SOURCES_C := \
	ffmpeg_live_test.c \
	stubs_retroarch.c \
	$(CORE_DIR)/camera/drivers/ffmpeg.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/memmap/memalign.c

OBJS := $(SOURCES_C:.c=.o)

FF_CFLAGS := $(shell pkg-config --cflags libavcodec libavformat libavdevice libavutil libswscale 2>/dev/null)
FF_LIBS   := $(shell pkg-config --libs   libavcodec libavformat libavdevice libavutil libswscale 2>/dev/null)

INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)
CFLAGS  += -Wall $(INCDIRS) $(FF_CFLAGS) -std=gnu99 -D_GNU_SOURCE -DHAVE_THREADS -DHAVE_FFMPEG
# The synthetic camera: libavfilter as the input format, so the device
# is a filter graph rather than a webcam.
CFLAGS  += -DFFMPEG_CAMERA_DEFAULT_BACKEND='"lavfi"'
LDFLAGS += $(FF_LIBS) -lm -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
else
   CFLAGS  += -g -O1
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

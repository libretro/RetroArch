# The ffmpeg camera driver against lavfi: a synthetic camera, so the
# whole pipeline runs with no device attached.

include ../../sample_objdir.mk

TARGET := ffmpeg_live_test

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

SOURCES_C := \
	ffmpeg_live_test.c \
	stubs_retroarch.c \
	$(CORE_DIR)/camera/drivers/ffmpeg.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/memmap/memalign.c

OBJS := $(call sample_objs,$(SOURCES_C:.c=.o))

FF_CFLAGS := $(shell pkg-config --cflags libavcodec libavformat libavdevice libavutil libswscale 2>/dev/null)
FF_LIBS   := $(shell pkg-config --libs   libavcodec libavformat libavdevice libavutil libswscale 2>/dev/null)

INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)
CFLAGS  += -Wall $(INCDIRS) $(FF_CFLAGS) -std=gnu99 -D_GNU_SOURCE -DHAVE_THREADS -DHAVE_FFMPEG
# The synthetic camera: libavfilter as the input format, so the device
# is a filter graph rather than a webcam.
CFLAGS  += -DFFMPEG_CAMERA_DEFAULT_BACKEND='"lavfi"'
LDFLAGS += $(FF_LIBS) -lm -lpthread
LDFLAGS += -Wl,--wrap=av_image_copy_to_buffer
LDFLAGS += -Wl,--wrap=av_read_frame
LDFLAGS += -Wl,--wrap=avcodec_send_packet
LDFLAGS += -Wl,--wrap=avformat_open_input
LDFLAGS += -Wl,--wrap=avcodec_receive_frame
LDFLAGS += -Wl,--wrap=avformat_find_stream_info

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1 $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
else
   CFLAGS  += -g1 -O1
endif

all: $(TARGET) decoder_delay.mkv decoder_resize.h264

decoder_resize.h264:
	ffmpeg -hide_banner -loglevel error -f lavfi -i "color=c=black:size=32x16:rate=25,format=gray,geq=lum=32" -frames:v 3 -c:v libx264 -crf 1 -pix_fmt yuv420p -f h264 -y resize-small.h264
	ffmpeg -hide_banner -loglevel error -f lavfi -i "color=c=black:size=64x32:rate=25,format=gray,geq=lum=112" -frames:v 3 -c:v libx264 -crf 1 -pix_fmt yuv444p -f h264 -y resize-large.h264
	cat resize-small.h264 resize-large.h264 > $@
	rm -f resize-small.h264 resize-large.h264

decoder_delay.mkv:
	ffmpeg -hide_banner -loglevel error -f lavfi -i "color=c=black:size=32x16:rate=25,format=gray,geq=lum=N*16" -frames:v 8 -c:v libx264 -crf 1 -pix_fmt yuv420p -x264-params "bframes=2:b-adapt=0:keyint=30:min-keyint=30:scenecut=0" -y $@

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

check: all
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET)
	rm -f decoder_delay.mkv
	rm -f decoder_resize.h264 resize-small.h264 resize-large.h264

.PHONY: all check clean

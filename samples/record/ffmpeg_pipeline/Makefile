.DEFAULT_GOAL := all
# record_ffmpeg end to end: frames and audio pushed from this thread,
# taken by the driver's encoder thread, decoded back and checked.

include ../../sample_objdir.mk

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

TARGET    := ffmpeg_pipeline_test

SOURCES_C := \
	ffmpeg_pipeline_test.c \
	record_finalize_test.c \
	record_audio_growth_test.c \
	record_video_stride_test.c \
	stubs_retroarch.c \
	$(CORE_DIR)/record/drivers/record_ffmpeg.c \
	$(LIBRETRO_COMM_DIR)/queues/retro_spsc.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/file/config_file.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_int.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_filter.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/pixconv.c \
	$(LIBRETRO_COMM_DIR)/audio/conversion/float_to_s16.c \
	$(LIBRETRO_COMM_DIR)/audio/conversion/s16_to_float.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/audio_resampler.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/audio_resampler_int16.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/sinc_resampler.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/sinc_resampler_int16.c \
	$(LIBRETRO_COMM_DIR)/file/config_file_io.c \
	$(LIBRETRO_COMM_DIR)/file/config_file_userdata.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/audio/resampler/drivers/nearest_resampler.c \
	$(LIBRETRO_COMM_DIR)/memmap/memalign.c

OBJS      := $(call sample_objs,$(SOURCES_C:.c=.o))

FF_CFLAGS := $(shell pkg-config --cflags libavcodec libavformat libavutil libswscale libswresample 2>/dev/null)
FF_LIBS   := $(shell pkg-config --libs   libavcodec libavformat libavutil libswscale libswresample 2>/dev/null)

INCDIRS   := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) -I$(CORE_DIR)/deps
CFLAGS   += -Wall $(INCDIRS) $(FF_CFLAGS)
CFLAGS   += -DRARCH_INTERNAL -DHAVE_FFMPEG -DHAVE_THREADS -D_GNU_SOURCE -D_FILE_OFFSET_BITS=64
LDFLAGS  += $(FF_LIBS) -lm -lpthread \
   -Wl,--wrap=av_write_trailer -Wl,--wrap=avio_closep \
   -Wl,--wrap=avcodec_send_frame -Wl,--wrap=avcodec_receive_packet \
   -Wl,--wrap=av_interleaved_write_frame \
   -Wl,--wrap=av_malloc -Wl,--wrap=sthread_join -Wl,--wrap=av_realloc

# PLAIN_RING=1 builds the lane where the video ring cannot be mirrored.
ifneq ($(PLAIN_RING),)
CFLAGS  += -DPLAIN_RING
LDFLAGS += -Wl,--wrap=retro_spsc_init_mirrored
endif

ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1
LDFLAGS += -fsanitize=$(SANITIZER)
else
CFLAGS  += -g1 -O1
endif

record_finalize_fragment.c: $(CORE_DIR)/record/record_driver.c
	sed -n '/^bool recording_deinit(void)/,/^void streaming_set_state/p' $< | sed '$$d' > $@

record_finalize_test.o: record_finalize_fragment.c
record_audio_growth_test.o: $(CORE_DIR)/record/drivers/record_ffmpeg.c
record_video_stride_test.o: $(CORE_DIR)/record/drivers/record_ffmpeg.c

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -std=gnu99 -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -std=gnu99 -c -o $@ $<

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET).exe $(OBJS) ffmpeg_pipeline_test.mkv record_finalize_fragment.c

.PHONY: clean

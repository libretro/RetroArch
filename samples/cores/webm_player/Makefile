include ../../sample_objdir.mk

ROOT := ../../..
COMMON := $(ROOT)/libretro-common
TARGET := webm_player_test$(EXE)
AUDIO ?= 1
MP4 ?= 1
THREADS ?= 1

SOURCES := webm_player_test.c \
 $(ROOT)/cores/libretro-media/media_core.c \
 $(COMMON)/compat/compat_strl.c \
 $(COMMON)/compat/compat_strcasestr.c \
 $(COMMON)/compat/fopen_utf8.c \
 $(COMMON)/encodings/encoding_utf.c \
 $(COMMON)/encodings/encoding_crc32.c \
 $(COMMON)/features/features_cpu.c \
 $(COMMON)/file/file_path.c \
 $(COMMON)/file/file_path_io.c \
 $(COMMON)/formats/data_transfer.c \
 $(COMMON)/formats/h264/rh264.c $(COMMON)/formats/h265/rh265.c \
 $(COMMON)/formats/audio_transfer.c \
 $(COMMON)/formats/image/image_yuv_blit.c \
 $(COMMON)/formats/image/image_hdr_blit.c \
 $(COMMON)/formats/image/image_blit_bands.c \
 $(COMMON)/formats/vp8/rvp8.c \
 $(COMMON)/formats/vp9/rvp9.c \
 $(COMMON)/formats/webm/rwebm.c \
 $(COMMON)/formats/webm/rwebm_video.c \
 $(COMMON)/lists/string_list.c \
 $(COMMON)/memmap/memmap.c \
 $(COMMON)/memmap/memalign.c \
 $(COMMON)/streams/file_stream.c \
 $(COMMON)/string/rstrtod.c \
 $(COMMON)/string/stdstring.c \
 $(COMMON)/time/rtime.c \
 $(COMMON)/vfs/vfs_implementation.c

CFLAGS += -Wall -Wextra -std=gnu89 -O1 -g1 -D_GNU_SOURCE \
 -DHAVE_RWEBM -DHAVE_RVP9 -DHAVE_RWEBP -I$(COMMON)/include
LDLIBS += -lm
ifneq ($(OS),Windows_NT)
CFLAGS += -DHAVE_MMAP
else
LDLIBS += -lwinmm
endif
ifeq ($(MP4),1)
CFLAGS += -DHAVE_RMP4 -DHAVE_RH264 -DHAVE_RH265
SOURCES += $(COMMON)/formats/mp4/rmp4.c \
 $(COMMON)/formats/mp4/rmp4_video.c
endif
ifeq ($(AUDIO),1)
CFLAGS += -DHAVE_RAAC -DHAVE_ROPUS -DHAVE_RVORBIS
SOURCES += $(COMMON)/formats/aac/raac.c \
 $(COMMON)/formats/opus/ropus.c $(COMMON)/formats/vorbis/rvorbis.c
endif
ifeq ($(THREADS),1)
CFLAGS += -DHAVE_THREADS
SOURCES += $(COMMON)/rthreads/rthreads.c \
 $(COMMON)/rthreads/retro_eventcount.c $(COMMON)/rthreads/tpool.c
LDLIBS += -lpthread
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
CFLAGS += $(EXTRA_CFLAGS)
OBJS := $(SAMPLE_OBJDIR)/webm_player_test.o \
 $(call sample_objs,$(filter-out webm_player_test.o,$(SOURCES:.c=.o)))

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

ifeq ($(MP4),1)
HW_TARGET := webm_hw_test$(EXE)
HW_SOURCES := $(filter-out webm_player_test.c $(ROOT)/cores/libretro-media/media_core.c $(COMMON)/formats/h265/rh265.c,$(SOURCES))
HW_OBJS := $(call sample_objs,$(HW_SOURCES:.c=.o))
$(HW_TARGET): webm_hw_test.c $(ROOT)/cores/libretro-media/media_core.c $(ROOT)/cores/libretro-media/media_hw.h \
      ../../gfx/d3d11_video_decode/rh265_hw_stream_test.c \
      $(COMMON)/formats/h265/rh265.c $(HW_OBJS)
	$(CC) $(CFLAGS) -o $@ webm_hw_test.c $(HW_OBJS) $(LDFLAGS) $(LDLIBS)
endif

ifeq ($(AUDIO),1)
TIMELINE_TARGET := webm_audio_timeline_test$(EXE)
TIMELINE_SOURCES := $(filter-out webm_player_test.c $(ROOT)/cores/libretro-media/media_core.c,$(SOURCES))
TIMELINE_OBJS := $(call sample_objs,$(TIMELINE_SOURCES:.c=.o))
ifeq ($(MP4),1)
VIDEO_TIMELINE_TARGET := video_timeline_test$(EXE)
$(VIDEO_TIMELINE_TARGET): video_timeline_test.c webm_player_test.c $(ROOT)/cores/libretro-media/media_core.c $(TIMELINE_OBJS)
	$(CC) $(CFLAGS) -o $@ video_timeline_test.c $(TIMELINE_OBJS) $(LDFLAGS) $(LDLIBS)
endif

$(TIMELINE_TARGET): webm_audio_timeline_test.c $(ROOT)/cores/libretro-media/media_core.c $(TIMELINE_OBJS)
	$(CC) $(CFLAGS) -o $@ webm_audio_timeline_test.c $(TIMELINE_OBJS) $(LDFLAGS) $(LDLIBS)
endif

ifeq ($(MP4)$(AUDIO),11)
AUDIO_START_TARGET := webm_audio_start_test$(EXE)
AUDIO_START_SOURCES := $(filter-out webm_player_test.c $(ROOT)/cores/libretro-media/media_core.c,$(SOURCES))
AUDIO_START_OBJS := $(call sample_objs,$(AUDIO_START_SOURCES:.c=.o))

$(AUDIO_START_TARGET): webm_audio_start_test.c $(ROOT)/cores/libretro-media/media_core.c $(AUDIO_START_OBJS)
	$(CC) $(CFLAGS) -o $@ webm_audio_start_test.c $(AUDIO_START_OBJS) $(LDFLAGS) $(LDLIBS)
endif

AV1_HW_TARGET := $(SAMPLE_OBJDIR)/webm_av1_hw_test$(EXE)
AV1_HW_SOURCES := $(filter-out webm_player_test.c $(ROOT)/cores/libretro-media/media_core.c,$(SOURCES))
# the two that read HAVE_RAV1 or RWEBM_VIDEO_MAX_TS are compiled with
# them; the rest are the build's own objects
AV1_HW_OWN := $(filter $(COMMON)/formats/webm/rwebm_video.c $(COMMON)/formats/mp4/rmp4_video.c,$(AV1_HW_SOURCES))
AV1_HW_OBJS := $(call sample_objs,$(patsubst %.c,%.o,$(filter-out $(AV1_HW_OWN),$(AV1_HW_SOURCES))))
$(AV1_HW_TARGET): webm_av1_hw_test.c $(ROOT)/cores/libretro-media/media_core.c \
      $(COMMON)/samples/formats/av1/rav1_mp4_fixture.h \
      $(COMMON)/samples/formats/av1/rav1_webm_fixture.h $(AV1_HW_OWN) $(AV1_HW_OBJS) \
      $(COMMON)/formats/av1/rav1_hw.c $(COMMON)/include/formats/rav1_hw_stream.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DHAVE_RAV1 -DRWEBM_VIDEO_MAX_TS=2 -o $@ $< $(AV1_HW_OWN) $(COMMON)/formats/av1/rav1_hw.c $(AV1_HW_OBJS) $(LDFLAGS) $(LDLIBS)

RESIDENCY_TARGET := webm_residency_test$(EXE)
PROGRESSIVE_GROUP_TARGET := progressive_group_test$(EXE)
RESIDENCY_SOURCES := $(filter-out webm_player_test.c $(ROOT)/cores/libretro-media/media_core.c,$(SOURCES))
RESIDENCY_OBJS := $(call sample_objs,$(RESIDENCY_SOURCES:.c=.o))
$(RESIDENCY_TARGET): webm_residency_test.c $(ROOT)/cores/libretro-media/media_core.c $(RESIDENCY_OBJS)
	$(CC) $(CFLAGS) -o $@ webm_residency_test.c $(RESIDENCY_OBJS) $(LDFLAGS) $(LDLIBS)

$(PROGRESSIVE_GROUP_TARGET): progressive_group_test.c webm_player_test.c $(ROOT)/cores/libretro-media/media_core.c $(RESIDENCY_OBJS)
	$(CC) $(CFLAGS) -o $@ progressive_group_test.c $(RESIDENCY_OBJS) $(LDFLAGS) $(LDLIBS)

ifeq ($(AUDIO),1)
VORBIS_SEEK_TARGET := vorbis_seek_test$(EXE)
$(VORBIS_SEEK_TARGET): vorbis_seek_test.c opus_seek_test.c $(RESIDENCY_OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(RESIDENCY_OBJS) $(LDFLAGS) $(LDLIBS)
OPUS_SEEK_TARGET := opus_seek_test$(EXE)
$(OPUS_SEEK_TARGET): opus_seek_test.c $(RESIDENCY_OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(RESIDENCY_OBJS) $(LDFLAGS) $(LDLIBS)
endif

WEBM_TIMING_TARGET := webm_timing_test$(EXE)
$(WEBM_TIMING_TARGET): webm_timing_test.c $(RESIDENCY_OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(RESIDENCY_OBJS) $(LDFLAGS) $(LDLIBS)

WEBM_SEEK_TARGET := webm_seek_test$(EXE)
VP8_REFERENCE_TARGET := vp8_reference_test$(EXE)
$(VP8_REFERENCE_TARGET): vp8_reference_test.c $(COMMON)/formats/vp8/rvp8.c
	$(CC) $(CFLAGS) -fno-builtin-memcpy -o $@ $^ $(LDFLAGS) -Wl,--wrap=memcpy -Wl,--wrap=malloc $(LDLIBS)

$(WEBM_SEEK_TARGET): webm_seek_test.c $(RESIDENCY_OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(RESIDENCY_OBJS) $(LDFLAGS) $(LDLIBS)

ifeq ($(MP4),1)
MP4_SEEK_TARGET := mp4_seek_test$(EXE)
$(MP4_SEEK_TARGET): mp4_seek_test.c $(RESIDENCY_OBJS)
	$(CC) $(CFLAGS) -o $@ $< $(RESIDENCY_OBJS) $(LDFLAGS) $(LDLIBS)
endif

# Force the bounded timing cursor through ordinary short lease fixtures.
HW_TIMING_OBJ := $(SAMPLE_OBJDIR)/rwebm_video_timing_prefix.o
$(HW_TIMING_OBJ): $(COMMON)/formats/webm/rwebm_video.c
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DRWEBM_VIDEO_MAX_TS=2 -c -o $@ $<
VP9_HW_OBJS := $(filter-out %/formats/webm/rwebm_video.o,$(RESIDENCY_OBJS)) $(HW_TIMING_OBJ)
WEBM_SEEK_PREFIX_TARGET := webm_seek_prefix_test$(EXE)
$(WEBM_SEEK_PREFIX_TARGET): webm_seek_test.c $(VP9_HW_OBJS)
	$(CC) $(CFLAGS) -o $@ webm_seek_test.c $(VP9_HW_OBJS) $(LDFLAGS) $(LDLIBS)

VP9_HW_TARGET := webm_vp9_hw_test$(EXE)
$(VP9_HW_TARGET): webm_vp9_hw_test.c $(ROOT)/cores/libretro-media/media_core.c $(VP9_HW_OBJS)
	$(CC) $(CFLAGS) -o $@ webm_vp9_hw_test.c $(VP9_HW_OBJS) $(LDFLAGS) $(LDLIBS)

DEVICE_TARGET := webm_vp9_device_test$(EXE)
ifeq ($(EXE),.exe)
$(DEVICE_TARGET): webm_vp9_device_test.c $(ROOT)/cores/libretro-media/media_core.c \
      ../../gfx/d3d11_video_decode/d3d11_video_decode_test.c \
      ../../gfx/d3d11_video_decode/d3d11_video_decode_test_data.h \
      ../../gfx/d3d11_video_decode/hevc_device_data.h \
      ../../gfx/d3d11_video_decode/frontend_stubs.h \
      $(ROOT)/gfx/common/d3d11_video_decode.c $(ROOT)/gfx/common/d3d11_video_decode.h \
      $(ROOT)/gfx/common/dxva_vp9.h $(ROOT)/gfx/common/dxva_hevc.h \
      $(ROOT)/gfx/common/dxva_hevc_rh265.h $(RESIDENCY_OBJS)
	$(CC) $(CFLAGS) -I$(ROOT) -isystem $(ROOT)/gfx/include/dxsdk \
	   -ffunction-sections -fdata-sections -o $@ webm_vp9_device_test.c \
	   $(RESIDENCY_OBJS) $(LDFLAGS) -Wl,--gc-sections -ld3d11 $(LDLIBS)
else
$(DEVICE_TARGET): webm_vp9_device_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
endif

check: $(WEBM_TIMING_TARGET) $(VORBIS_SEEK_TARGET) $(OPUS_SEEK_TARGET) $(WEBM_SEEK_TARGET) $(MP4_SEEK_TARGET) $(TARGET) $(HW_TARGET) $(AV1_HW_TARGET) $(RESIDENCY_TARGET) $(VP9_HW_TARGET) $(DEVICE_TARGET) $(AUDIO_START_TARGET)
check: $(PROGRESSIVE_GROUP_TARGET) $(TIMELINE_TARGET) $(VIDEO_TIMELINE_TARGET) $(VP8_REFERENCE_TARGET) $(WEBM_SEEK_PREFIX_TARGET)
	$(RUNNER) ./$(VP8_REFERENCE_TARGET) fixtures/vp8-refs.ivf fixtures/vp8-refs.yuv
	$(RUNNER) ./$(VP8_REFERENCE_TARGET) fixtures/vp8-altref.ivf fixtures/vp8-altref.yuv
	$(RUNNER) ./$(VP8_REFERENCE_TARGET) fixtures/vp8-resize.ivf fixtures/vp8-resize.yuv
	$(RUNNER) ./$(PROGRESSIVE_GROUP_TARGET) fixtures/core-group-seed.webm fixtures/core-group.webm
	CORE_SPARSE=1 $(RUNNER) ./$(PROGRESSIVE_GROUP_TARGET) fixtures/core-group-seed.webm fixtures/core-sparse.webm
ifeq ($(AUDIO),1)
	$(RUNNER) ./$(TIMELINE_TARGET) fixtures/timeline-0.webm 0
	$(RUNNER) ./$(TIMELINE_TARGET) fixtures/timeline-1.webm 48000
	$(RUNNER) ./$(TIMELINE_TARGET) fixtures/timeline-2.webm 96000
	$(RUNNER) ./$(TIMELINE_TARGET) fixtures/timeline-gap-libopus.webm 0 50048 93952
	$(RUNNER) ./$(TIMELINE_TARGET) fixtures/timeline-gap-libvorbis.webm 0 50048 93952
	EXPECT_OVERLAP_FRAMES=24000 $(RUNNER) ./$(TIMELINE_TARGET) fixtures/timeline-overlap.webm 0
ifeq ($(MP4),1)
	$(RUNNER) ./$(TIMELINE_TARGET) fixtures/timeline-0.mp4 0
	$(RUNNER) ./$(TIMELINE_TARGET) fixtures/timeline-1.mp4 48000
	EXPECT_START_TRIM=4801 $(RUNNER) ./$(TIMELINE_TARGET) fixtures/timeline-vorbis-trim.mp4 0
	EXPECT_START_TRIM=4801 $(RUNNER) ./$(TIMELINE_TARGET) fixtures/timeline-opus-trim.mp4 0
	$(RUNNER) ./$(TIMELINE_TARGET) fixtures/timeline-opus.mp4 0
	$(RUNNER) ./$(TIMELINE_TARGET) fixtures/timeline-vorbis.mp4 0
	$(RUNNER) ./$(TIMELINE_TARGET) fixtures/timeline-aac-0.mkv 0
	$(RUNNER) ./$(TIMELINE_TARGET) fixtures/timeline-aac-1.mkv 48000
	$(RUNNER) ./$(VIDEO_TIMELINE_TARGET) fixtures/video-timeline-0.mp4 fixtures/video-timeline-1.mp4
endif
	$(RUNNER) ./$(VORBIS_SEEK_TARGET) fixtures/vp9-vorbis.webm fixtures/vp9-seek-vorbis.webm
	$(RUNNER) ./$(OPUS_SEEK_TARGET) fixtures/vp8-opus.webm fixtures/vp8-seek-opus.webm
endif
	$(RUNNER) ./$(WEBM_TIMING_TARGET) fixtures/vp8-long-timing.webm
	$(RUNNER) ./$(WEBM_SEEK_TARGET) fixtures/vp8-opus.webm fixtures/vp9-vorbis.webm fixtures/vp8-seek-opus.webm fixtures/vp9-seek-vorbis.webm fixtures/vp9-seek-live.webm fixtures/vp8-altref.webm
	$(RUNNER) ./$(WEBM_SEEK_PREFIX_TARGET) fixtures/vp8-opus.webm fixtures/vp9-vorbis.webm fixtures/vp8-seek-opus.webm fixtures/vp9-seek-vorbis.webm fixtures/vp9-seek-live.webm fixtures/vp8-altref.webm
	$(RUNNER) ./$(VP9_HW_TARGET)
	$(RUNNER) ./$(DEVICE_TARGET)
	$(RUNNER) ./$(RESIDENCY_TARGET)
	$(RUNNER) ./$(TARGET) fixtures
	$(RUNNER) ./$(AV1_HW_TARGET)
ifeq ($(MP4)$(AUDIO),11)
	$(RUNNER) ./$(AUDIO_START_TARGET) fixtures/hevc-audio-gap.mp4
	$(RUNNER) ./$(AUDIO_START_TARGET) fixtures/hevc-opus-audio-gap.mp4
	$(RUNNER) ./$(AUDIO_START_TARGET) fixtures/hevc-aac44-audio-gap.mp4 44100
endif
ifeq ($(MP4),1)
	$(RUNNER) ./$(MP4_SEEK_TARGET) fixtures/h264-seek-aac.mp4 fixtures/hevc-seek-aac.mp4 fixtures/h264-long-seek.mp4
	$(RUNNER) ./$(HW_TARGET)
endif

clean:
	rm -f video_timeline_test video_timeline_test.exe
	rm -f webm_audio_timeline_test webm_audio_timeline_test.exe
	rm -f $(PROGRESSIVE_GROUP_TARGET)
	rm -f webm_timing_test webm_timing_test.exe
	rm -f vorbis_seek_test vorbis_seek_test.exe
	rm -f opus_seek_test opus_seek_test.exe
	rm -f webm_seek_test webm_seek_test.exe
	rm -f webm_seek_prefix_test webm_seek_prefix_test.exe
	rm -f vp8_reference_test vp8_reference_test.exe
	rm -f mp4_seek_test mp4_seek_test.exe
	rm -f webm_audio_start_test webm_audio_start_test.exe webm_av1_hw_test webm_av1_hw_test.exe av1-core-fixture.mp4 av1-core-fixture.webm
	rm -rf $(SAMPLE_OBJDIR)
	rm -f webm_player_test.o $(TARGET) webm_hw_test webm_hw_test.exe webm_residency_test webm_residency_test.exe webm_vp9_hw_test webm_vp9_hw_test.exe webm_vp9_device_test webm_vp9_device_test.exe

.PHONY: all check clean

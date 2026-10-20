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
 $(COMMON)/formats/mp4/rmp4_video.c \
 $(COMMON)/formats/h264/rh264.c $(COMMON)/formats/h265/rh265.c
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

ifeq ($(MP4)$(AUDIO),11)
AUDIO_START_TARGET := webm_audio_start_test$(EXE)
AUDIO_START_SOURCES := $(filter-out webm_player_test.c $(ROOT)/cores/libretro-media/media_core.c,$(SOURCES))
AUDIO_START_OBJS := $(call sample_objs,$(AUDIO_START_SOURCES:.c=.o))
$(AUDIO_START_TARGET): webm_audio_start_test.c $(ROOT)/cores/libretro-media/media_core.c $(AUDIO_START_OBJS)
	$(CC) $(CFLAGS) -o $@ webm_audio_start_test.c $(AUDIO_START_OBJS) $(LDFLAGS) $(LDLIBS)
endif

AV1_HW_TARGET := $(SAMPLE_OBJDIR)/webm_av1_hw_test$(EXE)
AV1_HW_SOURCES := $(filter-out webm_player_test.c $(ROOT)/cores/libretro-media/media_core.c,$(SOURCES))
$(AV1_HW_TARGET): webm_av1_hw_test.c $(ROOT)/cores/libretro-media/media_core.c \
      $(COMMON)/samples/formats/av1/rav1_mp4_fixture.h \
      $(COMMON)/samples/formats/av1/rav1_webm_fixture.h $(AV1_HW_SOURCES) \
      $(COMMON)/formats/av1/rav1_hw.c $(COMMON)/include/formats/rav1_hw_stream.h
	@mkdir -p $(dir $@)
	$(CC) $(CFLAGS) -DHAVE_RAV1 -o $@ $< $(AV1_HW_SOURCES) $(COMMON)/formats/av1/rav1_hw.c $(LDFLAGS) $(LDLIBS)

RESIDENCY_TARGET := webm_residency_test$(EXE)
RESIDENCY_SOURCES := $(filter-out webm_player_test.c $(ROOT)/cores/libretro-media/media_core.c,$(SOURCES))
RESIDENCY_OBJS := $(call sample_objs,$(RESIDENCY_SOURCES:.c=.o))
$(RESIDENCY_TARGET): webm_residency_test.c $(ROOT)/cores/libretro-media/media_core.c $(RESIDENCY_OBJS)
	$(CC) $(CFLAGS) -o $@ webm_residency_test.c $(RESIDENCY_OBJS) $(LDFLAGS) $(LDLIBS)

VP9_HW_TARGET := webm_vp9_hw_test$(EXE)
$(VP9_HW_TARGET): webm_vp9_hw_test.c $(ROOT)/cores/libretro-media/media_core.c $(RESIDENCY_OBJS)
	$(CC) $(CFLAGS) -o $@ webm_vp9_hw_test.c $(RESIDENCY_OBJS) $(LDFLAGS) $(LDLIBS)

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

check: $(TARGET) $(HW_TARGET) $(AV1_HW_TARGET) $(RESIDENCY_TARGET) $(VP9_HW_TARGET) $(DEVICE_TARGET) $(AUDIO_START_TARGET)
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
	$(RUNNER) ./$(HW_TARGET)
endif

clean:
	rm -f webm_audio_start_test webm_audio_start_test.exe webm_av1_hw_test webm_av1_hw_test.exe av1-core-fixture.mp4 av1-core-fixture.webm
	rm -rf $(SAMPLE_OBJDIR)
	rm -f webm_player_test.o $(TARGET) webm_hw_test webm_hw_test.exe webm_residency_test webm_residency_test.exe webm_vp9_hw_test webm_vp9_hw_test.exe webm_vp9_device_test webm_vp9_device_test.exe

.PHONY: all check clean

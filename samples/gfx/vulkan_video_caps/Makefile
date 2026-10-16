CC ?= cc
RA := ../../..
SAMPLED_DEPS = ui_roundtrip.comp.inc ui_roundtrip.comp generate_ui_roundtrip.py $(RA)/gfx/drivers/vulkan_shaders/alpha_blend_linear.frag $(RA)/gfx/drivers/vulkan_shaders/hdr.frag $(RA)/gfx/drivers/vulkan_shaders/hdr_common.glsl ../d3d_p010_import/fp16_reference.h $(RA)/gfx/drivers/vulkan_shaders/yuv420_to_rgba16f.comp.inc $(RA)/gfx/drivers/vulkan_shaders/yuv_image_to_rgba16f.comp.inc $(RA)/gfx/common/video_import_color.h $(RA)/libretro-common/formats/image/image_hdr_blit.c $(VP9_DEPS) $(RA)/gfx/common/vulkan_video_planes.h \
      $(RA)/gfx/drivers/vulkan_shaders/yuv_image_to_rgba8888.comp.inc \
      $(RA)/libretro-common/include/formats/video_hw_frame.h
TARGET := vulkan_video_caps_test$(EXE)
AV1_DEPS := $(RA)/gfx/include/vk_video/vulkan_video_codec_av1std.h \
      $(RA)/gfx/include/vk_video/vulkan_video_codec_av1std_decode.h
AV1_PARAMS_DEPS := $(AV1_DEPS) $(RA)/gfx/include/vulkan/vulkan_core.h \
      $(RA)/gfx/common/vulkan_av1_params.h \
      $(wildcard $(RA)/libretro-common/include/formats/rav1*.h)
VP9_DEPS := $(AV1_DEPS) $(RA)/gfx/common/vulkan_vp9_picture.h $(RA)/gfx/include/vulkan/vulkan_core.h \
      $(RA)/gfx/include/vk_video/vulkan_video_codec_vp9std.h \
      $(RA)/gfx/include/vk_video/vulkan_video_codec_vp9std_decode.h
PARAMS := hevc_params_test$(EXE)
SESSION := hevc_session_test$(EXE)
DECODERS := $(RA)/libretro-common/formats/h265/rh265.c $(RA)/libretro-common/formats/mp4/rmp4.c
HEVC_DEPS := $(RA)/gfx/common/vulkan_hevc_params.h $(RA)/gfx/common/vulkan_hevc_picture.h \
             $(RA)/libretro-common/include/formats/rh265.h $(RA)/libretro-common/include/formats/rh265_hw.h \
             ../d3d11_video_decode/hevc_device_data.h
CFLAGS += -O1 -g1 -std=gnu89 -Wall -Wextra -Werror=declaration-after-statement \
          -I$(RA)/libretro-common/include -I$(RA)/gfx/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: av1_params_test$(EXE) vp9_picture_test$(EXE) $(TARGET) $(PARAMS) video_layout_test$(EXE)

$(PARAMS): hevc_params_test.c $(HEVC_DEPS) $(DECODERS)
	$(CC) $(CFLAGS) -o $@ hevc_params_test.c $(DECODERS) $(LDFLAGS) -lm

av1_session_test$(EXE): probe.c av1_session.h av1_session_data.h $(AV1_PARAMS_DEPS)
	$(CC) $(CFLAGS) -DAV1_SESSION_TEST -o $@ probe.c $(LDFLAGS) $(if $(filter .exe,$(EXE)),,-ldl)

av1-session-check: av1_session_test$(EXE)
	$(RUNNER) ./av1_session_test$(EXE)

$(SESSION): probe.c hevc_session.h hevc_params_test.c $(DECODERS) $(HEVC_DEPS) $(VP9_DEPS)
	$(CC) $(CFLAGS) -DHEVC_SESSION_TEST -o $@ probe.c $(DECODERS) $(LDFLAGS) -lm $(if $(filter .exe,$(EXE)),,-ldl)

AV1_CORE_SOURCES := $(RA)/libretro-common/compat/compat_strl.c \
      $(RA)/libretro-common/compat/compat_strcasestr.c \
      $(RA)/libretro-common/compat/fopen_utf8.c \
      $(RA)/libretro-common/encodings/encoding_utf.c \
      $(RA)/libretro-common/encodings/encoding_crc32.c \
      $(RA)/libretro-common/file/file_path.c \
      $(RA)/libretro-common/file/file_path_io.c \
      $(RA)/libretro-common/formats/data_transfer.c \
      $(RA)/libretro-common/formats/webm/rwebm.c \
      $(RA)/libretro-common/formats/webm/rwebm_video.c \
      $(RA)/libretro-common/lists/string_list.c \
      $(RA)/libretro-common/memmap/memmap.c \
      $(RA)/libretro-common/memmap/memalign.c \
      $(RA)/libretro-common/streams/file_stream.c \
      $(RA)/libretro-common/string/rstrtod.c \
      $(RA)/libretro-common/string/stdstring.c \
      $(RA)/libretro-common/vfs/vfs_implementation.c

AV1_MP4_SOURCES := $(RA)/libretro-common/formats/mp4/rmp4_video.c \
      $(RA)/libretro-common/formats/vp8/rvp8.c \
      $(RA)/libretro-common/formats/image/image_blit_bands.c \
      $(RA)/libretro-common/formats/image/image_hdr_blit.c \
      $(RA)/libretro-common/features/features_cpu.c $(RA)/libretro-common/time/rtime.c

hevc_decode_test$(EXE): av1_core_decode.h $(RA)/cores/libretro-media/media_core.c $(AV1_CORE_SOURCES) av1_mp4_decode.h av1_webm_decode.h $(AV1_MP4_SOURCES) $(AV1_PARAMS_DEPS) $(RA)/gfx/common/vulkan_video_decode_av1.h $(RA)/libretro-common/formats/av1/rav1_hw.c $(SAMPLED_DEPS) probe.c hevc_decode.h h264_decode.h vp9_decode.h av1_decode.h $(DECODERS) $(HEVC_DEPS) \
      $(RA)/gfx/common/vulkan_video_decode.c $(RA)/gfx/common/vulkan_video_decode_hevc.h \
      $(RA)/gfx/common/vulkan_video_decode_vp9.h $(RA)/gfx/common/vulkan_vp9_picture.h \
      $(RA)/gfx/common/vulkan_hevc_picture.h ../d3d11_video_decode/d3d11_video_decode_test_data.h \
      $(RA)/libretro-common/formats/h264/rh264.c $(RA)/libretro-common/vulkan/vulkan_symbol_wrapper.c \
      $(RA)/libretro-common/formats/vp9/rvp9.c
	$(CC) $(CFLAGS) -I$(RA) -DHEVC_DECODE_TEST -DHAVE_RMP4 -DHAVE_RH264 -DHAVE_VULKAN -DHAVE_RVP9 -DHAVE_RAV1 \
	   -o $@ probe.c $(DECODERS) $(RA)/libretro-common/formats/h264/rh264.c \
	   $(RA)/libretro-common/vulkan/vulkan_symbol_wrapper.c $(RA)/libretro-common/formats/vp9/rvp9.c $(RA)/libretro-common/formats/av1/rav1_hw.c $(AV1_MP4_SOURCES) $(AV1_CORE_SOURCES) $(LDFLAGS) -lm $(if $(filter .exe,$(EXE)),,-ldl)

av1_params_test$(EXE): av1_params_test.c av1_session_data.h $(AV1_PARAMS_DEPS)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

vp9_picture_test$(EXE): vp9_picture_test.c $(VP9_DEPS)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(TARGET): $(VP9_DEPS) vulkan_video_caps_test.c $(RA)/gfx/common/vulkan_video_caps.h
	$(CC) $(CFLAGS) -o $@ vulkan_video_caps_test.c $(LDFLAGS)

probe$(EXE): probe.c $(RA)/gfx/common/vulkan_video_caps.h $(VP9_DEPS)
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(if $(filter .exe,$(EXE)),,-ldl)

import_test$(EXE): $(SAMPLED_DEPS) probe.c import_test.h build_import_test.py $(RA)/gfx/drivers/vulkan.c \
      $(RA)/gfx/common/vulkan_video_import.h $(RA)/gfx/drivers/vulkan_shaders/yuv420_to_rgba8888.comp.inc
	CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" python3 build_import_test.py $@

hevc_render_test$(EXE): av1_core_decode.h $(RA)/cores/libretro-media/media_core.c $(AV1_CORE_SOURCES) av1_mp4_decode.h av1_webm_decode.h $(AV1_MP4_SOURCES) $(AV1_PARAMS_DEPS) av1_decode.h $(RA)/gfx/common/vulkan_video_decode_av1.h $(RA)/libretro-common/formats/av1/rav1_hw.c $(SAMPLED_DEPS) probe.c hevc_decode.h vp9_decode.h hevc_render.c hevc_render.h import_test.h \
      build_import_test.py $(HEVC_DEPS) $(DECODERS) $(RA)/gfx/drivers/vulkan.c \
      h264_decode.h ../d3d11_video_decode/d3d11_video_decode_test_data.h \
      $(RA)/gfx/common/vulkan_common.c $(RA)/gfx/common/vulkan_video_import.h \
      $(RA)/libretro-common/formats/h264/rh264.c \
      $(RA)/libretro-common/formats/vp9/rvp9.c \
      $(RA)/libretro-common/vulkan/vulkan_symbol_wrapper.c \
      $(RA)/libretro-common/formats/image/image_yuv_blit.c \
      $(RA)/gfx/common/vulkan_video_decode.c $(RA)/gfx/common/vulkan_video_decode_hevc.h \
      $(RA)/gfx/common/vulkan_video_decode_vp9.h $(RA)/gfx/common/vulkan_vp9_picture.h \
      $(RA)/gfx/drivers/vulkan_shaders/yuv420_to_rgba8888.comp.inc
	CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" python3 build_import_test.py $@ --hevc-render

render-check: hevc_render_test$(EXE)
	$(RUNNER) ./hevc_render_test$(EXE)

import-check: import_test$(EXE)
	$(RUNNER) ./import_test$(EXE)

video_layout_test$(EXE): video_layout_test.c $(RA)/gfx/common/vulkan_video_import.h $(RA)/gfx/common/video_import_color.h
	$(CC) $(CFLAGS) -o $@ $< $(RA)/libretro-common/formats/image/image_yuv_blit.c $(LDFLAGS) -lm

check: all
	$(RUNNER) ./av1_params_test$(EXE)
	$(RUNNER) ./vp9_picture_test$(EXE)
	$(RUNNER) ./$(TARGET)
	$(RUNNER) ./$(PARAMS)
	$(RUNNER) ./video_layout_test$(EXE)

session-check: $(SESSION)
	$(RUNNER) ./$(SESSION)

direct-check: hevc_decode_test$(EXE) hevc_render_test$(EXE) import_test$(EXE)
	VULKAN_TEST_GENERAL_IMPORT=1 $(RUNNER) ./import_test$(EXE)
	VULKAN_TEST_DIRECT_DPB=1 $(RUNNER) ./hevc_decode_test$(EXE)
	VULKAN_TEST_DIRECT_DPB=1 $(RUNNER) ./hevc_render_test$(EXE)

decode-check: hevc_decode_test$(EXE)
	$(RUNNER) ./hevc_decode_test$(EXE)

clean:
	rm -f av1_params_test av1_params_test.exe av1_session_test av1_session_test.exe
	rm -f vp9_picture_test vp9_picture_test.exe
	rm -f vulkan_video_caps_test vulkan_video_caps_test.exe probe probe.exe
	rm -f hevc_params_test hevc_params_test.exe hevc_session_test hevc_session_test.exe
	rm -f hevc_decode_test hevc_decode_test.exe hevc_render_test hevc_render_test.exe
	rm -f import_test import_test.exe video_layout_test video_layout_test.exe

.PHONY: all check av1-session-check session-check decode-check import-check render-check direct-check clean

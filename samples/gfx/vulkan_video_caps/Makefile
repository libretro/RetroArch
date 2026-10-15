CC ?= cc
RA := ../../..
SAMPLED_DEPS := $(RA)/gfx/common/vulkan_video_planes.h \
      $(RA)/gfx/drivers/vulkan_shaders/yuv_image_to_rgba8888.comp.inc \
      $(RA)/libretro-common/include/formats/video_hw_frame.h
TARGET := vulkan_video_caps_test$(EXE)
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

all: $(TARGET) $(PARAMS) video_layout_test$(EXE)

$(PARAMS): hevc_params_test.c $(HEVC_DEPS) $(DECODERS)
	$(CC) $(CFLAGS) -o $@ hevc_params_test.c $(DECODERS) $(LDFLAGS) -lm

$(SESSION): probe.c hevc_session.h hevc_params_test.c $(DECODERS) $(HEVC_DEPS)
	$(CC) $(CFLAGS) -DHEVC_SESSION_TEST -o $@ probe.c $(DECODERS) $(LDFLAGS) -lm $(if $(filter .exe,$(EXE)),,-ldl)

hevc_decode_test$(EXE): $(SAMPLED_DEPS) probe.c hevc_decode.h h264_decode.h $(DECODERS) $(HEVC_DEPS) \
      $(RA)/gfx/common/vulkan_video_decode.c $(RA)/gfx/common/vulkan_video_decode_hevc.h \
      $(RA)/gfx/common/vulkan_hevc_picture.h ../d3d11_video_decode/d3d11_video_decode_test_data.h \
      $(RA)/libretro-common/formats/h264/rh264.c $(RA)/libretro-common/vulkan/vulkan_symbol_wrapper.c
	$(CC) $(CFLAGS) -I$(RA) -DHEVC_DECODE_TEST -DHAVE_RMP4 -DHAVE_RH264 -DHAVE_VULKAN \
	   -o $@ probe.c $(DECODERS) $(RA)/libretro-common/formats/h264/rh264.c \
	   $(RA)/libretro-common/vulkan/vulkan_symbol_wrapper.c $(LDFLAGS) -lm $(if $(filter .exe,$(EXE)),,-ldl)

$(TARGET): vulkan_video_caps_test.c $(RA)/gfx/common/vulkan_video_caps.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

probe$(EXE): probe.c $(RA)/gfx/common/vulkan_video_caps.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(if $(filter .exe,$(EXE)),,-ldl)

import_test$(EXE): $(SAMPLED_DEPS) probe.c import_test.h build_import_test.py $(RA)/gfx/drivers/vulkan.c \
      $(RA)/gfx/common/vulkan_video_import.h $(RA)/gfx/drivers/vulkan_shaders/yuv420_to_rgba8888.comp.inc
	CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" python3 build_import_test.py $@

hevc_render_test$(EXE): $(SAMPLED_DEPS) probe.c hevc_decode.h hevc_render.c hevc_render.h import_test.h \
      build_import_test.py $(HEVC_DEPS) $(DECODERS) $(RA)/gfx/drivers/vulkan.c \
      h264_decode.h ../d3d11_video_decode/d3d11_video_decode_test_data.h \
      $(RA)/gfx/common/vulkan_common.c $(RA)/gfx/common/vulkan_video_import.h \
      $(RA)/libretro-common/formats/h264/rh264.c \
      $(RA)/libretro-common/vulkan/vulkan_symbol_wrapper.c \
      $(RA)/libretro-common/formats/image/image_yuv_blit.c \
      $(RA)/gfx/common/vulkan_video_decode.c $(RA)/gfx/common/vulkan_video_decode_hevc.h \
      $(RA)/gfx/drivers/vulkan_shaders/yuv420_to_rgba8888.comp.inc
	CC="$(CC)" CFLAGS="$(CFLAGS)" LDFLAGS="$(LDFLAGS)" python3 build_import_test.py $@ --hevc-render

render-check: hevc_render_test$(EXE)
	$(RUNNER) ./hevc_render_test$(EXE)

import-check: import_test$(EXE)
	$(RUNNER) ./import_test$(EXE)

video_layout_test$(EXE): video_layout_test.c $(RA)/gfx/common/vulkan_video_import.h
	$(CC) $(CFLAGS) -o $@ $< $(RA)/libretro-common/formats/image/image_yuv_blit.c $(LDFLAGS) -lm

check: all
	$(RUNNER) ./$(TARGET)
	$(RUNNER) ./$(PARAMS)
	$(RUNNER) ./video_layout_test$(EXE)

session-check: $(SESSION)
	$(RUNNER) ./$(SESSION)

decode-check: hevc_decode_test$(EXE)
	$(RUNNER) ./hevc_decode_test$(EXE)

clean:
	rm -f vulkan_video_caps_test vulkan_video_caps_test.exe probe probe.exe
	rm -f hevc_params_test hevc_params_test.exe hevc_session_test hevc_session_test.exe
	rm -f hevc_decode_test hevc_decode_test.exe hevc_render_test hevc_render_test.exe
	rm -f import_test import_test.exe video_layout_test video_layout_test.exe

.PHONY: all check session-check decode-check import-check render-check clean

# rh264's D3D11 video backend (gfx/common/d3d11_video_decode.c):
#
#   dxva_h264_test               the DXVA H.264 buffers byte by byte,
#                                and against dxva.h on Windows
#   d3d11_video_decode_mock_test the backend on a D3D11 that records what
#                                it is told (Windows x64, Wine will do)
#   d3d11_video_decode_test      the backend on a GPU, frames against
#                                software; skips without one
#
#   make check                                  Linux: dxva_h264_test
#   make check CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine
#                                               all three
RA     := ../../..
CC     ?= cc
CFLAGS ?= -O1 -g1
CFLAGS += -Wall -Wextra -Wno-unused-parameter -std=gnu99 \
          -I$(RA) -I$(RA)/libretro-common/include \
          -DHAVE_RMP4 -DHAVE_RH264 $(EXTRA_CFLAGS)
ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

DECODERS := $(RA)/libretro-common/formats/mp4/rmp4.c \
            $(RA)/libretro-common/formats/h264/rh264.c
DEPS     := $(RA)/gfx/common/dxva_h264.h d3d11_video_decode_test_data.h
WEBM_DECODERS := $(RA)/libretro-common/formats/webm/rwebm.c \
   $(RA)/libretro-common/formats/webm/rwebm_video.c \
   $(RA)/libretro-common/formats/vp8/rvp8.c \
   $(RA)/libretro-common/formats/image/image_blit_bands.c \
   $(RA)/libretro-common/formats/image/image_hdr_blit.c \
   $(RA)/libretro-common/features/features_cpu.c $(RA)/libretro-common/time/rtime.c
BACKEND  := $(RA)/gfx/common/d3d11_video_decode.c \
            $(RA)/gfx/common/d3d11_video_decode.h frontend_stubs.h \
            $(RA)/gfx/common/dxva_hevc.h $(RA)/gfx/common/dxva_hevc_rh265.h \
            $(RA)/gfx/common/dxva_vp9.h $(RA)/libretro-common/include/formats/rvp9_hw.h \
            $(RA)/libretro-common/include/formats/rh265.h \
            $(RA)/libretro-common/include/formats/video_hw_frame.h \
            $(RA)/libretro-common/include/formats/rh265_hw.h

TARGETS := dxva_h264_test$(EXE) dxva_hevc_test$(EXE) dxva_hevc_access_unit_test$(EXE)
TARGETS += dxva_vp9_test$(EXE)
TARGETS += rh265_slice_snapshot_test$(EXE)
TARGETS += rh265_hw_state_test$(EXE)
TARGETS += rh265_hw_session_test$(EXE)
TARGETS += rh265_hw_stream_test$(EXE)
TARGETS += video_hw_format_test$(EXE) hevc_device_fixture_test$(EXE) hevc_color_stream_test$(EXE)
ifeq ($(EXE),.exe)
TARGETS += d3d11_video_decode_mock_test$(EXE) d3d11_video_decode_test$(EXE)
TARGETS += d3d11_backpressure_test$(EXE)
LDFLAGS += -lwinmm
WINFLAGS := -isystem $(RA)/gfx/include/dxsdk -Wno-cast-function-type -DHAVE_RVP9
endif

all: $(TARGETS)

dxva_vp9_test$(EXE): dxva_vp9_test.c $(RA)/gfx/common/dxva_vp9.h \
      $(RA)/libretro-common/include/formats/rvp9_hw.h
	$(CC) $(CFLAGS) -o $@ dxva_vp9_test.c $(LDFLAGS)

hevc_device_fixture_test$(EXE): hevc_device_fixture_test.c hevc_device_data.h \
      $(RA)/libretro-common/formats/mp4/rmp4.c \
      $(RA)/libretro-common/formats/h265/rh265.c
	$(CC) $(CFLAGS) -o $@ hevc_device_fixture_test.c \
	   $(RA)/libretro-common/formats/mp4/rmp4.c \
	   $(RA)/libretro-common/formats/h265/rh265.c $(LDFLAGS) -lm

video_hw_format_test$(EXE): video_hw_format_test.c \
      $(RA)/gfx/gfx_surface_player.c $(RA)/libretro-common/include/formats/video_hw_frame.h
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -o $@ \
	   video_hw_format_test.c $(LDFLAGS) -Wl,--gc-sections

rh265_slice_snapshot_test$(EXE): rh265_slice_snapshot_test.c \
      $(RA)/libretro-common/formats/h265/rh265.c
	$(CC) $(CFLAGS) -o $@ rh265_slice_snapshot_test.c $(LDFLAGS) -lm

rh265_hw_state_test$(EXE): rh265_hw_state_test.c $(RA)/libretro-common/include/formats/rh265_hw.h
	$(CC) $(CFLAGS) -o $@ rh265_hw_state_test.c $(LDFLAGS)

rh265_hw_session_test$(EXE): rh265_hw_session_test.c $(RA)/libretro-common/include/formats/rh265_hw.h \
      $(RA)/libretro-common/include/formats/video_hw_frame.h
	$(CC) $(CFLAGS) -o $@ rh265_hw_session_test.c $(LDFLAGS)

dxva_hevc_test$(EXE): dxva_hevc_test.c $(RA)/gfx/common/dxva_hevc.h \
      $(RA)/gfx/common/dxva_hevc_rh265.h $(RA)/libretro-common/include/formats/rh265.h \
      $(RA)/libretro-common/include/formats/rh265_hw.h
	$(CC) $(CFLAGS) -o $@ dxva_hevc_test.c $(LDFLAGS)

dxva_hevc_access_unit_test$(EXE): dxva_hevc_access_unit_test.c \
      dxva_hevc_access_unit_data.h $(RA)/gfx/common/dxva_hevc.h \
      $(RA)/gfx/common/dxva_hevc_rh265.h $(RA)/libretro-common/include/formats/rh265.h \
      $(RA)/libretro-common/include/formats/rh265_hw.h \
      $(RA)/libretro-common/formats/h265/rh265.c
	$(CC) $(CFLAGS) -o $@ dxva_hevc_access_unit_test.c \
	   $(RA)/libretro-common/formats/h265/rh265.c $(LDFLAGS) -lm


dxva_h264_test$(EXE): dxva_h264_test.c $(DECODERS) $(DEPS)
	$(CC) $(CFLAGS) -o $@ dxva_h264_test.c $(DECODERS) $(LDFLAGS) -lm

d3d11_video_decode_mock_test$(EXE): d3d11_video_decode_mock_test.c \
      $(DECODERS) $(DEPS) $(BACKEND) dxva_hevc_access_unit_data.h \
      $(RA)/libretro-common/formats/vp9/rvp9.c \
      $(RA)/libretro-common/formats/h265/rh265.c
	$(CC) $(CFLAGS) $(WINFLAGS) -o $@ d3d11_video_decode_mock_test.c \
	   $(DECODERS) $(RA)/libretro-common/formats/h265/rh265.c \
	   $(RA)/libretro-common/formats/vp9/rvp9.c $(LDFLAGS) -lm

d3d11_video_decode_test$(EXE): d3d11_video_decode_test.c \
      $(WEBM_DECODERS) \
      $(DECODERS) $(DEPS) $(BACKEND) hevc_device_data.h \
      $(RA)/libretro-common/formats/vp9/rvp9.c $(RA)/libretro-common/formats/vp9/rvp9_recon.inc \
      $(RA)/libretro-common/formats/h265/rh265.c
	$(CC) $(CFLAGS) $(WINFLAGS) -o $@ d3d11_video_decode_test.c \
	   $(DECODERS) $(RA)/libretro-common/formats/h265/rh265.c \
	   $(RA)/libretro-common/formats/vp9/rvp9.c $(WEBM_DECODERS) $(LDFLAGS) -ld3d11 -lm

PLAYER_SOURCES := $(RA)/libretro-common/formats/video_player.c \
   $(RA)/libretro-common/formats/image_transfer.c \
   $(RA)/libretro-common/formats/mp4/rmp4_video.c \
   $(RA)/libretro-common/formats/h265/rh265.c \
   $(RA)/libretro-common/formats/vp8/rvp8.c \
   $(RA)/libretro-common/formats/webm/rwebm_video.c \
   $(RA)/libretro-common/formats/webm/rwebm.c \
   $(RA)/libretro-common/formats/image/image_blit_bands.c \
   $(RA)/libretro-common/formats/image/image_hdr_blit.c \
   $(RA)/libretro-common/features/features_cpu.c \
   $(RA)/libretro-common/time/rtime.c

rh265_hw_stream_test$(EXE): rh265_hw_stream_test.c dxva_hevc_access_unit_data.h \
      $(DECODERS) $(PLAYER_SOURCES) $(RA)/libretro-common/include/formats/rh265_hw.h \
      $(RA)/libretro-common/include/formats/rh265.h \
      $(RA)/libretro-common/include/formats/rmp4_video.h \
      $(RA)/samples/formats/video_hdr_fp16/fixtures/sdr_hevc.mp4
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -o $@ \
	   rh265_hw_stream_test.c $(DECODERS) \
	   $(filter-out $(RA)/libretro-common/formats/h265/rh265.c,$(PLAYER_SOURCES)) \
	   $(LDFLAGS) -Wl,--gc-sections -lm

hevc_color_stream_test$(EXE): hevc_color_stream_test.c hevc_color_data.h \
      rh265_hw_stream_test.c $(DECODERS) $(PLAYER_SOURCES)
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -o $@ \
	   hevc_color_stream_test.c $(DECODERS) \
	   $(filter-out $(RA)/libretro-common/formats/h265/rh265.c $(RA)/libretro-common/formats/mp4/rmp4_video.c,$(PLAYER_SOURCES)) \
	   $(LDFLAGS) -Wl,--gc-sections -lm

d3d11_backpressure_test$(EXE): d3d11_backpressure_test.c \
      d3d11_video_decode_test.c $(DECODERS) $(DEPS) $(BACKEND) $(PLAYER_SOURCES) \
      $(RA)/libretro-common/formats/vp9/rvp9.c $(RA)/libretro-common/formats/vp9/rvp9_recon.inc
	$(CC) $(CFLAGS) $(WINFLAGS) -ffunction-sections -fdata-sections -o $@ \
	   d3d11_backpressure_test.c $(DECODERS) $(PLAYER_SOURCES) \
	   $(RA)/libretro-common/formats/vp9/rvp9.c \
	   $(LDFLAGS) -Wl,--gc-sections $(if $(filter .exe,$(EXE)),-ld3d11) -lm

check: all
	$(RUNNER) ./dxva_vp9_test$(EXE)
	$(RUNNER) ./dxva_h264_test$(EXE)
	$(RUNNER) ./dxva_hevc_test$(EXE)
	$(RUNNER) ./dxva_hevc_access_unit_test$(EXE)
	$(RUNNER) ./rh265_slice_snapshot_test$(EXE)
	$(RUNNER) ./rh265_hw_state_test$(EXE)
	$(RUNNER) ./rh265_hw_session_test$(EXE)
	$(RUNNER) ./rh265_hw_stream_test$(EXE)
	$(RUNNER) ./video_hw_format_test$(EXE)
	$(RUNNER) ./hevc_device_fixture_test$(EXE)
	$(RUNNER) ./hevc_color_stream_test$(EXE)
	CC="$(CC)" CFLAGS="$(CFLAGS)" RUNNER="$(RUNNER)" \
	   python3 ../d3d_texture_update_formats/run.py
	CC="$(CC)" CFLAGS="$(CFLAGS)" RUNNER="$(RUNNER)" \
	   python3 ../d3d_p010_import/run.py
	CC="$(CC)" CFLAGS="$(CFLAGS)" RUNNER="$(RUNNER)" \
	   python3 ../movie_gpu_handoff/run.py
ifeq ($(EXE),.exe)
	$(RUNNER) ./d3d11_video_decode_mock_test$(EXE)
	$(RUNNER) ./d3d11_video_decode_test$(EXE)
	$(RUNNER) ./d3d11_backpressure_test$(EXE)
endif

clean:
	rm -f dxva_vp9_test dxva_vp9_test.exe
	rm -f dxva_hevc_test dxva_hevc_test.exe
	rm -f dxva_hevc_access_unit_test dxva_hevc_access_unit_test.exe
	rm -f rh265_hw_state_test rh265_hw_state_test.exe
	rm -f rh265_hw_session_test rh265_hw_session_test.exe
	rm -f rh265_hw_stream_test rh265_hw_stream_test.exe
	rm -f video_hw_format_test video_hw_format_test.exe
	rm -f dxva_h264_test dxva_h264_test.exe \
	   d3d11_video_decode_mock_test.exe d3d11_video_decode_test.exe
	rm -f d3d11_backpressure_test d3d11_backpressure_test.exe

.PHONY: all check clean

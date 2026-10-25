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

# rmp4.c and rh264.c, which most of the tests here link: compiled once a
# build, not once a test, into a directory of the compiler's and the
# sanitizer's own. Each function in a section of its own, for the tests
# linked with --gc-sections.
.DEFAULT_GOAL := all
comma      := ,
DEC_OBJDIR := obj/$(subst /,_,$(CC))$(EXE)$(if $(SANITIZER),-$(subst $(comma),-,$(SANITIZER)))
DEC_OBJS   := $(DEC_OBJDIR)/rmp4.o $(DEC_OBJDIR)/rh264.o

$(DEC_OBJDIR)/rmp4.o: $(RA)/libretro-common/formats/mp4/rmp4.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -c -o $@ $<

$(DEC_OBJDIR)/rh264.o: $(RA)/libretro-common/formats/h264/rh264.c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -c -o $@ $<
WEBM_DECODERS := $(RA)/libretro-common/formats/webm/rwebm.c \
   $(RA)/libretro-common/formats/webm/rwebm_video.c \
   $(RA)/libretro-common/formats/vp8/rvp8.c \
   $(RA)/libretro-common/formats/image/image_blit_bands.c \
   $(RA)/libretro-common/formats/image/image_hdr_blit.c \
   $(RA)/libretro-common/features/features_cpu.c $(RA)/libretro-common/time/rtime.c
BACKEND  := $(RA)/gfx/common/d3d11_video_decode.c \
            $(RA)/gfx/common/d3d11_video_decode.h frontend_stubs.h \
            $(RA)/gfx/common/d3d11_video_pack.h \
            $(RA)/gfx/common/dxva_hevc.h $(RA)/gfx/common/dxva_hevc_rh265.h \
            $(RA)/gfx/common/dxva_vp9.h $(RA)/libretro-common/include/formats/rvp9_hw.h \
            $(RA)/libretro-common/include/formats/rh265.h \
            $(RA)/libretro-common/include/formats/video_hw_frame.h \
            $(RA)/libretro-common/include/formats/rh265_hw.h

TARGETS := dxva_h264_test$(EXE) dxva_hevc_test$(EXE) dxva_hevc_access_unit_test$(EXE)
TARGETS += dxva_vp9_test$(EXE)
TARGETS += dxva_av1_test$(EXE)
TARGETS += rh265_slice_snapshot_test$(EXE)
TARGETS += rh265_hw_state_test$(EXE)
TARGETS += rh265_hw_session_test$(EXE)
TARGETS += rh265_hw_stream_test$(EXE)
TARGETS += video_hw_format_test$(EXE) hevc_device_fixture_test$(EXE) hevc_color_stream_test$(EXE)
ifeq ($(EXE),.exe)
TARGETS += d3d11_video_decode_mock_test$(EXE) d3d11_video_decode_test$(EXE)
TARGETS += d3d11_backpressure_test$(EXE)
TARGETS += av1_decode_test$(EXE)
TARGETS += vulkan_win32_import_test$(EXE)
LDFLAGS += -lwinmm
WINFLAGS := -isystem $(RA)/gfx/include/dxsdk -Wno-cast-function-type -DHAVE_RVP9
PROBE_LIBS := -ld3d11 -ldxgi -ldxguid
endif

all: $(TARGETS)

vulkan_win32_import_test$(EXE): vulkan_win32_import_test.c $(RA)/gfx/drivers/vulkan_win32_video.h
	$(CC) $(CFLAGS) $(WINFLAGS) -I$(RA)/gfx/include -o $@ $< $(LDFLAGS)

av1_profile_probe$(EXE): av1_profile_probe.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(PROBE_LIBS)

dxva_av1_test$(EXE): dxva_av1_test.c $(RA)/gfx/common/dxva_av1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

AV1_CORE_DEPS := $(RA)/libretro-common/compat/compat_strl.c \
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

ifeq ($(EXE),.exe)
av1_decode_test$(EXE): av1_decode_test.c av1_core_decode.h $(RA)/cores/libretro-media/media_core.c $(AV1_CORE_DEPS) d3d11_video_decode_test.c $(BACKEND) \
      $(RA)/gfx/common/dxva_av1.h $(DEC_OBJS) $(PLAYER_SOURCES) \
      $(RA)/libretro-common/formats/av1/rav1_hw.c
	$(CC) $(CFLAGS) $(WINFLAGS) -DHAVE_RAV1 -DHAVE_RWEBM -flto -ffunction-sections -fdata-sections -o $@ \
	   $< $(DEC_OBJS) $(PLAYER_SOURCES) $(filter-out $(PLAYER_SOURCES),$(AV1_CORE_DEPS)) $(RA)/libretro-common/formats/vp9/rvp9.c \
	   $(RA)/libretro-common/formats/av1/rav1_hw.c \
	   $(LDFLAGS) -Wl,--gc-sections -ld3d11 -lm
else
av1_decode_test: av1_decode_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
endif

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


dxva_h264_test$(EXE): dxva_h264_test.c $(DEC_OBJS) $(DEPS)
	$(CC) $(CFLAGS) -o $@ dxva_h264_test.c $(DEC_OBJS) $(LDFLAGS) -lm

d3d11_video_decode_mock_test$(EXE): d3d11_video_decode_mock_test.c \
      $(DEC_OBJS) $(DEPS) $(BACKEND) dxva_hevc_access_unit_data.h \
      $(RA)/libretro-common/formats/vp9/rvp9.c \
      $(RA)/libretro-common/formats/h265/rh265.c
	$(CC) $(CFLAGS) $(WINFLAGS) -o $@ d3d11_video_decode_mock_test.c \
	   $(DEC_OBJS) $(RA)/libretro-common/formats/h265/rh265.c \
	   $(RA)/libretro-common/formats/vp9/rvp9.c $(LDFLAGS) -lm

d3d11_video_decode_test$(EXE): d3d11_video_decode_test.c \
      ../video_fixture_stream.h $(RA)/libretro-common/formats/mp4/rmp4_video.c \
      $(WEBM_DECODERS) \
      $(DEC_OBJS) $(DEPS) $(BACKEND) hevc_device_data.h \
      $(RA)/libretro-common/formats/vp9/rvp9.c $(RA)/libretro-common/formats/vp9/rvp9_recon.inc \
      $(RA)/libretro-common/formats/h265/rh265.c
	$(CC) $(CFLAGS) $(WINFLAGS) -o $@ d3d11_video_decode_test.c \
	   $(DEC_OBJS) $(RA)/libretro-common/formats/h265/rh265.c \
	   $(RA)/libretro-common/formats/vp9/rvp9.c $(WEBM_DECODERS) \
	   $(RA)/libretro-common/formats/mp4/rmp4_video.c $(LDFLAGS) -ld3d11 -lm

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
      $(DEC_OBJS) $(PLAYER_SOURCES) $(RA)/libretro-common/include/formats/rh265_hw.h \
      $(RA)/libretro-common/include/formats/rh265.h \
      $(RA)/libretro-common/include/formats/rmp4_video.h \
      $(RA)/samples/formats/video_hdr_fp16/fixtures/sdr_hevc.mp4
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -o $@ \
	   rh265_hw_stream_test.c $(DEC_OBJS) \
	   $(filter-out $(RA)/libretro-common/formats/h265/rh265.c,$(PLAYER_SOURCES)) \
	   $(LDFLAGS) -Wl,--gc-sections -lm

hevc_color_stream_test$(EXE): hevc_color_stream_test.c hevc_color_data.h \
      rh265_hw_stream_test.c $(DEC_OBJS) $(PLAYER_SOURCES)
	$(CC) $(CFLAGS) -ffunction-sections -fdata-sections -o $@ \
	   hevc_color_stream_test.c $(DEC_OBJS) \
	   $(filter-out $(RA)/libretro-common/formats/h265/rh265.c $(RA)/libretro-common/formats/mp4/rmp4_video.c,$(PLAYER_SOURCES)) \
	   $(LDFLAGS) -Wl,--gc-sections -lm

d3d11_backpressure_test$(EXE): d3d11_backpressure_test.c \
      d3d11_video_decode_test.c $(DEC_OBJS) $(DEPS) $(BACKEND) $(PLAYER_SOURCES) \
      $(RA)/libretro-common/formats/vp9/rvp9.c $(RA)/libretro-common/formats/vp9/rvp9_recon.inc
	$(CC) $(CFLAGS) $(WINFLAGS) -ffunction-sections -fdata-sections -o $@ \
	   d3d11_backpressure_test.c $(DEC_OBJS) $(PLAYER_SOURCES) \
	   $(RA)/libretro-common/formats/vp9/rvp9.c \
	   $(LDFLAGS) -Wl,--gc-sections $(if $(filter .exe,$(EXE)),-ld3d11) -lm

check: all
	$(RUNNER) ./dxva_av1_test$(EXE)
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
	$(RUNNER) ./vulkan_win32_import_test$(EXE)
endif

clean:
	rm -rf obj
	rm -f av1_decode_test av1_decode_test.exe
	rm -f dxva_av1_test dxva_av1_test.exe
	rm -f av1_profile_probe av1_profile_probe.exe
	rm -f dxva_vp9_test dxva_vp9_test.exe
	rm -f dxva_hevc_test dxva_hevc_test.exe
	rm -f dxva_hevc_access_unit_test dxva_hevc_access_unit_test.exe
	rm -f rh265_hw_state_test rh265_hw_state_test.exe
	rm -f rh265_hw_session_test rh265_hw_session_test.exe
	rm -f rh265_hw_stream_test rh265_hw_stream_test.exe
	rm -f rh265_slice_snapshot_test rh265_slice_snapshot_test.exe
	rm -f hevc_device_fixture_test hevc_device_fixture_test.exe
	rm -f hevc_color_stream_test hevc_color_stream_test.exe
	rm -f video_hw_format_test video_hw_format_test.exe
	rm -f dxva_h264_test dxva_h264_test.exe \
	   d3d11_video_decode_mock_test.exe d3d11_video_decode_test.exe
	rm -f d3d11_backpressure_test d3d11_backpressure_test.exe
	rm -f vulkan_win32_import_test vulkan_win32_import_test.exe

matroska-check: d3d11_video_decode_test$(EXE)
	python3 ../vulkan_video_caps/check_matroska.py --d3d11-test ./d3d11_video_decode_test$(EXE) --codec $(or $(MATROSKA_CODEC),both)

.PHONY: all check matroska-check clean

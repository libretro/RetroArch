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
CFLAGS ?= -O1 -g
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
BACKEND  := $(RA)/gfx/common/d3d11_video_decode.c \
            $(RA)/gfx/common/d3d11_video_decode.h frontend_stubs.h

TARGETS := dxva_h264_test$(EXE)
ifeq ($(EXE),.exe)
TARGETS += d3d11_video_decode_mock_test$(EXE) d3d11_video_decode_test$(EXE)
WINFLAGS := -isystem $(RA)/gfx/include/dxsdk -Wno-cast-function-type
endif

all: $(TARGETS)

dxva_h264_test$(EXE): dxva_h264_test.c $(DECODERS) $(DEPS)
	$(CC) $(CFLAGS) -o $@ dxva_h264_test.c $(DECODERS) $(LDFLAGS) -lm

d3d11_video_decode_mock_test$(EXE): d3d11_video_decode_mock_test.c \
      $(DECODERS) $(DEPS) $(BACKEND)
	$(CC) $(CFLAGS) $(WINFLAGS) -o $@ d3d11_video_decode_mock_test.c \
	   $(DECODERS) $(LDFLAGS) -lm

d3d11_video_decode_test$(EXE): d3d11_video_decode_test.c \
      $(DECODERS) $(DEPS) $(BACKEND)
	$(CC) $(CFLAGS) $(WINFLAGS) -o $@ d3d11_video_decode_test.c \
	   $(DECODERS) $(LDFLAGS) -ld3d11 -lm

check: all
	$(RUNNER) ./dxva_h264_test$(EXE)
ifeq ($(EXE),.exe)
	$(RUNNER) ./d3d11_video_decode_mock_test$(EXE)
	$(RUNNER) ./d3d11_video_decode_test$(EXE)
endif

clean:
	rm -f dxva_h264_test dxva_h264_test.exe \
	   d3d11_video_decode_mock_test.exe d3d11_video_decode_test.exe

.PHONY: all check clean

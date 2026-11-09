CC ?= cc
RA := ../../..
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long -Wframe-larger-than=2048 -I$(RA)/libretro-common/include -I$(RA)/gfx/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: params_test$(EXE) session_test$(EXE) picture_test$(EXE) submit_test$(EXE) pool_test$(EXE) encoder_test$(EXE) worker_test$(EXE) capture_test$(EXE) capture_shader_test$(EXE)
params_test$(EXE): params_test.c $(RA)/gfx/common/vulkan_encode_h264_params.h $(RA)/gfx/include/vk_video/vulkan_video_codec_h264std.h
	$(CC) $(CFLAGS) params_test.c -o $@ $(LDFLAGS)
session_test$(EXE): session_test.c $(RA)/gfx/common/vulkan_encode_h264_session.c $(RA)/gfx/common/vulkan_encode_h264_session.h $(RA)/gfx/common/vulkan_encode_h264_params.h
	$(CC) $(CFLAGS) session_test.c -o $@ $(LDFLAGS)
picture_test$(EXE): picture_test.c $(RA)/gfx/common/vulkan_encode_h264_picture.h $(RA)/gfx/include/vk_video/vulkan_video_codec_h264std_encode.h
	$(CC) $(CFLAGS) picture_test.c -o $@ $(LDFLAGS)
submit_test$(EXE): submit_test.c $(RA)/gfx/common/vulkan_encode_h264_submit.c $(RA)/gfx/common/vulkan_encode_h264_submit.h $(RA)/gfx/common/vulkan_encode_h264_picture.h $(RA)/gfx/common/vulkan_encode_h264_session.h $(RA)/gfx/common/vulkan_encode_h264_params.h $(RA)/gfx/include/vk_video/vulkan_video_codec_h264std_encode.h
	$(CC) $(CFLAGS) submit_test.c -o $@ $(LDFLAGS)
pool_test$(EXE): pool_test.c $(RA)/gfx/common/vulkan_encode_pool.c $(RA)/gfx/common/vulkan_encode_pool.h $(RA)/gfx/include/vulkan/vulkan.h
	$(CC) $(CFLAGS) pool_test.c -o $@ $(LDFLAGS)
encoder_test$(EXE): encoder_test.c $(RA)/gfx/common/vulkan_encode_h264.c $(RA)/gfx/common/vulkan_encode_h264.h $(RA)/gfx/common/vulkan_encode_pool.h $(RA)/gfx/common/vulkan_encode_h264_submit.h $(RA)/gfx/common/vulkan_encode_h264_picture.h $(RA)/gfx/common/vulkan_encode_h264_session.h $(RA)/gfx/common/vulkan_encode_h264_params.h
	$(CC) $(CFLAGS) encoder_test.c -o $@ $(LDFLAGS)
worker_test$(EXE): worker_test.c $(RA)/record/record_gpu_worker_vulkan.c $(RA)/record/record_gpu_worker_vulkan.h $(RA)/record/record_gpu_queue.c $(RA)/record/record_gpu_queue.h $(RA)/libretro-common/queues/retro_spsc.c $(RA)/gfx/common/vulkan_encode_h264.h $(RA)/gfx/common/vulkan_encode_pool.h $(RA)/gfx/common/vulkan_encode_h264_submit.h $(RA)/gfx/common/vulkan_encode_h264_picture.h $(RA)/gfx/common/vulkan_encode_h264_session.h $(RA)/gfx/common/vulkan_encode_h264_params.h
	$(CC) $(CFLAGS) worker_test.c $(RA)/record/record_gpu_worker_vulkan.c $(RA)/record/record_gpu_queue.c $(RA)/libretro-common/queues/retro_spsc.c -o $@ $(LDFLAGS)
CAPTURE_HEADERS := $(RA)/gfx/common/vulkan_encode_capture.h $(RA)/gfx/common/vulkan_encode_pool.h
CAPTURE_SHADER_SOURCES := capture_shader_test.c $(RA)/gfx/common/vulkan_encode_capture_shader.c \
   $(RA)/libretro-common/gfx/shaders/rslc_front.c \
   $(RA)/libretro-common/gfx/shaders/rslc_ir.c \
   $(RA)/libretro-common/gfx/shaders/rslc_emit.c
capture_test$(EXE): capture_test.c $(RA)/gfx/common/vulkan_encode_capture.c $(CAPTURE_HEADERS)
	$(CC) $(CFLAGS) capture_test.c -o $@ $(LDFLAGS)
capture_shader_test$(EXE): $(CAPTURE_SHADER_SOURCES) $(CAPTURE_HEADERS) $(RA)/libretro-common/include/gfx/shaders/rslc.h $(RA)/libretro-common/gfx/shaders/rslc_share.h
	$(CC) $(CFLAGS) -DRSLC_EXTENDED -DRSLC_NO_DXBC $(CAPTURE_SHADER_SOURCES) -o $@ $(LDFLAGS) -lm
check: all
	$(RUNNER) ./params_test$(EXE)
	$(RUNNER) ./session_test$(EXE)
	$(RUNNER) ./picture_test$(EXE)
	$(RUNNER) ./submit_test$(EXE)
	$(RUNNER) ./pool_test$(EXE)
	$(RUNNER) ./encoder_test$(EXE)
	$(RUNNER) ./worker_test$(EXE)
	$(RUNNER) ./capture_test$(EXE)
	$(RUNNER) ./capture_shader_test$(EXE)
clean:
	rm -f params_test params_test.exe session_test session_test.exe picture_test picture_test.exe submit_test submit_test.exe pool_test pool_test.exe encoder_test encoder_test.exe worker_test worker_test.exe capture_test capture_test.exe capture_shader_test capture_shader_test.exe
.PHONY: all check clean

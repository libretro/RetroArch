CC ?= cc
RA := ../../..
CFLAGS += -DHAVE_THREADS -O1 -g1 -std=gnu89 -Wall -Wextra -Werror=declaration-after-statement -I$(RA)/libretro-common/include
SRC := $(RA)/libretro-common/formats/ac3/rac3_frame.c $(RA)/libretro-common/formats/ac3/rac3_encode.c $(RA)/libretro-common/encodings/encoding_utf.c $(RA)/libretro-common/compat/compat_strl.c device_test.c $(RA)/record/record_gpu_queue.c $(RA)/libretro-common/queues/retro_spsc.c
WORKER := $(RA)/record/record_gpu_worker_d3d12.c $(RA)/record/record_gpu_worker_d3d12.h
CAPTURE := $(RA)/record/record_gpu_capture_d3d12.h $(RA)/record/record_gpu_capture_d3d12.c $(RA)/record/shaders/capture_rgb.h $(RA)/record/shaders/capture_hdr.h
MUX := $(RA)/record/record_mux_mp4.c $(RA)/record/record_mux_mp4.h
OUTPUT := $(RA)/record/record_gpu_dispatch_d3d12.c $(RA)/record/record_gpu_dispatch_d3d12.h $(RA)/record/record_gpu_session_d3d12.c $(RA)/record/record_gpu_session_d3d12.h output_pipeline.h $(RA)/record/record_gpu_output_d3d12.c $(RA)/record/record_gpu_output_d3d12.h $(RA)/record/record_file_win32.c
METADATA := $(RA)/libretro-common/include/formats/video_color.h
TARGET := device_test$(EXE)
SDK_TARGET := device_test_sdk$(EXE)
LRC := $(RA)/libretro-common
STREAM_NET := $(wildcard $(LRC)/net/net_rtmp_*.c) $(LRC)/net/net_resolve.c $(LRC)/net/net_socket.c $(LRC)/net/net_compat.c
STREAM_COMMON := $(LRC)/features/features_cpu.c $(LRC)/string/stdstring.c $(LRC)/string/rstrtod.c $(LRC)/compat/compat_posix_string.c $(LRC)/time/rtime.c $(LRC)/rthreads/rthreads.c $(LRC)/rthreads/retro_eventcount.c $(LRC)/crypto/kdf.c $(LRC)/formats/flv/rflv_mux.c
STREAM_COMMON += $(LRC)/crypto/crypto.c $(LRC)/hash/lrc_hash.c $(LRC)/utils/md5.c
STREAM_COMMON += $(LRC)/net/net_socket_ssl_retro.c $(LRC)/crypto/pk.c $(LRC)/crypto/x509.c $(LRC)/encodings/encoding_base64.c $(LRC)/streams/file_stream.c $(LRC)/vfs/vfs_implementation.c $(LRC)/file/file_path.c $(LRC)/file/file_path_io.c $(LRC)/compat/fopen_utf8.c
all: hdr_capture_test$(EXE) hdr_capture_test_sdk$(EXE) $(TARGET) $(SDK_TARGET) worker_test$(EXE) worker_threadless$(EXE) swapchain_test$(EXE) swapchain_test_sdk$(EXE)
ifeq ($(EXE),.exe)
all: stream_device_test.exe
endif
stream_device_test.exe: $(SRC) $(CAPTURE) $(WORKER) $(MUX) $(OUTPUT) $(METADATA) output_pipeline.h $(RA)/record/record_stream_d3d12.c $(RA)/record/record_mf_audio.c $(STREAM_NET) $(STREAM_COMMON)
	$(CC) $(CFLAGS) -D_WIN32_WINNT=0x0601 -DNVENC_TEST_STREAM -DHAVE_D3D12 -DHAVE_MF_AUDIO -DHAVE_RTMP -DHAVE_RFLV -DHAVE_CRYPTO -DHAVE_SSL -DHAVE_RETROSSL -flto -fwhole-program -ffunction-sections -fdata-sections -o $@ $(SRC) $(RA)/record/record_mf_audio.c $(STREAM_NET) $(STREAM_COMMON) $(LDFLAGS) -flto -Wl,--gc-sections -lm -ld3d12 -ldxguid -lws2_32
$(TARGET): $(SRC) $(CAPTURE) $(WORKER) $(MUX) $(OUTPUT) $(METADATA) $(RA)/record/record_gpu_queue.h $(RA)/gfx/common/nvenc_d3d12.c $(RA)/gfx/common/nvenc_d3d12.h $(RA)/deps/nvenc/nvEncodeAPI.h
	$(CC) $(CFLAGS) -o $@ $(SRC) $(LDFLAGS) -lm $(if $(filter .exe,$(EXE)),-ld3d12 -ldxguid,)
$(SDK_TARGET): $(SRC) $(CAPTURE) $(WORKER) $(MUX) $(OUTPUT) $(METADATA) $(RA)/record/record_gpu_queue.h $(RA)/gfx/common/nvenc_d3d12.c $(RA)/gfx/common/nvenc_d3d12.h $(RA)/deps/nvenc/nvEncodeAPI.h
	$(CC) $(CFLAGS) -I$(RA)/gfx/include/dxsdk -o $@ $(SRC) $(LDFLAGS) -lm $(if $(filter .exe,$(EXE)),-ld3d12 -ldxguid,)
worker_test$(EXE): worker_test.c $(WORKER) $(RA)/record/record_gpu_queue.c $(RA)/record/record_gpu_queue.h $(RA)/libretro-common/queues/retro_spsc.c
	$(CC) $(CFLAGS) -o $@ worker_test.c $(RA)/record/record_gpu_worker_d3d12.c $(RA)/record/record_gpu_queue.c $(RA)/libretro-common/queues/retro_spsc.c $(LDFLAGS)
worker_threadless$(EXE): worker_test.c $(WORKER) $(RA)/record/record_gpu_queue.c $(RA)/record/record_gpu_queue.h $(RA)/libretro-common/queues/retro_spsc.c
	$(CC) $(CFLAGS) -UHAVE_THREADS -o $@ worker_test.c $(RA)/record/record_gpu_worker_d3d12.c $(RA)/record/record_gpu_queue.c $(RA)/libretro-common/queues/retro_spsc.c $(LDFLAGS)
SWAPCHAIN := swapchain_test.c $(RA)/record/record_gpu_capture_d3d12.c $(RA)/record/record_gpu_queue.c $(RA)/libretro-common/queues/retro_spsc.c
swapchain_test$(EXE): $(SWAPCHAIN) $(CAPTURE)
	$(CC) $(CFLAGS) -o $@ $(SWAPCHAIN) $(LDFLAGS) $(if $(filter .exe,$(EXE)),-ld3d12 -ldxgi -ldxguid -luser32,)
swapchain_test_sdk$(EXE): $(SWAPCHAIN) $(CAPTURE)
	$(CC) $(CFLAGS) -I$(RA)/gfx/include/dxsdk -o $@ $(SWAPCHAIN) $(LDFLAGS) $(if $(filter .exe,$(EXE)),-ld3d12 -ldxgi -ldxguid -luser32,)
HDR := hdr_capture_test.c $(RA)/record/record_gpu_worker_d3d12.c $(RA)/record/record_gpu_queue.c $(RA)/libretro-common/queues/retro_spsc.c $(RA)/gfx/common/nvenc_d3d12.c
HDR_DEPS := $(CAPTURE) $(WORKER) $(METADATA) $(RA)/record/record_gpu_queue.h $(RA)/gfx/common/nvenc_d3d12.h $(RA)/deps/nvenc/nvEncodeAPI.h
hdr_capture_test$(EXE): $(HDR) $(HDR_DEPS)
	$(CC) $(CFLAGS) -o $@ $(HDR) $(LDFLAGS) -lm $(if $(filter .exe,$(EXE)),-ld3d12 -ldxguid,)
hdr_capture_test_sdk$(EXE): $(HDR) $(HDR_DEPS)
	$(CC) $(CFLAGS) -I$(RA)/gfx/include/dxsdk -o $@ $(HDR) $(LDFLAGS) -lm $(if $(filter .exe,$(EXE)),-ld3d12 -ldxguid,)
check: all
	python3 hdr_capture.py --runner "$(RUNNER)" ./hdr_capture_test$(EXE)
	python3 hdr_capture.py --runner "$(RUNNER)" ./hdr_capture_test_sdk$(EXE)
	python3 $(RA)/record/shaders/generate_hdr.py --check
	python3 swapchain.py --runner "$(RUNNER)" ./swapchain_test$(EXE)
	python3 swapchain.py --runner "$(RUNNER)" ./swapchain_test_sdk$(EXE)
	$(RUNNER) ./worker_test$(EXE)
	$(RUNNER) ./worker_threadless$(EXE)
	python3 $(RA)/record/shaders/generate_rgb.py --check
	python3 run.py --runner "$(RUNNER)" ./$(TARGET)
	python3 run.py --runner "$(RUNNER)" ./$(SDK_TARGET)
clean:
	rm -f stream_device_test.exe
	rm -f hdr_capture_test hdr_capture_test.exe hdr_capture_test_sdk hdr_capture_test_sdk.exe swapchain_test swapchain_test.exe swapchain_test_sdk swapchain_test_sdk.exe device_test device_test.exe device_test_sdk device_test_sdk.exe worker_test worker_test.exe worker_threadless worker_threadless.exe
.PHONY: all check clean

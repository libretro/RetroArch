CC ?= cc
RA := ../../..
CFLAGS += -std=gnu89 -O1 -g1 -Wall -Wextra -Werror=declaration-after-statement -I$(RA)/libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
COMMON := mock_worker.c $(RA)/record/record_mux_mp4.c $(RA)/libretro-common/formats/ac3/rac3_frame.c $(RA)/libretro-common/formats/ac3/rac3_encode.c $(RA)/libretro-common/queues/retro_spsc.c
OUTPUT := $(RA)/record/record_gpu_output_d3d12.c $(RA)/record/record_gpu_output_d3d12.h
FILE := file_backend.c $(RA)/record/record_file_win32.c $(RA)/libretro-common/encodings/encoding_utf.c $(RA)/libretro-common/compat/compat_strl.c
all: output_test$(EXE) pipeline_file$(EXE) thread_test$(EXE) session_test$(EXE) session_threadless$(EXE) vulkan_file_test$(EXE) vulkan_output_test$(EXE) vulkan_thread_test$(EXE) vulkan_native_file_test$(EXE)
output_test$(EXE): output_test.c mock_file.c mock_file.h mock_worker.h $(COMMON) $(OUTPUT)
	$(CC) $(CFLAGS) -o $@ output_test.c mock_file.c $(COMMON) $(LDFLAGS) -lm
pipeline_file$(EXE): pipeline_file.c $(COMMON) $(FILE) $(OUTPUT)
	$(CC) $(CFLAGS) -o $@ pipeline_file.c $(COMMON) file_backend.c $(RA)/record/record_gpu_output_d3d12.c $(RA)/libretro-common/encodings/encoding_utf.c $(RA)/libretro-common/compat/compat_strl.c $(LDFLAGS) -lm
thread_test$(EXE): thread_test.c mock_file.c mock_file.h mock_worker.h $(COMMON) $(OUTPUT)
	$(CC) $(CFLAGS) -DHAVE_THREADS $(if $(filter .exe,$(EXE)),,-pthread) -o $@ thread_test.c mock_file.c $(COMMON) $(LDFLAGS) -lm
SESSION := $(RA)/record/record_gpu_session_d3d12.c $(RA)/record/record_gpu_worker_d3d12.c $(RA)/record/record_gpu_queue.c $(filter-out mock_worker.c,$(COMMON))
SESSION_HEADERS := $(RA)/record/record_gpu_session_d3d12.h $(RA)/record/record_gpu_capture_d3d12.h $(RA)/record/record_gpu_worker_d3d12.h $(RA)/record/record_gpu_queue.h $(RA)/record/record_mux_mp4.h $(RA)/gfx/common/nvenc_d3d12.h
session_test$(EXE): session_test.c mock_file.c mock_file.h $(SESSION) $(OUTPUT) $(SESSION_HEADERS)
	$(CC) $(CFLAGS) -DHAVE_THREADS $(if $(filter .exe,$(EXE)),,-pthread) -o $@ session_test.c mock_file.c $(SESSION) $(RA)/record/record_gpu_output_d3d12.c $(LDFLAGS) -lm
session_threadless$(EXE): session_test.c mock_file.c mock_file.h $(SESSION) $(OUTPUT) $(SESSION_HEADERS)
	$(CC) $(CFLAGS) -UHAVE_THREADS -o $@ session_test.c mock_file.c $(SESSION) $(RA)/record/record_gpu_output_d3d12.c $(LDFLAGS) -lm
VULKAN_CFLAGS := -std=c89 -pedantic -Werror -Wno-long-long -Wframe-larger-than=2048 -I$(RA)/gfx/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
VULKAN_CFLAGS += -Wno-c99-extensions
endif
VULKAN_OUTPUT := $(RA)/record/record_gpu_output_vulkan.c $(RA)/record/record_gpu_output_vulkan.h
VULKAN_HEADERS := $(RA)/record/record_gpu_worker_vulkan.h $(RA)/record/record_gpu_queue.h $(RA)/record/record_mux_mp4.h $(RA)/gfx/common/vulkan_encode_h264.h $(RA)/gfx/common/vulkan_encode_pool.h $(RA)/gfx/common/vulkan_encode_h264_submit.h $(RA)/gfx/common/vulkan_encode_h264_picture.h $(RA)/gfx/common/vulkan_encode_h264_session.h $(RA)/gfx/common/vulkan_encode_h264_params.h
VULKAN_FILE := $(RA)/record/record_file_posix.c $(RA)/record/record_file_posix.h $(RA)/record/record_mediacodec_file.c $(RA)/record/record_mediacodec_file.h
VULKAN_COMMON := vulkan_mock_file.c vulkan_mock_file.h vulkan_mock_worker.c vulkan_mock_worker.h $(filter-out mock_worker.c,$(COMMON))
vulkan_file_test$(EXE): vulkan_file_test.c $(VULKAN_FILE)
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) vulkan_file_test.c -o $@ $(LDFLAGS)
vulkan_native_file_test$(EXE): vulkan_native_file_test.c $(VULKAN_FILE)
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) -D_POSIX_C_SOURCE=200809L $(if $(filter .exe,$(EXE)),-DRECORD_FILE_POSIX_TEST -DRECORD_MEDIACODEC_FILE_TEST,) vulkan_native_file_test.c $(RA)/record/record_file_posix.c $(RA)/record/record_mediacodec_file.c -o $@ $(LDFLAGS)
vulkan_output_test$(EXE): vulkan_output_test.c $(VULKAN_OUTPUT) $(VULKAN_FILE) $(VULKAN_COMMON) $(VULKAN_HEADERS)
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) vulkan_output_test.c $(filter %.c,$(VULKAN_COMMON)) -o $@ $(LDFLAGS) -lm
vulkan_thread_test$(EXE): vulkan_thread_test.c $(VULKAN_OUTPUT) $(VULKAN_FILE) $(VULKAN_COMMON) $(VULKAN_HEADERS)
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) -D_POSIX_C_SOURCE=200809L -DHAVE_THREADS $(if $(filter .exe,$(EXE)),,-pthread) vulkan_thread_test.c $(filter %.c,$(VULKAN_COMMON)) -o $@ $(LDFLAGS) -lm
check: all
	$(RUNNER) ./vulkan_file_test$(EXE)
	$(RUNNER) ./vulkan_output_test$(EXE)
	$(RUNNER) ./vulkan_thread_test$(EXE)
	python3 run_vulkan_native_file.py "$(RUNNER)" ./vulkan_native_file_test$(EXE)
	$(RUNNER) ./output_test$(EXE)
	$(RUNNER) ./thread_test$(EXE)
	$(RUNNER) ./session_test$(EXE)
	$(RUNNER) ./session_threadless$(EXE)
	python3 ../mp4_mux/run.py --runner "$(RUNNER)" ./pipeline_file$(EXE)
clean:
	rm -f session_threadless session_threadless.exe session_test session_test.exe output_test output_test.exe thread_test thread_test.exe pipeline_file pipeline_file.exe vulkan_file_test vulkan_file_test.exe vulkan_output_test vulkan_output_test.exe vulkan_thread_test vulkan_thread_test.exe vulkan_native_file_test vulkan_native_file_test.exe
.PHONY: all check clean

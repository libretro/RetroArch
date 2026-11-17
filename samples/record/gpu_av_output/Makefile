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
all: output_test$(EXE) pipeline_file$(EXE) thread_test$(EXE) session_test$(EXE) session_threadless$(EXE) vulkan_file_test$(EXE) vulkan_output_test$(EXE) vulkan_thread_test$(EXE) vulkan_native_file_test$(EXE) vulkan_session_test$(EXE) vulkan_session_thread_test$(EXE)
all: vulkan_dispatch_test$(EXE) vulkan_dispatch_threadless$(EXE) vulkan_dispatch_fallback$(EXE)
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
VULKAN_HEADERS += $(RA)/gfx/common/vulkan_encode_packet.h
vulkan_file_test$(EXE): vulkan_file_test.c $(VULKAN_FILE)
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) vulkan_file_test.c -o $@ $(LDFLAGS)
vulkan_native_file_test$(EXE): vulkan_native_file_test.c $(VULKAN_FILE)
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) -D_POSIX_C_SOURCE=200809L $(if $(filter .exe,$(EXE)),-DRECORD_FILE_POSIX_TEST -DRECORD_MEDIACODEC_FILE_TEST,) vulkan_native_file_test.c $(RA)/record/record_file_posix.c $(RA)/record/record_mediacodec_file.c -o $@ $(LDFLAGS)
vulkan_output_test$(EXE): vulkan_output_test.c $(VULKAN_OUTPUT) $(VULKAN_FILE) $(VULKAN_COMMON) $(VULKAN_HEADERS)
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) vulkan_output_test.c $(filter %.c,$(VULKAN_COMMON)) -o $@ $(LDFLAGS) -lm
vulkan_thread_test$(EXE): vulkan_thread_test.c $(VULKAN_OUTPUT) $(VULKAN_FILE) $(VULKAN_COMMON) $(VULKAN_HEADERS)
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) -D_POSIX_C_SOURCE=200809L -DHAVE_THREADS $(if $(filter .exe,$(EXE)),,-pthread) vulkan_thread_test.c $(filter %.c,$(VULKAN_COMMON)) -o $@ $(LDFLAGS) -lm
VULKAN_SESSION := $(RA)/record/record_gpu_session_vulkan.c $(RA)/record/record_gpu_session_vulkan.h $(RA)/record/record_gpu_session_vulkan_backend.h
VULKAN_SESSION_SOURCES := vulkan_session_test.c vulkan_mock_file.c $(RA)/record/record_gpu_worker_vulkan.c $(RA)/record/record_gpu_queue.c $(RA)/record/record_mux_mp4.c $(RA)/libretro-common/formats/ac3/rac3_frame.c $(RA)/libretro-common/formats/ac3/rac3_encode.c $(RA)/libretro-common/queues/retro_spsc.c
vulkan_session_test$(EXE): $(VULKAN_SESSION_SOURCES) $(VULKAN_SESSION) $(VULKAN_HEADERS) $(VULKAN_OUTPUT) $(VULKAN_COMMON)
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) -UHAVE_THREADS $(VULKAN_SESSION_SOURCES) -o $@ $(LDFLAGS) -lm
vulkan_session_thread_test$(EXE): $(VULKAN_SESSION_SOURCES) $(VULKAN_SESSION) $(VULKAN_HEADERS) $(VULKAN_OUTPUT) $(VULKAN_COMMON)
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) -DHAVE_THREADS -D_POSIX_C_SOURCE=200809L $(if $(filter .exe,$(EXE)),,-pthread) $(VULKAN_SESSION_SOURCES) -o $@ $(LDFLAGS) -lm
VULKAN_DISPATCH := $(RA)/record/record_gpu_dispatch_vulkan.c $(RA)/record/record_gpu_dispatch_vulkan.h $(RA)/record/record_gpu_dispatch_vulkan_backend.h $(RA)/gfx/common/vulkan_encode_capture.h
VULKAN_DISPATCH_SOURCES := vulkan_dispatch_test.c $(filter-out vulkan_session_test.c,$(VULKAN_SESSION_SOURCES)) $(RA)/record/record_gpu_dispatch_vulkan.c
vulkan_dispatch_test$(EXE): $(VULKAN_DISPATCH_SOURCES) $(VULKAN_DISPATCH) vulkan_session_test.c $(VULKAN_SESSION) $(VULKAN_HEADERS) $(VULKAN_OUTPUT) rthreads_compat.c $(RA)/libretro-common/rthreads/rthreads.c $(RA)/libretro-common/rthreads/retro_eventcount.c
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) -DHAVE_THREADS -D_POSIX_C_SOURCE=200809L $(if $(filter .exe,$(EXE)),,-pthread) $(VULKAN_DISPATCH_SOURCES) rthreads_compat.c $(RA)/libretro-common/rthreads/retro_eventcount.c -o $@ $(LDFLAGS) -lm
vulkan_dispatch_threadless$(EXE): $(VULKAN_DISPATCH_SOURCES) $(VULKAN_DISPATCH) vulkan_session_test.c $(VULKAN_SESSION) $(VULKAN_HEADERS) $(VULKAN_OUTPUT)
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) -UHAVE_THREADS $(VULKAN_DISPATCH_SOURCES) -o $@ $(LDFLAGS) -lm
vulkan_dispatch_fallback$(EXE): $(VULKAN_DISPATCH_SOURCES) $(VULKAN_DISPATCH) vulkan_session_test.c $(VULKAN_SESSION) $(VULKAN_HEADERS) $(VULKAN_OUTPUT) rthreads_compat.c
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) -DHAVE_THREADS -DRETRO_EVENTCOUNT_FORCE_SCOND -D_POSIX_C_SOURCE=200809L $(if $(filter .exe,$(EXE)),,-pthread) $(VULKAN_DISPATCH_SOURCES) rthreads_compat.c $(RA)/libretro-common/rthreads/retro_eventcount.c -o $@ $(LDFLAGS) -lm
check: all
	$(RUNNER) ./vulkan_hevc_dispatch_factory_test$(EXE)
	$(RUNNER) ./vulkan_hevc_dispatch_threadless_test$(EXE)
	$(RUNNER) ./vulkan_hevc_session_test$(EXE)
	$(RUNNER) ./vulkan_hevc_session_thread_test$(EXE)
	$(RUNNER) ./vulkan_dispatch_test$(EXE)
	$(RUNNER) ./vulkan_dispatch_threadless$(EXE)
	$(RUNNER) ./vulkan_dispatch_fallback$(EXE)
	$(RUNNER) ./vulkan_session_test$(EXE)
	$(RUNNER) ./vulkan_session_thread_test$(EXE)
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
	rm -f vulkan_hevc_dispatch_factory_test vulkan_hevc_dispatch_factory_test.exe vulkan_hevc_dispatch_threadless_test vulkan_hevc_dispatch_threadless_test.exe
	rm -f vulkan_hevc_session_test vulkan_hevc_session_test.exe vulkan_hevc_session_thread_test vulkan_hevc_session_thread_test.exe
	rm -f vulkan_dispatch_test vulkan_dispatch_test.exe vulkan_dispatch_threadless vulkan_dispatch_threadless.exe
	rm -f vulkan_dispatch_fallback vulkan_dispatch_fallback.exe
	rm -f vulkan_session_test vulkan_session_test.exe vulkan_session_thread_test vulkan_session_thread_test.exe
	rm -f session_threadless session_threadless.exe session_test session_test.exe output_test output_test.exe thread_test thread_test.exe pipeline_file pipeline_file.exe vulkan_file_test vulkan_file_test.exe vulkan_output_test vulkan_output_test.exe vulkan_thread_test vulkan_thread_test.exe vulkan_native_file_test vulkan_native_file_test.exe
.PHONY: all check clean

HEVC_SESSION_SOURCES := vulkan_hevc_session_test.c $(filter-out vulkan_session_test.c,$(VULKAN_SESSION_SOURCES)) $(RA)/record/record_gpu_worker_vulkan_hevc.c
vulkan_hevc_session_test$(EXE): $(HEVC_SESSION_SOURCES) vulkan_session_test.c $(VULKAN_SESSION) $(VULKAN_HEADERS) $(VULKAN_OUTPUT) $(VULKAN_COMMON) $(RA)/record/record_gpu_session_vulkan_hevc.c $(RA)/record/record_gpu_session_vulkan_hevc.h
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) -UHAVE_THREADS $(HEVC_SESSION_SOURCES) -o $@ $(LDFLAGS) -lm

vulkan_hevc_session_thread_test$(EXE): $(HEVC_SESSION_SOURCES) vulkan_session_test.c $(VULKAN_SESSION) $(VULKAN_HEADERS) $(VULKAN_OUTPUT) $(VULKAN_COMMON) $(RA)/record/record_gpu_session_vulkan_hevc.c $(RA)/record/record_gpu_session_vulkan_hevc.h
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) -DHAVE_THREADS -D_POSIX_C_SOURCE=200809L $(if $(filter .exe,$(EXE)),,-pthread) $(HEVC_SESSION_SOURCES) -o $@ $(LDFLAGS) -lm
all: vulkan_hevc_session_test$(EXE) vulkan_hevc_session_thread_test$(EXE)

HEVC_DISPATCH_HEADERS := $(RA)/record/record_gpu_dispatch_vulkan_backend.h $(RA)/record/record_gpu_dispatch_vulkan_hevc.h $(RA)/record/record_gpu_dispatch_vulkan_hevc.c
vulkan_hevc_dispatch_factory_test$(EXE): vulkan_hevc_dispatch_factory_test.c $(HEVC_DISPATCH_HEADERS)
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) $< -o $@ $(LDFLAGS)
vulkan_hevc_dispatch_threadless_test$(EXE): vulkan_hevc_dispatch_threadless_test.c $(VULKAN_DISPATCH) $(HEVC_DISPATCH_HEADERS)
	$(CC) $(CFLAGS) $(VULKAN_CFLAGS) -UHAVE_THREADS $< -o $@ $(LDFLAGS)
all: vulkan_hevc_dispatch_factory_test$(EXE) vulkan_hevc_dispatch_threadless_test$(EXE)

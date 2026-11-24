CC ?= cc
RA := ../../..
LRC := $(RA)/libretro-common
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wframe-larger-than=2048 -I$(LRC)/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions -Wno-c11-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
ifneq ($(findstring undefined,$(SANITIZER)),)
CFLAGS += -fno-sanitize-recover=undefined
endif
LDFLAGS += -fsanitize=$(SANITIZER)
endif
SESSION := $(LRC)/net/net_rtmp_session.c $(LRC)/net/net_rtmp_handshake.c $(LRC)/net/net_rtmp_chunk.c $(LRC)/net/net_rtmp_receive.c $(LRC)/net/net_rtmp_publish.c $(LRC)/net/net_rtmp_command.c $(LRC)/net/net_rtmp_reply.c $(LRC)/net/net_rtmp_control.c
SOURCES := $(RA)/record/record_stream_gpu.c $(RA)/record/record_aac_audio.c $(RA)/record/record_stream_d3d12.c ../gpu_av_output/mock_worker.c $(LRC)/queues/retro_spsc.c $(LRC)/formats/flv/rflv_mux.c $(SESSION)
GPU_HEADERS := $(RA)/record/record_aac_audio.h $(RA)/record/record_mf_audio.h $(RA)/record/record_stream_gpu.h $(RA)/record/record_gpu_output_config.h
all: aac_audio_threadless_test$(EXE) aac_stream_test$(EXE) aac_audio_test$(EXE) vulkan_pressure_test$(EXE) vulkan_session_test$(EXE) vulkan_output_test$(EXE) vulkan_mapping_test$(EXE) vulkan_contract_test$(EXE) contract_test$(EXE) wrapper_test$(EXE) vt_contract_test$(EXE) vt_output_test$(EXE)
contract_test$(EXE): contract_test.c $(SOURCES) $(GPU_HEADERS) $(RA)/record/record_stream_d3d12.h $(LRC)/samples/net/rtmp/rtmp_session_test.c
	$(CC) $(CFLAGS) -o $@ contract_test.c $(SOURCES) $(LDFLAGS)
FILE_OUTPUT := $(RA)/record/record_gpu_output_d3d12.c $(RA)/record/record_mux_mp4.c ../gpu_av_output/mock_file.c $(LRC)/formats/ac3/rac3_frame.c $(LRC)/formats/ac3/rac3_encode.c
wrapper_test$(EXE): wrapper_test.c contract_test.c $(SOURCES) $(FILE_OUTPUT) $(GPU_HEADERS)
	$(CC) $(CFLAGS) -DHAVE_D3D12 -DHAVE_MF_AUDIO -DHAVE_RTMP -DHAVE_RFLV -o $@ wrapper_test.c $(SOURCES) $(FILE_OUTPUT) $(LDFLAGS) -lm
check: all
	$(RUNNER) ./aac_audio_threadless_test$(EXE)
	python3 aac_oracle.py --runner "$(RUNNER)" ./aac_audio_test$(EXE) ./aac_stream_test$(EXE)
	$(RUNNER) ./aac_stream_test$(EXE)
	$(RUNNER) ./aac_audio_test$(EXE)
	$(RUNNER) ./vulkan_pressure_test$(EXE)
	$(RUNNER) ./vulkan_session_test$(EXE)
	$(RUNNER) ./vulkan_output_test$(EXE)
	$(RUNNER) ./vulkan_mapping_test$(EXE)
	python3 build_selection_test.py --make "$(MAKE)"
	$(RUNNER) ./vulkan_contract_test$(EXE)
	$(RUNNER) ./vt_output_test$(EXE)
	python3 vt_oracle.py --runner "$(RUNNER)" ./vt_contract_test$(EXE)
	$(RUNNER) ./contract_test$(EXE)
	$(RUNNER) ./wrapper_test$(EXE)
clean:
	rm -f aac_audio_threadless_test aac_audio_threadless_test.exe
	rm -f aac_stream_test aac_stream_test.exe
	rm -f aac_audio_test aac_audio_test.exe
	rm -f vulkan_pressure_test vulkan_pressure_test.exe
	rm -f vulkan_dispatch_stream_test.exe
	rm -f vulkan_session_test vulkan_session_test.exe
	rm -f vulkan_output_test vulkan_output_test.exe
	rm -f vulkan_mapping_test vulkan_mapping_test.exe
	rm -f vulkan_contract_test vulkan_contract_test.exe
	rm -f vt_output_test vt_output_test.exe
	rm -f vt_contract_test vt_contract_test.exe vt_thread_test
	rm -f contract_test contract_test.exe wrapper_test wrapper_test.exe
.PHONY: all check clean

VT_SOURCES := $(RA)/record/record_stream_vt.c $(RA)/record/record_audio_queue.c $(LRC)/queues/retro_spsc.c $(LRC)/formats/flv/rflv_mux.c $(SESSION)
VT_OUTPUT_SOURCES := $(RA)/record/record_stream_output_vt.c $(RA)/record/record_avfoundation_session.c $(RA)/record/record_avfoundation_worker.c $(RA)/record/record_gpu_queue.c
vt_output_test$(EXE): vt_output_test.c vt_contract_test.c vt_native_session_test.h $(VT_SOURCES) $(VT_OUTPUT_SOURCES) $(RA)/record/record_stream_output_vt.h $(RA)/record/record_stream_session_vt_native.h
	$(CC) $(CFLAGS) -Wno-unused-function -o $@ vt_output_test.c $(VT_OUTPUT_SOURCES) $(VT_SOURCES) $(LDFLAGS)
vt_contract_test$(EXE): vt_contract_test.c $(VT_SOURCES) $(RA)/record/record_stream_vt.h
	$(CC) $(CFLAGS) -o $@ vt_contract_test.c $(VT_SOURCES) $(LDFLAGS)
vt-check: vt_contract_test$(EXE) vt_output_test$(EXE)
	$(RUNNER) ./vt_output_test$(EXE)
	python3 vt_oracle.py --runner "$(RUNNER)" ./vt_contract_test$(EXE)
.PHONY: vt-check

vt_thread_test: vt_contract_test.c $(VT_SOURCES) $(RA)/record/record_stream_vt.h
	$(CC) $(CFLAGS) -pthread -DHAVE_THREADS -DVT_STREAM_THREAD_TEST -o $@ vt_contract_test.c $(VT_SOURCES) $(LDFLAGS)

vulkan_contract_test$(EXE): vulkan_contract_test.c contract_test.c $(SOURCES) $(GPU_HEADERS) $(RA)/record/record_stream_vulkan.c $(RA)/record/record_stream_vulkan.h
	$(CC) $(CFLAGS) -Wno-long-long -I$(RA)/gfx/include -o $@ vulkan_contract_test.c $(filter-out $(RA)/record/record_stream_d3d12.c,$(SOURCES)) $(RA)/record/record_stream_vulkan.c $(LDFLAGS)

vulkan_mapping_test$(EXE): vulkan_mapping_test.c $(GPU_HEADERS) $(RA)/record/record_stream_vulkan.c $(RA)/record/record_stream_vulkan.h
	$(CC) $(CFLAGS) -Wno-long-long -I$(RA)/gfx/include -o $@ $< $(LDFLAGS)

VULKAN_OUTPUT_SOURCES := $(RA)/record/record_gpu_output_vulkan.c $(RA)/record/record_mux_mp4.c ../gpu_av_output/vulkan_mock_file.c $(LRC)/formats/ac3/rac3_frame.c $(LRC)/formats/ac3/rac3_encode.c
vulkan_output_test$(EXE): vulkan_output_test.c vulkan_contract_test.c contract_test.c $(SOURCES) $(VULKAN_OUTPUT_SOURCES) $(GPU_HEADERS) $(RA)/record/record_stream_vulkan.c $(RA)/record/record_gpu_output_vulkan.h
	$(CC) $(CFLAGS) -Wno-long-long -Wno-unused-function -I$(RA)/gfx/include -DHAVE_MF_AUDIO -DHAVE_RTMP -DHAVE_RFLV -o $@ vulkan_output_test.c $(filter-out $(RA)/record/record_stream_d3d12.c,$(SOURCES)) $(RA)/record/record_stream_vulkan.c $(VULKAN_OUTPUT_SOURCES) $(LDFLAGS) -lm

VULKAN_SESSION_SOURCES := $(filter-out $(RA)/record/record_gpu_output_vulkan.c,$(VULKAN_OUTPUT_SOURCES)) $(RA)/record/record_gpu_worker_vulkan.c $(RA)/record/record_gpu_queue.c
vulkan_session_test$(EXE): vulkan_session_test.c contract_test.c ../gpu_av_output/vulkan_session_test.c $(SOURCES) $(VULKAN_SESSION_SOURCES) $(RA)/record/record_stream_vulkan.c $(RA)/record/record_gpu_session_vulkan.c $(RA)/record/record_gpu_session_vulkan.h $(RA)/record/record_gpu_output_vulkan.c $(RA)/record/record_gpu_output_vulkan.h
	$(CC) $(CFLAGS) -Wno-long-long -Wno-unused-function -I$(RA)/gfx/include -DHAVE_MF_AUDIO -DHAVE_RTMP -DHAVE_RFLV -o $@ vulkan_session_test.c $(filter-out $(RA)/record/record_stream_d3d12.c,$(SOURCES)) $(RA)/record/record_stream_vulkan.c $(VULKAN_SESSION_SOURCES) $(LDFLAGS) -lm

ifeq ($(EXE),.exe)
all: vulkan_dispatch_stream_test$(EXE)
check: vulkan-dispatch-stream-check
vulkan-dispatch-stream-check: vulkan_dispatch_stream_test$(EXE)
	$(RUNNER) ./vulkan_dispatch_stream_test$(EXE)
vulkan_dispatch_stream_test$(EXE): vulkan_dispatch_stream_test.c $(RA)/record/record_gpu_dispatch_vulkan.c $(RA)/record/record_gpu_dispatch_vulkan_backend.h $(RA)/record/record_gpu_dispatch_vulkan.h $(RA)/record/record_gpu_session_vulkan.h
	$(CC) $(CFLAGS) -Wno-long-long -DHAVE_THREADS -DHAVE_MF_AUDIO -DHAVE_RTMP -DHAVE_RFLV -DHAVE_CRYPTO -I$(RA)/gfx/include $< ../gpu_av_output/rthreads_compat.c $(LRC)/rthreads/retro_eventcount.c $(RA)/record/record_gpu_queue.c $(LRC)/queues/retro_spsc.c -lws2_32 -o $@ $(LDFLAGS)
.PHONY: vulkan-dispatch-stream-check
endif

# Account for allocation calls in every production TU of this fixture.
vulkan_pressure_test$(EXE): pressure_alloc.c pressure_alloc.h vulkan_pressure_test.c vulkan_session_test.c contract_test.c ../gpu_av_output/vulkan_session_test.c $(SOURCES) $(VULKAN_SESSION_SOURCES) $(RA)/record/record_stream_vulkan.c $(RA)/record/record_gpu_session_vulkan.c
	$(CC) $(CFLAGS) -Wno-long-long -Wno-unused-function -I$(RA)/gfx/include -DHAVE_MF_AUDIO -DHAVE_RTMP -DHAVE_RFLV -include pressure_alloc.h -o $@ vulkan_pressure_test.c pressure_alloc.c $(filter-out $(RA)/record/record_stream_d3d12.c,$(SOURCES)) $(RA)/record/record_stream_vulkan.c $(VULKAN_SESSION_SOURCES) $(LDFLAGS) -lm

AAC_AUDIO_SOURCES := pressure_alloc.c aac_audio_test.c $(RA)/record/record_aac_audio.c $(LRC)/formats/aac/raac_encode.c
AAC_AUDIO_HEADERS := pressure_alloc.h $(RA)/record/record_mf_audio.h $(RA)/record/record_aac_audio.h $(LRC)/include/formats/raac_encode.h
aac_audio_test$(EXE): $(AAC_AUDIO_SOURCES) $(AAC_AUDIO_HEADERS)
	$(CC) $(CFLAGS) -Wno-long-long -DHAVE_RAAC -DHAVE_THREADS -include pressure_alloc.h -o $@ $(AAC_AUDIO_SOURCES) $(LDFLAGS) -lm

contract_test$(EXE) vulkan_contract_test$(EXE): CFLAGS += -DHAVE_MF_AUDIO

aac_stream_test$(EXE): aac_stream_test.c contract_test.c $(SOURCES) $(GPU_HEADERS) $(LRC)/formats/aac/raac_encode.c
	$(CC) $(CFLAGS) -Wno-unused-function -Wno-long-long -DHAVE_RAAC -o $@ aac_stream_test.c $(SOURCES) $(LRC)/formats/aac/raac_encode.c $(LDFLAGS) -lm

aac_audio_threadless_test$(EXE): $(AAC_AUDIO_SOURCES) $(AAC_AUDIO_HEADERS)
	$(CC) $(CFLAGS) -Wno-long-long -DHAVE_RAAC -DAAC_TEST_THREADLESS -UHAVE_THREADS -include pressure_alloc.h -o $@ $(AAC_AUDIO_SOURCES) $(LDFLAGS) -lm

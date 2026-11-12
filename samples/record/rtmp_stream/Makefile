CC ?= cc
RA := ../../..
LRC := $(RA)/libretro-common
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wframe-larger-than=2048 -I$(LRC)/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions -Wno-c11-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
SESSION := $(LRC)/net/net_rtmp_session.c $(LRC)/net/net_rtmp_handshake.c $(LRC)/net/net_rtmp_chunk.c $(LRC)/net/net_rtmp_receive.c $(LRC)/net/net_rtmp_publish.c $(LRC)/net/net_rtmp_command.c $(LRC)/net/net_rtmp_reply.c $(LRC)/net/net_rtmp_control.c
SOURCES := $(RA)/record/record_stream_d3d12.c ../gpu_av_output/mock_worker.c $(LRC)/queues/retro_spsc.c $(LRC)/formats/flv/rflv_mux.c $(SESSION)
all: contract_test$(EXE) wrapper_test$(EXE) vt_contract_test$(EXE) vt_output_test$(EXE)
contract_test$(EXE): contract_test.c $(SOURCES) $(RA)/record/record_stream_d3d12.h $(LRC)/samples/net/rtmp/rtmp_session_test.c
	$(CC) $(CFLAGS) -o $@ contract_test.c $(SOURCES) $(LDFLAGS)
FILE_OUTPUT := $(RA)/record/record_gpu_output_d3d12.c $(RA)/record/record_mux_mp4.c ../gpu_av_output/mock_file.c $(LRC)/formats/ac3/rac3_frame.c $(LRC)/formats/ac3/rac3_encode.c
wrapper_test$(EXE): wrapper_test.c contract_test.c $(SOURCES) $(FILE_OUTPUT)
	$(CC) $(CFLAGS) -DHAVE_D3D12 -DHAVE_MF_AUDIO -DHAVE_RTMP -DHAVE_RFLV -o $@ wrapper_test.c $(SOURCES) $(FILE_OUTPUT) $(LDFLAGS) -lm
check: all
	$(RUNNER) ./vt_output_test$(EXE)
	python3 vt_oracle.py --runner "$(RUNNER)" ./vt_contract_test$(EXE)
	$(RUNNER) ./contract_test$(EXE)
	$(RUNNER) ./wrapper_test$(EXE)
clean:
	rm -f vt_output_test vt_output_test.exe
	rm -f vt_contract_test vt_contract_test.exe vt_thread_test
	rm -f contract_test contract_test.exe wrapper_test wrapper_test.exe
.PHONY: all check clean

VT_SOURCES := $(RA)/record/record_stream_vt.c $(RA)/record/record_audio_queue.c $(LRC)/queues/retro_spsc.c $(LRC)/formats/flv/rflv_mux.c $(SESSION)
VT_OUTPUT_SOURCES := $(RA)/record/record_stream_output_vt.c $(RA)/record/record_avfoundation_session.c $(RA)/record/record_avfoundation_worker.c $(RA)/record/record_gpu_queue.c
vt_output_test$(EXE): vt_output_test.c vt_contract_test.c $(VT_SOURCES) $(VT_OUTPUT_SOURCES) $(RA)/record/record_stream_output_vt.h
	$(CC) $(CFLAGS) -Wno-unused-function -o $@ vt_output_test.c $(VT_OUTPUT_SOURCES) $(VT_SOURCES) $(LDFLAGS)
vt_contract_test$(EXE): vt_contract_test.c $(VT_SOURCES) $(RA)/record/record_stream_vt.h
	$(CC) $(CFLAGS) -o $@ vt_contract_test.c $(VT_SOURCES) $(LDFLAGS)
vt-check: vt_contract_test$(EXE) vt_output_test$(EXE)
	$(RUNNER) ./vt_output_test$(EXE)
	python3 vt_oracle.py --runner "$(RUNNER)" ./vt_contract_test$(EXE)
.PHONY: vt-check

vt_thread_test: vt_contract_test.c $(VT_SOURCES) $(RA)/record/record_stream_vt.h
	$(CC) $(CFLAGS) -pthread -DHAVE_THREADS -DVT_STREAM_THREAD_TEST -o $@ vt_contract_test.c $(VT_SOURCES) $(LDFLAGS)

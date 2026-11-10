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
all: contract_test$(EXE) wrapper_test$(EXE)
contract_test$(EXE): contract_test.c $(SOURCES) $(RA)/record/record_stream_d3d12.h $(LRC)/samples/net/rtmp/rtmp_session_test.c
	$(CC) $(CFLAGS) -o $@ contract_test.c $(SOURCES) $(LDFLAGS)
FILE_OUTPUT := $(RA)/record/record_gpu_output_d3d12.c $(RA)/record/record_mux_mp4.c ../gpu_av_output/mock_file.c $(LRC)/formats/ac3/rac3_frame.c $(LRC)/formats/ac3/rac3_encode.c
wrapper_test$(EXE): wrapper_test.c contract_test.c $(SOURCES) $(FILE_OUTPUT)
	$(CC) $(CFLAGS) -DHAVE_D3D12 -DHAVE_MF_AUDIO -DHAVE_RTMP -DHAVE_RFLV -o $@ wrapper_test.c $(SOURCES) $(FILE_OUTPUT) $(LDFLAGS) -lm
check: all
	$(RUNNER) ./contract_test$(EXE)
	$(RUNNER) ./wrapper_test$(EXE)
clean:
	rm -f contract_test contract_test.exe wrapper_test wrapper_test.exe
.PHONY: all check clean

CC ?= cc
RA := ../../..
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wframe-larger-than=2048 -I$(RA)/libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: rtmp_chunk_test$(EXE) rtmp_handshake_test$(EXE) rtmp_receive_test$(EXE) rtmp_command_test$(EXE) rtmp_reply_test$(EXE) rtmp_publish_test$(EXE) rtmp_control_test$(EXE) rtmp_session_test$(EXE)
SESSION := $(RA)/record/record_rtmp_session.c $(RA)/record/record_rtmp_handshake.c $(RA)/record/record_rtmp_chunk.c $(RA)/record/record_rtmp_receive.c $(RA)/record/record_rtmp_publish.c $(RA)/record/record_rtmp_command.c $(RA)/record/record_rtmp_reply.c $(RA)/record/record_rtmp_control.c
rtmp_session_test$(EXE): rtmp_session_test.c $(SESSION) $(wildcard $(RA)/record/record_rtmp*.h)
	$(CC) $(CFLAGS) -o $@ rtmp_session_test.c $(SESSION) $(LDFLAGS)
rtmp_control_test$(EXE): rtmp_control_test.c $(RA)/record/record_rtmp_control.c $(RA)/record/record_rtmp_receive.c $(RA)/record/record_rtmp_control.h
	$(CC) $(CFLAGS) -o $@ rtmp_control_test.c $(RA)/record/record_rtmp_control.c $(RA)/record/record_rtmp_receive.c $(LDFLAGS)
rtmp_chunk_test$(EXE): rtmp_chunk_test.c $(RA)/record/record_rtmp_chunk.c $(RA)/record/record_rtmp_chunk.h
	$(CC) $(CFLAGS) -o $@ rtmp_chunk_test.c $(RA)/record/record_rtmp_chunk.c $(LDFLAGS)
rtmp_handshake_test$(EXE): rtmp_handshake_test.c $(RA)/record/record_rtmp_handshake.c $(RA)/record/record_rtmp_handshake.h $(RA)/record/record_rtmp_chunk.h
	$(CC) $(CFLAGS) -o $@ rtmp_handshake_test.c $(RA)/record/record_rtmp_handshake.c $(LDFLAGS)
rtmp_receive_test$(EXE): rtmp_receive_test.c $(RA)/record/record_rtmp_receive.c $(RA)/record/record_rtmp_receive.h $(RA)/record/record_rtmp_chunk.h
	$(CC) $(CFLAGS) -o $@ rtmp_receive_test.c $(RA)/record/record_rtmp_receive.c $(LDFLAGS)
rtmp_command_test$(EXE): rtmp_command_test.c $(RA)/record/record_rtmp_command.c $(RA)/record/record_rtmp_command.h $(RA)/record/record_rtmp_chunk.h
	$(CC) $(CFLAGS) -o $@ rtmp_command_test.c $(RA)/record/record_rtmp_command.c $(LDFLAGS)
rtmp_reply_test$(EXE): rtmp_reply_test.c $(RA)/record/record_rtmp_reply.c $(RA)/record/record_rtmp_reply.h $(RA)/record/record_rtmp_chunk.h
	$(CC) $(CFLAGS) -o $@ rtmp_reply_test.c $(RA)/record/record_rtmp_reply.c $(LDFLAGS)
PUBLISH := $(RA)/record/record_rtmp_publish.c $(RA)/record/record_rtmp_command.c $(RA)/record/record_rtmp_reply.c
PUBLISH_HEADERS := $(RA)/record/record_rtmp_publish.h $(RA)/record/record_rtmp_command.h $(RA)/record/record_rtmp_reply.h $(RA)/record/record_rtmp_receive.h $(RA)/record/record_rtmp_chunk.h
rtmp_publish_test$(EXE): rtmp_publish_test.c $(PUBLISH) $(PUBLISH_HEADERS)
	$(CC) $(CFLAGS) -o $@ rtmp_publish_test.c $(PUBLISH) $(LDFLAGS)
check: all
	$(RUNNER) ./rtmp_chunk_test$(EXE)
	$(RUNNER) ./rtmp_handshake_test$(EXE)
	$(RUNNER) ./rtmp_receive_test$(EXE)
	$(RUNNER) ./rtmp_command_test$(EXE)
	$(RUNNER) ./rtmp_reply_test$(EXE)
	$(RUNNER) ./rtmp_publish_test$(EXE)
	$(RUNNER) ./rtmp_control_test$(EXE)
	$(RUNNER) ./rtmp_session_test$(EXE)
	python3 run.py --runner "$(RUNNER)" ./rtmp_chunk_test$(EXE)
	python3 receive.py --runner "$(RUNNER)" ./rtmp_receive_test$(EXE)
	python3 command.py --runner "$(RUNNER)" ./rtmp_command_test$(EXE)
	python3 reply.py --runner "$(RUNNER)" ./rtmp_reply_test$(EXE)
peer-check: all
	python3 peer.py --runner "$(RUNNER)" --receive-binary ./rtmp_receive_test$(EXE) --command-binary ./rtmp_command_test$(EXE) --reply-binary ./rtmp_reply_test$(EXE) ./rtmp_handshake_test$(EXE)
	python3 session_peer.py --runner "$(RUNNER)" ./rtmp_session_test$(EXE)
clean:
	rm -f rtmp_chunk_test rtmp_chunk_test.exe rtmp_handshake_test rtmp_handshake_test.exe
	rm -f rtmp_receive_test rtmp_receive_test.exe
	rm -f rtmp_command_test rtmp_command_test.exe
	rm -f rtmp_reply_test rtmp_reply_test.exe
	rm -f rtmp_publish_test rtmp_publish_test.exe
	rm -f rtmp_control_test rtmp_control_test.exe
	rm -f rtmp_session_test rtmp_session_test.exe
.PHONY: all check peer-check clean

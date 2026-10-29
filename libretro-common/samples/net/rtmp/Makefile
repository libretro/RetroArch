CC ?= cc
LRC := ../../..
include ../../sample_objdir.mk
CFLAGS += -O1 -g1 -std=c89 -pedantic -Wall -Wextra -Werror -Wframe-larger-than=2048 -I$(LRC)/include
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
CFLAGS += -Wno-c99-extensions
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: rtmp_chunk_test$(EXE) rtmp_handshake_test$(EXE) rtmp_receive_test$(EXE) rtmp_command_test$(EXE) rtmp_reply_test$(EXE) rtmp_publish_test$(EXE) rtmp_control_test$(EXE) rtmp_session_test$(EXE) rtmp_socket_test$(EXE) rtmp_socket_native_test$(EXE)
SESSION := $(LRC)/net/net_rtmp_session.c $(LRC)/net/net_rtmp_handshake.c $(LRC)/net/net_rtmp_chunk.c $(LRC)/net/net_rtmp_receive.c $(LRC)/net/net_rtmp_publish.c $(LRC)/net/net_rtmp_command.c $(LRC)/net/net_rtmp_reply.c $(LRC)/net/net_rtmp_control.c
rtmp_session_test$(EXE): rtmp_session_test.c $(SESSION) $(wildcard $(LRC)/include/net/net_rtmp*.h)
	$(CC) $(CFLAGS) -o $@ rtmp_session_test.c $(SESSION) $(LDFLAGS)
rtmp_socket_test$(EXE): rtmp_socket_test.c $(SESSION) $(LRC)/net/net_rtmp_socket.c $(wildcard $(LRC)/include/net/net_rtmp*.h)
	$(CC) $(CFLAGS) -D_GNU_SOURCE -o $@ rtmp_socket_test.c $(SESSION) $(LDFLAGS)
NETWORK := $(LRC)/net/net_socket.c $(LRC)/net/net_compat.c $(LRC)/features/features_cpu.c $(LRC)/string/stdstring.c $(LRC)/string/rstrtod.c $(LRC)/compat/compat_strl.c $(LRC)/compat/compat_posix_string.c $(LRC)/encodings/encoding_utf.c $(LRC)/time/rtime.c
NETWORK_OBJS := $(call sample_objs,$(NETWORK:.c=.o))
NETWORK_LIBS := -lm $(if $(findstring mingw,$(shell $(CC) -dumpmachine)),-lws2_32 -lwinmm,)
rtmp_socket_native_test$(EXE): rtmp_socket_native_test.c $(SESSION) $(NETWORK_OBJS) $(LRC)/net/net_rtmp_socket.c $(wildcard $(LRC)/include/net/net_rtmp*.h)
	$(CC) $(CFLAGS) -D_GNU_SOURCE -o $@ rtmp_socket_native_test.c $(SESSION) $(LRC)/net/net_rtmp_socket.c $(NETWORK_OBJS) $(LDFLAGS) $(NETWORK_LIBS)
$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(filter-out -pedantic -Werror,$(CFLAGS)) -D_GNU_SOURCE -c -o $@ $<
rtmp_control_test$(EXE): rtmp_control_test.c $(LRC)/net/net_rtmp_control.c $(LRC)/net/net_rtmp_receive.c $(LRC)/include/net/net_rtmp_control.h
	$(CC) $(CFLAGS) -o $@ rtmp_control_test.c $(LRC)/net/net_rtmp_control.c $(LRC)/net/net_rtmp_receive.c $(LDFLAGS)
rtmp_chunk_test$(EXE): rtmp_chunk_test.c $(LRC)/net/net_rtmp_chunk.c $(LRC)/include/net/net_rtmp_chunk.h
	$(CC) $(CFLAGS) -o $@ rtmp_chunk_test.c $(LRC)/net/net_rtmp_chunk.c $(LDFLAGS)
rtmp_handshake_test$(EXE): rtmp_handshake_test.c $(LRC)/net/net_rtmp_handshake.c $(LRC)/include/net/net_rtmp_handshake.h $(LRC)/include/net/net_rtmp_chunk.h
	$(CC) $(CFLAGS) -o $@ rtmp_handshake_test.c $(LRC)/net/net_rtmp_handshake.c $(LDFLAGS)
rtmp_receive_test$(EXE): rtmp_receive_test.c $(LRC)/net/net_rtmp_receive.c $(LRC)/include/net/net_rtmp_receive.h $(LRC)/include/net/net_rtmp_chunk.h
	$(CC) $(CFLAGS) -o $@ rtmp_receive_test.c $(LRC)/net/net_rtmp_receive.c $(LDFLAGS)
rtmp_command_test$(EXE): rtmp_command_test.c $(LRC)/net/net_rtmp_command.c $(LRC)/include/net/net_rtmp_command.h $(LRC)/include/net/net_rtmp_chunk.h
	$(CC) $(CFLAGS) -o $@ rtmp_command_test.c $(LRC)/net/net_rtmp_command.c $(LDFLAGS)
rtmp_reply_test$(EXE): rtmp_reply_test.c $(LRC)/net/net_rtmp_reply.c $(LRC)/include/net/net_rtmp_reply.h $(LRC)/include/net/net_rtmp_chunk.h
	$(CC) $(CFLAGS) -o $@ rtmp_reply_test.c $(LRC)/net/net_rtmp_reply.c $(LDFLAGS)
PUBLISH := $(LRC)/net/net_rtmp_publish.c $(LRC)/net/net_rtmp_command.c $(LRC)/net/net_rtmp_reply.c
PUBLISH_HEADERS := $(LRC)/include/net/net_rtmp_publish.h $(LRC)/include/net/net_rtmp_command.h $(LRC)/include/net/net_rtmp_reply.h $(LRC)/include/net/net_rtmp_receive.h $(LRC)/include/net/net_rtmp_chunk.h
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
	$(RUNNER) ./rtmp_socket_test$(EXE)
	python3 run.py --runner "$(RUNNER)" ./rtmp_chunk_test$(EXE)
	python3 receive.py --runner "$(RUNNER)" ./rtmp_receive_test$(EXE)
	python3 command.py --runner "$(RUNNER)" ./rtmp_command_test$(EXE)
	python3 reply.py --runner "$(RUNNER)" ./rtmp_reply_test$(EXE)
peer-check: all
	python3 peer.py --runner "$(RUNNER)" --receive-binary ./rtmp_receive_test$(EXE) --command-binary ./rtmp_command_test$(EXE) --reply-binary ./rtmp_reply_test$(EXE) ./rtmp_handshake_test$(EXE)
	python3 session_peer.py --runner "$(RUNNER)" ./rtmp_session_test$(EXE)
	python3 session_peer.py --runner "$(RUNNER)" --socket-binary ./rtmp_socket_native_test$(EXE) ./rtmp_session_test$(EXE)
clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f rtmp_chunk_test rtmp_chunk_test.exe rtmp_handshake_test rtmp_handshake_test.exe
	rm -f rtmp_receive_test rtmp_receive_test.exe
	rm -f rtmp_command_test rtmp_command_test.exe
	rm -f rtmp_reply_test rtmp_reply_test.exe
	rm -f rtmp_publish_test rtmp_publish_test.exe
	rm -f rtmp_control_test rtmp_control_test.exe
	rm -f rtmp_session_test rtmp_session_test.exe
	rm -f rtmp_socket_test rtmp_socket_test.exe
	rm -f rtmp_socket_native_test rtmp_socket_native_test.exe
.PHONY: all check peer-check clean

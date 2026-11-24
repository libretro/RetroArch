CC ?= cc
RA := ../../..
COMMON := $(RA)/libretro-common
TARGET := pyrowave_session_test$(EXE)
CONTROLLER := pyrowave_controller_test$(EXE)
DISPATCH := pyrowave_dispatch_test$(EXE)
LINK := pyrowave_link_test$(EXE)
SHADER := pyrowave_shader_test$(EXE)
SHADER11 := pyrowave_d3d11_shader_test$(EXE)
SHADERDIR := .pyrowave-shaders
PYTHON ?= python3
DEVICE5_ABI := pyrowave_device5_abi$(EXE)
CFLAGS += -O2 -std=c89 -pedantic -Wall -Wextra -Werror -I$(COMMON)/include
SHARED_SRC := $(RA)/record/record_pyrowave_pcm.c \
       $(RA)/record/record_pyrowave_dispatch.c \
       $(RA)/record/record_pyrowave_connection.c $(COMMON)/net/rpyrowave_socket.c \
       $(RA)/record/record_pyrowave_clock.c \
       $(RA)/record/record_pyrowave_audio_clock.c \
       $(RA)/record/record_pyrowave_session.c \
       $(RA)/record/record_gpu_queue.c $(COMMON)/queues/retro_spsc.c \
       $(COMMON)/formats/pyrowave/rpyrowave_stream.c \
       $(COMMON)/formats/pyrowave/rpyrowave.c
ifeq ($(EXE),.exe)
LDLIBS += -lws2_32
FRONTEND_SUFFIX := .dll
else
THREAD_LIB := -pthread
FRONTEND_SUFFIX := .so
FRONTEND_PIC := -fPIC
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET) $(CONTROLLER) $(DISPATCH) $(LINK) $(SHADER) $(SHADER11)

$(TARGET): pyrowave_session_test.c $(SHARED_SRC)
	$(CC) $(CFLAGS) pyrowave_session_test.c $(SHARED_SRC) $(LDFLAGS) $(LDLIBS) -o $@

$(CONTROLLER): pyrowave_controller_test.c pyrowave_connection_test.h pyrowave_audio_clock_test.h $(SHARED_SRC)
	$(CC) $(CFLAGS) pyrowave_controller_test.c $(SHARED_SRC) $(LDFLAGS) $(LDLIBS) -o $@

$(DISPATCH): pyrowave_controller_test.c pyrowave_connection_test.h pyrowave_dispatch_test.h pyrowave_audio_clock_test.h $(SHARED_SRC)
	$(CC) $(CFLAGS) -DHAVE_THREADS -DRPY_TEST_DISPATCH pyrowave_controller_test.c $(SHARED_SRC) $(LDFLAGS) $(LDLIBS) $(THREAD_LIB) -o $@

$(LINK): pyrowave_link_test.c $(RA)/record/record_pyrowave_video_link.c
	$(CC) $(CFLAGS) -DHAVE_THREADS $^ $(LDFLAGS) $(THREAD_LIB) -o $@

$(SHADERDIR)/rpyrowave_shaders.h: $(RA)/tools/pyrowave_embed_shaders.py $(wildcard $(COMMON)/formats/pyrowave/shaders/precompiled/*) \
      $(COMMON)/formats/pyrowave/shaders/color.hlsl \
      $(COMMON)/formats/pyrowave/shaders/dwt.hlsl \
      $(COMMON)/formats/pyrowave/shaders/packet.hlsl
	$(PYTHON) $(RA)/tools/pyrowave_embed_shaders.py --output $@

$(SHADER): pyrowave_shader_test.c $(RA)/record/record_pyrowave_source.c $(SHADERDIR)/rpyrowave_shaders.h
	$(CC) $(CFLAGS) -DHAVE_D3D12 -I$(SHADERDIR) pyrowave_shader_test.c $(RA)/record/record_pyrowave_source.c $(LDFLAGS) -o $@

$(SHADER11): pyrowave_shader_test.c $(RA)/record/record_pyrowave_source.c $(SHADERDIR)/rpyrowave_shaders.h
	$(CC) $(CFLAGS) -DHAVE_D3D11 -I$(SHADERDIR) pyrowave_shader_test.c $(RA)/record/record_pyrowave_source.c $(LDFLAGS) -o $@

check: $(TARGET) $(CONTROLLER) $(DISPATCH) $(LINK) $(SHADER) $(SHADER11)
	$(RUNNER) ./$(TARGET)
	$(RUNNER) ./$(CONTROLLER)
	$(RUNNER) ./$(DISPATCH)
	$(RUNNER) ./$(LINK)
	$(RUNNER) ./$(SHADER)
	$(RUNNER) ./$(SHADER11)

frontend_core$(FRONTEND_SUFFIX): frontend_core.c
	$(CC) $(CFLAGS) $(FRONTEND_PIC) -shared $< $(LDFLAGS) -o $@

frontend_codec$(FRONTEND_SUFFIX): $(COMMON)/formats/pyrowave/rpyrowave.c $(COMMON)/formats/pyrowave/rpyrowave_image.c
	$(CC) $(CFLAGS) $(FRONTEND_PIC) -shared $^ $(LDFLAGS) -lm -o $@

frontend-fixtures: frontend_core$(FRONTEND_SUFFIX) frontend_codec$(FRONTEND_SUFFIX)

$(DEVICE5_ABI): pyrowave_d3d12_device5_abi.c pyrowave_d3d12_device5_test.h
	$(CC) $(CFLAGS) $< $(LDFLAGS) -ldxguid -o $@

device5-abi-build: $(DEVICE5_ABI)

device5-abi-check: $(DEVICE5_ABI)
	$(RUNNER) ./$(DEVICE5_ABI)

clean:
	rm -f pyrowave_d3d11_shader_test pyrowave_d3d11_shader_test.exe
	rm -f pyrowave_shader_test pyrowave_shader_test.exe
	rm -rf $(SHADERDIR)
	rm -f pyrowave_link_test pyrowave_link_test.exe
	rm -f pyrowave_session_test pyrowave_session_test.exe
	rm -f pyrowave_controller_test pyrowave_controller_test.exe
	rm -f pyrowave_dispatch_test pyrowave_dispatch_test.exe
	rm -f frontend_core.dll frontend_core.so frontend_codec.dll frontend_codec.so
	rm -f pyrowave_device5_abi pyrowave_device5_abi.exe

.PHONY: all check frontend-fixtures device5-abi-build device5-abi-check clean

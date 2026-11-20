RA := ../../..
COMMON := $(RA)/libretro-common
ifeq ($(OS),Windows_NT)
CC ?= gcc
else
ifeq ($(origin CC),default)
CC := x86_64-w64-mingw32-gcc
endif
endif
TARGET := pyrowave_d3d12_test.exe
SHADERDIR ?= .pyrowave-dxbc
FXC ?= fxc
PYTHON ?= python3
CFLAGS += -I$(SHADERDIR) -O2 -std=c89 -pedantic -Wall -Wextra -Werror -I$(COMMON)/include
LDLIBS += -ld3d12 -ldxgi -ldxguid -lm
SRC := pyrowave_d3d12_test.c $(RA)/record/record_pyrowave_d3d12.c \
       $(RA)/record/record_pyrowave_session.c $(RA)/record/record_pyrowave_pcm.c \
       $(COMMON)/formats/pyrowave/rpyrowave_stream.c \
       $(RA)/record/record_gpu_queue.c $(COMMON)/queues/retro_spsc.c \
       $(COMMON)/formats/pyrowave/rpyrowave.c $(COMMON)/formats/pyrowave/rpyrowave_image.c \
       $(COMMON)/formats/pyrowave/rpyrowave_gpu.c
PLAYER := pyrowave_player_d3d12_test.exe
PLAYERDIR ?= .pyrowave-player-vfs
PLAYER_SRC := $(RA)/record/record_pyrowave_source.c $(RA)/record/record_pyrowave_source_d3d12.c
VFS_SRC := $(COMMON)/streams/file_stream.c $(COMMON)/vfs/vfs_implementation.c \
       $(COMMON)/file/file_path.c $(COMMON)/file/file_path_io.c \
       $(COMMON)/features/features_cpu.c $(COMMON)/compat/compat_strl.c $(COMMON)/compat/fopen_utf8.c \
       $(COMMON)/time/rtime.c $(COMMON)/string/rstrtod.c \
       $(COMMON)/string/stdstring.c $(COMMON)/encodings/encoding_utf.c
VFS_OBJ := $(patsubst $(COMMON)/%.c,$(PLAYERDIR)/%.o,$(VFS_SRC))
SHADERS := $(SHADERDIR)/color.dxbc $(SHADERDIR)/dwt.dxbc $(SHADERDIR)/packet.dxbc

all: $(TARGET)

$(TARGET): $(SRC) $(RA)/record/record_pyrowave_d3d12.h pyrowave_d3d12_session_test.h
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) $(LDLIBS) -o $@

$(PLAYER): $(SHADERDIR)/rpyrowave_shaders.h $(SRC) $(PLAYER_SRC) $(VFS_OBJ) pyrowave_d3d12_player_test.h \
      $(RA)/record/record_pyrowave_player_d3d12.h
	$(CC) $(CFLAGS) -DHAVE_D3D12 -DPYROWAVE_PLAYER_FACTORY -I$(RA)/gfx/include $(SRC) $(PLAYER_SRC) $(VFS_OBJ) $(LDFLAGS) $(LDLIBS) -ld3dcompiler -o $@

# Existing VFS helpers keep their normal GNU dialect; the factory stays C89.
$(VFS_OBJ): $(PLAYERDIR)/%.o: $(COMMON)/%.c
	mkdir -p $(dir $@)
	$(CC) $(filter-out -pedantic -Werror,$(CFLAGS)) -std=gnu99 -D_GNU_SOURCE -c $< -o $@

$(SHADERDIR)/rpyrowave_shaders.h: $(RA)/tools/pyrowave_embed_shaders.py $(wildcard $(COMMON)/formats/pyrowave/shaders/precompiled/*) \
      $(COMMON)/formats/pyrowave/shaders/color.hlsl \
      $(COMMON)/formats/pyrowave/shaders/dwt.hlsl \
      $(COMMON)/formats/pyrowave/shaders/packet.hlsl
	$(PYTHON) $(RA)/tools/pyrowave_embed_shaders.py --output $@

$(SHADERDIR)/%.dxbc: $(COMMON)/formats/pyrowave/shaders/%.hlsl
	mkdir -p $(SHADERDIR)
	$(FXC) /nologo /T cs_5_0 /E main /O3 /Fo $@ $<

check: $(TARGET) $(SHADERS)
	$(RUNNER) ./$(TARGET) $(SHADERS)

check-player: $(PLAYER) $(SHADERS)
	$(RUNNER) ./$(PLAYER) $(SHADERS) $(COMMON)/formats/pyrowave/shaders

clean:
	rm -f $(TARGET) $(PLAYER)
	rm -rf .pyrowave-dxbc .pyrowave-player-vfs

.PHONY: all check check-player clean

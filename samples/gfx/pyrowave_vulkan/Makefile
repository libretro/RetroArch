CC ?= cc
RA := ../../..
COMMON := $(RA)/libretro-common
TARGET := pyrowave_vulkan_test$(EXE)
ASSETDIR ?= .pyrowave-shaders
PYTHON ?= python3
CFLAGS += -I$(ASSETDIR) -O2 -std=c89 -pedantic -Wall -Wextra -Werror -I$(COMMON)/include
SRC := pyrowave_vulkan_test.c $(RA)/record/record_pyrowave_vulkan.c \
       $(RA)/record/record_gpu_queue.c $(COMMON)/queues/retro_spsc.c \
       $(COMMON)/formats/pyrowave/rpyrowave.c $(COMMON)/formats/pyrowave/rpyrowave_image.c \
       $(COMMON)/formats/pyrowave/rpyrowave_gpu.c
PLAYER := pyrowave_player_vulkan_test$(EXE)
PLAYER_SRC := $(RA)/record/record_pyrowave_source.c $(RA)/record/record_pyrowave_source_vulkan.c
VFS_SRC := $(COMMON)/streams/file_stream.c $(COMMON)/vfs/vfs_implementation.c \
       $(COMMON)/file/file_path.c $(COMMON)/file/file_path_io.c \
       $(COMMON)/features/features_cpu.c $(COMMON)/compat/compat_strl.c $(COMMON)/compat/fopen_utf8.c \
       $(COMMON)/time/rtime.c $(COMMON)/string/rstrtod.c \
       $(COMMON)/string/stdstring.c $(COMMON)/encodings/encoding_utf.c
VFS_OBJ := $(patsubst $(COMMON)/%.c,$(ASSETDIR)/vfs$(EXE)/%.o,$(VFS_SRC))
ifeq ($(EXE),.exe)
CFLAGS += -isystem $(RA)/gfx/include
else
LDLIBS += -ldl -lm
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(ASSETDIR)/rpyrowave_shaders.h: Makefile $(RA)/tools/pyrowave_shaders.py $(RA)/tools/spirv_optimize.py $(RA)/tools/glsl_minify.py \
      $(COMMON)/formats/pyrowave/shaders/color.hlsl $(COMMON)/formats/pyrowave/shaders/dwt.hlsl \
      $(COMMON)/formats/pyrowave/shaders/packet.hlsl
	$(PYTHON) $(RA)/tools/pyrowave_shaders.py --output $@ --export-spirv $(ASSETDIR)

$(TARGET): $(SRC) $(ASSETDIR)/rpyrowave_shaders.h $(RA)/record/record_pyrowave_vulkan.h
	$(CC) $(CFLAGS) $(SRC) $(LDFLAGS) $(LDLIBS) -o $@

$(PLAYER): $(SRC) $(PLAYER_SRC) $(VFS_OBJ) $(ASSETDIR)/rpyrowave_shaders.h \
      pyrowave_vulkan_player_test.h $(RA)/record/record_pyrowave_player_vulkan.h
	$(CC) $(CFLAGS) -DHAVE_VULKAN -DPYROWAVE_PLAYER_FACTORY $(SRC) $(PLAYER_SRC) $(VFS_OBJ) $(LDFLAGS) $(LDLIBS) -o $@

# Existing VFS helpers keep GNU extensions; the codec, factory and oracle stay C89.
$(VFS_OBJ): $(ASSETDIR)/vfs$(EXE)/%.o: $(COMMON)/%.c
	mkdir -p $(dir $@)
	$(CC) $(filter-out -pedantic -Werror,$(CFLAGS)) -std=gnu99 -D_GNU_SOURCE -c $< -o $@

check-player: $(PLAYER)
	$(RUNNER) ./$(PLAYER) $(ASSETDIR)

check: $(TARGET)
	$(RUNNER) ./$(TARGET)

clean:
	rm -f pyrowave_vulkan_test pyrowave_vulkan_test.exe pyrowave_player_vulkan_test pyrowave_player_vulkan_test.exe
	rm -rf .pyrowave-shaders

.PHONY: all check check-player clean

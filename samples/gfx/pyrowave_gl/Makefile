CC ?= cc
RA := ../../..
COMMON := $(RA)/libretro-common
TARGET := pyrowave_gl_test$(EXE)
PYTHON ?= python3
ASSETDIR ?= .pyrowave-shaders
SHADERS ?= $(ASSETDIR)/color.comp $(ASSETDIR)/dwt.comp $(ASSETDIR)/packet.comp
SHADER_SOURCE := $(COMMON)/formats/pyrowave/shaders
CFLAGS += -I$(ASSETDIR) -O2 -std=c89 -pedantic -Wall -Wextra -Werror -DHAVE_OPENGL -DHAVE_OPENGL_CORE \
          -I$(COMMON)/include
SRC := pyrowave_gl_test.c $(RA)/record/record_pyrowave_gl.c \
       $(RA)/record/record_gpu_queue.c $(COMMON)/queues/retro_spsc.c \
       $(COMMON)/formats/pyrowave/rpyrowave.c \
       $(COMMON)/formats/pyrowave/rpyrowave_image.c \
       $(COMMON)/formats/pyrowave/rpyrowave_gpu.c \
       $(COMMON)/glsym/rglgen.c $(COMMON)/glsym/glsym_gl.c \
       $(RA)/record/record_pyrowave_source.c $(RA)/record/record_pyrowave_source_gl.c
VFS_SRC := $(COMMON)/streams/file_stream.c $(COMMON)/vfs/vfs_implementation.c \
       $(COMMON)/file/file_path.c $(COMMON)/file/file_path_io.c \
       $(COMMON)/features/features_cpu.c $(COMMON)/compat/compat_strl.c $(COMMON)/compat/fopen_utf8.c \
       $(COMMON)/time/rtime.c $(COMMON)/string/rstrtod.c \
       $(COMMON)/string/stdstring.c $(COMMON)/encodings/encoding_utf.c
VFS_OBJ := $(patsubst $(COMMON)/%.c,$(ASSETDIR)/vfs$(EXE)/%.o,$(VFS_SRC))
ifeq ($(EXE),.exe)
LDLIBS += -lopengl32 -lgdi32 -luser32 -lm
CFLAGS += -I$(RA)/gfx/include
else
LDLIBS += -lEGL -lGL -lm
endif

ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): $(ASSETDIR)/rpyrowave_shaders.h $(SRC) $(VFS_OBJ) pyrowave_gl_receive_test.h pyrowave_gl_player_test.h pyrowave_gl_scrgb_test.h $(RA)/record/record_pyrowave_gl.h
	$(CC) $(CFLAGS) $(SRC) $(VFS_OBJ) $(LDFLAGS) $(LDLIBS) -o $@

# Existing VFS helpers use GNU extensions; codec, factory and oracle stay C89.
$(VFS_OBJ): $(ASSETDIR)/vfs$(EXE)/%.o: $(COMMON)/%.c
	mkdir -p $(dir $@)
	$(CC) $(filter-out -pedantic -Werror,$(CFLAGS)) -std=gnu99 -D_GNU_SOURCE -c $< -o $@

$(ASSETDIR)/rpyrowave_shaders.h: $(RA)/tools/pyrowave_shaders.py $(RA)/tools/spirv_optimize.py $(RA)/tools/glsl_minify.py \
      $(SHADER_SOURCE)/color.hlsl $(SHADER_SOURCE)/dwt.hlsl $(SHADER_SOURCE)/packet.hlsl
	$(PYTHON) $(RA)/tools/pyrowave_shaders.py --output $@ --export-glsl $(ASSETDIR) --minify-glsl-exports

shaders: $(ASSETDIR)/rpyrowave_shaders.h

check: $(TARGET) shaders
	$(RUNNER) ./$(TARGET) $(SHADERS)
	RPY_TEST_SCALE=1 $(RUNNER) ./$(TARGET) $(SHADERS)

clean:
	rm -f pyrowave_gl_test pyrowave_gl_test.exe
	rm -rf .pyrowave-shaders

.PHONY: all check clean shaders

# Bakes libretro/slang-shaders into the .gsh files the GX2 driver loads
# beside each .slang (gfd_open on the pass path with the extension
# swapped).  slang-cafe (C89, built here from RetroArch's own include
# expansion and rslc's GLSL printer) splits a .slang into the two
# CafeGLSL inputs; compiler.exe is Nintendo's GLSL-to-Latte compiler
# from the Cafe SDK and is the one piece that is not in this tree.
#
#   make parser                     builds ./slang-cafe
#   make SLANG_DIR=path/to/slang-shaders
#
# A stage rslc or CafeGLSL refuses stops at that shader; BLACKLIST
# names the ones known not to build on the Wii U.

GSH_COMPILER = ./compiler.exe
GSH_COMPILER_V = -v
GSH_COMPILER_P = -p
GSH_COMPILER_O = -o
GSH_COMPILER_ALIGN = -align

SLANG_DIR = slang-shaders

BLACKLIST :=
BLACKLIST += $(SLANG_DIR)/dithering/shaders/bayer-matrix-dithering.slang
BLACKLIST += $(SLANG_DIR)/test/decode-format.slang
BLACKLIST += $(SLANG_DIR)/test/format.slang
BLACKLIST += $(SLANG_DIR)/blurs/blur5x5-gamma-encode-every-fbo.slang
BLACKLIST += $(SLANG_DIR)/blurs/blur12x12shared.slang
BLACKLIST += $(SLANG_DIR)/ntsc/shaders/ntsc-xot.slang
BLACKLIST += $(SLANG_DIR)/retro/shaders/bandlimit-pixel.slang
BLACKLIST += $(wildcard $(SLANG_DIR)/vhs/shaders/gristleVHS/*.slang)
BLACKLIST += $(wildcard $(SLANG_DIR)/anti-aliasing/shaders/smaa/*.slang)
BLACKLIST += $(wildcard $(SLANG_DIR)/procedural/*.slang)

EXE_EXT :=

ifneq ($(findstring Linux,$(shell uname -a)),)
else ifneq ($(findstring Darwin,$(shell uname -a)),)
else
EXE_EXT := .exe
endif

SLANG_PARSER = ./slang-cafe$(EXE_EXT)

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

PARSER_SOURCES := slang-cafe.c \
                  $(REPO_ROOT)/gfx/drivers_shader/glslang_util.c \
                  $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_front.c \
                  $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_ir.c \
                  $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_emit.c \
                  $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
                  $(LIBRETRO_COMM_DIR)/string/stdstring.c \
                  $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

PARSER_CFLAGS := -O2 -Wall -std=gnu99 -Wno-overlength-strings -DRARCH_INTERNAL -DHAVE_SLANG \
                 -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include

rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))
SLANG_SHADERS := $(call rwildcard,$(SLANG_DIR)/,*.slang)
SLANG_SHADERS := $(filter-out $(BLACKLIST),$(SLANG_SHADERS))
SLANG_SHADERS := $(SLANG_SHADERS:.slang=.gsh)

all: $(SLANG_SHADERS)
parser: $(SLANG_PARSER)

$(SLANG_PARSER): $(PARSER_SOURCES)
	$(CC) $(PARSER_CFLAGS) $^ -o $@ -lm

shaders: $(SLANG_SHADERS)

%.vert %.frag: %.slang $(SLANG_PARSER)
	$(SLANG_PARSER) $< $*.vert $*.frag

%.gsh: %.vert %.frag
	$(GSH_COMPILER) $(GSH_COMPILER_ALIGN) $(GSH_COMPILER_V) $*.vert $(GSH_COMPILER_P) $*.frag $(GSH_COMPILER_O) $@

.PRECIOUS: %.vert %.frag

clean-shaders:
	rm -rf $(SLANG_SHADERS)

clean-temp:
	rm -rf $(SLANG_SHADERS:.gsh=.vert)
	rm -rf $(SLANG_SHADERS:.gsh=.frag)

clean-parser:
	rm -rf $(SLANG_PARSER)

clean: clean-parser clean-shaders clean-temp


GLSLC = glslc

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

SLANG_PARSER = ./slang-parse$(EXE_EXT)

rwildcard=$(foreach d,$(wildcard $1*),$(call rwildcard,$d/,$2) $(filter $(subst *,%,$2),$d))
SLANG_SHADERS := $(call rwildcard,$(SLANG_DIR)/,*.slang)
SLANG_SHADERS := $(filter-out $(BLACKLIST),$(SLANG_SHADERS))
SLANG_SHADERS := $(SLANG_SHADERS:.slang=.gsh)

all: $(SLANG_SHADERS)
parser: $(SLANG_PARSER)

$(SLANG_PARSER): slang-parse.cpp
	$(CXX) $< -o $@ -g -O0 -Wall -I../../deps/peglib

shaders: $(SLANG_SHADERS)

%.ppslang : %.slang
	$(GLSLC) -E $< -o $@

%.vert %.frag: %.ppslang $(SLANG_PARSER) grammar.txt
	$(SLANG_PARSER) --slang $< --vsh $*.vert --psh $*.frag

%.gsh: %.vert %.frag
	$(GSH_COMPILER) $(GSH_COMPILER_ALIGN) $(GSH_COMPILER_V) $*.vert $(GSH_COMPILER_P) $*.frag $(GSH_COMPILER_O) $@

.PRECIOUS: %.vert %.frag %.ppslang

clean-shaders:
	rm -rf $(SLANG_SHADERS)

clean-temp:
	rm -rf $(SLANG_SHADERS:.gsh=.ppslang)
	rm -rf $(SLANG_SHADERS:.gsh=.vert)
	rm -rf $(SLANG_SHADERS:.gsh=.frag)

clean-parser:
	rm -rf $(SLANG_PARSER) slang-parse.o

clean: clean-parser clean-shaders clean-temp

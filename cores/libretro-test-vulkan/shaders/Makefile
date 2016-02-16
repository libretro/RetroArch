VERT_SHADERS := $(wildcard *.vert)
FRAG_SHADERS := $(wildcard *.frag)
SPIRV := $(VERT_SHADERS:.vert=.vert.spv) $(FRAG_SHADERS:.frag=.frag.spv)
INCLUDES := $(SPIRV:.spv=.inc)

GLSLANG := glslangValidator

all: $(INCLUDES)

%.frag.spv: %.frag
	$(GLSLANG) -V -o $@ $<

%.vert.spv: %.vert
	$(GLSLANG) -V -o $@ $<

%.inc: %.spv
	xxd -i $< $@

clean:
	rm -f $(INCLUDES)
	rm -f $(SPIRV)

.PHONY: clean

VERT_SHADERS := $(wildcard *.vert)
FRAG_SHADERS := $(wildcard *.frag)
SPIRV := $(VERT_SHADERS:.vert=.vert.inc) $(FRAG_SHADERS:.frag=.frag.inc)

GLSLANG := glslc
GLSLFLAGS := -mfmt=c

all: $(SPIRV)

%.vert.inc: %.vert
	$(GLSLANG) $(GLSLFLAGS) -o $@ $<

%.frag.inc: %.frag
	$(GLSLANG) $(GLSLFLAGS) -o $@ $<

clean:
	rm -f $(SPIRV)

.PHONY: clean

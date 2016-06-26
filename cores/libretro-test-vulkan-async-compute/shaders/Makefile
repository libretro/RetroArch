COMP_SHADERS := $(wildcard *.comp)
SPIRV := $(COMP_SHADERS:.comp=.comp.inc)

GLSLANG := glslc
GLSLFLAGS := -mfmt=c

all: $(SPIRV)

%.comp.inc: %.comp
	$(GLSLANG) $(GLSLFLAGS) -o $@ $<

clean:
	rm -f $(SPIRV)

.PHONY: clean

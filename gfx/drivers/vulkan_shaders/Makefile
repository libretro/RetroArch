VERT_SHADERS := $(wildcard *.vert)
FRAG_SHADERS := $(wildcard *.frag)
COMP_SHADERS := $(wildcard *.comp)
SPIRV := $(VERT_SHADERS:.vert=.vert.inc) $(FRAG_SHADERS:.frag=.frag.inc) $(COMP_SHADERS:.comp=.comp.inc)

GLSLANG := glslc
GLSLFLAGS := -mfmt=c

SPIRV += yuv420_to_rgba16f.comp.inc yuv_image_to_rgba16f.comp.inc

all: $(SPIRV)

%.vert.inc: %.vert
	$(GLSLANG) $(GLSLFLAGS) -o $@ $<

%.frag.inc: %.frag
	$(GLSLANG) $(GLSLFLAGS) -o $@ $<

%.comp.inc: %.comp
	$(GLSLANG) $(GLSLFLAGS) -o $@ $<

yuv420_to_rgba8888.comp.inc yuv_image_to_rgba8888.comp.inc: ../../drivers_shader/video_hdr.inc.glsl

clean:
	rm -f $(SPIRV)

.PHONY: clean

yuv420_to_rgba16f.comp.inc: yuv420_to_rgba8888.comp ../../drivers_shader/video_hdr.inc.glsl
	$(GLSLANG) $(GLSLFLAGS) -DOUTPUT_FP16 -o $@ $<

yuv_image_to_rgba16f.comp.inc: yuv_image_to_rgba8888.comp ../../drivers_shader/video_hdr.inc.glsl
	$(GLSLANG) $(GLSLFLAGS) -DOUTPUT_FP16 -o $@ $<

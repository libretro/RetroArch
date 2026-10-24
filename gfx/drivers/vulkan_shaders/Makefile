VERT_SHADERS := $(wildcard *.vert)
FRAG_SHADERS := $(wildcard *.frag)
COMP_SHADERS := $(wildcard *.comp)
SPIRV := $(VERT_SHADERS:.vert=.vert.inc) $(FRAG_SHADERS:.frag=.frag.inc) $(COMP_SHADERS:.comp=.comp.inc)

GLSLANG := glslc
GLSLFLAGS := -mfmt=c

SPIRV += yuv420_to_rgba16f.comp.inc yuv_image_to_rgba16f.comp.inc

# The GLSL glcore builds its own pipelines from when there is no
# SPIRV-Cross to print the SPIR-V: each source as C string literals.
GLSL_SRC := alpha_blend.vert alpha_blend.frag alpha_blend_linear.frag \
            mesh.vert font.frag stereo_anaglyph.frag stereo_interlaced.frag \
            stock_rect.frag opaque.vert opaque.frag hdr_scrgb.vert \
            hdr_scrgb.frag pipeline_ribbon.vert pipeline_ribbon.frag \
            pipeline_ribbon_simple.vert pipeline_ribbon_simple.frag \
            pipeline_snow_simple.frag \
            pipeline_snow.frag pipeline_bokeh.frag pipeline_snowflake.frag
GLSL_INC := $(GLSL_SRC:=.src.inc)

all: $(SPIRV) $(GLSL_INC)

%.src.inc: %
	sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $< > $@

%.vert.inc: %.vert
	$(GLSLANG) $(GLSLFLAGS) -o $@ $<

%.frag.inc: %.frag
	$(GLSLANG) $(GLSLFLAGS) -o $@ $<

%.comp.inc: %.comp
	$(GLSLANG) $(GLSLFLAGS) -o $@ $<

yuv420_to_rgba8888.comp.inc yuv_image_to_rgba8888.comp.inc: ../../drivers_shader/video_hdr.inc.glsl

clean:
	rm -f $(SPIRV) $(GLSL_INC)

.PHONY: clean

yuv420_to_rgba16f.comp.inc: yuv420_to_rgba8888.comp ../../drivers_shader/video_hdr.inc.glsl
	$(GLSLANG) $(GLSLFLAGS) -DOUTPUT_FP16 -o $@ $<

yuv_image_to_rgba16f.comp.inc: yuv_image_to_rgba8888.comp ../../drivers_shader/video_hdr.inc.glsl
	$(GLSLANG) $(GLSLFLAGS) -DOUTPUT_FP16 -o $@ $<

VERT_SHADERS := $(wildcard *.vert effects/*.vert)
FRAG_SHADERS := $(wildcard *.frag effects/*.frag)
COMP_SHADERS := $(wildcard *.comp)
SPIRV := $(VERT_SHADERS:.vert=.vert.inc) $(FRAG_SHADERS:.frag=.frag.inc) $(COMP_SHADERS:.comp=.comp.inc)

PYTHON ?= python3
SPIRV_OPT ?= spirv-opt
SPIRV_VAL ?= spirv-val
OPTIMIZE = $(PYTHON) ../../../tools/spirv_optimize.py --format c --input $@ --output $@ --spirv-opt $(SPIRV_OPT) --spirv-val $(SPIRV_VAL)

GLSLANG := glslc
GLSLFLAGS := -mfmt=c

SPIRV += yuv420_to_rgba16f.comp.inc yuv_image_to_rgba16f.comp.inc

# The GLSL glcore builds its own pipelines from when there is no
# SPIRV-Cross to print the SPIR-V: each source as C string literals.
GLSL_SRC := alpha_blend.vert alpha_blend.frag alpha_blend_linear.frag \
            mesh.vert font.frag stereo_anaglyph.frag stereo_interlaced.frag \
            stock_rect.frag opaque.vert opaque.frag hdr_scrgb.vert \
            hdr_scrgb.frag effects/pipeline_ribbon.vert effects/pipeline_ribbon.frag \
            effects/pipeline_ribbon_simple.vert effects/pipeline_ribbon_simple.frag \
            effects/pipeline_snow_simple.frag \
            effects/pipeline_snow.frag effects/pipeline_bokeh.frag effects/pipeline_snowflake.frag
GLSL_INC := $(GLSL_SRC:=.src.inc)

GLSL_INC += effects/xmb_plastic.frag.src.inc effects/xmb_font.frag.src.inc

all: $(SPIRV) $(GLSL_INC)

# Names are reflection inputs for the shared GL modules: do not strip them.
$(SPIRV): ../../../tools/spirv_optimize.py
$(GLSL_INC): ../../../tools/glsl_minify.py

effects/pipeline_ribbon.vert.inc effects/pipeline_ribbon.frag.inc: ../menu_ribbon_shader.h

effects/xmb_plastic.frag.inc effects/xmb_font.frag.inc: ../xmb_plastic.h

effects/pipeline_ribbon.vert.src.inc effects/pipeline_ribbon.frag.src.inc: ../menu_ribbon_shader.h
effects/xmb_plastic.frag.src.inc effects/xmb_font.frag.src.inc: ../xmb_plastic.h

effects/pipeline_ribbon.vert.src.inc effects/pipeline_ribbon.frag.src.inc effects/xmb_plastic.frag.src.inc effects/xmb_font.frag.src.inc: %.src.inc: %
	$(GLSLANG) -E $< | sed '/^#extension GL_GOOGLE_include_directive/d; /^#line/d' | od -An -v -tu1 | sed 's/ *$$/,/; s/  */,/g; s/^,//' > $@
	echo 0 >> $@
	$(PYTHON) ../../../tools/glsl_minify.py --input $@ --output $@

%.src.inc: %
	sed -e 's/\\/\\\\/g' -e 's/"/\\"/g' -e 's/^/"/' -e 's/$$/\\n"/' $< > $@
	$(PYTHON) ../../../tools/glsl_minify.py --input $@ --output $@

%.vert.inc: %.vert
	$(GLSLANG) $(GLSLFLAGS) -o $@ $<
	$(OPTIMIZE)

%.frag.inc: %.frag
	$(GLSLANG) $(GLSLFLAGS) -o $@ $<
	$(OPTIMIZE)

%.comp.inc: %.comp
	$(GLSLANG) $(GLSLFLAGS) -o $@ $<
	$(OPTIMIZE)

yuv420_to_rgba8888.comp.inc yuv_image_to_rgba8888.comp.inc: ../../drivers_shader/video_hdr.inc.glsl

clean:
	rm -f $(SPIRV) $(GLSL_INC)

.PHONY: clean

yuv420_to_rgba16f.comp.inc: yuv420_to_rgba8888.comp ../../drivers_shader/video_hdr.inc.glsl
	$(GLSLANG) $(GLSLFLAGS) -DOUTPUT_FP16 -o $@ $<
	$(OPTIMIZE)

yuv_image_to_rgba16f.comp.inc: yuv_image_to_rgba8888.comp ../../drivers_shader/video_hdr.inc.glsl
	$(GLSLANG) $(GLSLFLAGS) -DOUTPUT_FP16 -o $@ $<
	$(OPTIMIZE)

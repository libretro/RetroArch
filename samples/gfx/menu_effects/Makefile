# XMB's background effects as the menu installs them, against what the
# video drivers take from them; see menu_effects_test.c.
#
#   make check        GL (desktop, legacy and core profile) and GLES on
#                     Mesa's EGL device (EGL_PLATFORM=surfaceless; skips
#                     without one), and spirv-val over the SPIR-V when
#                     it is installed
#   make check-wine   the HLSL compiled as D3D9 to 12 compile it, cross-
#                     built with mingw-w64 and run under Wine
#   make check-cg     the Cg gl2 draws with where it has no GLSL, through
#                     the Cg backend in a GLX context (Cg finds its
#                     profiles through GLX): run under xvfb-run without a
#                     display; needs libCg and libCgGL
#
# SANITIZER (e.g. address,undefined) instruments the Linux builds.

include ../../sample_objdir.mk

REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common

GL_TARGET   := menu_effects_gl_test
GLES_TARGET := menu_effects_gles_test
WIN_TARGET  := menu_effects_test.exe
CG_TARGET   := menu_effects_cg_test
MINGW_CC    ?= x86_64-w64-mingw32-gcc
WINE        ?= wine

DEPS := menu_effects_test.c \
        $(REPO_ROOT)/tools/d3d_shader_sources.c \
        $(wildcard $(REPO_ROOT)/gfx/drivers/d3d_sm*_shaders/*.h) \
        $(REPO_ROOT)/menu/drivers/xmb_effects.h \
        $(REPO_ROOT)/gfx/gfx_display.h \
        $(REPO_ROOT)/gfx/drivers_shader/shader_glsl.c \
        $(REPO_ROOT)/gfx/common/menu_ribbon.h \
        $(wildcard $(REPO_ROOT)/gfx/drivers/gl*_shaders/effects/*.h) \
        $(wildcard $(REPO_ROOT)/gfx/drivers/d3d_sm*_shaders/effects/*.h) \
        $(REPO_ROOT)/gfx/drivers/menu_ribbon_shader.h \
        $(REPO_ROOT)/gfx/common/menu_ribbon_shader.h \
        $(wildcard $(REPO_ROOT)/gfx/drivers/vulkan_shaders/effects/pipeline_*.src.inc)

BASE := -Wall -std=gnu99 -g1 -O1 -DRARCH_INTERNAL -DHAVE_SHADERPIPELINE \
        -I$(REPO_ROOT) -I$(LRC)/include -I$(REPO_ROOT)/gfx/include \
        -I$(REPO_ROOT)/deps
# spirv_shaders.c's SPIR-V is built for Vulkan and slang
SPIRV := -DHAVE_VULKAN

ifneq ($(SANITIZER),)
   SAN := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
endif

all: $(GL_TARGET) $(GLES_TARGET)

$(GL_TARGET): $(DEPS)
	$(CC) $(SAN) $(BASE) -DHAVE_OPENGL -DHAVE_GLSL -DEFFECTS_GL \
	   -o $@ menu_effects_test.c \
	   $(LRC)/glsym/glsym_gl.c $(LRC)/glsym/rglgen.c \
	   $(LRC)/compat/compat_strl.c \
	   $(SPIRV) $(REPO_ROOT)/gfx/common/spirv_shaders.c \
	   -lEGL -lGL -lm

$(GLES_TARGET): $(DEPS)
	$(CC) $(SAN) $(BASE) -DHAVE_OPENGL -DHAVE_OPENGLES -DHAVE_OPENGLES3 \
	   -DHAVE_GLSL -DEFFECTS_GLES -o $@ menu_effects_test.c \
	   $(LRC)/compat/compat_strl.c \
	   $(SPIRV) $(REPO_ROOT)/gfx/common/spirv_shaders.c \
	   -lEGL -lGLESv2 -lm

$(WIN_TARGET): $(DEPS) $(REPO_ROOT)/gfx/common/d3dcompiler_common.c
	$(MINGW_CC) $(BASE) -DHAVE_DYLIB -o $@ menu_effects_test.c \
	   $(REPO_ROOT)/gfx/common/d3dcompiler_common.c \
	   $(LRC)/dynamic/dylib.c $(LRC)/encodings/encoding_utf.c \
	   $(LRC)/compat/compat_strl.c \
	   $(SPIRV) $(REPO_ROOT)/gfx/common/spirv_shaders.c

$(CG_TARGET): menu_effects_cg_test.c $(DEPS) \
      $(REPO_ROOT)/gfx/drivers_shader/shader_gl_cg.c \
      $(REPO_ROOT)/gfx/drivers_shader/shader_gl_cg.h \
      $(REPO_ROOT)/gfx/common/gl_common.c
	$(CC) $(SAN) $(BASE) -DHAVE_OPENGL -DHAVE_CG -o $@ menu_effects_cg_test.c \
	   $(LRC)/glsym/glsym_gl.c $(LRC)/glsym/rglgen.c \
	   $(LRC)/compat/compat_strl.c \
	   -lCg -lCgGL -lGL -lX11 -lm

check: all
	@rm -rf spv && mkdir spv
	EGL_PLATFORM=surfaceless ./$(GL_TARGET) --spirv spv
	EGL_PLATFORM=surfaceless ./$(GLES_TARGET)
	@if command -v spirv-val >/dev/null 2>&1; then \
	   for f in spv/*.spv; do spirv-val --target-env vulkan1.0 $$f \
	      || { echo "[FAIL] spirv-val $$f"; exit 1; }; done; \
	   echo "  [ok]   spirv-val: every effect's SPIR-V"; \
	 else echo "  [skip] no spirv-val"; fi
	@rm -rf spv

check-wine: $(WIN_TARGET)
	WINEDEBUG=-all $(WINE) ./$(WIN_TARGET)

check-cg: $(CG_TARGET)
	@if [ -n "$$DISPLAY" ]; then ./$(CG_TARGET); \
	 else xvfb-run -a ./$(CG_TARGET); fi

clean:
	rm -rf $(GL_TARGET) $(GLES_TARGET) $(WIN_TARGET) $(CG_TARGET) spv

.PHONY: all check check-wine check-cg clean

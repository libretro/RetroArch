include ../../sample_objdir.mk

TARGET := rslc_ext_test
TARGET_TEST := rslc_plane_test
TARGET_TEST2 := rslc_ext_build_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rslc_ext_test.c \
	$(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_front.c \
	$(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_ir.c \
	$(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_emit.c \
	$(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_dxbc.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

# RSLC_EXTENDED: the stages and types the frontend's own shaders do
# not use. The compiler is C89 everywhere it ships; the test is held to the
# same flags so a C99-ism cannot hide in it.
CFLAGS  += -Wall -Wextra -pedantic -std=c89 -Wno-long-long -Wno-overlength-strings -D_GNU_SOURCE \
	-Werror=implicit-function-declaration -g1 -DRSLC_EXTENDED \
	-I$(LIBRETRO_COMM_DIR)/include -I$(LIBRETRO_COMM_DIR)/gfx/shaders

# Constant folding of the GLSL math builtins runs on libm.
LDLIBS  += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(TARGET_TEST) $(TARGET_TEST2)

# every object sees both headers, so a layout change rebuilds all of them
$(OBJS): $(LIBRETRO_COMM_DIR)/include/gfx/shaders/rslc.h $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_share.h

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

# The SPIR-V it writes also goes through spirv-val when that is installed;
# the subgroup stage is SPIR-V 1.3, Vulkan 1.1's.
# Separate objects keep the optional backend out of this compiler mode.
rslc_plane_test: rslc_plane_test.c $(filter-out rslc_ext_test.c,$(SOURCES)) $(LIBRETRO_COMM_DIR)/include/gfx/shaders/rslc.h $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_share.h
	$(CC) $(filter-out -Wno-overlength-strings,$(CFLAGS)) -Werror -Wframe-larger-than=2048 -DRSLC_NO_DXBC -o $@ $(filter %.c,$^) $(LDFLAGS) $(LDLIBS)

# The builder (RSLC_BUILD) against the parser, stage for stage.
rslc_ext_build_test: rslc_ext_build_test.c $(filter-out rslc_ext_test.c,$(SOURCES)) $(LIBRETRO_COMM_DIR)/include/gfx/shaders/rslc.h $(LIBRETRO_COMM_DIR)/include/gfx/shaders/rslc_build.h $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_share.h
	$(CC) $(CFLAGS) -DRSLC_BUILD -o $@ $(filter %.c,$^) $(LDFLAGS) $(LDLIBS)

plane-check: rslc_plane_test
	@mkdir -p $(SAMPLE_OBJDIR)
	./rslc_plane_test $(SAMPLE_OBJDIR)/planes
	@if command -v spirv-val >/dev/null 2>&1; then spirv-val --target-env vulkan1.1 $(SAMPLE_OBJDIR)/planes.spv; fi
	@if command -v glslangValidator >/dev/null 2>&1; then glslangValidator -S comp $(SAMPLE_OBJDIR)/planes.glsl; fi

check: $(TARGET) $(TARGET_TEST2) plane-check
	./$(TARGET) obj
	./$(TARGET_TEST2)
	@if command -v spirv-val >/dev/null 2>&1; then \
		for f in obj/*.spv; do \
			env=vulkan1.0; case $$f in */subgroup.spv) env=vulkan1.1;; esac; \
			spirv-val --target-env $$env $$f || exit 1; \
		done; \
	fi
	@if command -v glslangValidator >/dev/null 2>&1; then \
		glslangValidator -S frag obj/subgroup.glsl && glslangValidator -S frag obj/depth_unchanged.glsl \
			&& glslangValidator -S frag obj/es_samplers.glsl \
			&& glslangValidator -S frag obj/es_sampler_param.glsl; \
	fi

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(OBJS)

.PHONY: all check plane-check clean

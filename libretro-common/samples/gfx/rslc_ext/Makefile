include ../../sample_objdir.mk

TARGET := rslc_ext_test

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

all: $(TARGET)

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
check: $(TARGET)
	./$(TARGET) obj
	@if command -v spirv-val >/dev/null 2>&1; then \
		for f in obj/*.spv; do \
			env=vulkan1.0; case $$f in */subgroup.spv) env=vulkan1.1;; esac; \
			spirv-val --target-env $$env $$f || exit 1; \
		done; \
	fi

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean

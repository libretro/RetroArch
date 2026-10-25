include ../../sample_objdir.mk

# rslc_reflect_check: compiles corpus stages with rslc and holds the
# reflection section of each blob against what SPIRV-Cross reports
# for the same module - member offsets and sizes, the declared block
# size, the active member set, sampler bindings - since those tables
# are what the drivers read once SPIRV-Cross leaves the runtime path.
#
#   make
#   ./rslc_reflect_check <dumpdir>           the directory rslc_corpus_dump wrote
#   ./rslc_reflect_check --ingest <dumpdir>  glslang's .spv through rslc_compile_spirv
#
# No check target: the tool needs a corpus on disk to do anything.

TARGET := rslc_reflect_check

# SPIRV-Cross is the system's (spirv-cross-c-shared), the reference the
# tables are held against.
SPIRV_CROSS_CFLAGS := $(shell pkg-config --cflags spirv-cross-c-shared)
SPIRV_CROSS_LIBS   := $(shell pkg-config --libs spirv-cross-c-shared)


REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := rslc_reflect_check.c \
           $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_front.c \
           $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_ir.c \
           $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_emit.c

CFLAGS  += -Wall -Wextra -pedantic -std=c89 -Wno-long-long -Wno-overlength-strings -D_GNU_SOURCE -g1 -O1 \
           -I$(LIBRETRO_COMM_DIR)/include -I$(LIBRETRO_COMM_DIR)/gfx/shaders $(SPIRV_CROSS_CFLAGS)

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(SPIRV_CROSS_LIBS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

.PHONY: all clean

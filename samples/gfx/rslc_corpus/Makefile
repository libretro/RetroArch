include ../../sample_objdir.mk

# rslc_corpus_dump: expands and stage-splits .slang files exactly as
# RetroArch does (gfx/drivers_shader/glslang_util.c) and compiles each
# stage with the system's glslang through the same C ABI RetroArch
# uses (gfx/drivers_shader/glslang.cpp). It writes the per-stage GLSL
# and SPIR-V to a directory and a manifest of what compiled. This is
# the authoritative "works today" the rslc census and corpus check are
# measured against; tools/rslc-census.py reads what it writes.
#
#   make
#   ./rslc_corpus_dump out/ path/to/a.slang path/to/b.slang ...
#   find ../../../../slang-shaders -name '*.slang' | ./rslc_corpus_dump out/ -
#
# No check target: the tool needs a corpus on disk to do anything.

TARGET := rslc_corpus_dump

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

C_SOURCES := rslc_corpus_dump.c \
             $(REPO_ROOT)/gfx/drivers_shader/glslang_util.c \
             $(LIBRETRO_COMM_DIR)/file/file_path.c \
             $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
             $(LIBRETRO_COMM_DIR)/string/stdstring.c \
             $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
             $(LIBRETRO_COMM_DIR)/time/rtime.c

CXX_SOURCES := $(REPO_ROOT)/gfx/drivers_shader/glslang.cpp

# glslang is the system's: newer releases carry MachineIndependent,
# OSDependent and GenericCodeGen in libglslang itself
GLSLANG_LIBS := -lglslang $(foreach l,MachineIndependent OSDependent GenericCodeGen OGLCompiler,\
                   $(if $(wildcard $(addsuffix /lib$(l).*,/usr/lib /usr/lib/$(shell $(CC) -dumpmachine) /usr/local/lib)),-l$(l))) \
                -lSPIRV -lSPIRV-Tools-opt -lSPIRV-Tools
ifeq ($(findstring mingw,$(CC)),)
   LDLIBS += -lpthread
endif
LDLIBS += $(GLSLANG_LIBS)

INCS := -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include

DEFS := -DRARCH_INTERNAL -DHAVE_SLANG -DHAVE_GLSLANG

# The harness itself is C89; the frontend units it links are built
# with the flags their own lanes use.
CFLAGS   += -Wall -O1 -g1 -D_GNU_SOURCE $(DEFS) $(INCS)
HARNESS_CFLAGS := -std=c89 -pedantic -Wno-long-long \
                  -Werror=declaration-after-statement \
                  -Werror=implicit-function-declaration
CXXFLAGS += -std=c++11 -O2 -w $(DEFS) $(INCS)
CXX      ?= g++

OBJS := $(call sample_objs,$(C_SOURCES:.c=.o)) \
        $(call sample_objs,$(CXX_SOURCES:.cpp=.o))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS) $(LDLIBS)

%.o: %.c
	$(CC) $(CFLAGS) $(HARNESS_CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).cpp
	@mkdir -p $(@D)
	$(CXX) $(CXXFLAGS) -c -o $@ $<

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET).exe rslc_corpus_dump.o

.PHONY: all clean

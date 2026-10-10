include ../../sample_objdir.mk

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# The real slang_process.c, slang_rect.c and glslang_util.c, the pass
# read from memory; SPIRV-Cross from the unity build's griffin_cpp.cpp,
# compiled once per configuration so each gets the HLSL emitter exactly
# when that build would.
C_SOURCES := slang_hlsl_backend_test.c \
             $(REPO_ROOT)/gfx/drivers_shader/slang_process.c \
             $(REPO_ROOT)/gfx/drivers_shader/slang_rect.c \
             $(REPO_ROOT)/gfx/drivers_shader/glslang_util.c \
             $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
             $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
             $(LIBRETRO_COMM_DIR)/file/file_path.c \
             $(LIBRETRO_COMM_DIR)/string/stdstring.c \
             $(LIBRETRO_COMM_DIR)/time/rtime.c

GRIFFIN_CPP := $(REPO_ROOT)/griffin/griffin_cpp.cpp

DEFINES  := -DRARCH_INTERNAL -DHAVE_SLANG -DHAVE_GLSLANG -DHAVE_SPIRV_CROSS
INCLUDES := -I. -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include \
            -I$(REPO_ROOT)/deps -I$(REPO_ROOT)/deps/SPIRV-Cross

CFLAGS   += -Wall -std=gnu99 -g -O1 $(DEFINES) $(INCLUDES)
# SPIRV-Cross is a vendored dependency, built plain and unoptimised:
# what is under test is which of its emitters a configuration gets.
DEPFLAGS := -O0 -w -DHAVE_SPIRV_CROSS $(INCLUDES)
# The amalgamation is past COFF's section count
ifneq ($(filter Windows_NT,$(OS))$(findstring mingw,$(CXX)),)
DEPFLAGS += -Wa,-mbig-obj
endif

ifneq ($(SANITIZER),)
   CFLAGS   := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS  := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

# Each build's Direct3D drivers, and what slang must hand them:
#   d3d11:  a Direct3D 11 build without the Direct3D 9 HLSL driver -
#           the pass comes back as HLSL
#   none:   no Direct3D at all - no HLSL emitter, and HLSL is refused
# Run end to end. The other configurations are held at the unity
# build's own switch: the C API wrapper's HLSL section, which is on
# exactly when spirv_hlsl.cpp is amalgamated.
RUN_LANES    := d3d11 none
d3d11_DEFS   := -DHAVE_D3D11
none_DEFS    :=
d3d11_EXPECT := -DEXPECT_HLSL
none_EXPECT  :=
HLSL_CONFIGS    := "-DHAVE_D3D10" "-DHAVE_D3D11" "-DHAVE_D3D12" \
                   "-DHAVE_D3D9 -DHAVE_HLSL"
NO_HLSL_CONFIGS := "-DHAVE_OPENGL" "-DHAVE_D3D9" \
                   "-DHAVE_OPENGL -DHAVE_VULKAN"

C_OBJS  := $(call sample_objs,$(filter-out slang_hlsl_backend_test.o,$(C_SOURCES:.c=.o)))
TARGETS := $(foreach l,$(RUN_LANES),hlsl_lane_$(l))

all: $(TARGETS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

spirv_cross_%.o: $(GRIFFIN_CPP)
	$(CXX) $(DEPFLAGS) $($*_DEFS) -c -o $@ $<

test_%.o: slang_hlsl_backend_test.c stock_spirv.h
	$(CC) $(CFLAGS) $($*_EXPECT) -c -o $@ $<

hlsl_lane_%: test_%.o spirv_cross_%.o $(C_OBJS)
	$(CXX) -o $@ $^ $(LDFLAGS)

check: $(TARGETS)
	@for l in $(RUN_LANES); do ./hlsl_lane_$$l || exit 1; done
	@for d in $(HLSL_CONFIGS); do \
	   $(CXX) $(DEPFLAGS) $$d -E -dM $(GRIFFIN_CPP) \
	      | grep -q '^#define SPIRV_CROSS_C_API_HLSL 1$$' \
	   || { echo "[FAIL] no HLSL emitter with $$d"; exit 1; }; \
	   echo "[pass] HLSL emitter with $$d"; \
	done
	@for d in $(NO_HLSL_CONFIGS); do \
	   $(CXX) $(DEPFLAGS) $$d -E -dM $(GRIFFIN_CPP) \
	      | grep -q '^#define SPIRV_CROSS_C_API_HLSL 0$$' \
	   || { echo "[FAIL] an HLSL emitter with $$d"; exit 1; }; \
	   echo "[pass] no HLSL emitter with $$d"; \
	done

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(C_OBJS) $(TARGETS) $(TARGETS:=.exe) test_*.o spirv_cross_*.o

.SECONDARY:
.PHONY: all check clean

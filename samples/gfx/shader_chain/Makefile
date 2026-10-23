include ../../sample_objdir.mk

# shader_chain_test: what a chain of passes costs to compile through the
# real slang_process.c, counted by the blob table - a .slang two passes
# name compiles once, and a chain built while the one it replaces is
# alive (append, remove, reload) compiles only the passes the live chain
# does not have.  rslc compiles; SPIRV-Cross links because
# slang_process.c names it; the frontend is stubbed, the sources are
# served from memory.
#
#   make check

TARGET := shader_chain_test

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := shader_chain_test.c \
           $(REPO_ROOT)/gfx/drivers_shader/slang_process.c \
           $(REPO_ROOT)/gfx/drivers_shader/slang_blob.c \
           $(REPO_ROOT)/gfx/drivers_shader/slang_rect.c \
           $(REPO_ROOT)/gfx/drivers_shader/glslang_util.c \
           $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_front.c \
           $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_ir.c \
           $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_emit.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c

CFLAGS  += -Wall -std=gnu99 -Wno-overlength-strings -g1 -O1 -DRARCH_INTERNAL -DHAVE_SLANG -DHAVE_RSLC -DHAVE_SPIRV_CROSS \
           -I. -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include -I$(LIBRETRO_COMM_DIR)/gfx/shaders \
           -I$(REPO_ROOT)/deps -I$(REPO_ROOT)/deps/SPIRV-Cross

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

DEPFLAGS := -O0 -w -std=c++11 -DHAVE_SPIRV_CROSS \
            -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)/deps \
            -I$(REPO_ROOT)/deps/SPIRV-Cross
ifneq ($(filter Windows_NT,$(OS))$(findstring mingw,$(CXX)),)
DEPFLAGS += -Wa,-mbig-obj
endif

all: $(TARGET)

spirv_cross.o: $(REPO_ROOT)/griffin/griffin_cpp.cpp
	$(CXX) $(DEPFLAGS) -c -o $@ $<

$(TARGET): $(OBJS) spirv_cross.o
	$(CXX) -o $@ $^ $(LDFLAGS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) spirv_cross.o $(TARGET) $(TARGET).exe

.PHONY: all check clean

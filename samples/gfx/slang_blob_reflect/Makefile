include ../../sample_objdir.mk

# slang_blob_reflect_check: the filter chain's reflection built from
# the compiler's tables against the one SPIRV-Cross builds from the
# same words, over every pass of a corpus dump (rslc's modules, and
# glslang's modules through the ingest).  Links the real
# slang_process.c with the frontend stubbed.
#
#   make
#   ./slang_blob_reflect_check <dumpdir>   the directory rslc_corpus_dump wrote
#
# No check target: the tool needs a corpus on disk to do anything.

TARGET := slang_blob_reflect_check

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := slang_blob_reflect_check.c \
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

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) spirv_cross.o $(TARGET) $(TARGET).exe

.PHONY: all clean

include ../../sample_objdir.mk

TARGET := slang_last_pass_target_test

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# The real gfx/drivers_shader/glslang_util.c; the sources the presets name
# are served from memory by the test, in place of video_shader_parse.c.
SOURCES := slang_last_pass_target_test.c feedback_test.c \
           $(REPO_ROOT)/gfx/drivers_shader/slang_process.c \
           $(REPO_ROOT)/gfx/drivers_shader/glslang_util.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS  += -Wall -std=gnu99 -g1 -O1 -DRARCH_INTERNAL -DHAVE_SLANG -ffunction-sections -fdata-sections -flto \
           -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)/deps/SPIRV-Cross

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

DEPFLAGS := -O0 -w -std=c++11 -DHAVE_SPIRV_CROSS -DHAVE_GLSLANG \
            -DWANT_GLSLANG -DHAVE_BUILTINGLSLANG \
            -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)/deps \
            -I$(REPO_ROOT)/deps/SPIRV-Cross
ifneq ($(filter Windows_NT,$(OS))$(findstring mingw,$(CXX)),)
DEPFLAGS += -Wa,-mbig-obj
endif
LDFLAGS += -Wl,--gc-sections

all: $(TARGET)

spirv_cross.o: $(REPO_ROOT)/griffin/griffin_cpp.cpp
	$(CXX) $(DEPFLAGS) -c -o $@ $<

glslang.o: $(REPO_ROOT)/griffin/griffin_glslang.cpp
	$(CXX) $(DEPFLAGS) -c -o $@ $<

$(TARGET): $(OBJS) spirv_cross.o glslang.o
	$(CXX) -flto -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) spirv_cross.o glslang.o $(TARGET) $(TARGET).exe

.PHONY: all check clean

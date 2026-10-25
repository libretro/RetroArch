include ../../sample_objdir.mk

TARGET := slang_last_pass_target_test

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# The real gfx/drivers_shader/glslang_util.c; the sources the presets name
# are served from memory by the test, in place of video_shader_parse.c.
SOURCES := slang_last_pass_target_test.c feedback_test.c \
           $(REPO_ROOT)/gfx/drivers_shader/slang_process.c \
           $(REPO_ROOT)/gfx/drivers_shader/slang_blob.c \
           $(REPO_ROOT)/gfx/drivers_shader/glslang_util.c \
           $(REPO_ROOT)/gfx/drivers_shader/slang_rect.c \
           $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_front.c \
           $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_ir.c \
           $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_emit.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS  += -Wall -std=gnu99 -Wno-overlength-strings -g1 -O1 -DRARCH_INTERNAL -DHAVE_SLANG -DHAVE_RSLC \
           -ffunction-sections -fdata-sections -flto \
           -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include -I$(LIBRETRO_COMM_DIR)/gfx/shaders

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

LDFLAGS += -Wl,--gc-sections

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -flto -o $@ $^ $(LDFLAGS) -lm

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

.PHONY: all check clean

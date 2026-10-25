include ../../sample_objdir.mk

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# gfx/drivers_shader/slang_rect.c as built, and the system's SPIRV-Cross
# (pkg-config spirv-cross-c-shared) with its HLSL emitter, which the D3D
# drivers' presets go through in a build that has it.
#
# GL=1 (the default) also draws each rewritten stage, with EGL
# and Mesa's surfaceless platform; GL=0 builds the checks without.
TARGET := slang_rect_test

GL ?= 1

C_SOURCES := slang_rect_test.c \
             $(REPO_ROOT)/gfx/drivers_shader/slang_rect.c
C_OBJS    := $(call sample_objs,$(C_SOURCES:.c=.o))

SPVC_CFLAGS := $(shell pkg-config --cflags spirv-cross-c-shared)
SPVC_LIBS   := $(shell pkg-config --libs spirv-cross-c-shared)

# The test and slang_rect.c are C89; the SPIRV-Cross header is not.
CFLAGS   += -Wall -Wextra -std=c89 -pedantic -g1 -O1 -I. \
            -I$(LIBRETRO_COMM_DIR)/include \
            $(patsubst -I%,-isystem %,$(SPVC_CFLAGS))
LDLIBS   += $(SPVC_LIBS)

ifeq ($(GL),1)
CFLAGS  += -DSLANG_RECT_TEST_GL
LDLIBS  += -lEGL -lGL
endif

ifneq ($(SANITIZER),)
   CFLAGS   := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS  := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

slang_rect_test.o: slang_rect_spirv.h

$(TARGET): $(C_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

check: $(TARGET)
	./$(TARGET) $(CHECK_ARGS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(C_OBJS) $(TARGET) $(TARGET).exe

.PHONY: all check clean

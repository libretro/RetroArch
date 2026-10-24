include ../../sample_objdir.mk

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# rslc's frame-rectangle remap, held to gfx/drivers_shader/slang_rect.c
# on the shaders of ../slang_rect/shaders (read at run time, so run from
# this directory). C89 throughout: no SPIRV-Cross here, rslc prints.
#
# GL=1 (the default on Linux) also draws each rewritten stage, with EGL
# and Mesa's surfaceless platform; GL=0 builds the checks without.
TARGET := rslc_remap_test

ifeq ($(OS),Windows_NT)
GL ?= 0
else ifneq ($(findstring mingw,$(CC)),)
GL ?= 0
else
GL ?= 1
endif

SOURCES := rslc_remap_test.c \
           $(REPO_ROOT)/gfx/drivers_shader/slang_rect.c \
           $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_front.c \
           $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_ir.c \
           $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_emit.c
OBJS    := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -Wextra -std=c89 -pedantic -Wno-long-long -Wno-overlength-strings \
           -D_GNU_SOURCE -g1 -O1 -Werror=implicit-function-declaration \
           -I$(LIBRETRO_COMM_DIR)/include -I$(LIBRETRO_COMM_DIR)/gfx/shaders
LDLIBS  += -lm

ifeq ($(GL),1)
CFLAGS  += -DRSLC_REMAP_TEST_GL
LDLIBS  += -lEGL -lGL
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

check: $(TARGET)
	./$(TARGET) $(CHECK_ARGS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

.PHONY: all check clean

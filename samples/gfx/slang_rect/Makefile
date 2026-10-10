include ../../sample_objdir.mk

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# gfx/drivers_shader/slang_rect.c as built, and SPIRV-Cross from the
# unity build's griffin_cpp.cpp with its HLSL emitter, which the D3D
# drivers' presets go through.
#
# GL=1 (the default on Linux) also draws each rewritten stage, with EGL
# and Mesa's surfaceless platform; GL=0 builds the checks without.
TARGET := slang_rect_test

ifeq ($(OS),Windows_NT)
GL ?= 0
else ifneq ($(findstring mingw,$(CC)),)
GL ?= 0
else
GL ?= 1
endif

C_SOURCES := slang_rect_test.c \
             $(REPO_ROOT)/gfx/drivers_shader/slang_rect.c
C_OBJS    := $(call sample_objs,$(C_SOURCES:.c=.o))

GRIFFIN_CPP := $(REPO_ROOT)/griffin/griffin_cpp.cpp

INCLUDES := -I. -I$(LIBRETRO_COMM_DIR)/include \
            -I$(REPO_ROOT)/deps -I$(REPO_ROOT)/deps/SPIRV-Cross

# The test and slang_rect.c are C89; the vendored headers are not.
CFLAGS   += -Wall -Wextra -std=c89 -pedantic -g -O1 -I. \
            -I$(LIBRETRO_COMM_DIR)/include -isystem $(REPO_ROOT)/deps/SPIRV-Cross
# SPIRV-Cross is a vendored dependency, built plain and unoptimised.
DEPFLAGS := -O0 -w -DHAVE_SPIRV_CROSS -DHAVE_D3D11 $(INCLUDES)
ifneq ($(filter Windows_NT,$(OS))$(findstring mingw,$(CXX)),)
DEPFLAGS += -Wa,-mbig-obj
endif

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

spirv_cross.o: $(GRIFFIN_CPP)
	$(CXX) $(DEPFLAGS) -c -o $@ $<

$(TARGET): $(C_OBJS) spirv_cross.o
	$(CXX) -o $@ $^ $(LDFLAGS) $(LDLIBS)

check: $(TARGET)
	./$(TARGET) $(CHECK_ARGS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(C_OBJS) spirv_cross.o $(TARGET) $(TARGET).exe

.PHONY: all check clean

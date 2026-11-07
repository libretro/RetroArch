# gfx/drivers_shader/glsl_rect.h's rewrite, and shader_glsl.c's first
# pass reading a view in place; both draw on an EGL device with GLES
# (Mesa's llvmpipe on the runners: EGL_PLATFORM=surfaceless), and skip
# without one.

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

CFLAGS  += -Wall -std=gnu99 -g1 -O1 -I$(LIBRETRO_COMM_DIR)/include
LDLIBS  := -lEGL -lGLESv2

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

TARGETS := glsl_rect_test glsl_in_place_test

all: $(TARGETS)

glsl_rect_test: glsl_rect_test.c $(CORE_DIR)/gfx/drivers_shader/glsl_rect.h
	$(CC) $(CFLAGS) -DGLSL_RECT_ORACLE -o $@ glsl_rect_test.c $(LDFLAGS) $(LDLIBS)

glsl_in_place_test: glsl_in_place_test.c \
		$(CORE_DIR)/gfx/drivers_shader/shader_glsl.c \
		$(CORE_DIR)/gfx/drivers_shader/glsl_rect.h
	$(CC) $(CFLAGS) -DHAVE_OPENGLES -DHAVE_OPENGLES3 -DHAVE_OPENGL \
		-DHAVE_GLSL -DRARCH_INTERNAL -I$(CORE_DIR) -I$(CORE_DIR)/gfx/include \
		-I$(CORE_DIR)/deps -o $@ glsl_in_place_test.c \
		$(LIBRETRO_COMM_DIR)/compat/compat_strl.c $(LDFLAGS) $(LDLIBS)

check: all
	EGL_PLATFORM=surfaceless ./glsl_rect_test
	EGL_PLATFORM=surfaceless ./glsl_in_place_test

clean:
	rm -f $(TARGETS)

.PHONY: all check clean

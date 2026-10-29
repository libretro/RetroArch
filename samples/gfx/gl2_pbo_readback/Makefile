# gfx/drivers/gl2.c's PBO readback ring against a scripted GL: the
# driver source is included by the test, the GL entry points it uses
# are the test's own, and the rest of the frontend is stubbed.

include ../../sample_objdir.mk

TARGET := gl2_pbo_readback_test

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

SOURCES_C := \
	gl2_pbo_readback_test.c \
	stubs.c \
	$(CORE_DIR)/gfx/common/gl_common.c \
	$(CORE_DIR)/gfx/video_views.c \
	$(LIBRETRO_COMM_DIR)/gfx/gl_capabilities.c \
	$(LIBRETRO_COMM_DIR)/glsym/glsym_gl.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_int.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_filter.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/pixconv.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/memmap/memalign.c

OBJS := $(call sample_objs,$(SOURCES_C:.c=.o))

INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR) -I$(CORE_DIR)/deps -I$(CORE_DIR)/gfx/include
CFLAGS  += -Wall $(INCDIRS) -std=gnu99 -D_GNU_SOURCE \
	-DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_CONFIGFILE -DHAVE_OPENGL -DHAVE_GL_SYNC -DHAVE_GLSL -DHAVE_MENU
LDFLAGS += -lm -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1 $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
else
   CFLAGS  += -g1 -O1
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

gl2_pbo_readback_test.o: $(CORE_DIR)/gfx/drivers/gl2.c $(CORE_DIR)/gfx/common/gl2_common.h gl11_fakes.h

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)
	CC="$(CC)" python3 gles3_init_test.py

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

.PHONY: all check clean

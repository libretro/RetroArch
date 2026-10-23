include ../../sample_objdir.mk

TARGET := rslc_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rslc_test.c \
	$(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_front.c \
	$(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_ir.c \
	$(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_emit.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

# The compiler is C89 everywhere it ships; the test is held to the
# same flags so a C99-ism cannot hide in it.
CFLAGS  += -Wall -Wextra -pedantic -std=c89 -Wno-long-long -Wno-overlength-strings -D_GNU_SOURCE \
	-Werror=implicit-function-declaration -g1 \
	-I$(LIBRETRO_COMM_DIR)/include -I$(LIBRETRO_COMM_DIR)/gfx/shaders

# Constant folding of the GLSL math builtins runs on libm.
LDLIBS  += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

# every object sees both headers, so a layout change rebuilds all of them
$(OBJS): $(LIBRETRO_COMM_DIR)/include/gfx/shaders/rslc.h $(LIBRETRO_COMM_DIR)/gfx/shaders/rslc_share.h

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean

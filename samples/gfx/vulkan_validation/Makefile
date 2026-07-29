TARGET := vulkan_validation_test

# Path back to the repo root from this sample dir.  Like the
# display_servers samples, this one links the real code rather than
# mirroring it -- the point is to run RetroArch's own Vulkan setup
# under the validation layer, so a copy would prove nothing.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# VULKAN_DEBUG is the flag RetroArch already uses to enable
# VK_LAYER_KHRONOS_validation and VK_EXT_debug_utils and to install
# vulkan_debug_cb().  That callback reports through RARCH_LOG, which
# stubs_retroarch.c supplies and scores, so the test needs no debug
# messenger of its own.
SOURCES := vulkan_validation_test.c \
           stubs_retroarch.c \
           $(REPO_ROOT)/gfx/common/vulkan_common.c \
           $(LIBRETRO_COMM_DIR)/vulkan/vulkan_symbol_wrapper.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/dynamic/dylib.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS  += -Wall -std=gnu99 -g -O0 \
           -DHAVE_VULKAN -DVULKAN_DEBUG -DHAVE_THREADS -DHAVE_DYLIB \
           -DHAVE_XLIB \
           -I$(REPO_ROOT) \
           -I$(LIBRETRO_COMM_DIR)/include

LDFLAGS += -lvulkan -lpthread -ldl -lX11

OBJS := $(SOURCES:.c=.o)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

# Needs an X display, a Vulkan device and the validation layer.  Xvfb
# plus lavapipe is enough, and both are packaged:
#   apt-get install xvfb mesa-vulkan-drivers vulkan-validationlayers
#   Xvfb :99 -screen 0 1280x720x24 &
# With no display or no device the test exits 77 (skipped) rather than
# failing, so it is safe to run unconditionally.
VK_DRIVER_FILES ?= /usr/share/vulkan/icd.d/lvp_icd.json

run: $(TARGET)
	VK_DRIVER_FILES=$(VK_DRIVER_FILES) ./$(TARGET)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: all run clean

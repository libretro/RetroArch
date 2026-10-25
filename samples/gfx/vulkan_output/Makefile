include ../../sample_objdir.mk

TARGET := vulkan_output_test

# gfx/common/vulkan_common.c's second output (vulkan_output_*), the
# real code against a fake device: every Vulkan entry point it calls
# is a pointer in libretro-common's symbol wrapper, and the test points
# them at fakes. No GPU, display or Vulkan loader needed.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := vulkan_output_test.c \
           stubs_win32.c \
           ../vulkan_validation/stubs_retroarch.c \
           $(REPO_ROOT)/gfx/common/vulkan_common.c \
           $(LIBRETRO_COMM_DIR)/vulkan/vulkan_symbol_wrapper.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/dynamic/dylib.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c

CFLAGS  += -Wall -std=gnu99 -g1 -O0 \
           -DHAVE_VULKAN -DHAVE_THREADS -DHAVE_DYLIB -DHAVE_VIDEO_WINDOWS \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/gfx/include \
           -I$(LIBRETRO_COMM_DIR)/include

# The fake surface is an Xlib one, but for mingw-w64 (Win32)
ifeq ($(findstring mingw,$(CC))$(filter Windows_NT,$(OS)),)
   CFLAGS  += -DHAVE_XLIB
   LDFLAGS += -lpthread -ldl
else
   SOURCES += $(REPO_ROOT)/gfx/common/d3d_driver_pacing.c \
              $(LIBRETRO_COMM_DIR)/gfx/nvidia/nvapi_loader.c
endif

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe $(OBJS)
	rm -rf $(SAMPLE_OBJDIR)

.PHONY: all check clean

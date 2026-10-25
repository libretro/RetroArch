include ../../sample_objdir.mk

TARGET := vulkan_upscaler_test

# Links the real gfx/common/vulkan_ngx.c and gfx/video_upscaler.c and
# drives them on a Vulkan device (lavapipe) against a stand-in NGX core
# library, so the glue between the core's inputs and NGX's parameters
# is pinned on every runner without an NVIDIA GPU.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := vulkan_upscaler_test.c \
           ../vulkan_validation/stubs_retroarch.c \
           $(REPO_ROOT)/gfx/common/vulkan_ngx.c \
           $(REPO_ROOT)/gfx/video_upscaler.c \
           $(LIBRETRO_COMM_DIR)/gfx/nvidia/ngx_loader.c \
           $(LIBRETRO_COMM_DIR)/vulkan/vulkan_symbol_wrapper.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c

ifneq ($(findstring mingw,$(CC)),)
   EXE    := .exe
   FAKE   := _nvngx.dll
   LDLIBS := -ladvapi32
   RUNNER ?= wine
else
   EXE    :=
   FAKE   := libnvidia-ngx.so.1
   LDLIBS := -lvulkan -lpthread -ldl -lm
   RUNNER ?= env LD_LIBRARY_PATH=.
endif

CFLAGS  += -Wall -std=gnu99 -g1 -O0 \
           -DHAVE_VULKAN -DHAVE_NGX -DHAVE_THREADS -DHAVE_DYLIB -DHAVE_MENU \
           -DRARCH_INTERNAL -DHAVE_CONFIGFILE \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/gfx/include \
           -I$(LIBRETRO_COMM_DIR)/include

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)$(EXE) $(FAKE)

_nvngx.dll: fake_ngx.c fake_ngx.h
	$(CC) -shared -O1 -Wall -o $@ $<

libnvidia-ngx.so.1: fake_ngx.c fake_ngx.h
	$(CC) -shared -fPIC -O1 -Wall -Wl,-soname,libnvidia-ngx.so.1 -o $@ $<

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET)$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

check: all
	$(RUNNER) ./$(TARGET)$(EXE)

clean:
	rm -f $(TARGET) $(TARGET).exe $(TARGET).o _nvngx.dll libnvidia-ngx.so.1
	rm -rf $(SAMPLE_OBJDIR) Zdir

.PHONY: all check clean

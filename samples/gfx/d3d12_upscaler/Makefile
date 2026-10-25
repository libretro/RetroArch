include ../../sample_objdir.mk

TARGET := d3d12_upscaler_test

# Links the real gfx/common/d3d12_ngx.c and gfx/video_upscaler.c and
# drives them on a Direct3D 12 device against a stand-in NGX core
# library, so the glue between the core's inputs and NGX's parameters
# is pinned on every runner without an NVIDIA GPU.
#
# Windows only. Elsewhere it is cross-built with mingw-w64 and run
# under Wine, where vkd3d implements D3D12 over Vulkan (lavapipe; Wine
# wants a display for it, see the workflow); with no device the run
# skips. SANITIZER is taken and ignored: mingw-w64 has none of them.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := d3d12_upscaler_test.c \
           ../vulkan_validation/stubs_retroarch.c \
           $(REPO_ROOT)/gfx/common/d3d12_ngx.c \
           $(REPO_ROOT)/gfx/video_upscaler.c \
           $(LIBRETRO_COMM_DIR)/gfx/nvidia/ngx_loader.c \
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

ifeq ($(OS),Windows_NT)
   RUNNER ?=
else
   CC     := x86_64-w64-mingw32-gcc
   RUNNER ?= env WINEDEBUG=-all wine
endif

CFLAGS  += -Wall -std=gnu99 -g1 -O0 \
           -DHAVE_D3D12 -DHAVE_NGX -DHAVE_THREADS -DHAVE_DYLIB -DHAVE_MENU \
           -DRARCH_INTERNAL -DHAVE_CONFIGFILE \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/gfx/include \
           -I$(REPO_ROOT)/gfx/include/dxsdk \
           -I$(LIBRETRO_COMM_DIR)/include
LDLIBS  += -ld3d12 -ldxguid -luuid -ladvapi32

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

all: $(TARGET).exe _nvngx.dll

_nvngx.dll: fake_ngx.c fake_ngx.h
	$(CC) -shared -O1 -Wall -o $@ $< -ldxguid

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET).exe: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

check: all
	$(RUNNER) ./$(TARGET).exe

clean:
	rm -f $(TARGET).exe $(TARGET).o _nvngx.dll
	rm -rf $(SAMPLE_OBJDIR) Zdir

.PHONY: all check clean

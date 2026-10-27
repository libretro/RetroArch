include ../../sample_objdir.mk

TARGET := d3d12_upscaler_test

# Links the real gfx/common/d3d12_ngx.c, d3d12_ffx.c, d3d12_xess.c,
# the dispatch of d3d12_upscaler.c and gfx/video_upscaler.c and drives
# them on a Direct3D 12 device against stand-in NGX, FidelityFX API
# and XeSS libraries (fake_ngx.c here; libretro-common's fake_ffx.c
# and fake_xess.c, planted under the SDKs' file names in Zdir_fsr and
# Zdir_xess), so the glue between the core's inputs and each library's
# parameters is pinned on every runner without the GPU.
#
# Windows only. Elsewhere it is cross-built with mingw-w64 and run
# under Wine, where vkd3d implements D3D12 over Vulkan (lavapipe; Wine
# wants a display for it, see the workflow); with no device the run
# skips. SANITIZER is taken and ignored: mingw-w64 has none of them.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := d3d12_upscaler_test.c \
           ../vulkan_validation/stubs_retroarch.c \
           $(REPO_ROOT)/gfx/common/d3d12_upscaler.c \
           $(REPO_ROOT)/gfx/common/d3d12_ngx.c \
           $(REPO_ROOT)/gfx/common/d3d12_ffx.c \
           $(REPO_ROOT)/gfx/common/d3d12_xess.c \
           $(REPO_ROOT)/gfx/common/ffx_common.c \
           $(REPO_ROOT)/gfx/common/xess_common.c \
           $(REPO_ROOT)/gfx/video_upscaler.c \
           $(LIBRETRO_COMM_DIR)/gfx/nvidia/ngx_loader.c \
           $(LIBRETRO_COMM_DIR)/gfx/amd/ffx_loader.c \
           $(LIBRETRO_COMM_DIR)/gfx/intel/xess_loader.c \
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
           -DHAVE_D3D12 -DHAVE_TEMPORAL_UPSCALER -DHAVE_THREADS -DHAVE_DYLIB -DHAVE_MENU \
           -DRARCH_INTERNAL -DHAVE_CONFIGFILE \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/gfx/include \
           -I$(REPO_ROOT)/gfx/include/dxsdk \
           -I$(LIBRETRO_COMM_DIR)/include
LDLIBS  += -ld3d12 -ldxguid -luuid -ladvapi32

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

FAKE_DLSS := $(LIBRETRO_COMM_DIR)/samples/gfx/nvidia/fake_dlss.c
FAKE_FFX  := $(LIBRETRO_COMM_DIR)/samples/gfx/amd/fake_ffx.c
FAKE_XESS := $(LIBRETRO_COMM_DIR)/samples/gfx/intel/fake_xess.c

all: $(TARGET).exe _nvngx.dll nvngx_dlss.dll Zdir_fsr/amd_fidelityfx_dx12.dll Zdir_xess/libxess.dll

# (with its file version resource, as the real one has)
nvngx_dlss.dll: $(FAKE_DLSS) $(FAKE_DLSS:.c=.rc)
	$(patsubst %gcc,%windres,$(CC)) -o fake_dlss_res.o $(FAKE_DLSS:.c=.rc)
	$(CC) -shared -O1 -Wall -o $@ $< fake_dlss_res.o

_nvngx.dll: fake_ngx.c fake_ngx.h
	$(CC) -shared -O1 -Wall -o $@ $< -ldxguid

Zdir_fsr/amd_fidelityfx_dx12.dll: $(FAKE_FFX) $(FAKE_FFX:.c=.h)
	@mkdir -p $(@D)
	$(CC) -shared -O1 -Wall -o $@ $<

Zdir_xess/libxess.dll: $(FAKE_XESS) $(FAKE_XESS:.c=.h)
	@mkdir -p $(@D)
	$(CC) -shared -O1 -Wall -o $@ $<

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
	rm -f $(TARGET).exe $(TARGET).o _nvngx.dll nvngx_dlss.dll fake_dlss_res.o
	rm -rf $(SAMPLE_OBJDIR) Zdir Zdir_fsr Zdir_xess Zdir_none

.PHONY: all check clean

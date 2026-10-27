include ../../sample_objdir.mk

TARGET := vulkan_upscaler_test

# Links the real gfx/common/vulkan_ngx.c, vulkan_ffx.c, vulkan_xess.c,
# the dispatch of vulkan_upscaler.c and gfx/video_upscaler.c and drives
# them on a Vulkan device (lavapipe) against stand-in NGX, FidelityFX
# API and XeSS libraries (fake_ngx.c here; libretro-common's fake_ffx.c
# and fake_xess.c, planted under the SDKs' file names in Zdir_fsr and
# Zdir_xess), so the glue between the core's inputs and each library's
# parameters is pinned on every runner without the GPU.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := vulkan_upscaler_test.c \
           ../vulkan_validation/stubs_retroarch.c \
           $(REPO_ROOT)/gfx/common/vulkan_upscaler.c \
           $(REPO_ROOT)/gfx/common/vulkan_ngx.c \
           $(REPO_ROOT)/gfx/common/vulkan_ffx.c \
           $(REPO_ROOT)/gfx/common/vulkan_xess.c \
           $(REPO_ROOT)/gfx/common/ffx_common.c \
           $(REPO_ROOT)/gfx/common/xess_common.c \
           $(REPO_ROOT)/gfx/video_upscaler.c \
           $(LIBRETRO_COMM_DIR)/gfx/nvidia/ngx_loader.c \
           $(LIBRETRO_COMM_DIR)/gfx/amd/ffx_loader.c \
           $(LIBRETRO_COMM_DIR)/gfx/intel/xess_loader.c \
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
   FAKE   := _nvngx.dll nvngx_dlss.dll Zdir_fsr/amd_fidelityfx_vk.dll Zdir_xess/libxess.dll
   LDLIBS := -ladvapi32
   RUNNER ?= wine
   SOFLAGS :=
else
   EXE    :=
   FAKE   := libnvidia-ngx.so.1 libnvidia-ngx-dlss.so.310.2.1 Zdir_fsr/libamd_fidelityfx_vk.so Zdir_xess/libxess.so
   LDLIBS := -lvulkan -lpthread -ldl -lm
   RUNNER ?= env LD_LIBRARY_PATH=.
   SOFLAGS := -fPIC
endif

FAKE_DLSS := $(LIBRETRO_COMM_DIR)/samples/gfx/nvidia/fake_dlss.c
FAKE_FFX  := $(LIBRETRO_COMM_DIR)/samples/gfx/amd/fake_ffx.c
FAKE_XESS := $(LIBRETRO_COMM_DIR)/samples/gfx/intel/fake_xess.c

CFLAGS  += -Wall -std=gnu99 -g1 -O0 \
           -DHAVE_VULKAN -DHAVE_TEMPORAL_UPSCALER -DHAVE_THREADS -DHAVE_DYLIB -DHAVE_MENU \
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

# (with its file version resource on Windows, as the real one has)
nvngx_dlss.dll: $(FAKE_DLSS) $(FAKE_DLSS:.c=.rc)
	$(patsubst %gcc,%windres,$(CC)) -o fake_dlss_res.o $(FAKE_DLSS:.c=.rc)
	$(CC) -shared -O1 -Wall -o $@ $< fake_dlss_res.o

libnvidia-ngx-dlss.so.310.2.1: $(FAKE_DLSS)
	$(CC) -shared $(SOFLAGS) -O1 -Wall -o $@ $<

Zdir_fsr/amd_fidelityfx_vk.dll Zdir_fsr/libamd_fidelityfx_vk.so: $(FAKE_FFX) $(FAKE_FFX:.c=.h)
	@mkdir -p $(@D)
	$(CC) -shared $(SOFLAGS) -O1 -Wall -o $@ $<

Zdir_xess/libxess.dll Zdir_xess/libxess.so: $(FAKE_XESS) $(FAKE_XESS:.c=.h)
	@mkdir -p $(@D)
	$(CC) -shared $(SOFLAGS) -O1 -Wall -o $@ $<

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
	rm -f $(TARGET) $(TARGET).exe $(TARGET).o _nvngx.dll libnvidia-ngx.so.1 nvngx_dlss.dll libnvidia-ngx-dlss.so.310.2.1 fake_dlss_res.o
	rm -rf $(SAMPLE_OBJDIR) Zdir Zdir_fsr Zdir_xess Zdir_none Zdir_keep

.PHONY: all check clean

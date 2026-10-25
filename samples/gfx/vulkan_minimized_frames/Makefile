include ../../sample_objdir.mk

TARGET := vulkan_minimized_frames_test

# Links the real gfx/common/vulkan_common.c, as vulkan_driver_pacing
# does, and drives vulkan_acquire_next_image() through fake entry
# points for a window with a 0x0 surface: no GPU, no loader.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := vulkan_minimized_frames_test.c \
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
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

# Windows (mingw-w64, run under Wine): the Win32 WSI build of the file
ifneq ($(findstring mingw,$(CC)),)
   EXE     := .exe
   SOURCES += $(REPO_ROOT)/gfx/common/d3d_driver_pacing.c \
              $(LIBRETRO_COMM_DIR)/gfx/nvidia/nvapi_loader.c \
              $(LIBRETRO_COMM_DIR)/time/rtime.c
   LDLIBS  := -lm
else
   EXE     :=
   LDLIBS  := -lpthread -ldl -lm
endif

CFLAGS  += -Wall -std=gnu99 -g1 -O0 \
           -DHAVE_VULKAN -DHAVE_THREADS -DHAVE_DYLIB \
           -I$(REPO_ROOT) \
           -I$(REPO_ROOT)/gfx/include \
           -I$(LIBRETRO_COMM_DIR)/include

LDFLAGS += $(LDLIBS)

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)$(EXE)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET)$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)$(EXE)

clean:
	rm -f $(TARGET) $(TARGET).exe vulkan_minimized_frames_test.o
	rm -rf $(SAMPLE_OBJDIR)

.PHONY: all check clean

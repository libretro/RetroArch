include ../../sample_objdir.mk

TARGET := vulkan_chain_alloc_test

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# gfx/drivers_shader/shader_vulkan.c compiled into the test, its Vulkan
# calls through the real symbol wrapper pointed at the test's mocks, and
# the real slang_rect.c it rewrites stages with and the SPIR-V it loads.
SOURCES := vulkan_chain_alloc_test.c \
           stubs.c \
           $(REPO_ROOT)/gfx/drivers_shader/slang_rect.c \
           $(REPO_ROOT)/gfx/drivers_shader/slang_blob.c \
           $(REPO_ROOT)/gfx/common/spirv_shaders.c \
           $(LIBRETRO_COMM_DIR)/vulkan/vulkan_symbol_wrapper.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c

CFLAGS  += -Wall -Wno-unused-function -std=gnu99 -g1 -O1 -DRARCH_INTERNAL \
           -DHAVE_THREADS -DHAVE_VULKAN -DHAVE_SLANG -DHAVE_RSLC \
           -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include \
           -I$(LIBRETRO_COMM_DIR)/gfx/shaders -I$(REPO_ROOT)/gfx/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

vulkan_chain_alloc_test.o: $(REPO_ROOT)/gfx/drivers_shader/shader_vulkan.c

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

.PHONY: all check clean

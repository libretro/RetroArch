TARGET := vulkan_chain_alloc_test

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# gfx/drivers_shader/shader_vulkan.c compiled into the test, its Vulkan
# calls through the real symbol wrapper pointed at the test's mocks.
SOURCES := vulkan_chain_alloc_test.c \
           stubs.c \
           $(LIBRETRO_COMM_DIR)/vulkan/vulkan_symbol_wrapper.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c

CFLAGS  += -Wall -Wno-unused-function -std=gnu99 -g -O1 -DRARCH_INTERNAL \
           -DHAVE_THREADS -DHAVE_VULKAN -DHAVE_SLANG -DHAVE_GLSLANG \
           -DHAVE_SPIRV_CROSS \
           -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)/deps \
           -I$(REPO_ROOT)/deps/SPIRV-Cross -I$(REPO_ROOT)/gfx/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

vulkan_chain_alloc_test.o: $(REPO_ROOT)/gfx/drivers_shader/shader_vulkan.c

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

.PHONY: all check clean

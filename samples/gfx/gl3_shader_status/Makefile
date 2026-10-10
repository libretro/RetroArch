TARGET       := gl3_shader_status_test
CHAIN_TARGET := gl3_chain_alloc_test

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# The real gfx/drivers_shader/shader_gl3.c, with GL entry points through
# the real glsym table the test points at its own mocks, and SPIRV-Cross
# and the rest of the frontend stubbed in stubs.c.
COMMON  := stubs.c \
           $(REPO_ROOT)/gfx/drivers_shader/slang_rect.c \
           spvc_stubs.c \
           $(LIBRETRO_COMM_DIR)/glsym/glsym_gl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c
SOURCES := gl3_shader_status_test.c \
           $(REPO_ROOT)/gfx/drivers_shader/shader_gl3.c \
           $(COMMON)
# The chain allocations: shader_gl3.c compiled into the test itself,
# with calloc() routed through it.
CHAIN_SOURCES := gl3_chain_alloc_test.c $(COMMON)

CFLAGS  += -Wall -std=gnu99 -g -O1 -DRARCH_INTERNAL \
           -DHAVE_OPENGL_CORE -DHAVE_SLANG -DHAVE_SPIRV_CROSS -DHAVE_GLSLANG \
           -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)/deps \
           -I$(REPO_ROOT)/deps/SPIRV-Cross -I$(REPO_ROOT)/gfx/include \
           -I$(REPO_ROOT)/gfx/include/MESA

# <GL/gl.h> is the tree's own copy under gfx/include/MESA, so the test
# builds with no GL development package installed.
# The GL 1.1 entry points glsym does not carry: Windows has them from
# opengl32, elsewhere stubs.c defines them.
ifeq ($(OS),Windows_NT)
LDFLAGS += -lopengl32
else
CFLAGS  += -DSTUB_GL11
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS       := $(SOURCES:.c=.o)
CHAIN_OBJS := $(CHAIN_SOURCES:.c=.o)

all: $(TARGET) $(CHAIN_TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(CHAIN_TARGET): $(CHAIN_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

gl3_chain_alloc_test.o: $(REPO_ROOT)/gfx/drivers_shader/shader_gl3.c

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET) $(CHAIN_TARGET)
	./$(TARGET)
	./$(CHAIN_TARGET)

clean:
	rm -f $(OBJS) $(CHAIN_OBJS) $(TARGET) $(TARGET).exe \
	      $(CHAIN_TARGET) $(CHAIN_TARGET).exe

.PHONY: all check clean

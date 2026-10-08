TARGET := gl3_shader_status_test

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# The real gfx/drivers_shader/shader_gl3.c, with GL entry points through
# the real glsym table the test points at its own mocks, and SPIRV-Cross
# and the rest of the frontend stubbed in stubs.c.
SOURCES := gl3_shader_status_test.c \
           stubs.c \
           $(REPO_ROOT)/gfx/drivers_shader/shader_gl3.c \
           $(LIBRETRO_COMM_DIR)/glsym/glsym_gl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c

CFLAGS  += -Wall -std=gnu99 -g -O1 -DRARCH_INTERNAL \
           -DHAVE_OPENGL_CORE -DHAVE_SLANG -DHAVE_SPIRV_CROSS -DHAVE_GLSLANG \
           -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include -I$(REPO_ROOT)/deps \
           -I$(REPO_ROOT)/deps/SPIRV-Cross -I$(REPO_ROOT)/gfx/include

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

OBJS := $(SOURCES:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

.PHONY: all check clean

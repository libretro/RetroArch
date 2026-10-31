include ../../sample_objdir.mk

TARGET := vulkan_custom_driver_test

# The pure half of the Android custom Vulkan driver loader, fed from
# buffers: package checks, the extraction key, the crash state and the
# session's choices. Builds on Linux and with mingw-w64.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := vulkan_custom_driver_test.c \
           $(REPO_ROOT)/gfx/common/vulkan_custom_driver.c \
           $(LIBRETRO_COMM_DIR)/formats/json/rjson.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c

ifneq ($(findstring mingw,$(CC)),)
   EXE := .exe
else
   EXE :=
endif

CFLAGS += -Wall -g1 -O0 -I$(LIBRETRO_COMM_DIR)/include

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

# libretro-common is built as the tree builds it; vulkan_custom_driver.c
# and the test as C89
STRICT_OBJS  := vulkan_custom_driver_test.o \
                $(call sample_objs,$(REPO_ROOT)/gfx/common/vulkan_custom_driver.o)
STRICT_FLAGS := -std=c89 -pedantic -Wdeclaration-after-statement \
                -Wframe-larger-than=4096 -Werror
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
# boolean.h uses the compiler _Bool spelling in C89 builds.
STRICT_FLAGS += -Wno-c99-extensions
endif
$(STRICT_OBJS): CFLAGS += $(STRICT_FLAGS)

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
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean

include ../../sample_objdir.mk

TARGET  := vulkan_custom_driver_test
DTARGET := vulkan_custom_driver_desktop_test

# The pure half of the custom Vulkan driver loader, fed from buffers:
# package and ICD manifest checks, the extraction key, the crash state,
# the session's choices and the loader copy's soname patch; and the
# desktop glue against stand-in drivers. Builds on Linux and with
# mingw-w64.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

SOURCES := vulkan_custom_driver_test.c \
           $(REPO_ROOT)/gfx/common/vulkan_custom_driver.c \
           $(REPO_ROOT)/gfx/common/vulkan_custom_driver_linker.c \
           $(LIBRETRO_COMM_DIR)/formats/json/rjson.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c

# The desktop glue with real library loads: stand-in drivers built from
# fake_icd.c, named by ICD manifests the test writes.
DSOURCES := vulkan_custom_driver_desktop_test.c \
            $(REPO_ROOT)/gfx/common/vulkan_custom_driver.c \
            $(REPO_ROOT)/gfx/common/vulkan_custom_driver_glue.c \
            $(REPO_ROOT)/gfx/common/vulkan_custom_driver_desktop.c \
            $(LIBRETRO_COMM_DIR)/formats/json/rjson.c \
            $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
            $(LIBRETRO_COMM_DIR)/string/stdstring.c \
            $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
            $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
            $(LIBRETRO_COMM_DIR)/lists/string_list.c \
            $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
            $(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
            $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
            $(LIBRETRO_COMM_DIR)/file/file_path.c \
            $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
            $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
            $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
            $(LIBRETRO_COMM_DIR)/dynamic/dylib.c \
            $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
            $(LIBRETRO_COMM_DIR)/time/rtime.c

ifneq ($(findstring mingw,$(CC)),)
   EXE     := .exe
   FAKE    := fake_icd.dll fake_icd_layer.dll
   DLDLIBS :=
else
   EXE     :=
   FAKE    := libfake_icd.so libfake_icd_layer.so
   DLDLIBS := -ldl
endif

CFLAGS += -Wall -g1 -O0 -I$(LIBRETRO_COMM_DIR)/include

OBJS  := $(call sample_objs,$(SOURCES:.c=.o))
DOBJS := $(call sample_objs,$(DSOURCES:.c=.o))

# libretro-common is built as the tree builds it; vulkan_custom_driver*.c
# and the test as C89
STRICT_OBJS  := vulkan_custom_driver_test.o \
                $(call sample_objs,$(REPO_ROOT)/gfx/common/vulkan_custom_driver.o) \
                $(call sample_objs,$(REPO_ROOT)/gfx/common/vulkan_custom_driver_linker.o) \
                vulkan_custom_driver_desktop_test.o \
                $(call sample_objs,$(REPO_ROOT)/gfx/common/vulkan_custom_driver_glue.o) \
                $(call sample_objs,$(REPO_ROOT)/gfx/common/vulkan_custom_driver_desktop.o)
STRICT_FLAGS := -std=c89 -pedantic -Wdeclaration-after-statement \
                -Wframe-larger-than=4096 -Werror
ifneq ($(findstring clang,$(shell $(CC) --version 2>/dev/null)),)
# boolean.h uses the compiler _Bool spelling in C89 builds.
STRICT_FLAGS += -Wno-c99-extensions -Wno-c11-extensions
endif
$(STRICT_OBJS): CFLAGS += $(STRICT_FLAGS)

# The desktop glue as the tree builds it: Vulkan, dynamic loading, and on
# glibc the dladdr() _GNU_SOURCE declares
DESKTOP_FLAGS := -I$(REPO_ROOT) -I$(REPO_ROOT)/gfx/include \
                 -DHAVE_VULKAN -DHAVE_VULKAN_CUSTOM_DRIVER -DHAVE_DYLIB \
                 -DRARCH_INTERNAL -Wno-long-long
ifeq ($(findstring mingw,$(CC)),)
DESKTOP_FLAGS += -D_GNU_SOURCE
endif
$(DOBJS): CFLAGS += $(DESKTOP_FLAGS)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)$(EXE) $(DTARGET)$(EXE) $(FAKE)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET)$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(DTARGET)$(EXE): $(DOBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(DLDLIBS)

# Stand-in drivers: no sanitizer, nothing of the test linked in
FAKE_FLAGS := -std=c89 -pedantic -Wall -Werror -O1 -shared
ifeq ($(findstring mingw,$(CC)),)
FAKE_FLAGS += -fPIC -fvisibility=hidden
endif

libfake_icd.so fake_icd.dll: fake_icd.c
	$(CC) $(FAKE_FLAGS) -o $@ $<

libfake_icd_layer.so fake_icd_layer.dll: fake_icd.c
	$(CC) $(FAKE_FLAGS) -DFAKE_ICD_LAYER -o $@ $<

check: all
	$(RUNNER) ./$(TARGET)$(EXE)
	$(RUNNER) ./$(DTARGET)$(EXE)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET).exe $(OBJS) $(DTARGET) $(DTARGET).exe \
	      vulkan_custom_driver_desktop_test.o libfake_icd.so \
	      libfake_icd_layer.so fake_icd.dll fake_icd_layer.dll

.PHONY: all check clean

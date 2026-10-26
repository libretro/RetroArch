include ../../sample_objdir.mk

TARGET := nvidia_loader_test

LIBRETRO_COMM_DIR := ../../..

# The loaders against stand-in libraries: the NGX core as the driver
# ships it (_nvngx.dll beside the program on Windows, libnvidia-ngx.so.1
# on Linux, found through LD_LIBRARY_PATH=. at run time) and nvapi64.dll
# on Windows. The loaders are C89 everywhere they ship and are held to
# it here.
SOURCES := \
	nvidia_loader_test.c \
	$(LIBRETRO_COMM_DIR)/gfx/nvidia/ngx_loader.c \
	$(LIBRETRO_COMM_DIR)/gfx/nvidia/nvapi_loader.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/file/retro_dirent.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

ifneq ($(findstring mingw,$(CC)),)
   EXE     := .exe
   FAKES   := _nvngx.dll nvapi64.dll nvidia_loader_test_feature/nvngx_dlss.dll \
              nvidia_loader_test_snippet/nvngx_dlss.dll nvidia_loader_test_feature/OptiScaler.dll
   LDLIBS  := -ladvapi32
   RUNNER  ?= wine
   WINDRES ?= $(patsubst %gcc,%windres,$(CC))
else
   EXE     :=
   FAKES   := libnvidia-ngx.so.1 nvidia_loader_test_feature/libnvidia-ngx-dlss.so.310.2.1 \
              nvidia_loader_test_snippet/libnvidia-ngx-dlss.so
   LDLIBS  := -ldl
   RUNNER  ?= env LD_LIBRARY_PATH=.
endif

CFLAGS += -Wall -pedantic -std=c89 -Wno-long-long -Wno-variadic-macros -D_GNU_SOURCE -g1 \
	-Werror=implicit-function-declaration \
	-I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)$(EXE) $(FAKES)

# (the core stand-in carries OptiScaler's product name on Windows, so
# the version-resource check runs under Wine)
_nvngx.dll: fake_ngx.c fake_ngx.rc
	$(WINDRES) -o fake_ngx_res.o fake_ngx.rc
	$(CC) -shared -O1 -Wall -o $@ $< fake_ngx_res.o

# (one with the file version resource, one without: the snippet word)
nvidia_loader_test_feature/nvngx_dlss.dll: fake_dlss.c fake_dlss.rc
	@mkdir -p $(@D)
	$(WINDRES) -o fake_dlss_res.o fake_dlss.rc
	$(CC) -shared -O1 -Wall -o $@ $< fake_dlss_res.o

nvidia_loader_test_snippet/nvngx_dlss.dll: fake_dlss.c
	@mkdir -p $(@D)
	$(CC) -shared -O1 -Wall -o $@ $<

# (OptiScaler under its own name in the DLSS directory: any library
# will do for the load)
nvidia_loader_test_feature/OptiScaler.dll: fake_dlss.c
	@mkdir -p $(@D)
	$(CC) -shared -O1 -Wall -o $@ $<

# (one with the version in its name, one without: the snippet word)
nvidia_loader_test_feature/libnvidia-ngx-dlss.so.310.2.1: fake_dlss.c
	@mkdir -p $(@D)
	$(CC) -shared -fPIC -O1 -Wall -o $@ $<

nvidia_loader_test_snippet/libnvidia-ngx-dlss.so: fake_dlss.c
	@mkdir -p $(@D)
	$(CC) -shared -fPIC -O1 -Wall -o $@ $<

nvapi64.dll: fake_nvapi.c
	$(CC) -shared -O1 -Wall -o $@ $<

libnvidia-ngx.so.1: fake_ngx.c
	$(CC) -shared -fPIC -O1 -Wall -Wl,-soname,libnvidia-ngx.so.1 -o $@ $<

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
	rm -f $(TARGET) $(TARGET).exe $(TARGET).o _nvngx.dll nvapi64.dll libnvidia-ngx.so.1 fake_ngx_res.o fake_dlss_res.o
	rm -rf $(SAMPLE_OBJDIR) nvidia_loader_test_dir nvidia_loader_test_feature nvidia_loader_test_snippet

.PHONY: all check clean

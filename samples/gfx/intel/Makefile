include ../../sample_objdir.mk

TARGET := xess_loader_test

LIBRETRO_COMM_DIR := ../../..

# The XeSS loader against a stand-in library (fake_xess.c), planted by
# the test under the SDK's file name in a directory of its own, as the
# user's runtime directory is. The loader is C89 everywhere it ships
# and is held to it here.
SOURCES := \
	xess_loader_test.c \
	$(LIBRETRO_COMM_DIR)/gfx/intel/xess_loader.c \
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
   EXE    := .exe
   FAKE   := fake_xess.dll
   LDLIBS :=
   RUNNER ?= wine
else
   EXE    :=
   FAKE   := libfake_xess.so
   LDLIBS := -ldl
   RUNNER ?=
endif

CFLAGS += -Wall -pedantic -std=c89 -Wno-long-long -Wno-variadic-macros -D_GNU_SOURCE -g1 \
	-Werror=implicit-function-declaration \
	-I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)$(EXE) $(FAKE)

fake_xess.dll: fake_xess.c fake_xess.h
	$(CC) -shared -O1 -Wall -o $@ $<

libfake_xess.so: fake_xess.c fake_xess.h
	$(CC) -shared -fPIC -O1 -Wall -o $@ $<

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
	rm -f $(TARGET) $(TARGET).exe $(TARGET).o fake_xess.dll libfake_xess.so
	rm -rf $(SAMPLE_OBJDIR) xess_loader_test_dir

.PHONY: all check clean

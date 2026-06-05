CC      ?= gcc
CFLAGS  ?= -g
LDFLAGS ?=

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

SOURCES := \
	image_core.c \
	../../libretro-common/compat/compat_strcasestr.c \
	../../libretro-common/compat/compat_strl.c \
	../../libretro-common/compat/fopen_utf8.c \
	../../libretro-common/encodings/encoding_utf.c \
	../../libretro-common/file/file_path.c \
	../../libretro-common/file/file_path_io.c \
	../../libretro-common/file/retro_dirent.c \
	../../libretro-common/lists/dir_list.c \
	../../libretro-common/lists/string_list.c \
	../../libretro-common/streams/file_stream.c \
	../../libretro-common/time/rtime.c \
	../../libretro-common/vfs/vfs_implementation.c

INCLUDES := \
	-I../../libretro-common/include/ \
	-I../../deps/stb/

DEFINES := -DHAVE_STB_IMAGE

TARGET := image_core.so

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(DEFINES) $(INCLUDES) $(SOURCES) \
		-shared -fPIC -Wl,--no-undefined -lm \
		$(LDFLAGS) -o $(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all clean

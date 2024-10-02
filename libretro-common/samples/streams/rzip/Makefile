TARGET := rzip

LIBRETRO_COMM_DIR := ../../..
LIBRETRO_DEPS_DIR := ../../../../deps

# Attempt to detect target platform
ifeq '$(findstring ;,$(PATH))' ';'
	UNAME := Windows
else
	UNAME := $(shell uname 2>/dev/null || echo Unknown)
	UNAME := $(patsubst CYGWIN%,Cygwin,$(UNAME))
	UNAME := $(patsubst MSYS%,MSYS,$(UNAME))
	UNAME := $(patsubst MINGW%,MSYS,$(UNAME))
endif

# Add '.exe' extension on Windows platforms
ifeq ($(UNAME), Windows)
	TARGET := rzip.exe
endif
ifeq ($(UNAME), MSYS)
	TARGET := rzip.exe
endif

SOURCES := \
	rzip.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream_transforms.c \
	$(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/rzip_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/stdin_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
	$(LIBRETRO_COMM_DIR)/streams/trans_stream_zlib.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

ifneq ($(wildcard $(LIBRETRO_DEPS_DIR)/*),)
	# If we are building from inside the RetroArch
	# directory (i.e. if an 'external' deps directory
	# is available), bake in zlib support
	SOURCES += \
		$(LIBRETRO_DEPS_DIR)/libz/adler32.c \
		$(LIBRETRO_DEPS_DIR)/libz/libz-crc32.c \
		$(LIBRETRO_DEPS_DIR)/libz/deflate.c \
		$(LIBRETRO_DEPS_DIR)/libz/gzclose.c \
		$(LIBRETRO_DEPS_DIR)/libz/gzlib.c \
		$(LIBRETRO_DEPS_DIR)/libz/gzread.c \
		$(LIBRETRO_DEPS_DIR)/libz/gzwrite.c \
		$(LIBRETRO_DEPS_DIR)/libz/inffast.c \
		$(LIBRETRO_DEPS_DIR)/libz/inflate.c \
		$(LIBRETRO_DEPS_DIR)/libz/inftrees.c \
		$(LIBRETRO_DEPS_DIR)/libz/trees.c \
		$(LIBRETRO_DEPS_DIR)/libz/zutil.c
	INCLUDE_DIRS := -I$(LIBRETRO_COMM_DIR)/include/compat/zlib
else
	# If this is a stand-alone libretro-common directory,
	# rely on system zlib library (note: only likely to
	# work on Unix-based platforms...)
	LDFLAGS += -lz
endif

OBJS := $(SOURCES:.c=.o)
INCLUDE_DIRS += -I$(LIBRETRO_COMM_DIR)/include
CFLAGS += -DHAVE_ZLIB -Wall -pedantic -std=gnu99 $(INCLUDE_DIRS)

# Silence "ISO C does not support the 'I64' ms_printf length modifier"
# warnings when using MinGW
ifeq ($(UNAME), Windows)
	CFLAGS += -Wno-format
endif
ifeq ($(UNAME), MSYS)
	CFLAGS += -Wno-format
endif

ifeq ($(DEBUG), 1)
	CFLAGS += -O0 -g -DDEBUG -D_DEBUG
else
	CFLAGS += -O2 -DNDEBUG
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

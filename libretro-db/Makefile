DEBUG                = 0
LIBRETRODB_DIR      := .
LIBRETRO_COMM_DIR   := ../libretro-common
INCFLAGS             = -I. -I$(LIBRETRO_COMM_DIR)/include

TARGETS              = rmsgpack_test libretrodb_tool c_converter

ifeq ($(DEBUG), 1)
CFLAGS               = -g -O0 -Wall
else
CFLAGS               = -g -O2 -Wall -DNDEBUG
endif

LIBRETRO_COMMON_C = \
			 $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
			 $(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
			 $(LIBRETRO_COMM_DIR)/file/file_path.c \
			 $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
			 $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c \
			 $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
			 $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
			 $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c

C_CONVERTER_C = \
			 $(LIBRETRODB_DIR)/rmsgpack.c \
			 $(LIBRETRODB_DIR)/rmsgpack_dom.c \
			 $(LIBRETRODB_DIR)/libretrodb.c \
			 $(LIBRETRODB_DIR)/bintree.c \
			 $(LIBRETRODB_DIR)/query.c \
			 $(LIBRETRODB_DIR)/c_converter.c \
			 $(LIBRETRO_COMM_DIR)/hash/rhash.c \
			 $(LIBRETRO_COMM_DIR)/compat/compat_fnmatch.c \
			 $(LIBRETRO_COMM_DIR)/string/stdstring.c \
			 $(LIBRETRO_COMMON_C)

C_CONVERTER_OBJS := $(C_CONVERTER_C:.c=.o)

RARCHDB_TOOL_C = \
			 $(LIBRETRODB_DIR)/rmsgpack.c \
			 $(LIBRETRODB_DIR)/rmsgpack_dom.c \
			 $(LIBRETRODB_DIR)/libretrodb_tool.c \
			 $(LIBRETRODB_DIR)/bintree.c \
			 $(LIBRETRODB_DIR)/query.c \
			 $(LIBRETRODB_DIR)/libretrodb.c \
			 $(LIBRETRO_COMM_DIR)/compat/compat_fnmatch.c \
			 $(LIBRETRO_COMM_DIR)/string/stdstring.c \
			 $(LIBRETRO_COMMON_C)

RARCHDB_TOOL_OBJS := $(RARCHDB_TOOL_C:.c=.o)

RMSGPACK_C = \
			$(LIBRETRODB_DIR)/rmsgpack.c \
			$(LIBRETRODB_DIR)/rmsgpack_test.c \
			 $(LIBRETRO_COMMON_C)

RMSGPACK_OBJS := $(RMSGPACK_C:.c=.o)

TESTLIB_FLAGS = $(CFLAGS) -shared -fpic

.PHONY: all clean

all: $(TARGETS)

%.o: %.c
	$(CC) $(INCFLAGS) $< -c $(CFLAGS) -o $@

c_converter: $(C_CONVERTER_OBJS)
	$(CC) $(INCFLAGS) $(C_CONVERTER_OBJS) $(CFLAGS) -o $@

libretrodb_tool: $(RARCHDB_TOOL_OBJS)
	$(CC) $(INCFLAGS) $(RARCHDB_TOOL_OBJS) -o $@

rmsgpack_test: $(RMSGPACK_OBJS)
	$(CC) $(INCFLAGS) $(RMSGPACK_OBJS) -g -o $@

clean:
	rm -rf $(TARGETS) $(C_CONVERTER_OBJS) $(RARCHDB_TOOL_OBJS) $(RMSGPACK_OBJS) $(TESTLIB_OBJS)

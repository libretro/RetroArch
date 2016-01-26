DEBUG                = 0
LIBRETRO_COMMON_DIR := ../libretro-common
INCFLAGS             = -I. -I$(LIBRETRO_COMMON_DIR)/include

TARGETS              = rmsgpack_test libretrodb_tool c_converter

ifeq ($(DEBUG), 1)
CFLAGS               = -g -O0 -Wall
else
CFLAGS               = -g -O2 -Wall -DNDEBUG
endif

C_CONVERTER_C = \
			 rmsgpack.c \
			 rmsgpack_dom.c \
			 libretrodb.c \
			 bintree.c \
			 query.c \
			 c_converter.c \
			 $(LIBRETRO_COMMON_DIR)/hash/rhash.c \
			 $(LIBRETRO_COMMON_DIR)/compat/compat_fnmatch.c \
			 $(LIBRETRO_COMMON_DIR)/file/retro_file.c \
			 $(LIBRETRO_COMMON_DIR)/compat/compat_strl.c

C_CONVERTER_OBJS := $(C_CONVERTER_C:.c=.o)

RARCHDB_TOOL_C = \
			 rmsgpack.c \
			 rmsgpack_dom.c \
			 libretrodb_tool.c \
			 bintree.c \
			 query.c \
			 libretrodb.c \
			 $(LIBRETRO_COMMON_DIR)/compat/compat_fnmatch.c \
			 $(LIBRETRO_COMMON_DIR)/file/retro_file.c \
			 $(LIBRETRO_COMMON_DIR)/compat/compat_strl.c

RARCHDB_TOOL_OBJS := $(RARCHDB_TOOL_C:.c=.o)

RMSGPACK_C = \
			rmsgpack.c \
			rmsgpack_test.c \
			$(LIBRETRO_COMMON_DIR)/file/retro_file.c

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

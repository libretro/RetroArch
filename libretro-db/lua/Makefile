DEBUG                = 0
LIBRETRODB_DIR      := ../
LIBRETRO_COMMON_DIR := ../../libretro-common
INCFLAGS             = -I. -I$(LIBRETRODB_DIR) -I$(LIBRETRO_COMMON_DIR)/include

TARGETS              = rmsgpack_test libretrodb_tool lua_converter

ifeq ($(DEBUG), 1)
CFLAGS               = -g -O0 -Wall
else
CFLAGS               = -g -O2 -Wall -DNDEBUG
endif

LUA_CONVERTER_C = \
			 $(LIBRETRODB_DIR)/rmsgpack.c \
			 $(LIBRETRODB_DIR)/rmsgpack_dom.c \
			 lua_common.c \
			 $(LIBRETRODB_DIR)/libretrodb.c \
			 $(LIBRETRODB_DIR)/bintree.c \
			 $(LIBRETRODB_DIR)/query.c \
			 lua_converter.c \
			 $(LIBRETRO_COMMON_DIR)/compat/compat_fnmatch.c \
			 $(LIBRETRO_COMMON_DIR)/streams/file_stream.c \
			 $(LIBRETRO_COMMON_DIR)/compat/compat_strl.c

LUA_CONVERTER_OBJS := $(LUA_CONVERTER_C:.c=.o)

RARCHDB_TOOL_C = \
			 $(LIBRETRODB_DIR)/rmsgpack.c \
			 $(LIBRETRODB_DIR)/rmsgpack_dom.c \
			 $(LIBRETRODB_DIR)/libretrodb_tool.c \
			 $(LIBRETRODB_DIR)/bintree.c \
			 $(LIBRETRODB_DIR)/query.c \
			 ($LIBRETRODB_DIR)/libretrodb.c \
			 $(LIBRETRO_COMMON_DIR)/compat/compat_fnmatch.c \
			 $(LIBRETRO_COMMON_DIR)/streams/file_stream.c \
			 $(LIBRETRO_COMMON_DIR)/compat/compat_strl.c

RARCHDB_TOOL_OBJS := $(RARCHDB_TOOL_C:.c=.o)

TESTLIB_C = \
			 testlib.c \
			 $(LIBRETRODB_DIR)/query.c \
			 ($LIBRETRODB_DIR)/libretrodb.c \
			 $(LIBRETRODB_DIR)/bintree.c \
			 $(LIBRETRODB_DIR)/rmsgpack.c \
			 $(LIBRETRODB_DIR)/rmsgpack_dom.c \
			 $(LIBRETRO_COMMON_DIR)/compat/compat_fnmatch.c \
			 $(LIBRETRO_COMMON_DIR)/streams/file_stream.c \
			 $(LIBRETRO_COMMON_DIR)/compat/compat_strl.c

TESTLIB_OBJS := $(TESTLIB_C:.c=.o)

RMSGPACK_C = \
			$(LIBRETRODB_DIR)/rmsgpack.c \
			$(LIBRETRODB_DIR)/rmsgpack_test.c \
			$(LIBRETRO_COMMON_DIR)/streams/file_stream.c

RMSGPACK_OBJS := $(RMSGPACK_C:.c=.o)

LUA_FLAGS     = `pkg-config lua --libs`
TESTLIB_FLAGS = $(CFLAGS) $(LUA_FLAGS) -shared -fpic

.PHONY: all clean check

all: $(TARGETS)

%.o: %.c
	$(CC) $(INCFLAGS) $< -c $(CFLAGS) -o $@

lua_converter: $(LUA_CONVERTER_OBJS)
	$(CC) $(INCFLAGS) $(LUA_CONVERTER_OBJS) $(LUA_FLAGS) -o $@

libretrodb_tool: $(RARCHDB_TOOL_OBJS)
	$(CC) $(INCFLAGS) $(RARCHDB_TOOL_OBJS) -o $@

rmsgpack_test: $(RMSGPACK_OBJS)
	$(CC) $(INCFLAGS) $(RMSGPACK_OBJS) -g -o $@

testlib.so: CFLAGS += -fPIC
testlib.so: clean $(TESTLIB_OBJS)
	$(CC) $(INCFLAGS) $(TESTLIB_FLAGS) $(TESTLIB_OBJS) -o $@

check: testlib.so lua/tests.lua
	lua ./lua/tests.lua

clean:
	rm -rf $(TARGETS) $(LUA_CONVERTER_OBJS) $(C_CONVERTER_OBJS) $(RARCHDB_TOOL_OBJS) $(RMSGPACK_OBJS) $(TESTLIB_OBJS) testlib.so

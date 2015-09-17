CFLAGS               = -g -O2
LIBRETRO_COMMON_DIR := ../libretro-common
INCFLAGS             = -I. -I$(LIBRETRO_COMMON_DIR)/include

LUA_CONVERTER_C = \
			 rmsgpack.c \
		    rmsgpack_dom.c \
		    lua_common.c \
		    libretrodb.c \
		    bintree.c \
		    query.c \
		    lua_converter.c \
			 $(LIBRETRO_COMMON_DIR)/compat/compat_fnmatch.c \
			 $(LIBRETRO_COMMON_DIR)/compat/compat.c

LUA_CONVERTER_C_OBJS := $(LUA_CONVERTER_C:.c=.o)

RARCHDB_TOOL_C = \
			rmsgpack.c \
		   rmsgpack_dom.c \
		   libretrodb_tool.c \
		   bintree.c \
		   query.c \
		   libretrodb.c \
		   $(LIBRETRO_COMMON_DIR)/compat/compat_fnmatch.c \
			$(LIBRETRO_COMMON_DIR)/compat/compat.c

RARCHDB_TOOL_OBJS := $(RARCHDB_TOOL_C:.c=.o)

TESTLIB_C = \
			testlib.c \
	      lua_common.c \
	      query.c \
	      libretrodb.c \
	      bintree.c \
	      rmsgpack.c \
	      rmsgpack_dom.c \
		   $(LIBRETRO_COMMON_DIR)/compat/compat_fnmatch.c \
			$(LIBRETRO_COMMON_DIR)/compat/compat.c

TESTLIB_OBJS := $(TESTLIB_C:.c=.o)

LUA_FLAGS     = `pkg-config lua --libs`
TESTLIB_FLAGS = ${CFLAGS} ${LUA_FLAGS} -shared -fpic

.PHONY: all clean check

all: rmsgpack_test libretrodb_tool lua_converter

%.o: %.c
	${CC} $(INCFLAGS) $< -c ${CFLAGS} -o $@

lua_converter: ${LUA_CONVERTER_C_OBJS}
	${CC} $(INCFLAGS) ${LUA_CONVERTER_C_OBJS} ${LUA_FLAGS} -o $@

libretrodb_tool: ${RARCHDB_TOOL_OBJS}
	${CC} $(INCFLAGS) ${RARCHDB_TOOL_OBJS} -o $@

rmsgpack_test:
	${CC} $(INCFLAGS) rmsgpack.c rmsgpack_test.c -g -o $@

testlib.so: ${TESTLIB_OBJS}
	${CC} ${INCFLAGS} ${TESTLIB_FLAGS} ${TESTLIB_OBJS} -o $@

check: testlib.so tests.lua
	lua ./tests.lua

clean:
	rm -rf *.o rmsgpack_test lua_converter libretrodb_tool testlib.so

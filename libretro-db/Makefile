CFLAGS   = -g
LIBRETRO_COMMON_DIR := ../libretro-common
INCFLAGS = -I. -I$(LIBRETRO_COMMON_DIR)/include

LUA_CONVERTER_OBJ = rmsgpack.o \
		    rmsgpack_dom.o \
		    lua_common.o \
		    libretrodb.o \
		    bintree.o \
		    query.o \
		    lua_converter.o \
		    compat_fnmatch.c \
			 $(LIBRETRO_COMMON_DIR)/compat/compat.o

RARCHDB_TOOL_OBJ = rmsgpack.o \
		   rmsgpack_dom.o \
		   libretrodb_tool.o \
		   bintree.o \
		   query.o \
		   libretrodb.o \
		   compat_fnmatch.c \
			$(LIBRETRO_COMMON_DIR)/compat/compat.o

TESTLIB_C = testlib.c \
	      lua_common.c \
	      query.c \
	      compat_fnmatch.c \
	      libretrodb.c \
	      bintree.c \
	      rmsgpack.c \
	      rmsgpack_dom.c \
			$(LIBRETRO_COMMON_DIR)/compat/compat.o

LUA_FLAGS = `pkg-config lua --libs`
TESTLIB_FLAGS = ${CFLAGS} ${LUA_FLAGS} -shared -fpic

.PHONY: all clean check

all: rmsgpack_test libretrodb_tool lua_converter

%.o: %.c
	${CC} $(INCFLAGS) $< -c ${CFLAGS} -o $@

lua_converter: ${LUA_CONVERTER_OBJ}
	${CC} $(INCFLAGS) ${LUA_CONVERTER_OBJ} ${LUA_FLAGS} -o $@

libretrodb_tool: ${RARCHDB_TOOL_OBJ}
	${CC} $(INCFLAGS) ${RARCHDB_TOOL_OBJ} -o $@

rmsgpack_test:
	${CC} $(INCFLAGS) rmsgpack.c rmsgpack_test.c -g -o $@

testlib.so: ${TESTLIB_C}
	${CC} ${INCFLAGS} ${TESTLIB_FLAGS} ${TESTLIB_C} -o $@

check: testlib.so tests.lua
	lua ./tests.lua

clean:
	rm -rf *.o rmsgpack_test lua_converter libretrodb_tool testlib.so

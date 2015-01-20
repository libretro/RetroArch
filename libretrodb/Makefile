CFLAGS   = -g
INCFLAGS = -I. -I../libretro-sdk/include

LUA_CONVERTER_OBJ = rmsgpack.o \
		    rmsgpack_dom.o \
		    lua_common.o \
		    rarchdb.o \
		    bintree.o \
		    query.o \
		    lua_converter.o \
		    rl_fnmatch.c \
		    $(NULL)

RARCHDB_TOOL_OBJ = rmsgpack.o \
		   rmsgpack_dom.o \
		   rarchdb_tool.o \
		   bintree.o \
		   query.o \
		   rarchdb.o \
		   rl_fnmatch.c \
		   $(NULL)

TESTLIB_C = testlib.c \
	      lua_common.c \
	      query.c \
	      rl_fnmatch.c \
	      rarchdb.c \
	      bintree.c \
	      rmsgpack.c \
	      rmsgpack_dom.c \
	      $(NULL)

LUA_FLAGS = `pkg-config lua --libs`
TESTLIB_FLAGS = ${CFLAGS} ${LUA_FLAGS} -shared -fpic

.PHONY: all clean check

all: rmsgpack_test rarchdb_tool lua_converter

%.o: %.c
	${CC} $(INCFLAGS) $< -c ${CFLAGS} -o $@

lua_converter: ${LUA_CONVERTER_OBJ}
	${CC} $(INCFLAGS) ${LUA_CONVERTER_OBJ} ${LUA_FLAGS} -o $@

rarchdb_tool: ${RARCHDB_TOOL_OBJ}
	${CC} $(INCFLAGS) ${RARCHDB_TOOL_OBJ} -o $@

rmsgpack_test:
	${CC} $(INCFLAGS) rmsgpack.c rmsgpack_test.c -g -o $@

testlib.so: ${TESTLIB_C}
	${CC} ${INCFLAGS} ${TESTLIB_FLAGS} ${TESTLIB_C} -o $@

check: testlib.so tests.lua
	lua ./tests.lua

clean:
	rm -rf *.o rmsgpack_test lua_converter rarchdb_tool testlib.so

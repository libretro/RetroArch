CFLAGS   = -g
INCFLAGS = -I. -I../libretro-sdk/include

LUA_CONVERTER_OBJ = rmsgpack.o \
		    rmsgpack_dom.o \
		    rarchdb.o \
		    bintree.o \
		    lua_converter.o \
		    $(NULL)

RARCHDB_TOOL_OBJ = rmsgpack.o \
		    rmsgpack_dom.o \
		    db_parser.o \
		    rarchdb_tool.o \
		    bintree.o \
		    rarchdb.o \
		    $(NULL)

LUA_FLAGS = `pkg-config lua --libs`

all: rmsgpack_test rarchdb_tool lua_converter

%.o: %.c
	${CC} $(INCFLAGS) $< -c ${CFLAGS} -o $@

lua_converter: ${LUA_CONVERTER_OBJ}
	${CC} $(INCFLAGS) ${LUA_CONVERTER_OBJ} ${LUA_FLAGS} -o $@

rarchdb_tool: ${RARCHDB_TOOL_OBJ}
	${CC} $(INCFLAGS) ${RARCHDB_TOOL_OBJ} -o $@

rmsgpack_test:
	${CC} $(INCFLAGS) rmsgpack.c rmsgpack_test.c -g -o $@
clean:
	rm -rf *.o rmsgpack_test lua_converter rarchdb_tool

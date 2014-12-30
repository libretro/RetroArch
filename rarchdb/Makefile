CFLAGS   = -g
INCFLAGS = -I. -I../libretro-sdk/include
DAT_CONVERTER_OBJ = rmsgpack.o \
		    rmsgpack_dom.o \
		    rarchdb.o \
		    bintree.o \
		    db_parser.o \
		    dat_converter.o \
		    $(NULL)
RARCHDB_TOOL_OBJ = rmsgpack.o \
		    rmsgpack_dom.o \
		    db_parser.o \
		    rarchdb_tool.o \
		    bintree.o \
		    rarchdb.o \
		    $(NULL)

all: dat_converter rmsgpack_test rarchdb_tool

%.o: %.c
	${CC} $(INCFLAGS) $< -c ${CFLAGS} -o $@

dat_converter: ${DAT_CONVERTER_OBJ}
	${CC} $(INCFLAGS) ${DAT_CONVERTER_OBJ} -o $@

rarchdb_tool: ${RARCHDB_TOOL_OBJ}
	${CC} $(INCFLAGS) ${RARCHDB_TOOL_OBJ} -o $@

rmsgpack_test:
	${CC} $(INCFLAGS) rmsgpack.c rmsgpack_test.c -g -o $@
clean:
	rm -rf *.o rmsgpack_test dat_converter rarchdb_tool

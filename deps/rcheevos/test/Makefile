RC_SRC=../src/rcheevos
RC_URL_SRC=../src/rurl
LUA_SRC=lua/src

OBJ=$(RC_SRC)/trigger.o $(RC_SRC)/condset.o $(RC_SRC)/condition.o $(RC_SRC)/operand.o \
    $(RC_SRC)/term.o $(RC_SRC)/expression.o $(RC_SRC)/value.o $(RC_SRC)/lboard.o \
    $(RC_SRC)/alloc.o $(RC_SRC)/format.o $(RC_SRC)/richpresence.o \
    $(RC_URL_SRC)/url.o \
    $(LUA_SRC)/lapi.o $(LUA_SRC)/lcode.o $(LUA_SRC)/lctype.o $(LUA_SRC)/ldebug.o \
    $(LUA_SRC)/ldo.o $(LUA_SRC)/ldump.o $(LUA_SRC)/lfunc.o $(LUA_SRC)/lgc.o $(LUA_SRC)/llex.o \
    $(LUA_SRC)/lmem.o $(LUA_SRC)/lobject.o $(LUA_SRC)/lopcodes.o $(LUA_SRC)/lparser.o \
    $(LUA_SRC)/lstate.o $(LUA_SRC)/lstring.o $(LUA_SRC)/ltable.o $(LUA_SRC)/ltm.o \
    $(LUA_SRC)/lundump.o $(LUA_SRC)/lvm.o $(LUA_SRC)/lzio.o $(LUA_SRC)/lauxlib.o \
    $(LUA_SRC)/lbaselib.o $(LUA_SRC)/lbitlib.o $(LUA_SRC)/lcorolib.o $(LUA_SRC)/ldblib.o \
    $(LUA_SRC)/liolib.o $(LUA_SRC)/lmathlib.o $(LUA_SRC)/loslib.o $(LUA_SRC)/lstrlib.o \
    $(LUA_SRC)/ltablib.o $(LUA_SRC)/lutf8lib.o $(LUA_SRC)/loadlib.o $(LUA_SRC)/linit.o \
    test.o

all: test

%.o: %.c
	gcc -Wall -O0 -g -std=c89 -ansi -Wno-long-long -DLUA_32BITS -I../include -I$(RC_SRC) -I$(LUA_SRC) -c $< -o $@

test: $(OBJ)
	gcc -o $@ $+ -lm

test.o: smw_snes.h galaga_nes.h

smw_snes.h: smw_snes.json
	xxd -i $< | sed "s@unsigned@const@" > $@

galaga_nes.h: galaga_nes.json
	xxd -i $< | sed "s@unsigned@const@" > $@

clean:
	rm -f test $(OBJ) smw_snes.h galaga_nes.h

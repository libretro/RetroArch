# gfx/common/d3dcompiler_common.c: the D3DCompiler_*.dll the shaders are
# compiled with is the first of the list that loads, newest first.
#
# Windows only. Elsewhere it is cross-built with mingw-w64 and run under
# Wine, whose built-in compiler DLLs are disabled one way and another to
# stand in for a system without the newest. SANITIZER is taken and
# ignored: mingw-w64 has none of them.

TARGET    := d3dcompiler_fallback_test.exe
REPO_ROOT := ../../..
LRC       := $(REPO_ROOT)/libretro-common

ifeq ($(OS),Windows_NT)
RUN :=
else
CC  := x86_64-w64-mingw32-gcc
RUN := WINEDEBUG=-all $(or $(WINE),wine)
endif

SOURCES := d3dcompiler_fallback_test.c \
           $(REPO_ROOT)/gfx/common/d3dcompiler_common.c \
           $(LRC)/dynamic/dylib.c \
           $(LRC)/encodings/encoding_utf.c \
           $(LRC)/compat/compat_strl.c

CFLAGS += -Wall -g -O1 -DHAVE_DYLIB -I$(LRC)/include

ALL_47_TO_33 := d3dcompiler_47,d3dcompiler_46,d3dcompiler_45,d3dcompiler_44,d3dcompiler_43,d3dcompiler_42,d3dcompiler_41,d3dcompiler_40,d3dcompiler_39,d3dcompiler_38,d3dcompiler_37,d3dcompiler_36,d3dcompiler_35,d3dcompiler_34,d3dcompiler_33

all: $(TARGET)

$(TARGET): $(SOURCES) $(REPO_ROOT)/gfx/common/d3dcompiler_common.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

ifeq ($(OS),Windows_NT)
check: all
	./$(TARGET)
else
check: all
	$(RUN) ./$(TARGET)
	WINEDLLOVERRIDES="d3dcompiler_47=d" $(RUN) ./$(TARGET)
	WINEDLLOVERRIDES="$(ALL_47_TO_33)=d" $(RUN) ./$(TARGET) none
endif

clean:
	rm -f $(TARGET)

.PHONY: all check clean

# The host tools build.bat runs: vita-mksfoex writes the bubble's
# param.sfo and vita-pack-vpk packs the VPK. Built with the host's C
# compiler (HOST_CC=... to pick another); on Windows, under MSYS2's
# MINGW64 shell, they come out as .exe files beside build.bat.
LIBRETRO_COMM_DIR := ../../../../libretro-common

HOST_CC ?= gcc

ifneq ($(findstring Linux,$(shell uname -a)),)
else ifneq ($(findstring Darwin,$(shell uname -a)),)
else
   EXE := .exe
endif

PACK_SOURCES := vita-pack-vpk.c \
                $(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
                $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
                $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
                $(LIBRETRO_COMM_DIR)/time/rtime.c

CFLAGS := -O2 -Wall -std=gnu89 -I$(LIBRETRO_COMM_DIR)/include

all: vita-mksfoex$(EXE) vita-pack-vpk$(EXE)

vita-mksfoex$(EXE): vita-mksfoex.c
	$(HOST_CC) $(CFLAGS) -o $@ vita-mksfoex.c

vita-pack-vpk$(EXE): $(PACK_SOURCES)
	$(HOST_CC) $(CFLAGS) -o $@ $(PACK_SOURCES)

clean:
	rm -f vita-mksfoex vita-mksfoex.exe vita-pack-vpk vita-pack-vpk.exe

.PHONY: all clean

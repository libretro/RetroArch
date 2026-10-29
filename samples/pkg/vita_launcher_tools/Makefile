# pkg/vita/launcher/builder's vita-mksfoex and vita-pack-vpk, run on
# inputs the test writes itself; the VPK is read back with
# libretro-common's zip reader. Builds for Linux and, with
# CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine, for Windows.
TARGET   := vita_launcher_tools_test$(EXE)
MKSFO    := vita_mksfoex_tool$(EXE)
PACKVPK  := vita_pack_vpk_tool$(EXE)
CORE_DIR := ../../..
BUILDER  := $(CORE_DIR)/pkg/vita/launcher/builder
LRC      := $(CORE_DIR)/libretro-common

# The units the tools link, as the builder's Makefile lists them
LRC_DEFLATE := $(LRC)/encodings/encoding_deflate.c \
               $(LRC)/encodings/encoding_crc32.c \
               $(LRC)/features/features_cpu.c \
               $(LRC)/time/rtime.c

CFLAGS  += -std=gnu89 -Wall -g1 -O1 -I$(LRC)/include
TFLAGS  := -std=gnu89 -Wall -g1 -O2 -I$(LRC)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
   TFLAGS  := -fsanitize=$(SANITIZER) -fno-sanitize-recover=all \
              -fno-omit-frame-pointer $(TFLAGS)
endif

ifeq ($(EXE),.exe)
   TFLAGS += -static
endif

all: $(TARGET) $(MKSFO) $(PACKVPK)

$(MKSFO): $(BUILDER)/vita-mksfoex.c
	$(CC) $(TFLAGS) -o $@ $<

$(PACKVPK): $(BUILDER)/vita-pack-vpk.c $(LRC_DEFLATE)
	$(CC) $(TFLAGS) -o $@ $(BUILDER)/vita-pack-vpk.c $(LRC_DEFLATE)

$(TARGET): vita_launcher_tools_test.c $(LRC)/formats/zip/rzip_archive.c $(LRC_DEFLATE)
	$(CC) $(CFLAGS) -o $@ vita_launcher_tools_test.c \
	   $(LRC)/formats/zip/rzip_archive.c $(LRC_DEFLATE) $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)

clean:
	rm -rf vita_launcher_tools_test vita_launcher_tools_test.exe \
	       vita_mksfoex_tool vita_mksfoex_tool.exe \
	       vita_pack_vpk_tool vita_pack_vpk_tool.exe \
	       vlt_out.sfo vlt_out.vpk output.vpk vlt_in

.PHONY: all check clean

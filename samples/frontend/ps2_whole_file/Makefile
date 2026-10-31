# Whole-file reads and writes through the shipping file_stream.c,
# vfs_implementation.c and config_file.c.
#
#   make check                 host build, run here (Linux; Windows with
#                              CC=x86_64-w64-mingw32-gcc EXE=.exe
#                              RUNNER=wine)
#   make PS2=1                 PS2 ELF with the ps2dev toolchain
#                              ($PS2SDK), run by .github/workflows/
#                              PS2-runtime.yml under Play!'s headless
#                              AutoTest
CORE_DIR := ../../..
LRC      := $(CORE_DIR)/libretro-common

SOURCES := ps2_whole_file_test.c \
           $(LRC)/streams/file_stream.c \
           $(LRC)/vfs/vfs_implementation.c \
           $(LRC)/file/config_file.c \
           $(LRC)/file/config_file_io.c \
           $(LRC)/file/file_path.c \
           $(LRC)/file/file_path_io.c \
           $(LRC)/string/stdstring.c \
           $(LRC)/string/rstrtod.c \
           $(LRC)/compat/compat_strl.c \
           $(LRC)/compat/fopen_utf8.c \
           $(LRC)/encodings/encoding_utf.c \
           $(LRC)/time/rtime.c

ifeq ($(PS2),1)
   ifeq ($(strip $(PS2SDK)),)
      $(error PS2=1 needs PS2SDK set to the ps2sdk directory)
   endif
   TARGET  := ps2_whole_file_test.elf
   CC      := mips64r5900el-ps2-elf-gcc
   CFLAGS  += -D_EE -DPS2 -O2 -G0 -Wall -I$(LRC)/include \
              -I$(PS2SDK)/ee/include -I$(PS2SDK)/common/include \
              -I$(PS2SDK)/ports/include
   LDFLAGS += -T$(PS2SDK)/ee/startup/linkfile -L$(PS2SDK)/ee/lib \
              -L$(PS2SDK)/ports/lib -lps2_drivers -lpatches -lm
else
   TARGET  := ps2_whole_file_test$(EXE)
   CFLAGS  += -std=gnu89 -Wall -g1 -O1 -I$(LRC)/include
   LDFLAGS += -lm
   ifneq ($(SANITIZER),)
      CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
      LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
   endif
endif

all: $(TARGET)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)

clean:
	rm -f ps2_whole_file_test ps2_whole_file_test.exe \
	      ps2_whole_file_test.elf ps2_whole_file.info ps2_whole_file_w.bin

.PHONY: all check clean

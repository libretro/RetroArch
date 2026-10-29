# os/wiiu/wut/elf2rpl, the host tool that turns the linked Wii U ELF
# into an RPL, run on a linked ELF the test writes itself in the shape
# a -pie link gives it, then on truncated copies of it. No PowerPC
# toolchain is needed. Builds for Linux and, with
# CC=x86_64-w64-mingw32-gcc EXE=.exe RUNNER=wine, for Windows.
TARGET   := wiiu_elf2rpl_test$(EXE)
TOOL     := wiiu_elf2rpl_tool$(EXE)
CORE_DIR := ../../..
ELF2RPL  := $(CORE_DIR)/os/wiiu/wut/elf2rpl
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

# The tool and the libretro-common units it links, as its own Makefile
# lists them
TOOL_SOURCES := $(ELF2RPL)/main.c \
                $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
                $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
                $(LIBRETRO_COMM_DIR)/time/rtime.c

CFLAGS  += -std=c89 -Wall -g1 -O1 -DELF2RPL='"$(if $(EXE),,./)$(TOOL)"'
TFLAGS  := -std=gnu89 -Wall -g1 -O2 -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
   TFLAGS  := -fsanitize=$(SANITIZER) -fno-sanitize-recover=all \
              -fno-omit-frame-pointer $(TFLAGS)
endif

ifeq ($(EXE),.exe)
   TFLAGS += -static
endif

all: $(TARGET) $(TOOL)

$(TOOL): $(TOOL_SOURCES)
	$(CC) $(TFLAGS) -o $@ $(TOOL_SOURCES)

$(TARGET): wiiu_elf2rpl_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)

clean:
	rm -f wiiu_elf2rpl_test wiiu_elf2rpl_test.exe \
	      wiiu_elf2rpl_tool wiiu_elf2rpl_tool.exe \
	      wiiu_elf2rpl_in.tmp wiiu_elf2rpl_out.tmp \
	      wiiu_elf2rpl_trunc.tmp

.PHONY: all check clean

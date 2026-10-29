# os/wiiu/wut/elf2rpl, the host tool that turns the linked Wii U ELF
# into an RPL, run on a linked ELF the test writes itself in the shape
# a -pie link gives it. No PowerPC toolchain is needed. Builds for Linux
# and, with CC=x86_64-w64-mingw32-gcc CXX=x86_64-w64-mingw32-g++
# EXE=.exe RUNNER=wine, for Windows.
TARGET   := wiiu_elf2rpl_test$(EXE)
TOOL     := wiiu_elf2rpl_tool$(EXE)
CORE_DIR := ../../..
ELF2RPL  := $(CORE_DIR)/os/wiiu/wut/elf2rpl

CXX     ?= g++
CFLAGS  += -std=c89 -Wall -g1 -O1 -DELF2RPL='"$(if $(EXE),,./)$(TOOL)"'
TFLAGS  := -O2 -std=c++14 -I$(ELF2RPL) -Wno-class-memaccess

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
   TFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(TFLAGS)
   # The tool reads and writes ELF headers through packed big-endian
   # fields, and keeps its section graph to exit, as a one-shot tool
   # may; its other undefined behaviour and memory errors still show.
   ifneq ($(findstring undefined,$(SANITIZER)),)
      TFLAGS += -fno-sanitize=alignment
   endif
   CFLAGS  += -DELF2RPL_ENV='"ASAN_OPTIONS=detect_leaks=0 "'
endif

ifeq ($(EXE),.exe)
   TFLAGS += -static
endif

all: $(TARGET) $(TOOL)

$(TOOL): $(ELF2RPL)/main.cpp
	$(CXX) $(TFLAGS) -o $@ $<

$(TARGET): wiiu_elf2rpl_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)

clean:
	rm -f wiiu_elf2rpl_test wiiu_elf2rpl_test.exe \
	      wiiu_elf2rpl_tool wiiu_elf2rpl_tool.exe \
	      wiiu_elf2rpl_in.tmp wiiu_elf2rpl_out.tmp

.PHONY: all check clean

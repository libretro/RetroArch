compiler    := gcc
extra_flags :=
use_neon    := 0
release	   := release
EXE_EXT	      :=

ifeq ($(platform),)
platform = unix
ifeq ($(shell uname -a),)
   platform = win
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   platform = osx
   arch = intel
ifeq ($(shell uname -p),powerpc)
   arch = ppc
endif
else ifneq ($(findstring win,$(shell uname -a)),)
   platform = win
endif
endif

ifeq ($(compiler),gcc)
extra_rules_gcc := $(shell $(compiler) -dumpmachine)
endif

ifneq (,$(findstring armv7,$(extra_rules_gcc)))
extra_flags += -mcpu=cortex-a9 -mtune=cortex-a9 -mfpu=neon
use_neon := 1
endif

ifneq (,$(findstring hardfloat,$(extra_rules_gcc)))
extra_flags += -mfloat-abi=hard
endif

ifeq (release,$(build))
extra_flags += -O2
endif

ifeq (debug,$(build))
extra_flags += -O0 -g
endif

ldflags :=

EXE_EXT :=
ifeq ($(platform), unix)
else ifeq ($(platform), osx)
compiler := $(CC)
else
EXE_EXT = .exe
endif

LIBRETRO_COMM_DIR := ../..
CORE_DIR := $(LIBRETRO_COMM_DIR)/utils

CC      := $(compiler)
CXX     := $(subst CC,++,$(compiler))
flags   := -I$(LIBRETRO_COMM_DIR)/include
asflags := $(extra_flags)
LDFLAGS :=
flags   += -std=c99 -DMD5_BUILD_UTILITY -DSHA1_BUILD_UTILITY

ifeq (1,$(use_neon))
ASMFLAGS := -INEON/asm
asflags += -mfpu=neon
endif

OBJS += $(CORE_DIR)/djb2.o \
		  $(CORE_DIR)/md5.o \
		  $(CORE_DIR)/sha1.o \
		  $(CORE_DIR)/sha1_main.o \
		  $(CORE_DIR)/crc32.o

UTILS := djb2$(EXE_EXT) md5$(EXE_EXT) sha1$(EXE_EXT) crc32$(EXE_EXT)

all: $(UTILS)

djb2$(EXE_EXT): $(CORE_DIR)/djb2.o

md5$(EXE_EXT): $(CORE_DIR)/md5.o

sha1$(EXE_EXT): $(CORE_DIR)/sha1.o $(CORE_DIR)/sha1_main.o

crc32$(EXE_EXT): $(CORE_DIR)/crc32.o $(CORE_DIR)/../encodings/encoding_crc32.o

%.o: %.S
	$(CC) -c -o $@ $(asflags) $(LDFLAGS)  $(ASMFLAGS)  $<

%.o: %.c
	$(CC) -c -o $@ $(flags) $<

$(UTILS):
	$(CC) -o $@ $(ldflags) $(flags) $^

clean:
	rm -f $(CORE_DIR)/*.o
	rm -f $(UTILS)

strip:
	strip -s $(UTILS)

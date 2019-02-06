###
##
# Makefile for app_booter.
##

# system platform
system_platform = unix
ifeq ($(shell uname -a),)
EXE_EXT = .exe
   system_platform = win
else ifneq ($(findstring Darwin,$(shell uname -a)),)
   system_platform = osx
else ifneq ($(findstring MINGW,$(shell uname -a)),)
   system_platform = win
endif

EXTERNAL_LIBOGC = 0
CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
LD = $(DEVKITPPC)/bin/powerpc-eabi-ld$(EXE_EXT)
OBJCOPY = $(DEVKITPPC)/bin/powerpc-eabi-objcopy$(EXE_EXT)

BIN_TARGET := app_booter.bin
ELF_TARGET := app_booter.elf

LIBRETRO_COMM_DIR := ../../libretro-common

LIBDIRS :=
INCLUDE := -I.  -I$(LIBRETRO_COMM_DIR)/crt/include

ifeq ($(EXTERNAL_LIBOGC), 1)
INCLUDE += -I$(DEVKITPRO)/libogc/include
LIBDIRS += -L$(DEVKITPRO)/libogc/lib/wii
else
INCLUDE += -I../libogc/include
LIBDIRS += -Lwii/libogc/libs/wii
endif

MACHDEP := -DHW_RVL -DGEKKO -mno-eabi -mno-sdata -mcpu=750
# todo: find out why -Os spits out linker errors
CFLAGS += -Wall -O2 -ffreestanding -std=gnu99 $(MACHDEP) $(INCLUDE)

LDFLAGS := -T link.ld

OBJ = crt0.o main.o $(LIBRETRO_COMM_DIR)/crt/string.o

all: $(BIN_TARGET)

%.bin: %.elf
	$(OBJCOPY) -O binary $< $@

$(ELF_TARGET): $(OBJ)
	$(LD) -o $@ $(LDFLAGS) $(LIBDIRS) $(OBJ) $(LIBS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

%.o: %.s
	$(CC) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(BIN_TARGET)
	rm -f $(ELF_TARGET)
	rm -f $(OBJ)

.PHONY: clean

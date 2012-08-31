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

CC = $(DEVKITPPC)/bin/powerpc-eabi-gcc$(EXE_EXT)
LD = $(DEVKITPPC)/bin/powerpc-eabi-ld$(EXE_EXT)
OBJCOPY = $(DEVKITPPC)/bin/powerpc-eabi-objcopy$(EXE_EXT)

ifeq ($(platform),wii)
   BIN_TARGET := app_booter_wii.bin
   ELF_TARGET := app_booter_wii.elf
else
   BIN_TARGET := app_booter_gc.bin
   ELF_TARGET := app_booter_gc.elf
endif

INCLUDE := -I. -I$(DEVKITPRO)/libogc/include
LIBDIRS := -L$(DEVKITPRO)/libogc/lib/wii

MACHDEP := -mno-eabi -mno-sdata -mcpu=750
# todo: find out why -Os spits out linker errors
CFLAGS += -Wall -O2 -ffreestanding -std=gnu99 $(MACHDEP) $(INCLUDE)

ifeq ($(platform),wii)
   CFLAGS += -DHW_RVL
else
   CFLAGS += -DHW_DOL
endif

LDFLAGS := -T link.ld

OBJ = crt0.o dolloader.o elfloader.o main.o string.o sync.o

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


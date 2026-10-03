# Dolphin-run checks for os/gekko.  Needs devkitPPC; run with
#   make -C os/gekko/tests            (Wii)
#   make -C os/gekko/tests PLATFORM=dol
# and run-dolphin.sh to execute them; sd_test wants an SD image:
#   ./sd-image.sh make sd.img && SDIMG=sd.img ./run-dolphin.sh sd_test_rvl.elf
#   ./sd-image.sh check sd.img
# kernel_test also checks loader arguments when run with
#   ARGS='sd:/k.dol|sd:/roms|a b.nes' ./run-dolphin.sh kernel_test_rvl.elf
# exec_test runs a copy of itself, attached as an ELF or a DOL:
#   ARGS='sd:/exec_test.elf|first' ATTACH=exec_test_rvl.elf \
#      ./run-dolphin.sh exec_test_rvl.elf
#   ./elf2dol.py exec_test_rvl.elf exec_test.dol (then ATTACH=exec_test.dol)
# content-test.sh runs RetroArch itself with a core and a test ROM.
# The FAT code also runs on the host: make host (no devkitPPC needed).

PLATFORM ?= rvl
DEVKITPPC ?= /opt/devkitpro/devkitPPC
CC   := $(DEVKITPPC)/bin/powerpc-eabi-gcc
ROOT := ../../..
GK   := ..

ifeq ($(PLATFORM),rvl)
   MACHDEP := -mrvl -DHW_RVL
else
   MACHDEP := -mogc -DHW_DOL
endif
MACHDEP += -DGEKKO -mcpu=750 -meabi -mhard-float

CFLAGS  := -O2 -Wall -Wextra -Wno-unused-parameter -std=gnu89 $(MACHDEP) \
           -I$(GK)/include -I$(ROOT)/libretro-common/include
LDFLAGS := $(MACHDEP) -T $(GK)/boot/$(PLATFORM).ld -L$(GK)/boot -Wl,--gc-sections

GEKKO_DIR      := $(GK)
GEKKO_PLATFORM := $(PLATFORM)
include $(GK)/gekko.mk
KERNEL := $(GEKKO_SOURCES) $(ROOT)/gfx/bitmapfont.c

TESTS := kernel_test vi_test gx_test io_test exi_test exec_test
ifeq ($(PLATFORM),rvl)
   TESTS  += ios_test sd_test usb_test wiimote_test net_test
endif

all: $(addsuffix _$(PLATFORM).elf,$(TESTS))

%_$(PLATFORM).elf: %.c $(KERNEL) $(wildcard $(GK)/include/gekko/*.h $(GK)/kernel/*.h $(GK)/fs/*.h $(GK)/disk/*.h) Makefile $(GK)/gekko.mk
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(KERNEL) -lm -o $@

host:
	./host/run.sh

clean:
	rm -f *.elf

.PHONY: all clean host

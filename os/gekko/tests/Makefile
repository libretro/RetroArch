# Dolphin-run checks for os/gekko.  Needs devkitPPC; run with
#   make -C os/gekko/tests            (Wii)
#   make -C os/gekko/tests PLATFORM=dol
# and run-dolphin.sh to execute them; sd_test wants an SD image:
#   ./sd-image.sh make sd.img && SDIMG=sd.img ./run-dolphin.sh sd_test_rvl.elf
#   ./sd-image.sh check sd.img
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

KERNEL := $(GK)/boot/crt0.S $(GK)/cpu/exception.S $(GK)/cpu/cache.S \
          $(GK)/kernel/boot.c $(GK)/kernel/exception.c $(GK)/kernel/irq.c \
          $(GK)/kernel/sched.c $(GK)/kernel/newlib.c $(GK)/kernel/power.c \
          $(GK)/kernel/vi.c $(GK)/kernel/console.c $(GK)/kernel/gx.c \
          $(GK)/kernel/audio.c $(GK)/kernel/pad.c $(GK)/kernel/exi.c \
          $(GK)/disk/sdspi.c $(GK)/disk/sdgecko.c \
          $(GK)/fs/fat.c $(GK)/fs/fat_devoptab.c \
          $(ROOT)/gfx/bitmapfont.c

TESTS := kernel_test vi_test gx_test io_test exi_test
ifeq ($(PLATFORM),rvl)
   KERNEL += $(GK)/rvl/ipc.c $(GK)/rvl/stm.c $(GK)/rvl/es.c $(GK)/rvl/conf.c \
             $(GK)/rvl/ave.c $(GK)/rvl/sdio.c
   TESTS  += ios_test sd_test
endif

all: $(addsuffix _$(PLATFORM).elf,$(TESTS))

%_$(PLATFORM).elf: %.c $(KERNEL) $(wildcard $(GK)/include/gekko/*.h $(GK)/kernel/*.h $(GK)/fs/*.h $(GK)/disk/*.h) Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(KERNEL) -lm -o $@

host:
	./host/run.sh

clean:
	rm -f *.elf

.PHONY: all clean host

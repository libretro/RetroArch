# Dolphin-run checks for os/gekko.  Needs devkitPPC; run with
#   make -C os/gekko/tests            (Wii)
#   make -C os/gekko/tests PLATFORM=dol
# and run-dolphin.sh to execute them.

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
          $(GK)/kernel/sched.c $(GK)/kernel/newlib.c $(GK)/kernel/power.c

TESTS := kernel_test
ifeq ($(PLATFORM),rvl)
   KERNEL += $(GK)/rvl/ipc.c $(GK)/rvl/stm.c $(GK)/rvl/es.c $(GK)/rvl/conf.c
   TESTS  += ios_test
endif

all: $(addsuffix _$(PLATFORM).elf,$(TESTS))

%_$(PLATFORM).elf: %.c $(KERNEL) $(wildcard $(GK)/include/gekko/*.h $(GK)/kernel/*.h) Makefile
	$(CC) $(CFLAGS) $(LDFLAGS) $< $(KERNEL) -o $@

clean:
	rm -f *.elf

.PHONY: all clean

# os/gekko's sources, for the builds that include this.  Set GEKKO_DIR
# to where os/gekko is and GEKKO_PLATFORM to rvl (Wii) or dol
# (GameCube) first; GEKKO_SOURCES is the list.  The console's text
# uses RetroArch's gfx/bitmapfont.c, which the including build adds.

GEKKO_SRC := boot/crt0.S cpu/exception.S cpu/cache.S \
             kernel/boot.c kernel/exception.c kernel/irq.c kernel/sched.c \
             kernel/newlib.c kernel/power.c kernel/vi.c kernel/console.c \
             kernel/gx.c kernel/audio.c kernel/pad.c kernel/exi.c \
             kernel/usbgecko.c kernel/exec.c kernel/exec_image.c cpu/exec.S \
             disk/sdspi.c disk/sdgecko.c disk/usbmsc.c \
             fs/fat.c fs/fat_devoptab.c

ifeq ($(GEKKO_PLATFORM),rvl)
GEKKO_SRC += rvl/ipc.c rvl/stm.c rvl/es.c rvl/conf.c rvl/ave.c rvl/sdio.c \
             rvl/iobuf.c rvl/usb.c rvl/usbstorage.c rvl/bt.c rvl/wiimote.c \
             rvl/net.c rvl/ir.c rvl/ave_gamma.c rvl/hidboot.c cpu/l2.S
endif

GEKKO_SOURCES := $(addprefix $(GEKKO_DIR)/,$(GEKKO_SRC))

TARGET := ctr_task_queue_prio_test

LIBRETRO_COMM_DIR := ../../..

# A 3DS homebrew, built with devkitARM and libctru; run the .3dsx on
# hardware or in an emulator set to Old 3DS.  The threaded runner is
# the one under test.
ifeq ($(strip $(DEVKITPRO)),)
   DEVKITPRO := /opt/devkitpro
endif
DEVKITARM ?= $(DEVKITPRO)/devkitARM
CC        := $(DEVKITARM)/bin/arm-none-eabi-gcc
CTRULIB   ?= $(DEVKITPRO)/libctru
3DSXTOOL  ?= $(DEVKITPRO)/tools/bin/3dsxtool
# As Makefile.ctr: libctru 2.0 brings the CondVar API rthreads
# otherwise carries a copy of.
USE_CTRULIB_2 ?= $(if $(shell grep -sl gspPresentBuffer $(CTRULIB)/include/3ds/services/gspgpu.h),1,0)

SOURCES := \
	ctr_task_queue_prio_test.c \
	$(LIBRETRO_COMM_DIR)/queues/task_queue.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

# Objects stay here: the shared sources are also built for the host.
OBJS := $(addprefix obj-ctr/,$(notdir $(SOURCES:.c=.o)))
vpath %.c $(sort $(dir $(SOURCES)))

ARCH    := -march=armv6k -mtune=mpcore -mfloat-abi=hard -marm -mfpu=vfp \
           -mtp=soft
CFLAGS  += $(ARCH) -Wall -std=gnu99 -O2 -DARM11 -D_3DS -D__3DS__ \
           -DHAVE_THREADS -I$(LIBRETRO_COMM_DIR)/include \
           -I$(CTRULIB)/include
ifeq ($(USE_CTRULIB_2),1)
   CFLAGS += -DUSE_CTRULIB_2
endif
LDFLAGS += -specs=3dsx.specs $(ARCH) -L$(CTRULIB)/lib
LIBS    += -lctru -lm

all: $(TARGET).3dsx

obj-ctr/%.o: %.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET).elf: $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LIBS)

$(TARGET).3dsx: $(TARGET).elf
	$(3DSXTOOL) $< $@

clean:
	rm -rf obj-ctr
	rm -f $(TARGET).elf $(TARGET).3dsx

.PHONY: all clean

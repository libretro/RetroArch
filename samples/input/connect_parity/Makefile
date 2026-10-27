# The converted pad handlers of input/connect against the copying
# handlers they replace, and the DualSense's, both Switch Pro
# Controllers' and the original Xbox pad's against their reports; the
# DualShock 3's axes against the copying handler's.
ROOT   := ../../..
TARGET := connect_parity_test
CC     ?= cc
CFLAGS += -std=gnu89 -O1 -g1 -Wall -DHAVE_HID \
          -I$(ROOT) -I$(ROOT)/libretro-common/include
SOURCES := connect_parity_test.c \
           $(ROOT)/input/connect/connect_ps3.c \
           $(ROOT)/input/connect/connect_ps4.c \
           $(ROOT)/input/connect/connect_ps5.c \
           $(ROOT)/input/connect/connect_switch_pro.c \
           $(ROOT)/input/connect/connect_switch2_pro.c \
           $(ROOT)/input/connect/connect_wiiugca.c \
           $(ROOT)/input/connect/connect_xid.c \
           $(ROOT)/input/connect/connect_ps2adapter.c \
           $(ROOT)/input/connect/connect_psxadapter.c \
           $(ROOT)/input/connect/connect_snesusb.c \
           $(ROOT)/input/connect/connect_nesusb.c \
           $(ROOT)/input/connect/connect_zerodelay_dragonrise.c \
           $(ROOT)/input/connect/connect_ps4_hori_mini.c \
           $(ROOT)/input/connect/connect_kade.c \
           $(ROOT)/input/connect/connect_wiiupro.c \
           $(ROOT)/input/connect/connect_retrode.c \
           $(ROOT)/input/connect/connect_8bitdo.c \
           $(ROOT)/input/connect/connect_gamecube_pc.c \
           $(ROOT)/input/common/input_hid_common.c \
           $(ROOT)/libretro-common/encodings/encoding_crc32.c \
           $(ROOT)/libretro-common/compat/compat_strcasestr.c \
           $(ROOT)/libretro-common/compat/compat_strl.c \
           $(ROOT)/libretro-common/features/features_cpu.c \
           $(ROOT)/libretro-common/time/rtime.c
ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: $(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS) -lm
check: all
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean

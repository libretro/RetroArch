# The converted pad handlers of input/connect against the copying
# handlers they replace.
ROOT   := ../../..
TARGET := connect_parity_test
CC     ?= cc
CFLAGS += -std=gnu89 -O1 -g1 -Wall -DHAVE_HID \
          -I$(ROOT) -I$(ROOT)/libretro-common/include
SOURCES := connect_parity_test.c \
           $(ROOT)/input/connect/connect_ps4.c \
           $(ROOT)/input/connect/connect_wiiugca.c \
           $(ROOT)/input/connect/connect_ps2adapter.c \
           $(ROOT)/input/connect/connect_psxadapter.c \
           $(ROOT)/input/connect/connect_snesusb.c \
           $(ROOT)/input/connect/connect_nesusb.c \
           $(ROOT)/input/connect/connect_zerodelay_dragonrise.c \
           $(ROOT)/input/connect/connect_ps4_hori_mini.c \
           $(ROOT)/input/common/input_hid_common.c
ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: $(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
check: all
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean

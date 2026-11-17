include ../../sample_objdir.mk

TARGET := iohidmanager_hid_test

# The macOS/iOS HID driver with the real pad connections behind it,
# built on the host against stand-ins for IOKit (iokit_sdk/): the test
# includes the driver and answers IOKit's calls itself.
RARCH_DIR := ../../..
LRC       := $(RARCH_DIR)/libretro-common

SOURCES := \
	iohidmanager_hid_test.c \
	$(RARCH_DIR)/input/common/input_hid_common.c \
	$(wildcard $(RARCH_DIR)/input/connect/*.c) \
	$(LRC)/compat/compat_strl.c \
	$(LRC)/string/stdstring.c \
	$(LRC)/rthreads/rthreads.c \
	$(LRC)/rthreads/retro_eventcount.c \
	$(LRC)/features/features_cpu.c \
	$(LRC)/encodings/encoding_utf.c \
	$(LRC)/encodings/encoding_crc32.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -std=gnu99 -g1 -O1 -D_GNU_SOURCE \
           -DHAVE_HID -DHAVE_THREADS -DRARCH_INTERNAL \
           -I$(RARCH_DIR) -I$(RARCH_DIR)/input -I$(LRC)/include \
           -Iiokit_sdk -I$(RARCH_DIR)/tools/platform_stubs/apple
LDFLAGS += -lpthread -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

iohidmanager_hid_test.o: $(RARCH_DIR)/input/drivers_hid/iohidmanager_hid.c \
      ../joypad_bridge_checked/bridge_view.h $(wildcard iokit_sdk/IOKit/hid/*.h)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean

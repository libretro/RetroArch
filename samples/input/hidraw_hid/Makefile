include ../../sample_objdir.mk

TARGET := hidraw_hid_test

# The hidraw HID driver with the real pad handlers behind it, against
# stand-ins for the calls it makes on /dev and /sys, linked in with
# --wrap (GNU ld). hidraw is Linux's: elsewhere the targets say so and
# succeed.
RARCH_DIR := ../../..
LRC       := $(RARCH_DIR)/libretro-common

SOURCES := \
	hidraw_hid_test.c \
	$(RARCH_DIR)/input/drivers_hid/hidraw_hid.c \
	$(RARCH_DIR)/input/common/input_hid_common.c \
	$(wildcard $(RARCH_DIR)/input/connect/*.c) \
	$(LRC)/compat/compat_strl.c \
	$(LRC)/string/stdstring.c \
	$(LRC)/rthreads/rthreads.c \
	$(LRC)/features/features_cpu.c \
	$(LRC)/encodings/encoding_utf.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -std=gnu99 -g -O1 -U_FORTIFY_SOURCE -D_GNU_SOURCE \
           -DHAVE_HID -DHAVE_HIDRAW -DHAVE_THREADS \
           -I$(RARCH_DIR) -I$(RARCH_DIR)/input -I$(LRC)/include
WRAP    := open read write ioctl close opendir readdir closedir
LDFLAGS += $(foreach f,$(WRAP),-Wl,--wrap=$(f)) -lpthread -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

ifeq ($(shell uname -s 2>/dev/null),Linux)
all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: all
	./$(TARGET)
else
all check:
	@echo "SKIP hidraw_hid_test: hidraw is Linux only"
endif

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean

# The V4L2 camera driver against a scripted device: open, close, ioctl,
# mmap and munmap are link-wrapped, so no device is needed.

include ../../sample_objdir.mk

TARGET := v4l2_poll_test

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

SOURCES_C := \
	v4l2_poll_test.c \
	../pipewire_poll/stubs_retroarch.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_int.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_filter.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/pixconv.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/memmap/memalign.c

OBJS := $(call sample_objs,$(SOURCES_C:.c=.o))

INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)
CFLAGS  += -Wall $(INCDIRS) -std=gnu99 -D_GNU_SOURCE -DHAVE_V4L2
LDFLAGS += -lm
LDFLAGS += -Wl,--wrap=open -Wl,--wrap=close -Wl,--wrap=ioctl \
	-Wl,--wrap=mmap -Wl,--wrap=munmap

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1 $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
else
   CFLAGS  += -g1 -O1
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

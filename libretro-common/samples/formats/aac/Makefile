include ../../sample_objdir.mk

TARGET := raac_size_test
# Savestates: a decoder saved between units, loaded into a new one.
TARGET_TEST := raac_state_test
# ADTS headers and the WAVE channel order.
TARGET_LAYOUT := raac_layout_test
# Parametric stereo, with reset and savestates.
TARGET_PS := raac_ps_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	raac_size_test.c \
	$(LIBRETRO_COMM_DIR)/formats/aac/raac.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))
STATE_SOURCES := raac_state_test.c $(LIBRETRO_COMM_DIR)/formats/aac/raac.c
STATE_OBJS    := $(call sample_objs,$(STATE_SOURCES:.c=.o))
LAYOUT_SOURCES := raac_layout_test.c $(LIBRETRO_COMM_DIR)/formats/aac/raac.c
LAYOUT_OBJS    := $(call sample_objs,$(LAYOUT_SOURCES:.c=.o))
PS_SOURCES     := raac_ps_test.c $(LIBRETRO_COMM_DIR)/formats/aac/raac.c
PS_OBJS        := $(call sample_objs,$(PS_SOURCES:.c=.o))

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -O1 -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

LDLIBS += -lm

all: $(TARGET) $(TARGET_TEST) $(TARGET_LAYOUT) $(TARGET_PS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TARGET_TEST): $(STATE_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TARGET_LAYOUT): $(LAYOUT_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TARGET_PS): $(PS_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS) $(TARGET_TEST) $(STATE_OBJS) $(TARGET_LAYOUT) $(LAYOUT_OBJS) $(TARGET_PS) $(PS_OBJS)

.PHONY: all clean

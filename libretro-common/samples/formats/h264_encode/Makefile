include ../../sample_objdir.mk

TARGET := rh264_encode_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rh264_encode_test.c \
	$(LIBRETRO_COMM_DIR)/formats/h264/rh264_encode.c \
	$(LIBRETRO_COMM_DIR)/formats/h264/rh264.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

# rh264.c, the decoder that checks the encoder, is gnu99
CFLAGS  += -Wall -std=gnu99 -g1 -O1 -I$(LIBRETRO_COMM_DIR)/include

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

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS) -lm

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

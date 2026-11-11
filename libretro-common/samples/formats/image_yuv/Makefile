include ../../sample_objdir.mk

TARGET := image_yuv_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	image_yuv_test.c \
	$(LIBRETRO_COMM_DIR)/formats/image/image_yuv_blit.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -Wextra -pedantic -std=c89 -g1 -I$(LIBRETRO_COMM_DIR)/include
LDLIBS  += -lm

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
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

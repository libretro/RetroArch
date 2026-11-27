include ../../sample_objdir.mk

TARGET := utf16_utf32_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	utf16_utf32_test.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c

CFLAGS  += -Wall -pedantic -std=c89 -Wno-long-long -g1 -O2 -I$(LIBRETRO_COMM_DIR)/include

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

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
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

include ../../sample_objdir.mk

TARGET := float_to_ulaw_test

LIBRETRO_COMM_DIR := ../../..

COMMON_SOURCES := \
	$(LIBRETRO_COMM_DIR)/audio/conversion/float_to_ulaw.c

COMMON_OBJS := $(call sample_objs,$(COMMON_SOURCES:.c=.o))
OBJS        := float_to_ulaw_test.o $(COMMON_OBJS)

CFLAGS  += -Wall -pedantic -std=c89 -g1 -O2 -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lm

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

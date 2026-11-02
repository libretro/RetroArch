include ../../sample_objdir.mk

TARGET := raac_size_test
# Savestates: a decoder saved between units, loaded into a new one.
TARGET_TEST := raac_state_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	raac_size_test.c \
	$(LIBRETRO_COMM_DIR)/formats/aac/raac.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))
STATE_SOURCES := raac_state_test.c $(LIBRETRO_COMM_DIR)/formats/aac/raac.c
STATE_OBJS    := $(call sample_objs,$(STATE_SOURCES:.c=.o))

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -O1 -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

LDLIBS += -lm

all: $(TARGET) $(TARGET_TEST)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

$(TARGET_TEST): $(STATE_OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS) $(TARGET_TEST) $(STATE_OBJS)

.PHONY: all clean

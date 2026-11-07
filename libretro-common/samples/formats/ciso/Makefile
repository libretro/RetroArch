include ../../sample_objdir.mk

TARGET        := ciso_test
.DEFAULT_GOAL := all

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	ciso_test.c \
	$(LIBRETRO_COMM_DIR)/formats/ciso/ciso.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -O2 -I$(LIBRETRO_COMM_DIR)/include

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

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean

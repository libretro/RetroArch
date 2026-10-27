include ../../sample_objdir.mk

TARGET := rzstd_raw_block_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rzstd_raw_block_test.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_rzstd.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_xxh64.c

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -O2 -I$(LIBRETRO_COMM_DIR)/include

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

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean

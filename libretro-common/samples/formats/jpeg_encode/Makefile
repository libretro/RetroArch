include ../../sample_objdir.mk

TARGET := rjpeg_encode_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rjpeg_encode_test.c \
	$(LIBRETRO_COMM_DIR)/formats/jpeg/rjpeg_encode.c \
	$(LIBRETRO_COMM_DIR)/formats/jpeg/rjpeg.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -pedantic -std=gnu99 -g1 -I$(LIBRETRO_COMM_DIR)/include

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

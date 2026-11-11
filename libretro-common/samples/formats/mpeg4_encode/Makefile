include ../../sample_objdir.mk

TARGET := rmpeg4_encode_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rmpeg4_encode_test.c \
	$(LIBRETRO_COMM_DIR)/formats/mpeg4/rmpeg4_encode.c \
	$(LIBRETRO_COMM_DIR)/formats/mpeg4/rmpeg4_video.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -Wextra -pedantic -std=c89 -g1 -I$(LIBRETRO_COMM_DIR)/include

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

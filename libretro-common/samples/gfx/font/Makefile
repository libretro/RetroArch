include ../../sample_objdir.mk

TARGET := rtt_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rtt_test.c \
	$(LIBRETRO_COMM_DIR)/gfx/font/rtt.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -O1 -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(findstring mingw,$(CC)),)
   EXE := .exe
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

LDLIBS += -lm

all: $(TARGET)$(EXE)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET)$(EXE): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all clean

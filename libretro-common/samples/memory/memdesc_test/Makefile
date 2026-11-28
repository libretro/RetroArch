include ../../sample_objdir.mk

EXE ?=
TARGET := memdesc_test$(EXE)

LIBRETRO_COMM_DIR := ../../..

COMMON_SOURCES := $(LIBRETRO_COMM_DIR)/memory/memdesc.c

COMMON_OBJS := $(call sample_objs,$(COMMON_SOURCES:.c=.o))
OBJS        := memdesc_test.o $(COMMON_OBJS)

CFLAGS  += -Wall -pedantic -std=c89 -g1 -O2 -I$(LIBRETRO_COMM_DIR)/include

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
	rm -f memdesc_test memdesc_test.exe $(OBJS)

.PHONY: clean

include ../../sample_objdir.mk

TARGET := snprintf

CORE_DIR          := .
LIBRETRO_COMM_DIR := ../../..

SOURCES_C := 	\
	$(CORE_DIR)/snprintf_test.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_snprintf.c

OBJS := $(call sample_objs,$(SOURCES_C:.c=.o))

CFLAGS += -Wall -pedantic -std=gnu99 -I$(LIBRETRO_COMM_DIR)/include

# The samples workflow passes SANITIZER=address,undefined to every
# sample dir.  Without this block the variable was accepted and
# ignored, so these targets built unsanitized while the workflow
# reported them as checked.
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

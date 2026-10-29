# midi/drivers/coremidi.c over a stand-in for CoreMIDI (stub/), so the
# macOS driver's device removal handling builds and runs anywhere.
# Includes the driver.

include ../../sample_objdir.mk

TARGET   := coremidi_driver_test
CORE_DIR := ../../..
LRC      := $(CORE_DIR)/libretro-common

SOURCES := coremidi_driver_test.c \
           $(LRC)/rthreads/rthreads.c
CFLAGS  += -Wall -std=gnu99 -g1 -O1 -D_GNU_SOURCE -DHAVE_THREADS \
           -Istub -I$(CORE_DIR) -I$(CORE_DIR)/midi -I$(LRC)/include
LDFLAGS += -lpthread -lm
OBJS    := $(call sample_objs,$(SOURCES:.c=.o))

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
	rm -f $(TARGET) $(TARGET).exe coremidi_driver_test.o

.PHONY: all check clean

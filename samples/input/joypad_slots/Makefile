# The pad slots of input/connect/joypad_connection.c: read on one
# thread while pads are plugged and unplugged on another.

include ../../sample_objdir.mk

TARGET   := joypad_slots_test
CORE_DIR := ../../..
LRC      := $(CORE_DIR)/libretro-common

SOURCES := joypad_slots_test.c \
           $(CORE_DIR)/input/connect/joypad_connection.c \
           $(LRC)/rthreads/rthreads.c
CFLAGS  += -Wall -std=gnu99 -g -O1 -D_GNU_SOURCE -DHAVE_THREADS -DHAVE_HID \
           -I$(CORE_DIR) -I$(CORE_DIR)/input -I$(LRC)/include
LDFLAGS += -lpthread
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
	rm -f $(TARGET) $(OBJS)
.PHONY: all check clean

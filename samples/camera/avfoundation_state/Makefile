# The AVFoundation camera's authorization and session state, from the
# driver's own header: no Apple SDK needed.

include ../../sample_objdir.mk

TARGET := avfoundation_state_test

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

SOURCES_C := \
	avfoundation_state_test.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c

OBJS := $(call sample_objs,$(SOURCES_C:.c=.o))

INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)
CFLAGS  += -Wall -Wextra -std=c89 $(INCDIRS) -DHAVE_THREADS
LDFLAGS += -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1 $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
else
   CFLAGS  += -g1 -O1
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

include ../../sample_objdir.mk

TARGET := led_driver_test

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

SOURCES := \
	led_driver_test.c \
	$(CORE_DIR)/led/led_driver.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

# The harness is the overlay driver: HAVE_OVERLAY makes "overlay"
# choose it.
CFLAGS  += -Wall -std=gnu89 -g1 -O1 -DHAVE_THREADS -DHAVE_OVERLAY \
           -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)
LDFLAGS += -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

led_driver_test.o: $(CORE_DIR)/led/led_driver.h $(CORE_DIR)/led/led_defines.h

$(call sample_objs,$(CORE_DIR)/led/led_driver.o): \
	$(CORE_DIR)/led/led_driver.h $(CORE_DIR)/led/led_defines.h \
	$(CORE_DIR)/input/input_output_store.h

check: all
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET) $(TARGET).exe

.PHONY: all check clean

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(COMPILE.c) $(OUTPUT_OPTION) $<

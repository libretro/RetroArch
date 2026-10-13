# The pipewire camera driver's poll against a scripted stream: the
# stream calls are link-wrapped, so no daemon or device is needed.

include ../../sample_objdir.mk

TARGET := pipewire_poll_test

CORE_DIR          := ../../..
LIBRETRO_COMM_DIR := $(CORE_DIR)/libretro-common

SOURCES_C := \
	pipewire_poll_test.c \
	stubs_retroarch.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_int.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_filter.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/pixconv.c \
	$(LIBRETRO_COMM_DIR)/memmap/memalign.c

OBJS := $(call sample_objs,$(SOURCES_C:.c=.o))

PW_CFLAGS := $(shell pkg-config --cflags libpipewire-0.3 2>/dev/null)
PW_LIBS   := $(shell pkg-config --libs   libpipewire-0.3 2>/dev/null)

INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I$(CORE_DIR)
CFLAGS  += -Wall $(INCDIRS) $(PW_CFLAGS) -std=gnu99 -D_GNU_SOURCE -DHAVE_THREADS -DHAVE_PIPEWIRE
LDFLAGS += $(PW_LIBS) -lm -lpthread
LDFLAGS += -Wl,--wrap=pw_stream_dequeue_buffer \
	-Wl,--wrap=pw_stream_queue_buffer \
	-Wl,--wrap=pw_stream_get_state \
	-Wl,--wrap=pw_stream_get_nsec \
	-Wl,--wrap=pw_stream_update_params

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

# A poll that waits for a frame never returns; the bound turns that
# into a failure.
check: $(TARGET)
	timeout 30 ./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

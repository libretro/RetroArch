include ../../sample_objdir.mk

TARGET := rwebaudio_test

LIBRETRO_COMM_DIR := ../../../libretro-common
# The driver is #included by the test and built against the tree's
# compile-only emscripten stand-ins; its JavaScript half is a stand-in
# in the test, and runs for real under Node in `make check`.
INCDIRS := -I../../../tools/platform_stubs/emscripten -I$(LIBRETRO_COMM_DIR)/include -I../../.. -I../../../deps

CFLAGS  += -std=gnu99 -Wall $(INCDIRS) -DRARCH_INTERNAL -D_GNU_SOURCE
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g1 $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(call sample_objs,rwebaudio_test.o ../../../audio/audio_upmix.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

rwebaudio_test.o: rwebaudio_test.c ../../../audio/drivers/rwebaudio.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

check: all
	./$(TARGET)
	node rwebaudio_js_test.js

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

TARGET := filter_plugins_test

RA := ../../..
VIDEO_DIR := $(RA)/gfx/video_filters
DSP_DIR   := $(RA)/libretro-common/audio/dsp_filters

# No -lm: the harness must not load libm on the plugins' behalf.
CFLAGS += -std=gnu99 -Wall -g -I$(VIDEO_DIR) -I$(RA)/libretro-common/include
LDLIBS := -ldl -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
   PLUGIN_FLAGS := CFLAGS="-fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g" \
                   LDFLAGS="-fsanitize=$(SANITIZER)"
endif

all: $(TARGET) plugins

$(TARGET): filter_plugins_test.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS) $(LDLIBS)

plugins:
	$(MAKE) -C $(VIDEO_DIR) $(PLUGIN_FLAGS)
	$(MAKE) -C $(DSP_DIR) $(PLUGIN_FLAGS)

check: all
	./$(TARGET) $(VIDEO_DIR)/*.so $(DSP_DIR)/*.so

# Per-frame time of every video filter at one worker and at as many
# as there are CPUs online (or THREADS=N); not part of 'check'.
bench: all
	./$(TARGET) --bench $(if $(THREADS),--threads $(THREADS)) $(VIDEO_DIR)/*.so

clean:
	rm -f $(TARGET)
	$(MAKE) -C $(VIDEO_DIR) clean
	$(MAKE) -C $(DSP_DIR) clean

.PHONY: all plugins check bench clean

# Live check for the KMS beam estimate and async page flips; see
# raster_tear_live.c for how to run it. It needs a display and DRM
# master, so it has no check target: the catch-all samples job runs
# only what has one.
TARGET    := raster_tear_live
REPO_ROOT := ../../..
SOURCES   := raster_tear_live.c
CFLAGS    += -Wall -std=gnu99 -g -O2 -I$(REPO_ROOT) -I$(REPO_ROOT)/libretro-common/include \
             $(shell pkg-config --cflags libdrm)
LDFLAGS   += $(shell pkg-config --libs libdrm)

all: $(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
clean:
	rm -f $(TARGET)
.PHONY: all clean

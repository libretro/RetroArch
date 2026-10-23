# The pattern core and the image gate script; see rslc_image_check.sh.
# No check target: the gate needs a display and a driver.  A Windows
# core for a RetroArch run under Wine: make CC=x86_64-w64-mingw32-gcc EXT=dll
EXT       ?= so
TARGET    := pattern_core.$(EXT)
REPO_ROOT := ../../..
CFLAGS    += -Wall -Wextra -pedantic -std=c89 -Wno-long-long -O2 \
             -I$(REPO_ROOT)/libretro-common/include
ifneq ($(EXT),dll)
CFLAGS    += -fPIC
endif

all: $(TARGET)
$(TARGET): pattern_core.c
	$(CC) $(CFLAGS) -shared -o $@ $<
clean:
	rm -f pattern_core.so pattern_core.dll
.PHONY: all clean

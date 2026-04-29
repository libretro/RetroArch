# Makefile for ntsc_crt RetroArch softfilter
#
# Usage:
#   make          -> builds ntsc_crt.so (Linux)
#   make WINDOWS=1 -> cross-compiles ntsc_crt.dll (requires mingw-w64)
#   make install  -> copies .so + .filt to FILTERDIR

CC        = gcc
CFLAGS    = -O2 -std=c99 -Wall -fPIC -DCRT_SYSTEM=0 -I..
LDFLAGS   = -shared -lm

# Adjust this to your RetroArch installation path
FILTERDIR = /usr/share/retroarch/filters/video

SRC = ntsc_crt.c crt_core.c crt_ntsc.c
TARGET = ntsc_crt.so

ifdef WINDOWS
  CC     = x86_64-w64-mingw32-gcc
  TARGET = ntsc_crt.dll
  CFLAGS += -D_WIN32
endif

all: $(TARGET)

$(TARGET): $(SRC)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $^

install: $(TARGET)
	install -d $(FILTERDIR)
	install -m 644 $(TARGET) $(FILTERDIR)/
	install -m 644 ntsc_crt.filt $(FILTERDIR)/

clean:
	rm -f $(TARGET)

.PHONY: all install clean

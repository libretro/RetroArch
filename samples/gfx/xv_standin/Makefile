# An XVideo adaptor for an X server with none, preloaded in front of
# libXv: what the xvideo driver's views end-to-end cases run against on
# Xvfb. See xv_standin.c.
CC     ?= gcc
CFLAGS += -Wall -Wextra -O2 -fPIC $(EXTRA_CFLAGS)
TARGET := libxv_standin.so

all: $(TARGET)

$(TARGET): xv_standin.c
	$(CC) $(CFLAGS) -shared -o $@ xv_standin.c -lXext -lX11 $(LDFLAGS)

clean:
	rm -f $(TARGET)

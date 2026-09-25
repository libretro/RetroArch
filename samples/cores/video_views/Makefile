TARGET := video_views_libretro.so

CFLAGS  += -O2 -g -fPIC -Wall -std=gnu99 -I../../../libretro-common/include \
           -I../../../gfx/include
LDFLAGS += -shared -Wl,--no-undefined

all: $(TARGET)

$(TARGET): video_views_core.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean

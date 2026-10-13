TARGET := video_views_libretro.so

CFLAGS  += -O2 -g1 -fPIC -Wall -std=gnu99 -I../../../libretro-common/include \
           -I../../../gfx/include
LDFLAGS += -shared -Wl,--no-undefined

# make DLL=1 CC=x86_64-w64-mingw32-gcc: the Windows core, for Wine
ifeq ($(DLL),1)
TARGET  := video_views_libretro.dll
CFLAGS  := $(filter-out -fPIC,$(CFLAGS))
LDFLAGS += -static-libgcc -Wl,-Bstatic -lwinpthread -Wl,-Bdynamic -ldxguid
endif

all: $(TARGET)

$(TARGET): video_views_core.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f video_views_libretro.so video_views_libretro.dll

.PHONY: all clean

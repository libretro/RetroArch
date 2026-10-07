CORE := egl_hw_core_libretro.so
SHIM := egl_shim.so

CFLAGS += -Wall -O2 -fPIC -I../../../libretro-common/include

all: $(CORE) $(SHIM)

$(CORE): egl_hw_core.c
	$(CC) -std=c89 -pedantic $(CFLAGS) -shared -o $@ egl_hw_core.c $(LDFLAGS)

$(SHIM): egl_shim.c
	$(CC) $(CFLAGS) -shared -o $@ egl_shim.c $(LDFLAGS) -ldl

# Needs a built ./retroarch with the gl driver and EGL on X11, Xvfb
# and Mesa.
check: all
	python3 egl_hw_core_test.py ../../../retroarch

clean:
	rm -f $(CORE) $(SHIM)

.PHONY: all check clean

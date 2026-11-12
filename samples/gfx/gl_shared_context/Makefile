CORE := gl_shared_context_libretro.so

CFLAGS += -Wall -O2 -fPIC -I../../../libretro-common/include

all: $(CORE)

# C99 for unsigned long long, which glWaitSync's timeout is
$(CORE): gl_shared_context.c
	$(CC) -std=c99 -pedantic $(CFLAGS) -shared -o $@ gl_shared_context.c $(LDFLAGS) -lpthread

# Needs a built ./retroarch with the gl driver on X11 (GLX and EGL), Xvfb
# and Mesa.
check: all
	python3 gl_shared_context_test.py ../../../retroarch

clean:
	rm -f $(CORE)

.PHONY: all check clean

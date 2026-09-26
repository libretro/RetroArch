TARGET := libxr_test_layer.so

CFLAGS  += -O2 -g -fPIC -Wall -std=gnu11 -fvisibility=hidden
# nodelete: the loader unloads layers at xrDestroyInstance, which would
# restart the recording and replay the script for the next instance.
LDFLAGS += -shared -Wl,--no-undefined -Wl,-z,nodelete -lvulkan -lz -lpthread -lm

all: $(TARGET)

$(TARGET): xr_test_layer.c
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

clean:
	rm -f $(TARGET)

.PHONY: all clean

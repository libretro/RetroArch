TARGET := gl1_upload_core_libretro.so

CFLAGS += -std=c89 -pedantic -Wall -O2 -fPIC -I../../../libretro-common/include

all: $(TARGET)

$(TARGET): gl1_upload_core.c
	$(CC) $(CFLAGS) -shared -o $@ gl1_upload_core.c $(LDFLAGS)

# Needs a built ./retroarch with the gl1 driver, Xvfb and Mesa.
check: $(TARGET)
	python3 gl1_upload_test.py ../../../retroarch

clean:
	rm -f $(TARGET)

.PHONY: all check clean

# input/drivers/openxr_input.c against a stand-in for the OpenXR
# runtime: the host's compiler, the vendored OpenXR headers, no NDK.
TARGET := openxr_input_test
ROOT   := ../../..
CFLAGS += -std=gnu99 -O1 -Wall -Wno-unused-function -Wno-unused-parameter \
          -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_OPENXR \
          -Istandin -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/deps \
          -I$(ROOT)/deps/openxr/include

all: $(TARGET)

$(TARGET): openxr_input_test.c $(ROOT)/input/drivers/openxr_input.c
	$(CC) $(CFLAGS) -o $@ openxr_input_test.c \
		$(ROOT)/libretro-common/compat/compat_strl.c -lm

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

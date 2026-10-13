# input/common/input_openxr.c, the headset's controllers, against a
# stand-in for the OpenXR runtime and the frontend around it: the
# host's compiler, the vendored OpenXR headers, no headset.
TARGET := openxr_input_test
ROOT   := ../../..
CFLAGS += -std=gnu99 -O1 -g1 -Wall -Wno-unused-function -Wno-unused-parameter \
          -DRARCH_INTERNAL -DHAVE_THREADS -DHAVE_OPENXR -DHAVE_MENU \
          -I$(ROOT) -I$(ROOT)/libretro-common/include -I$(ROOT)/deps \
          -I$(ROOT)/deps/openxr/include

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

SRCS := openxr_input_test.c \
        $(ROOT)/libretro-common/compat/compat_strl.c \
        $(ROOT)/libretro-common/string/stdstring.c \
        $(ROOT)/libretro-common/encodings/encoding_utf.c \
        $(ROOT)/gfx/video_xr.c \
        $(ROOT)/gfx/video_views.c

all: $(TARGET)

$(TARGET): $(SRCS) $(ROOT)/input/common/input_openxr.c
	$(CC) $(CFLAGS) -o $@ $(SRCS) $(LDFLAGS) -lm

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

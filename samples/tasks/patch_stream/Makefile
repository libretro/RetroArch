TARGET := patch_stream_test

SOURCES := patch_stream_test.c ../../../tasks/patch_stream.c \
	../../../libretro-common/encodings/encoding_vcdiff.c \
	../../../libretro-common/encodings/encoding_crc32.c \
	../../../libretro-common/features/features_cpu.c \
	../../../libretro-common/compat/compat_strl.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g -O0 -DHAVE_XDELTA -I../../../libretro-common/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

TARGET := hdr_metadata_test
SOURCES := hdr_metadata_test.c
OBJS := $(SOURCES:.c=.o)
CFLAGS += -std=gnu89 -O1 -g1 -Wall -Wextra -Werror=declaration-after-statement -I../../../libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(OBJS): ../../../gfx/video_hdr_metadata.h ../../../libretro-common/include/libretro.h

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean

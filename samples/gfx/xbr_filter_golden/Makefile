TARGET := xbr_filter_golden_test

CFLAGS  += -Wall -std=gnu99 -O2 -I../../../libretro-common/include \
	-I../../../gfx/video_filters
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): xbr_filter_golden_test.c golden.h ../../../gfx/video_filters/2xbr.c
	$(CC) $(CFLAGS) -o $@ xbr_filter_golden_test.c $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

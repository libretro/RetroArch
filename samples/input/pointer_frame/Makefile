TARGET    := pointer_frame_test
REPO_ROOT := ../../..
CFLAGS    += -Wall -std=gnu99 -O1 -g1 -DRARCH_INTERNAL -DHAVE_VIDEO_WINDOWS \
             -DHAVE_OVERLAY \
             -I$(REPO_ROOT) -I$(REPO_ROOT)/libretro-common/include -I$(REPO_ROOT)/deps

ifeq ($(SANITIZER),address)
   CFLAGS  += -fsanitize=address -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=address
endif
ifeq ($(SANITIZER),undefined)
   CFLAGS  += -fsanitize=undefined -fno-sanitize-recover=undefined
   LDFLAGS += -fsanitize=undefined
endif

all: $(TARGET)

$(TARGET): pointer_frame_test.c $(REPO_ROOT)/input/input_pointer_frame.h \
           $(REPO_ROOT)/input/drivers/android_pointers.h
	$(CC) $(CFLAGS) -o $@ pointer_frame_test.c $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

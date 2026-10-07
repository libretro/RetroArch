TARGET := key_lane_test

REPO_ROOT := ../../..

CFLAGS += -Wall -Wno-unused -std=gnu99 -g -O1 \
   -I$(REPO_ROOT)/libretro-common/include -I$(REPO_ROOT)
LDFLAGS += -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): key_lane_test.c $(REPO_ROOT)/input/input_key_lane.h
	$(CC) $(CFLAGS) -o $@ key_lane_test.c $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

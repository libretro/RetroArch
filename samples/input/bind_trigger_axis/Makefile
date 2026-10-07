TARGET := bind_trigger_axis_test

REPO_ROOT := ../../..

CFLAGS += -Wall -Wno-unused -std=gnu99 -g -O1 \
   -I$(REPO_ROOT)/libretro-common/include

ifneq ($(SANITIZER),)
   CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
   LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): bind_trigger_axis_test.c $(REPO_ROOT)/menu/menu_bind_trigger.h
	$(CC) $(CFLAGS) -o $@ bind_trigger_axis_test.c $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

ROOT := ../../..
CFLAGS += -std=gnu89 -O1 -g -Wall -Wextra -I$(ROOT) \
          -I$(ROOT)/libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: winraw_mouse_pending_test
winraw_mouse_pending_test: winraw_mouse_pending_test.c $(ROOT)/input/drivers/winraw_mouse_pending.h
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $<
check: all
	./winraw_mouse_pending_test
clean:
	rm -f winraw_mouse_pending_test winraw_mouse_pending_test.exe
.PHONY: all check clean

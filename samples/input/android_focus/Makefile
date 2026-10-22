CC ?= cc
CFLAGS ?= -O2 -g1
CFLAGS += -Wall -Wextra -std=gnu89 -I../../../input/drivers -I../../../libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
endif
TARGET = android_focus_test$(EXE)

all: $(TARGET)

$(TARGET): android_focus_test.c ../../../input/drivers/android_focus.h
	$(CC) $(CFLAGS) android_focus_test.c -o $@

check: $(TARGET)
	$(RUNNER) ./$(TARGET)

clean:
	rm -f android_focus_test android_focus_test.exe

.PHONY: all check clean

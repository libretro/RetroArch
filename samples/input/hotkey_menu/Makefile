CC ?= cc
PYTHON ?= python3
CFLAGS ?= -O2 -g1
CFLAGS += -Wall -Wextra -std=gnu89 -I../../.. -I../../../libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
endif
TARGET = hotkey_menu_test$(EXE)

all: $(TARGET)

driver_code.inc: extract.py ../../../input/input_driver.c
	$(PYTHON) extract.py

$(TARGET): hotkey_menu_test.c driver_code.inc
	$(CC) $(CFLAGS) hotkey_menu_test.c -o $@

check: $(TARGET)
	$(RUNNER) ./$(TARGET)

clean:
	rm -f driver_code.inc hotkey_menu_test hotkey_menu_test.exe

.PHONY: all check clean

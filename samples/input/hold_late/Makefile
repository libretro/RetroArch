CC ?= cc
PYTHON ?= python3
CFLAGS ?= -O2 -g1
CFLAGS += -Wall -Wextra -std=gnu89 -I../../.. -I../../../libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
endif
TARGET = hold_late_test$(EXE)

all: $(TARGET)

driver_code.inc: extract.py ../../../input/input_driver.c
	$(PYTHON) extract.py

$(TARGET): hold_late_test.c driver_code.inc
	$(CC) $(CFLAGS) hold_late_test.c -o $@

check: $(TARGET)
	$(RUNNER) ./$(TARGET)

clean:
	rm -f driver_code.inc hold_late_test hold_late_test.exe

.PHONY: all check clean

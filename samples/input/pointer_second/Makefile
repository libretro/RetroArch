CC ?= cc
PYTHON ?= python3
CFLAGS ?= -O2 -g1
CFLAGS += -Wall -Wextra -Wno-unused-parameter -std=gnu89 -DHAVE_OVERLAY -DHAVE_VIDEO_WINDOWS -I../../.. -I../../../libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
endif
TARGET = pointer_second_test$(EXE)

all: $(TARGET)

driver_code.inc: extract.py ../../../input/input_driver.c
	$(PYTHON) extract.py

$(TARGET): pointer_second_test.c driver_code.inc
	$(CC) $(CFLAGS) pointer_second_test.c -o $@

check: $(TARGET)
	$(RUNNER) ./$(TARGET)

clean:
	rm -f driver_code.inc pointer_second_test pointer_second_test.exe

.PHONY: all check clean

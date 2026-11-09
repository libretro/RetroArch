CC ?= cc
PYTHON ?= python3
CFLAGS ?= -O2 -g1
CFLAGS += -Wall -Wextra -Wno-unused-function -Wno-unused-parameter -std=gnu89 \
   -DRARCH_INTERNAL -I../../.. -I../../../libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
endif
TARGET = bind_capture_test$(EXE)
SOURCES = bind_capture_test.c ../../../libretro-common/compat/compat_strl.c

all: $(TARGET)

driver_code.inc: extract.py ../../../input/input_driver.c ../../../menu/menu_driver.c
	$(PYTHON) extract.py

$(TARGET): $(SOURCES) driver_code.inc ../../../input/input_pointer_frame.h
	$(CC) $(CFLAGS) $(SOURCES) -o $@

check: $(TARGET)
	$(RUNNER) ./$(TARGET)

clean:
	rm -f driver_code.inc bind_capture_test bind_capture_test.exe

.PHONY: all check clean

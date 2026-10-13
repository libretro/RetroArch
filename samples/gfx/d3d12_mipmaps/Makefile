CC ?= cc
PYTHON ?= python3
CFLAGS ?= -O2 -g1
CFLAGS += -Wall -Wextra -pedantic -std=c89
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
endif
TARGET = d3d12_mipmaps_test$(EXE)

all: $(TARGET)

driver_code.inc: extract.py ../../../gfx/drivers/d3d12.c
	$(PYTHON) extract.py

$(TARGET): d3d12_mipmaps_test.c driver_code.inc
	$(CC) $(CFLAGS) d3d12_mipmaps_test.c -o $@

check: $(TARGET)
	$(RUNNER) ./$(TARGET)

clean:
	rm -f driver_code.inc d3d12_mipmaps_test d3d12_mipmaps_test.exe

.PHONY: all check clean

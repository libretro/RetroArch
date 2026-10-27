CORE_DIR = ../../..
CC      ?= cc
PYTHON  ?= python3
RETROARCH ?= $(CORE_DIR)/retroarch

ifeq ($(OS),Windows_NT)
   CORE := netplay_e2e_libretro.dll
else
   CORE := netplay_e2e_libretro.so
endif

CFLAGS += -O1 -g -std=gnu99 -Wall -fPIC -I$(CORE_DIR)/libretro-common/include

all: $(CORE)

$(CORE): netplay_e2e_core.c
	$(CC) $(CFLAGS) -shared -o $@ $< $(LDFLAGS)

check: $(CORE)
	$(PYTHON) netplay_e2e.py $(RETROARCH) ./$(CORE)

clean:
	rm -f netplay_e2e_libretro.so netplay_e2e_libretro.dll

.PHONY: all check clean

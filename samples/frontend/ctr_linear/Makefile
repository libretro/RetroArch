# os/ctr/ctr_linear.c, the 3DS linear heap allocator, against a stand-in
# <3ds.h> and a linear heap at an address it hands out but never
# touches. Builds for Linux and, with CC=x86_64-w64-mingw32-gcc EXE=.exe
# RUNNER=wine, for Windows.
TARGET   := ctr_linear_test$(EXE)
CORE_DIR := ../../..

SOURCES := ctr_linear_test.c $(CORE_DIR)/os/ctr/ctr_linear.c

CFLAGS  += -std=c89 -Wall -g1 -O1 -I. -I$(CORE_DIR)/libretro-common/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

$(TARGET): $(SOURCES) 3ds.h 3ds/types.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

check: all
	$(RUNNER) ./$(TARGET)

clean:
	rm -f ctr_linear_test ctr_linear_test.exe

.PHONY: all check clean

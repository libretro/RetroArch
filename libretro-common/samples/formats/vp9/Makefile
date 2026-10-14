CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c89
CFLAGS += -I../../../include
TARGET := rvp9_reference_test

ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): rvp9_reference_test.c ../../../formats/vp9/rvp9.c ../../../formats/vp9/rvp9_recon.inc ../../../include/formats/rvp9.h
	$(CC) $(CFLAGS) rvp9_reference_test.c -o $@ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean

# input/input_pad_tag.h on its own: the tags that tell same-named
# controllers apart and the identity hash behind them.
CC ?= cc
CFLAGS ?= -O1 -g1
CFLAGS += -Wall -Wextra -std=c89 -pedantic \
   -I../../../input -I../../../libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
TARGET = pad_tag_test$(EXE)

all: $(TARGET)

$(TARGET): pad_tag_test.c ../../../input/input_pad_tag.h
	$(CC) $(CFLAGS) pad_tag_test.c -o $@ $(LDFLAGS)

check: $(TARGET)
	$(RUNNER) ./$(TARGET)

clean:
	rm -f pad_tag_test pad_tag_test.exe

.PHONY: all check clean

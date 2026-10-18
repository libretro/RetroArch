# ui/drivers/ui_qt_float_sync.h against model float widgets that round
# the way the Qt spinbox and slider do: a setting finer than the
# widget's precision is pushed once, not on every repaint.

TARGET := qt_float_resync_test

SOURCES := qt_float_resync_test.c
CFLAGS  += -Wall -Wextra -std=c89 -pedantic -g1 -O1
OBJS    := $(SOURCES:.c=.o)

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c ../../../ui/drivers/ui_qt_float_sync.h
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean

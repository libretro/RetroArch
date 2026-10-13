ROOT := ../../..
PYTHON ?= python3
THREADS ?= 1
TARGET := mmdevice_notification_test
CFLAGS += -std=gnu89 -O1 -g1 -Wall -Wextra -I$(ROOT)/libretro-common/include
ifeq ($(THREADS),1)
CFLAGS += -DHAVE_THREADS
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
all: $(TARGET)
driver_start.inc: extract.py $(ROOT)/audio/drivers/xaudio.c $(ROOT)/audio/drivers/dsound.c
	$(PYTHON) extract.py
$(TARGET): mmdevice_notification_test.c driver_start.inc
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)
check: all
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe driver_start.inc
.PHONY: all check clean

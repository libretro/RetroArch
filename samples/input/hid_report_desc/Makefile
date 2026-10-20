TARGET := hid_report_desc_test

# A gamepad's reports read from its HID report descriptor
# (input/common/hid_report_desc.h), checked without a pad.
#    make && make check
REPO_ROOT := ../../..

CFLAGS += -Wall -std=gnu89 -Wdeclaration-after-statement -g1 -O1 \
          -I$(REPO_ROOT)/libretro-common/include
ifneq ($(SANITIZER),)
CFLAGS  += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET)

$(TARGET): hid_report_desc_test.c $(REPO_ROOT)/input/common/hid_report_desc.h
	$(CC) $(CFLAGS) -o $@ hid_report_desc_test.c $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET) $(TARGET).exe

.PHONY: all check clean

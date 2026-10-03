TARGET := sony_pad_output_test

# The rumble reports of the DualShock 4 and the DualSense, built and
# checked without a pad.
#    make && make check
REPO_ROOT := ../../..

CFLAGS += -Wall -std=gnu89 -Wdeclaration-after-statement -g -O1 \
          -I$(REPO_ROOT)/libretro-common/include

all: $(TARGET)

$(TARGET): sony_pad_output_test.c $(REPO_ROOT)/input/common/sony_pad_output.h
	$(CC) $(CFLAGS) -o $@ sony_pad_output_test.c

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

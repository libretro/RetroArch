TARGET := sony_pad_motion_test

# The motion sensors of the DualShock 4 and the DualSense, read out of
# their input reports and checked without a pad.
#    make && make check
REPO_ROOT := ../../..

CFLAGS += -Wall -std=gnu89 -Wdeclaration-after-statement -g -O1 \
          -I$(REPO_ROOT)/libretro-common/include

all: $(TARGET)

$(TARGET): sony_pad_motion_test.c $(REPO_ROOT)/input/common/sony_pad_motion.h
	$(CC) $(CFLAGS) -o $@ sony_pad_motion_test.c \
		$(REPO_ROOT)/libretro-common/encodings/encoding_crc32.c \
		$(REPO_ROOT)/libretro-common/features/features_cpu.c \
		$(REPO_ROOT)/libretro-common/time/rtime.c \
		$(REPO_ROOT)/libretro-common/compat/compat_strl.c -lm

check: all
	./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

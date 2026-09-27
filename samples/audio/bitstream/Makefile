TARGET := bitstream_test

LIBRETRO_COMM_DIR := ../../../libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include -I../../..

SOURCES_C := \
	bitstream_test.c \
	../../../audio/audio_bitstream.c \
	$(LIBRETRO_COMM_DIR)/formats/dts/rdts.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_frame.c \
	$(LIBRETRO_COMM_DIR)/formats/iec61937/iec61937.c

CFLAGS  += -std=gnu99 -Wall -Wextra $(INCDIRS) -DHAVE_RDTS -DHAVE_RAC3
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(SOURCES_C:.c=.o)

all: $(TARGET)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS) $(TARGET)

.PHONY: all check clean

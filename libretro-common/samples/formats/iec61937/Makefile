compiler     := gcc
TARGET       := iec61937_test

LIBRETRO_COMM_DIR = ../../..
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include

CC      := $(compiler)
flags   += -std=gnu99

SOURCES_C := \
	iec61937_test.c \
	$(LIBRETRO_COMM_DIR)/formats/iec61937/iec61937.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_frame.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_decode.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_encode.c

CFLAGS += -Wall -pedantic $(INCDIRS) -D_GNU_SOURCE -O2
LDFLAGS += -lm

ifneq ($(SAN),)
   SANITIZER := $(SAN)
endif
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(SOURCES_C:.c=.o)

all: $(TARGET)

%.o: %.c
	$(CC) $(INCDIRS) $(CFLAGS) $(flags) -c -o$@ $<

$(TARGET): $(OBJS)
	$(CC) -o$@ $(OBJS) $(LDFLAGS)

# The ffmpeg checks are skipped where it is not installed.
check: $(TARGET)
	./$(TARGET)

clean:
	rm -f $(OBJS)
	rm -f $(TARGET)

.PHONY: clean all check

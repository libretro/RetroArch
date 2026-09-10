compiler     := gcc
TARGET       := rac3_frame_test
TARGET2      := rac3_tables_test

LIBRETRO_COMM_DIR = ../../..
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include

CC      := $(compiler)
flags   += -std=gnu99

SOURCES_C := \
	rac3_frame_test.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_frame.c

CFLAGS += -Wall -pedantic $(INCDIRS) -D_GNU_SOURCE
LDFLAGS += -lm

ifneq ($(SAN),)
   SANITIZER := $(SAN)
endif
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(SOURCES_C:.c=.o)

all: $(TARGET) $(TARGET2)

$(TARGET2): rac3_tables_test.c $(LIBRETRO_COMM_DIR)/formats/ac3/rac3_tables.h
	$(CC) $(INCDIRS) $(CFLAGS) $(flags) -Wno-unused-variable -Wno-unused-const-variable -o$@ rac3_tables_test.c $(LDFLAGS)

%.o: %.c
	$(CC) $(INCDIRS) $(CFLAGS) $(flags) -c -o$@ $<

$(TARGET): $(OBJS)
	$(CC) -o$@ $(OBJS) $(LDFLAGS)

# The reference streams come from ffmpeg, so the check is skipped
# where it is not installed; the frame walk is then unexercised, and
# the job says so.
check: $(TARGET) $(TARGET2)
	./$(TARGET2)
	./$(TARGET)

clean:
	rm -f $(OBJS)
	rm -f $(TARGET) $(TARGET2)

.PHONY: clean all check

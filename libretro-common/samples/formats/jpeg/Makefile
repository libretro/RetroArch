TARGET := rjpeg_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rjpeg_test.c \
	$(LIBRETRO_COMM_DIR)/formats/jpeg/rjpeg.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c

OBJS := $(SOURCES:.c=.o)

CFLAGS  += -Wall -pedantic -std=gnu99 -g -I$(LIBRETRO_COMM_DIR)/include

# The samples workflow passes SANITIZER=address,undefined to every
# sample dir.  Without this block the variable was accepted and
# ignored, so these targets built unsanitized while the workflow
# reported them as checked.
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

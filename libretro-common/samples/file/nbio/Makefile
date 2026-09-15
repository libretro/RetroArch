TARGET := nbio_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	nbio_test.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_intf.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_linux.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_unixmmap.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_windowsmmap.c \
	$(LIBRETRO_COMM_DIR)/file/nbio/nbio_stdio.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -pedantic -std=gnu99 -g -I$(LIBRETRO_COMM_DIR)/include

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
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

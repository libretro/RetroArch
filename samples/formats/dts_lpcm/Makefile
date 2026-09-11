TARGET := dts_lpcm_test

LIBRETRO_COMM_DIR := ../../../libretro-common
INCDIRS := -I$(LIBRETRO_COMM_DIR)/include

SOURCES_C := \
	dts_lpcm_test.c \
	$(LIBRETRO_COMM_DIR)/formats/dts/rdts.c \
	$(LIBRETRO_COMM_DIR)/formats/lpcm/rlpcm.c

CFLAGS  += -std=gnu99 -Wall -Wextra $(INCDIRS)
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

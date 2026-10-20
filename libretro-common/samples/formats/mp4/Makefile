include ../../sample_objdir.mk

TARGET := rmp4_fragment_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := rmp4_fragment_test.c

CFLAGS  += -Wall -std=gnu99 -g1 -O1 -DHAVE_RMP4 -DRMP4_TEST_ALLOCATOR -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

all: $(TARGET)

$(OBJS): $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c \
         $(LIBRETRO_COMM_DIR)/include/formats/rmp4.h

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -lm

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET).exe $(OBJS)

.PHONY: all check clean

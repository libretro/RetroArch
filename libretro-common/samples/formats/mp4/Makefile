include ../../sample_objdir.mk

TARGET := rmp4_fragment_test
TARGET_TEST := rmp4_language_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := rmp4_fragment_test.c

CFLAGS  += -Wall -std=gnu99 -g1 -O1 -DHAVE_RMP4 -DRMP4_TEST_ALLOCATOR -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

all: $(TARGET) $(TARGET_TEST)

$(OBJS): $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c \
         $(LIBRETRO_COMM_DIR)/include/formats/rmp4.h

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) -lm

$(TARGET_TEST): rmp4_language_test.c $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c \
                   $(LIBRETRO_COMM_DIR)/include/formats/rmp4.h
	$(CC) $(CFLAGS) -std=c89 -pedantic -Wextra -Werror -Wno-long-long \
	 -Wframe-larger-than=2048 rmp4_language_test.c \
	 $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c -o $@ $(LDFLAGS) -lm

check: $(TARGET) $(TARGET_TEST)
	./$(TARGET)
	./$(TARGET_TEST)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET).exe $(TARGET_TEST) $(TARGET_TEST).exe $(OBJS)

.PHONY: all check clean

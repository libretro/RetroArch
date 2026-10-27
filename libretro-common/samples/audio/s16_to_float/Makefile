include ../../sample_objdir.mk

TARGET := s16_to_float_test

LIBRETRO_COMM_DIR := ../../..

COMMON_SOURCES := \
	$(LIBRETRO_COMM_DIR)/audio/conversion/s16_to_float.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c

COMMON_OBJS := $(call sample_objs,$(COMMON_SOURCES:.c=.o))
OBJS        := s16_to_float_test.o $(COMMON_OBJS)

CFLAGS  += -DS16_TO_FLOAT_TEST_HOOKS -Wall -pedantic -std=gnu99 -g1 -O2 -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: clean

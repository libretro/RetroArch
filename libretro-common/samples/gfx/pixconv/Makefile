TARGET      := pixconv_test
TARGET_TEST := pixconv_test_nosimd
TARGET_TEST2 := pixconv_test_scalar

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	pixconv_test.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/pixconv.c

CFLAGS += -Wall -pedantic -std=c89 -g -I$(LIBRETRO_COMM_DIR)/include

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

# pixconv_test_nosimd drops the SSE2 paths, which puts rows through
# the MMX paths where x86 has them.  pixconv_test_scalar drops MMX and
# NEON too, so every row runs the scalar path non-SIMD targets use.
all: $(TARGET) $(TARGET_TEST) $(TARGET_TEST2)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

$(TARGET_TEST): $(SOURCES)
	$(CC) $(CFLAGS) -DSCALER_NO_SIMD -o $@ $(SOURCES) $(LDFLAGS)

$(TARGET_TEST2): $(SOURCES)
	$(CC) $(CFLAGS) -DSCALER_NO_SIMD -U__MMX__ -U__ARM_NEON -U__ARM_NEON__ -o $@ $(SOURCES) $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET_TEST) $(TARGET_TEST2)

.PHONY: all clean

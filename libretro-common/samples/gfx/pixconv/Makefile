TARGET      := pixconv_test
TARGET_TEST := pixconv_test_nosimd
TARGET_TEST2 := pixconv_test_scalar
TARGET_WIDE  := pixconv_test_wide

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	pixconv_test.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/pixconv.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_int.c \
	$(LIBRETRO_COMM_DIR)/gfx/scaler/scaler_filter.c \
	$(LIBRETRO_COMM_DIR)/memmap/memalign.c

CFLAGS += -Wall -pedantic -std=c89 -g1 -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

# pixconv_test_nosimd drops the SSE2 paths, which puts rows through
# the MMX paths where x86 has them.  pixconv_test_scalar drops MMX and
# NEON too, so every row runs the scalar path non-SIMD targets use.
# pixconv_test_wide compiles in the run-time AVX2 / AVX-512 converters
# (as RetroArch's own build does) and runs each tier the CPU has; it
# needs features_cpu.c and only means something on x86 hosts.
ALL_TARGETS := $(TARGET) $(TARGET_TEST) $(TARGET_TEST2)
ifneq ($(filter x86_64% i686% i386%,$(shell $(CC) -dumpmachine)),)
ALL_TARGETS += $(TARGET_WIDE)
endif

all: $(ALL_TARGETS)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)

$(TARGET_TEST): $(SOURCES)
	$(CC) $(CFLAGS) -DSCALER_NO_SIMD -o $@ $(SOURCES) $(LDFLAGS)

$(TARGET_TEST2): $(SOURCES)
	$(CC) $(CFLAGS) -DSCALER_NO_SIMD -U__MMX__ -U__ARM_NEON -U__ARM_NEON__ -o $@ $(SOURCES) $(LDFLAGS)

$(TARGET_WIDE): $(SOURCES)
	$(CC) $(CFLAGS) -DPIXCONV_RUNTIME_SIMD -DPIXCONV_TEST_HOOKS \
		-D_POSIX_C_SOURCE=200112L -o $@ $(SOURCES) \
		$(LIBRETRO_COMM_DIR)/features/features_cpu.c $(LDFLAGS)

clean:
	rm -f $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(TARGET_WIDE)

.PHONY: all clean

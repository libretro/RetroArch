include ../../sample_objdir.mk

TARGET            := rinflate_window_test
TARGET_TEST       := rdeflate_hash_test
TARGET_TEST2      := rdeflate_flush_test
TARGET_TEST3      := encoding_adler32_test

LIBRETRO_COMM_DIR := ../../..

# encoding_deflate.c folds CRC-32 through encoding_crc32.c, which asks
# cpu_features_get() for PCLMULQDQ; features_cpu.c needs compat_strl.c,
# and on Windows rtime.c (retro_sleep_us).
SOURCES := \
	rinflate_window_test.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

CFLAGS  += -Wall -pedantic -std=gnu99 -g1 -O2 -I$(LIBRETRO_COMM_DIR)/include

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

# The hash test compiles encoding_deflate.c into itself; it needs the
# rest of what the deflater links, not the window test.
HASH_SOURCES := rdeflate_hash_test.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

# x86: built for a CPU without SSE4.2, so the hash is the run-time one
HASH_ARCH :=
ifneq ($(filter x86_64 i%86,$(shell $(CC) -dumpmachine 2>/dev/null | cut -d- -f1)),)
   HASH_ARCH := -march=core2
endif

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

# The flush test links what the window test does, and so does the adler32 one
FLUSH_SOURCES := rdeflate_flush_test.c $(filter-out rinflate_window_test.c,$(SOURCES))
ADLER_SOURCES := encoding_adler32_test.c $(filter-out rinflate_window_test.c,$(SOURCES))

all: $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(TARGET_TEST3)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST): $(HASH_SOURCES) $(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c
	$(CC) $(CFLAGS) $(HASH_ARCH) -o $@ $(HASH_SOURCES) $(LDFLAGS)

$(TARGET_TEST2): $(FLUSH_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(FLUSH_SOURCES) $(LDFLAGS)

$(TARGET_TEST3): $(ADLER_SOURCES)
	$(CC) $(CFLAGS) -o $@ $(ADLER_SOURCES) $(LDFLAGS)

check: all
	./$(TARGET)
	./$(TARGET_TEST)
	./$(TARGET_TEST2)
	./$(TARGET_TEST3)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET_TEST) $(TARGET_TEST2) $(TARGET_TEST3) $(TARGET).exe $(TARGET_TEST).exe $(TARGET_TEST2).exe $(TARGET_TEST3).exe $(OBJS)

.PHONY: all check clean

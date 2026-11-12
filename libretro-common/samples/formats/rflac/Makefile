include ../../sample_objdir.mk

TARGET      := rflac_s16_test
TARGET_TEST := rflac_s16_nosimd_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -pedantic -std=gnu99 -g1 -O1 -I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -lm

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(TARGET_TEST)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

# rflac itself twice: with the SIMD loops the target has, and without
$(SAMPLE_OBJDIR)/rflac_simd.o: $(LIBRETRO_COMM_DIR)/formats/flac/rflac.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/rflac_nosimd.o: $(LIBRETRO_COMM_DIR)/formats/flac/rflac.c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS) -DRFLAC_NO_SIMD

$(TARGET): rflac_s16_test.o $(SAMPLE_OBJDIR)/rflac_simd.o $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

$(TARGET_TEST): rflac_s16_test.o $(SAMPLE_OBJDIR)/rflac_nosimd.o $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

rflac_s16_test.o: rflac_s16_test.c
	$(CC) -c -o $@ $< $(CFLAGS)

check: all
	./$(TARGET)
	./$(TARGET_TEST)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET_TEST) rflac_s16_test.o

.PHONY: all check clean

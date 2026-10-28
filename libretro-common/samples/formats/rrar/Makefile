include ../../sample_objdir.mk

TARGET := rrar_archive_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rrar_archive_test.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/formats/rar/rrar_archive.c \
	$(LIBRETRO_COMM_DIR)/formats/rar/rrar_ppmd7.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -I$(LIBRETRO_COMM_DIR)/include
# every member streamed out, in steps of 16 KiB, so the archives' small
# members go through rrar_archive_extract_sink()'s ring too, and the
# 241 MB one comes round its 4 MiB dictionary's a hundred times
CFLAGS += -DRRAR_SINK_ALWAYS -DRRAR_SINK_STEP=0x4000

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c $< -o $@

$(TARGET): $(OBJS)
	$(CC) $(OBJS) $(LDFLAGS) -o $@ -lm

check: $(TARGET)
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean

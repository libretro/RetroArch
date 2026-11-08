include ../../sample_objdir.mk

TARGET := rzstd_raw_block_test
# The same test with the decoder's vector copies switched off, as on
# builds with no SSE2 or NEON, where the copy widths differ
TARGET_TEST := rzstd_raw_block_test_novec
TARGET_DICT := rzstd_dict_test

LIBRETRO_COMM_DIR := ../../..

SOURCES := \
	rzstd_raw_block_test.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_rzstd.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_xxh64.c

CFLAGS += -Wall -pedantic -std=gnu99 -g1 -O2 -I$(LIBRETRO_COMM_DIR)/include

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(TARGET_TEST) $(TARGET_DICT)

# Three files: built in one step, so no objects to keep apart from the
# vector build's
$(TARGET_TEST): $(SOURCES)
	$(CC) -o $@ $(SOURCES) -U__SSE2__ $(CFLAGS) $(LDFLAGS)

DICT_SOURCES := rzstd_dict_test.c $(filter-out rzstd_raw_block_test.c,$(SOURCES))
$(TARGET_DICT): $(DICT_SOURCES)
	$(CC) -o $@ $(DICT_SOURCES) $(CFLAGS) $(LDFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: $(TARGET) $(TARGET_TEST) $(TARGET_DICT)
	./$(TARGET)
	./$(TARGET_TEST)
	./$(TARGET_DICT)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(TARGET_TEST) $(TARGET_DICT) $(OBJS)

.PHONY: all check clean

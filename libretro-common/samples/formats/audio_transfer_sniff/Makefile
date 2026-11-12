include ../../sample_objdir.mk

TARGET := audio_transfer_sniff_test

LIBRETRO_COMM_DIR := ../../..

DEFINES := -DHAVE_RWAV -DHAVE_RFLAC -DHAVE_RVORBIS -DHAVE_ROPUS -DHAVE_RMP3 \
	-DHAVE_RAAC -DHAVE_RMP4 -DHAVE_RAC3 -DHAVE_RWMA -DHAVE_RMODTRACKER -DHAVE_RWEBM

SOURCES := \
	audio_transfer_sniff_test.c \
	$(LIBRETRO_COMM_DIR)/formats/audio_transfer.c \
	$(LIBRETRO_COMM_DIR)/formats/wav/rwav.c \
	$(LIBRETRO_COMM_DIR)/formats/flac/rflac.c \
	$(LIBRETRO_COMM_DIR)/formats/vorbis/rvorbis.c \
	$(LIBRETRO_COMM_DIR)/formats/opus/ropus.c \
	$(LIBRETRO_COMM_DIR)/formats/mp3/rmp3.c \
	$(LIBRETRO_COMM_DIR)/formats/aac/raac.c \
	$(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_frame.c \
	$(LIBRETRO_COMM_DIR)/formats/ac3/rac3_decode.c \
	$(LIBRETRO_COMM_DIR)/formats/wma/rwma_standard.c \
	$(LIBRETRO_COMM_DIR)/formats/wma/rwma_pro.c \
	$(LIBRETRO_COMM_DIR)/formats/wma/rwma_stream.c \
	$(LIBRETRO_COMM_DIR)/formats/mod/rmodtracker.c \
	$(LIBRETRO_COMM_DIR)/formats/webm/rwebm.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
	$(LIBRETRO_COMM_DIR)/memmap/memalign.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -pedantic -std=gnu99 -g1 -O2 -I$(LIBRETRO_COMM_DIR)/include $(DEFINES)
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

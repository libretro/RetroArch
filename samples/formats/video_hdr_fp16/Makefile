include ../../sample_objdir.mk

TARGET := video_hdr_fp16_test

REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

# The video decode stack the thumbnails use; the sections nothing here
# reaches are dropped at the link.
SOURCES := video_hdr_fp16_test.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
	$(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
	$(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
	$(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
	$(LIBRETRO_COMM_DIR)/features/features_cpu.c \
	$(LIBRETRO_COMM_DIR)/file/file_path.c \
	$(LIBRETRO_COMM_DIR)/file/file_path_io.c \
	$(LIBRETRO_COMM_DIR)/formats/data_transfer.c \
	$(LIBRETRO_COMM_DIR)/formats/image_texture.c \
	$(LIBRETRO_COMM_DIR)/formats/image_transfer.c \
	$(LIBRETRO_COMM_DIR)/formats/image/image_hdr_blit.c \
	$(LIBRETRO_COMM_DIR)/formats/image/image_blit_bands.c \
	$(LIBRETRO_COMM_DIR)/formats/h264/rh264.c \
	$(LIBRETRO_COMM_DIR)/formats/h265/rh265.c \
	$(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c \
	$(LIBRETRO_COMM_DIR)/formats/mp4/rmp4_video.c \
	$(LIBRETRO_COMM_DIR)/formats/vp8/rvp8.c \
	$(LIBRETRO_COMM_DIR)/formats/vp9/rvp9.c \
	$(LIBRETRO_COMM_DIR)/formats/webm/rwebm.c \
	$(LIBRETRO_COMM_DIR)/formats/webm/rwebm_video.c \
	$(LIBRETRO_COMM_DIR)/lists/string_list.c \
	$(LIBRETRO_COMM_DIR)/memmap/memalign.c \
	$(LIBRETRO_COMM_DIR)/memmap/memmap.c \
	$(LIBRETRO_COMM_DIR)/memory/mem_stats.c \
	$(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
	$(LIBRETRO_COMM_DIR)/rthreads/tpool.c \
	$(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
	$(LIBRETRO_COMM_DIR)/streams/file_stream.c \
	$(LIBRETRO_COMM_DIR)/string/rstrtod.c \
	$(LIBRETRO_COMM_DIR)/string/stdstring.c \
	$(LIBRETRO_COMM_DIR)/time/rtime.c \
	$(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

OBJS := $(call sample_objs,$(SOURCES:.c=.o))

CFLAGS  += -Wall -std=gnu99 -g -O1 -ffunction-sections \
	-DHAVE_RMP4 -DHAVE_RWEBM -DHAVE_RH264 -DHAVE_RH265 -DHAVE_RVP9 -DHAVE_THREADS \
	-I$(LIBRETRO_COMM_DIR)/include
LDFLAGS += -Wl,--gc-sections -lm -lpthread

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS)

check: all
	./$(TARGET)

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f $(TARGET) $(OBJS)

.PHONY: all check clean

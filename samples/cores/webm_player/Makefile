include ../../sample_objdir.mk

ROOT := ../../..
COMMON := $(ROOT)/libretro-common
TARGET := webm_player_test$(EXE)
AUDIO ?= 1
MP4 ?= 1
THREADS ?= 1

SOURCES := webm_player_test.c \
 $(ROOT)/cores/libretro-webm/webm_core.c \
 $(COMMON)/compat/compat_strl.c \
 $(COMMON)/compat/compat_strcasestr.c \
 $(COMMON)/compat/fopen_utf8.c \
 $(COMMON)/encodings/encoding_utf.c \
 $(COMMON)/encodings/encoding_crc32.c \
 $(COMMON)/features/features_cpu.c \
 $(COMMON)/file/file_path.c \
 $(COMMON)/file/file_path_io.c \
 $(COMMON)/formats/data_transfer.c \
 $(COMMON)/formats/audio_transfer.c \
 $(COMMON)/formats/image/image_yuv_blit.c \
 $(COMMON)/formats/image/image_hdr_blit.c \
 $(COMMON)/formats/image/image_blit_bands.c \
 $(COMMON)/formats/vp8/rvp8.c \
 $(COMMON)/formats/vp9/rvp9.c \
 $(COMMON)/formats/webm/rwebm.c \
 $(COMMON)/formats/webm/rwebm_video.c \
 $(COMMON)/lists/string_list.c \
 $(COMMON)/memmap/memmap.c \
 $(COMMON)/memmap/memalign.c \
 $(COMMON)/streams/file_stream.c \
 $(COMMON)/string/rstrtod.c \
 $(COMMON)/string/stdstring.c \
 $(COMMON)/time/rtime.c \
 $(COMMON)/vfs/vfs_implementation.c

CFLAGS += -Wall -Wextra -std=gnu89 -O1 -g1 -D_GNU_SOURCE \
 -DHAVE_RWEBM -DHAVE_RVP9 -DHAVE_RWEBP -I$(COMMON)/include
LDLIBS += -lm
ifneq ($(OS),Windows_NT)
CFLAGS += -DHAVE_MMAP
else
LDLIBS += -lwinmm
endif
ifeq ($(MP4),1)
CFLAGS += -DHAVE_RMP4 -DHAVE_RH264 -DHAVE_RH265
SOURCES += $(COMMON)/formats/mp4/rmp4.c \
 $(COMMON)/formats/mp4/rmp4_video.c \
 $(COMMON)/formats/h264/rh264.c $(COMMON)/formats/h265/rh265.c
endif
ifeq ($(AUDIO),1)
CFLAGS += -DHAVE_RAAC -DHAVE_ROPUS -DHAVE_RVORBIS
SOURCES += $(COMMON)/formats/aac/raac.c \
 $(COMMON)/formats/opus/ropus.c $(COMMON)/formats/vorbis/rvorbis.c
endif
ifeq ($(THREADS),1)
CFLAGS += -DHAVE_THREADS
SOURCES += $(COMMON)/rthreads/rthreads.c \
 $(COMMON)/rthreads/retro_eventcount.c $(COMMON)/rthreads/tpool.c
LDLIBS += -lpthread
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
CFLAGS += $(EXTRA_CFLAGS)
OBJS := $(SAMPLE_OBJDIR)/webm_player_test.o \
 $(call sample_objs,$(filter-out webm_player_test.o,$(SOURCES:.c=.o)))

all: $(TARGET)

%.o: %.c
	$(CC) $(CFLAGS) -c -o $@ $<

$(SAMPLE_OBJDIR)/%.o: $$(call sample_src,$$*).c
	@mkdir -p $(@D)
	$(CC) $(CFLAGS) -c -o $@ $<

$(TARGET): $(OBJS)
	$(CC) -o $@ $^ $(LDFLAGS) $(LDLIBS)

check: $(TARGET)
	$(RUNNER) ./$(TARGET) fixtures

clean:
	rm -rf $(SAMPLE_OBJDIR)
	rm -f webm_player_test.o $(TARGET)

.PHONY: all check clean

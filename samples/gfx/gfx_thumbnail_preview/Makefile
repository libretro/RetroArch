TARGET := gfx_thumbnail_preview_test
DECODE_TARGET          := preview_audio_decode_test
DECODE_TARGET_NORWEBM  := preview_audio_decode_test_norwebm

# Links the REAL gfx/gfx_thumbnail.c and the REAL
# libretro-common/audio/audio_mixer.c.  Stubbing either is what let
# three separate faults through: the harness that stubbed the mixer
# was modelling the two functions the defects lived in.  Only the
# frontend edges below the mixer (video driver, animation driver,
# task queue, config) are stubbed, in stubs_retroarch.c.
REPO_ROOT         := ../../..
LIBRETRO_COMM_DIR := $(REPO_ROOT)/libretro-common

UUT_SRC := $(REPO_ROOT)/gfx/gfx_thumbnail.c
UUT_OBJ := gfx_thumbnail_uut.o

SOURCES := gfx_thumbnail_preview_test.c \
           stubs_retroarch.c \
           $(REPO_ROOT)/libretro-common/audio/audio_mixer.c \
           $(LIBRETRO_COMM_DIR)/audio/resampler/audio_resampler.c \
           $(LIBRETRO_COMM_DIR)/audio/resampler/drivers/sinc_resampler.c \
           $(LIBRETRO_COMM_DIR)/audio/resampler/drivers/sinc_resampler_int16.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strcasestr.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/formats/audio_transfer.c \
           $(LIBRETRO_COMM_DIR)/formats/data_transfer.c \
           $(LIBRETRO_COMM_DIR)/formats/image_texture.c \
           $(LIBRETRO_COMM_DIR)/formats/image_transfer.c \
           $(LIBRETRO_COMM_DIR)/formats/image/image_hdr_blit.c \
           $(LIBRETRO_COMM_DIR)/formats/aac/raac.c \
           $(LIBRETRO_COMM_DIR)/formats/h264/rh264.c \
           $(LIBRETRO_COMM_DIR)/formats/h265/rh265.c \
           $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c \
           $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4_video.c \
           $(LIBRETRO_COMM_DIR)/formats/vp8/rvp8.c \
           $(LIBRETRO_COMM_DIR)/formats/webm/rwebm.c \
           $(LIBRETRO_COMM_DIR)/formats/webm/rwebm_video.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/memmap/memmap.c \
           $(LIBRETRO_COMM_DIR)/memmap/memalign.c \
           $(LIBRETRO_COMM_DIR)/memory/mem_stats.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -DHAVE_RMP4 -DHAVE_RWEBM -DHAVE_RAAC -DHAVE_RH264 \
           -DHAVE_AUDIOMIXER \
           -DHAVE_MMAP -D_GNU_SOURCE \
           -I. -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include

UUT_CFLAGS := $(CFLAGS) -O0 -fno-inline -DHAVE_MENU -DRARCH_INTERNAL \
              -I$(REPO_ROOT)/deps

LDFLAGS += -lpthread -lm

ifneq ($(SANITIZER),)
CFLAGS     += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
UUT_CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS    += -fsanitize=$(SANITIZER)
endif

CFLAGS     += $(EXTRA_CFLAGS)
UUT_CFLAGS += $(EXTRA_CFLAGS)

OBJS := $(SOURCES:.c=.o)

# The decode test drives the real mixer directly (no gfx UUT, no
# driver stubs), so it builds from its own leaner source list.  The
# -norwebm build is the same test compiled without HAVE_RWEBM: the
# lane that catches audio_transfer_set_avail compiling to a no-op on
# RWEBM-less builds (every bound silently dropped; rmp4's open then
# walked off the committed head).
DECODE_SOURCES := preview_audio_decode_test.c \
           $(REPO_ROOT)/libretro-common/audio/audio_mixer.c \
           $(LIBRETRO_COMM_DIR)/audio/resampler/audio_resampler.c \
           $(LIBRETRO_COMM_DIR)/audio/resampler/drivers/sinc_resampler.c \
           $(LIBRETRO_COMM_DIR)/audio/resampler/drivers/sinc_resampler_int16.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/formats/audio_transfer.c \
           $(LIBRETRO_COMM_DIR)/formats/data_transfer.c \
           $(LIBRETRO_COMM_DIR)/formats/aac/raac.c \
           $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/memmap/memmap.c \
           $(LIBRETRO_COMM_DIR)/memmap/memalign.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

DECODE_SOURCES_WEBM := $(DECODE_SOURCES) \
           $(LIBRETRO_COMM_DIR)/formats/webm/rwebm.c

DECODE_CFLAGS_COMMON := -Wall -std=gnu99 -g -O1 \
           -DHAVE_RMP4 -DHAVE_RAAC -DHAVE_AUDIOMIXER \
           -DHAVE_MMAP -D_GNU_SOURCE \
           -I$(LIBRETRO_COMM_DIR)/include $(EXTRA_CFLAGS)
ifneq ($(SANITIZER),)
DECODE_CFLAGS_COMMON += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
endif

all: $(TARGET) $(DECODE_TARGET) $(DECODE_TARGET_NORWEBM)

$(DECODE_TARGET): $(DECODE_SOURCES_WEBM) preview_audio_decode_test.c
	$(CC) -o $@ $(DECODE_SOURCES_WEBM) \
	      $(DECODE_CFLAGS_COMMON) -DHAVE_RWEBM $(LDFLAGS)

$(DECODE_TARGET_NORWEBM): $(DECODE_SOURCES) preview_audio_decode_test.c
	$(CC) -o $@ $(DECODE_SOURCES) $(DECODE_CFLAGS_COMMON) $(LDFLAGS)

$(UUT_OBJ): $(UUT_SRC)
	$(CC) -c -o $@ $< $(UUT_CFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS) $(UUT_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

# The fixtures are container layouts, not clips: see mkfixtures.py.
# Sparse, so the 7.28 GB ones cost ~40 MB of disk.
fixtures:
	python3 mkfixtures.py fixtures

check: $(TARGET) $(DECODE_TARGET) $(DECODE_TARGET_NORWEBM) fixtures
	./$(TARGET) fixtures/*.mp4
	./$(DECODE_TARGET) fixtures/trailing_large.mp4 1024   island
	./$(DECODE_TARGET) fixtures/trailing_large.mp4 262144 island
	./$(DECODE_TARGET_NORWEBM) fixtures/trailing_large.mp4 1024 island
	./$(DECODE_TARGET_NORWEBM) fixtures/trailing_large.mp4 262144 island

# Deliberately keeps fixtures/: the sanitizer passes each rebuild with
# a different SANITIZER=, and a clean that took the fixtures with it
# left the following run globbing nothing.  Use distclean to drop them.
clean:
	rm -f $(TARGET) $(DECODE_TARGET) $(DECODE_TARGET_NORWEBM) \
	      $(OBJS) $(UUT_OBJ)

distclean: clean
	rm -rf fixtures

.PHONY: all check fixtures clean distclean

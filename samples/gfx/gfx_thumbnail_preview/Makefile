# HAVE_MMAP means POSIX mmap: the VFS and memmap take the Win32
# mapping calls without it, the way the tree's own Windows build does.
ifeq ($(OS),Windows_NT)
MMAP_DEF :=
else
MMAP_DEF := -DHAVE_MMAP
endif

TARGET := gfx_thumbnail_preview_test
DECODE_TARGET          := preview_audio_decode_test
DECODE_TARGET_NORWEBM  := preview_audio_decode_test_norwebm
ANIM_WINDOW_TARGET     := preview_anim_window_test
CATCHUP_TARGET         := preview_catchup_test
BENCH_TARGET           := preview_decode_bench

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
# The preview session gfx_thumbnail drives its feeder and audio through.
ANIM_PREVIEW_SRC := $(REPO_ROOT)/gfx/gfx_anim_preview.c
ANIM_PREVIEW_OBJ := gfx_anim_preview_uut.o
# The streaming surface the animation frames go through, real.
SURFACE_SRC := $(REPO_ROOT)/gfx/gfx_surface.c
SURFACE_OBJ := gfx_surface_uut.o

# Layout facts (settings_t offsets/size) for the stubs, compiled with
# UUT_CFLAGS so they match what the UUT's configuration.h produces.
LAYOUT_OBJ := settings_layout_uut.o

SOURCES := gfx_thumbnail_preview_test.c \
           stubs_retroarch.c \
           $(REPO_ROOT)/libretro-common/audio/audio_mixer.c \
           $(LIBRETRO_COMM_DIR)/audio/resampler/audio_resampler.c \
 $(LIBRETRO_COMM_DIR)/audio/resampler/audio_resampler_int16.c \
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
           $(LIBRETRO_COMM_DIR)/formats/image/image_blit_bands.c \
           $(LIBRETRO_COMM_DIR)/formats/aac/raac.c \
           $(LIBRETRO_COMM_DIR)/formats/h264/rh264.c \
           $(LIBRETRO_COMM_DIR)/formats/h265/rh265.c \
           $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c \
           $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4_video.c \
           $(LIBRETRO_COMM_DIR)/formats/vp8/rvp8.c \
           $(LIBRETRO_COMM_DIR)/formats/webp/rwebp.c \
           $(LIBRETRO_COMM_DIR)/formats/webm/rwebm.c \
           $(LIBRETRO_COMM_DIR)/formats/webm/rwebm_video.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/memmap/memmap.c \
           $(LIBRETRO_COMM_DIR)/memmap/memalign.c \
           $(LIBRETRO_COMM_DIR)/memory/mem_stats.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

CFLAGS  += -Wall -std=gnu99 -g -O1 \
           -DHAVE_RMP4 -DHAVE_RWEBM -DHAVE_RAAC -DHAVE_RH264 -DHAVE_RWEBP \
           -DHAVE_AUDIOMIXER \
           $(MMAP_DEF) -D_GNU_SOURCE -DMEMMAP_TEST_NO_RESERVE \
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
           $(LIBRETRO_COMM_DIR)/audio/resampler/audio_resampler_int16.c \
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
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

DECODE_SOURCES_WEBM := $(DECODE_SOURCES) \
           $(LIBRETRO_COMM_DIR)/formats/webm/rwebm.c

DECODE_CFLAGS_COMMON := -Wall -std=gnu99 -g -O1 \
           -DHAVE_RMP4 -DHAVE_RAAC -DHAVE_AUDIOMIXER \
           $(MMAP_DEF) -D_GNU_SOURCE \
           -I$(LIBRETRO_COMM_DIR)/include $(EXTRA_CFLAGS)
ifneq ($(SANITIZER),)
DECODE_CFLAGS_COMMON += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
endif

# The frame-indexed animation types (animated WEBP, APNG) through the
# shared session, windowed: progressive open, byte cursor, a lowered
# bound, a frame larger than the lookahead.  Its own source list: it
# needs rpng (zlib) and rwebp, which the video harness above does not
# link, and none of the audio path.
ANIM_WINDOW_SOURCES := preview_anim_window_test.c \
           $(REPO_ROOT)/gfx/gfx_anim_preview.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strldup.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_posix_string.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/formats/data_transfer.c \
           $(LIBRETRO_COMM_DIR)/formats/image_texture.c \
           $(LIBRETRO_COMM_DIR)/formats/image_transfer.c \
           $(LIBRETRO_COMM_DIR)/formats/png/rpng.c \
           $(LIBRETRO_COMM_DIR)/formats/png/rpng_apng.c \
           $(LIBRETRO_COMM_DIR)/formats/vp8/rvp8.c \
           $(LIBRETRO_COMM_DIR)/formats/webp/rwebp.c \
           $(LIBRETRO_COMM_DIR)/formats/h264/rh264.c \
           $(LIBRETRO_COMM_DIR)/formats/h265/rh265.c \
           $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c \
           $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4_video.c \
           $(LIBRETRO_COMM_DIR)/formats/webm/rwebm.c \
           $(LIBRETRO_COMM_DIR)/formats/webm/rwebm_video.c \
           $(LIBRETRO_COMM_DIR)/formats/image/image_hdr_blit.c \
           $(LIBRETRO_COMM_DIR)/formats/image/image_blit_bands.c \
           $(LIBRETRO_COMM_DIR)/memmap/memmap.c \
           $(LIBRETRO_COMM_DIR)/memory/mem_stats.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/interface_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/memory_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/rzip_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
           $(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c \
           $(LIBRETRO_COMM_DIR)/streams/trans_stream_zlib.c \
           $(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

ANIM_WINDOW_CFLAGS := -Wall -std=gnu99 -g -O1 \
           -DHAVE_RPNG -DHAVE_RWEBP -DHAVE_RMP4 -DHAVE_RH264 -DHAVE_RWEBM -DRARCH_INTERNAL \
           $(MMAP_DEF) -D_GNU_SOURCE \
           -I$(REPO_ROOT) -I$(LIBRETRO_COMM_DIR)/include $(EXTRA_CFLAGS)
ifneq ($(SANITIZER),)
ANIM_WINDOW_CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
endif

BLIT_BANDS_TARGET  := preview_blit_bands_test

# The windowed-open oracle measures residency through madvise and
# /proc/self/statm: it is a Linux oracle and is built on Linux. The
# rest of the sample - the decoders, the bench, the catch-up and
# blit-band oracles - builds anywhere with a C compiler.
ifeq ($(OS),Windows_NT)
PLATFORM_TARGETS :=
else
PLATFORM_TARGETS := $(ANIM_WINDOW_TARGET)
endif


# Defined ahead of all: a prerequisite list expands when make reads it,
# and a variable set below it is empty there.
THREADED_TARGET := gfx_thumbnail_preview_test_threaded
AUDIO_ORACLE_TARGET := preview_audio_oracle_test
CATCHUP_THREADED_TARGET := $(CATCHUP_TARGET)_threaded
THREADED_DEFS   := -DHAVE_THREADS -DPREVIEW_THREADED
THREADED_EXTRA  := $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
                   $(LIBRETRO_COMM_DIR)/rthreads/tpool.c \
                   $(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c
THREADED_OBJS   := $(SOURCES:.c=.t.o) $(THREADED_EXTRA:.c=.t.o)

all: $(TARGET) $(DECODE_TARGET) $(DECODE_TARGET_NORWEBM) $(PLATFORM_TARGETS) $(BLIT_BANDS_TARGET) $(CATCHUP_TARGET) $(CATCHUP_THREADED_TARGET) $(AUDIO_ORACLE_TARGET) $(THREADED_TARGET)

$(ANIM_WINDOW_TARGET): $(ANIM_WINDOW_SOURCES)
	$(CC) -o $@ $(ANIM_WINDOW_SOURCES) $(ANIM_WINDOW_CFLAGS) $(LDFLAGS) -lz

# Same tree the windowed oracle links, with its own main.
CATCHUP_SOURCES := preview_catchup_test.c $(filter-out preview_anim_window_test.c,$(ANIM_WINDOW_SOURCES))
$(CATCHUP_TARGET): $(CATCHUP_SOURCES)
	$(CC) -o $@ $(CATCHUP_SOURCES) $(ANIM_WINDOW_CFLAGS) $(LDFLAGS) -lz

# The same oracle with four pictures decoding concurrently on a pool:
# catching up drops pictures at submit, and what remains must still
# be the full decode to the byte. (CATCHUP_THREADED_TARGET is set with
# the other names ahead of all:.)
$(CATCHUP_THREADED_TARGET): $(CATCHUP_SOURCES) $(THREADED_EXTRA)
	$(CC) -o $@ $(CATCHUP_SOURCES) $(THREADED_EXTRA) $(ANIM_WINDOW_CFLAGS) $(THREADED_DEFS) $(LDFLAGS) -lz -lpthread


$(DECODE_TARGET): $(DECODE_SOURCES_WEBM) preview_audio_decode_test.c
	$(CC) -o $@ $(DECODE_SOURCES_WEBM) \
	      $(DECODE_CFLAGS_COMMON) -DHAVE_RWEBM $(LDFLAGS)

$(DECODE_TARGET_NORWEBM): $(DECODE_SOURCES) preview_audio_decode_test.c
	$(CC) -o $@ $(DECODE_SOURCES) $(DECODE_CFLAGS_COMMON) $(LDFLAGS)

# The preview's audio chain against ffmpeg's decode of the same track,
# at the track's rate and through the mixer's resampler.
AUDIO_ORACLE_SOURCES := preview_audio_oracle_test.c $(filter-out preview_audio_decode_test.c,$(DECODE_SOURCES_WEBM)) \
                        $(LIBRETRO_COMM_DIR)/file/file_path_io.c
$(AUDIO_ORACLE_TARGET): $(AUDIO_ORACLE_SOURCES)
	$(CC) -o $@ $(AUDIO_ORACLE_SOURCES) $(DECODE_CFLAGS_COMMON) -DHAVE_RWEBM $(LDFLAGS) -lm

# The banded colour conversion against the single-threaded one, frame
# for frame, through the real rmp4_video stream on a real tpool.
BLIT_BANDS_SOURCES := preview_blit_bands_test.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c \
           $(LIBRETRO_COMM_DIR)/compat/fopen_utf8.c \
           $(LIBRETRO_COMM_DIR)/encodings/encoding_utf.c \
           $(LIBRETRO_COMM_DIR)/features/features_cpu.c \
           $(LIBRETRO_COMM_DIR)/file/file_path.c \
           $(LIBRETRO_COMM_DIR)/file/file_path_io.c \
           $(LIBRETRO_COMM_DIR)/formats/h264/rh264.c \
           $(LIBRETRO_COMM_DIR)/formats/h265/rh265.c \
           $(LIBRETRO_COMM_DIR)/formats/image/image_blit_bands.c \
           $(LIBRETRO_COMM_DIR)/rthreads/retro_eventcount.c \
           $(LIBRETRO_COMM_DIR)/formats/image/image_hdr_blit.c \
           $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4.c \
           $(LIBRETRO_COMM_DIR)/formats/mp4/rmp4_video.c \
           $(LIBRETRO_COMM_DIR)/formats/vp8/rvp8.c \
           $(LIBRETRO_COMM_DIR)/formats/vp9/rvp9.c \
           $(LIBRETRO_COMM_DIR)/formats/webm/rwebm.c \
           $(LIBRETRO_COMM_DIR)/formats/webm/rwebm_video.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/memmap/memmap.c \
           $(LIBRETRO_COMM_DIR)/memmap/memalign.c \
           $(LIBRETRO_COMM_DIR)/rthreads/rthreads.c \
           $(LIBRETRO_COMM_DIR)/rthreads/tpool.c \
           $(LIBRETRO_COMM_DIR)/streams/file_stream.c \
           $(LIBRETRO_COMM_DIR)/string/rstrtod.c \
           $(LIBRETRO_COMM_DIR)/string/stdstring.c \
           $(LIBRETRO_COMM_DIR)/time/rtime.c \
           $(LIBRETRO_COMM_DIR)/vfs/vfs_implementation.c

# Not a test: frames per second per codec and thread count. Built by
# `make bench`, never by check, and meaningless under a sanitizer.
BENCH_SOURCES := preview_decode_bench.c $(filter-out preview_blit_bands_test.c,$(BLIT_BANDS_SOURCES)) \
                 $(LIBRETRO_COMM_DIR)/formats/image_transfer.c \
                 $(LIBRETRO_COMM_DIR)/formats/png/rpng.c \
                 $(LIBRETRO_COMM_DIR)/formats/png/rpng_apng.c \
                 $(LIBRETRO_COMM_DIR)/encodings/encoding_deflate.c \
                 $(LIBRETRO_COMM_DIR)/encodings/encoding_crc32.c \
                 $(LIBRETRO_COMM_DIR)/streams/trans_stream.c \
                 $(LIBRETRO_COMM_DIR)/streams/trans_stream_deflate.c \
                 $(LIBRETRO_COMM_DIR)/streams/trans_stream_pipe.c
$(BENCH_TARGET): $(BENCH_SOURCES)
	$(CC) -o $@ $(BENCH_SOURCES) \
	      -Wall -std=gnu99 -O2 -DHAVE_RMP4 -DHAVE_RWEBM -DHAVE_RH264 -DHAVE_RH265 -DHAVE_RVP9 -DHAVE_RPNG -DHAVE_THREADS \
	      $(MMAP_DEF) -D_GNU_SOURCE -I$(LIBRETRO_COMM_DIR)/include \
	      $(EXTRA_CFLAGS) $(LDFLAGS) -lpthread

bench: $(BENCH_TARGET) fixtures
	./$(BENCH_TARGET) fixtures/long_video.mp4 fixtures/hevc_wpp.mp4 fixtures/vp9_tiles.webm fixtures/bframes_h264.mp4
	./$(BENCH_TARGET) --catchup fixtures/bframes_h264.mp4 fixtures/bframes_hevc.mp4

$(BLIT_BANDS_TARGET): $(BLIT_BANDS_SOURCES)
	$(CC) -o $@ $(BLIT_BANDS_SOURCES) \
	      -Wall -std=gnu99 -g -O1 -DHAVE_RMP4 -DHAVE_RWEBM -DHAVE_RH264 -DHAVE_RH265 -DHAVE_RVP9 -DHAVE_THREADS \
	      $(MMAP_DEF) -D_GNU_SOURCE -I$(LIBRETRO_COMM_DIR)/include \
	      $(EXTRA_CFLAGS) $(LDFLAGS) -lpthread

$(UUT_OBJ): $(UUT_SRC)
	$(CC) -c -o $@ $< $(UUT_CFLAGS)

$(ANIM_PREVIEW_OBJ): $(ANIM_PREVIEW_SRC)
	$(CC) -c -o $@ $< $(UUT_CFLAGS)

$(SURFACE_OBJ): $(SURFACE_SRC)
	$(CC) -c -o $@ $< $(UUT_CFLAGS)

$(LAYOUT_OBJ): settings_layout_uut.c
	$(CC) -c -o $@ $< $(UUT_CFLAGS)

%.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS)

$(TARGET): $(OBJS) $(UUT_OBJ) $(LAYOUT_OBJ) $(ANIM_PREVIEW_OBJ) $(SURFACE_OBJ)
	$(CC) -o $@ $^ $(LDFLAGS)

# The same test with HAVE_THREADS: the thumbnail decodes on its worker
# thread and, for THREADS above one, on the pool - the app's own path,
# which the single-threaded build above cannot show. rthreads, the
# pool and the eventcount are the real ones; the stubs' thread shims
# are compiled out under PREVIEW_THREADED. Objects carry .t.o so the
# two builds never share one.

%.t.o: %.c
	$(CC) -c -o $@ $< $(CFLAGS) $(THREADED_DEFS)

gfx_thumbnail_uut.t.o: $(UUT_SRC)
	$(CC) -c -o $@ $< $(UUT_CFLAGS) $(THREADED_DEFS)
gfx_anim_preview_uut.t.o: $(ANIM_PREVIEW_SRC)
	$(CC) -c -o $@ $< $(UUT_CFLAGS) $(THREADED_DEFS)
gfx_surface_uut.t.o: $(SURFACE_SRC)
	$(CC) -c -o $@ $< $(UUT_CFLAGS) $(THREADED_DEFS)
settings_layout_uut.t.o: settings_layout_uut.c
	$(CC) -c -o $@ $< $(UUT_CFLAGS) $(THREADED_DEFS)

$(THREADED_TARGET): $(THREADED_OBJS) gfx_thumbnail_uut.t.o settings_layout_uut.t.o \
                    gfx_anim_preview_uut.t.o gfx_surface_uut.t.o
	$(CC) -o $@ $^ $(LDFLAGS)

# The fixtures are container layouts, not clips: see mkfixtures.py.
# Sparse, so the 7.28 GB ones cost ~40 MB of disk.
fixtures:
	python3 mkfixtures.py fixtures

check: $(TARGET) $(THREADED_TARGET) $(DECODE_TARGET) $(DECODE_TARGET_NORWEBM) $(PLATFORM_TARGETS) $(BLIT_BANDS_TARGET) $(CATCHUP_TARGET) $(CATCHUP_THREADED_TARGET) $(AUDIO_ORACLE_TARGET) fixtures
	./$(TARGET) fixtures/*.mp4 fixtures/anim_lossless.webp
ifneq ($(OS),Windows_NT)
	./$(ANIM_WINDOW_TARGET) fixtures
endif
	./$(CATCHUP_TARGET) fixtures/bframes_h264.mp4 fixtures/bframes_hevc.mp4
	./$(CATCHUP_THREADED_TARGET) fixtures/bframes_h264.mp4 fixtures/bframes_hevc.mp4
	./$(AUDIO_ORACLE_TARGET) fixtures/trailing_small.mp4 fixtures/trailing_large.mp4
	FRAMES=180 PACE_HZ=60 PACE_MAX_STALL_MS=500 NOAUDIO=1 ./$(TARGET) fixtures/bframes_h264.mp4 fixtures/bframes_hevc.mp4
	THREADS=4 FRAMES=180 PACE_HZ=60 PACE_MAX_STALL_MS=500 NOAUDIO=1 ./$(THREADED_TARGET) fixtures/bframes_h264.mp4 fixtures/bframes_hevc.mp4
	./$(BLIT_BANDS_TARGET) fixtures/long_video.mp4
	./$(BLIT_BANDS_TARGET) fixtures/vp9_tiles.webm
	./$(BLIT_BANDS_TARGET) fixtures/hevc_wpp.mp4 fixtures/bframes_hevc.mp4 fixtures/hevc_nowpp.mp4 fixtures/mbaff_h264.mp4
	RH265_PUBLISH_DELAY=200 ./$(BLIT_BANDS_TARGET) fixtures/hevc_wpp.mp4 fixtures/bframes_hevc.mp4 fixtures/hevc_nowpp.mp4
	./$(DECODE_TARGET) fixtures/trailing_large.mp4 1024   island
	./$(DECODE_TARGET) fixtures/trailing_large.mp4 262144 island
	./$(DECODE_TARGET_NORWEBM) fixtures/trailing_large.mp4 1024 island
	./$(DECODE_TARGET_NORWEBM) fixtures/trailing_large.mp4 262144 island

# Deliberately keeps fixtures/: the sanitizer passes each rebuild with
# a different SANITIZER=, and a clean that took the fixtures with it
# left the following run globbing nothing.  Use distclean to drop them.
clean:
	rm -f $(TARGET) $(THREADED_TARGET) $(THREADED_TARGET).exe $(THREADED_OBJS) \
	      gfx_thumbnail_uut.t.o settings_layout_uut.t.o gfx_anim_preview_uut.t.o gfx_surface_uut.t.o \
	      $(DECODE_TARGET) $(DECODE_TARGET_NORWEBM) \
	      $(ANIM_WINDOW_TARGET) $(BLIT_BANDS_TARGET) $(CATCHUP_TARGET) $(CATCHUP_THREADED_TARGET) $(AUDIO_ORACLE_TARGET) $(BENCH_TARGET) \
	      $(TARGET).exe $(CATCHUP_TARGET).exe $(BENCH_TARGET).exe $(BLIT_BANDS_TARGET).exe \
	      $(OBJS) $(UUT_OBJ) $(LAYOUT_OBJ) $(ANIM_PREVIEW_OBJ) $(SURFACE_OBJ)

distclean: clean
	rm -rf fixtures

.PHONY: bench all check fixtures clean distclean

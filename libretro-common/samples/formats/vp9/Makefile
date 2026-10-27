CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c89
CFLAGS += -I../../../include
GC_SECTIONS := -Wl,--gc-sections
ifeq ($(shell uname -s),Darwin)
CFLAGS += -D_DARWIN_C_SOURCE
GC_SECTIONS := -Wl,-dead_strip
endif
TARGETS := rmp4_vp9_hw_stream_test rwebm_language_test rmp4_vp9_config_test rvp9_config_test rvp9_reference_test rvp9_hw_session_test rwebm_hw_stream_test
THREADS ?= 1
PIXEL_FLAGS :=
PIXEL_SOURCES :=
ifeq ($(THREADS),1)
PIXEL_FLAGS += -DHAVE_THREADS -D_POSIX_C_SOURCE=200809L -pthread
PIXEL_SOURCES += ../../../rthreads/rthreads.c ../../../rthreads/retro_eventcount.c ../../../rthreads/tpool.c
endif

WEBM_SOURCES := ../../../formats/vp9/rvp9.c ../../../formats/webm/rwebm.c \
 ../../../formats/h264/rh264.c ../../../formats/h265/rh265.c \
 ../../../formats/video_player.c \
 ../../../formats/image_transfer.c ../../../formats/vp8/rvp8.c \
 ../../../formats/image/image_blit_bands.c ../../../formats/image/image_hdr_blit.c \
 ../../../compat/compat_strl.c ../../../features/features_cpu.c ../../../time/rtime.c

ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGETS)

rwebm_h26x_stream_test: rwebm_h26x_stream_test.c $(WEBM_SOURCES) ../../../formats/webm/rwebm_video.c
	$(CC) $(CFLAGS) $(PIXEL_FLAGS) -D_POSIX_C_SOURCE=200809L -DHAVE_RWEBM -DHAVE_RVP9 \
	 rwebm_h26x_stream_test.c $(WEBM_SOURCES) $(PIXEL_SOURCES) \
	 ../../../formats/webm/rwebm_video.c -o $@ $(LDFLAGS) -lm

rwebm_h26x_hw_test: rwebm_h26x_hw_test.c $(WEBM_SOURCES) ../../../formats/webm/rwebm_video.c
	$(CC) $(CFLAGS) $(PIXEL_FLAGS) -D_POSIX_C_SOURCE=200809L -DHAVE_RWEBM -DHAVE_RVP9 \
	 rwebm_h26x_hw_test.c $(WEBM_SOURCES) $(PIXEL_SOURCES) \
	 ../../../formats/webm/rwebm_video.c -o $@ $(LDFLAGS) -lm

MP4_SOURCES := ../../../formats/mp4/rmp4.c ../../../formats/vp9/rvp9.c \
 ../../../formats/vp8/rvp8.c ../../../formats/h264/rh264.c ../../../formats/h265/rh265.c \
 ../../../formats/image/image_blit_bands.c ../../../formats/image/image_hdr_blit.c \
 ../../../compat/compat_strl.c ../../../features/features_cpu.c ../../../time/rtime.c

rmp4_h26x_stream_test: rmp4_h26x_stream_test.c $(MP4_SOURCES) ../../../formats/mp4/rmp4_video.c
	$(CC) $(CFLAGS) $(PIXEL_FLAGS) -D_POSIX_C_SOURCE=200809L -DHAVE_RMP4 -DHAVE_RVP9 \
	 rmp4_h26x_stream_test.c $(MP4_SOURCES) $(PIXEL_SOURCES) \
	 ../../../formats/mp4/rmp4_video.c -o $@ $(LDFLAGS) -lm

rmp4_h26x_hw_test: rmp4_h26x_hw_test.c $(MP4_SOURCES) ../../../formats/mp4/rmp4_video.c
	$(CC) $(CFLAGS) $(PIXEL_FLAGS) -D_POSIX_C_SOURCE=200809L -DHAVE_RMP4 -DHAVE_RVP9 \
	 rmp4_h26x_hw_test.c $(MP4_SOURCES) $(PIXEL_SOURCES) \
	 ../../../formats/mp4/rmp4_video.c -o $@ $(LDFLAGS) -lm

rmp4_vp9_hw_stream_test: rmp4_vp9_hw_stream_test.c rvp9_mp4_fixture.h $(MP4_SOURCES) \
 ../../../formats/mp4/rmp4_video.c ../../../include/formats/rvp9_hw.h
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -DHAVE_RMP4 -DHAVE_RVP9 \
	 rmp4_vp9_hw_stream_test.c $(MP4_SOURCES) -o $@ $(LDFLAGS) -lm

rmp4_vp9_config_test: rmp4_vp9_config_test.c rvp9_mp4_fixture.h $(MP4_SOURCES) \
 ../../../formats/mp4/rmp4_video.c ../../../include/formats/rvp9_config.h ../../../include/formats/rvp9_color.h
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -DHAVE_RMP4 -DHAVE_RVP9 \
	 rmp4_vp9_config_test.c $(MP4_SOURCES) -o $@ $(LDFLAGS) -lm

rvp9_config_test: rvp9_config_test.c ../../../include/formats/rvp9_config.h ../../../include/formats/rvp9_color.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

rvp9_reference_test: rvp9_reference_test.c ../../../formats/vp9/rvp9.c ../../../formats/vp9/rvp9_recon.inc ../../../include/formats/rvp9.h
	$(CC) $(CFLAGS) rvp9_reference_test.c -o $@ $(LDFLAGS)

rvp9_hw_session_test: rvp9_hw_session_test.c ../../../formats/vp9/rvp9.c ../../../formats/vp9/rvp9_recon.inc ../../../include/formats/rvp9.h ../../../include/formats/rvp9_hw.h
	$(CC) $(CFLAGS) rvp9_hw_session_test.c ../../../formats/vp9/rvp9.c -o $@ $(LDFLAGS)

rwebm_hw_stream_test: rwebm_hw_stream_test.c $(WEBM_SOURCES) ../../../formats/webm/rwebm_video.c \
 ../../../include/formats/rvp9_hw.h ../../../include/formats/rwebm_video.h ../../../include/formats/rvp9_color.h
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -DHAVE_RWEBM -DHAVE_RVP9 -ffunction-sections -fdata-sections \
	 rwebm_hw_stream_test.c $(WEBM_SOURCES) -o $@ $(LDFLAGS) $(GC_SECTIONS) -lm

rwebm_language_test: rwebm_language_test.c ../../../formats/webm/rwebm.c ../../../include/formats/rwebm.h
	$(CC) $(CFLAGS) rwebm_language_test.c ../../../formats/webm/rwebm.c -o $@ $(LDFLAGS) -lm

check: $(TARGETS)
	./rwebm_language_test
	./rmp4_vp9_hw_stream_test
	./rmp4_vp9_config_test
	./rvp9_config_test
	./rvp9_reference_test
	./rvp9_hw_session_test
	./rwebm_hw_stream_test

stream-check: rvp9_hw_session_test
	python3 check_streams.py ./rvp9_hw_session_test

h26x-check: rwebm_h26x_stream_test rwebm_h26x_hw_test
	python3 check_h26x.py
	python3 check_h26x.py --laced

mp4-h26x-check: rmp4_h26x_stream_test rmp4_h26x_hw_test check_mp4_h26x.py h26x_fixture.py
	python3 check_mp4_h26x.py

h26x-color-check: rwebm_h26x_stream_test rwebm_h26x_hw_test
	python3 check_h26x.py --codec h264 --pixel-format yuv422p
	python3 check_h26x.py --codec h264 --pixel-format yuv422p10le
	python3 check_h26x.py --codec h264 --pixel-format yuv444p
	python3 check_h26x.py --codec h264 --pixel-format yuv444p10le
	python3 check_h26x.py --pixel-format yuv420p10le
	python3 check_h26x.py --full-range
	python3 check_h26x.py --pixel-format yuv420p10le --full-range

stack-check:
	CC="$(CC)" python3 check_stack.py

pixel-check: rvp9_pixel_test
	python3 check_pixels.py ./rvp9_pixel_test

rvp9_pixel_test: rvp9_pixel_test.c ../../../formats/vp9/rvp9.c ../../../formats/vp9/rvp9_recon.inc ../../../include/formats/rvp9.h $(PIXEL_SOURCES)
	$(CC) $(CFLAGS) $(PIXEL_FLAGS) rvp9_pixel_test.c ../../../formats/vp9/rvp9.c $(PIXEL_SOURCES) -o $@ $(LDFLAGS)

clean:
	rm -f rmp4_h26x_stream_test rmp4_h26x_stream_test.exe rmp4_h26x_hw_test rmp4_h26x_hw_test.exe
	rm -f rvp9_pixel_test rvp9_pixel_test.exe
	rm -f rwebm_h26x_stream_test rwebm_h26x_stream_test.exe
	rm -f rwebm_h26x_hw_test rwebm_h26x_hw_test.exe
	rm -f $(TARGETS) $(addsuffix .exe,$(TARGETS))

.PHONY: all check stream-check h26x-check mp4-h26x-check h26x-color-check pixel-check stack-check clean

CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c89
CFLAGS += -I../../../include
TARGETS := rvp9_hw_segment_test rmp4_vp9_hw_stream_test rmp4_vp9_config_test rvp9_config_test rvp9_reference_test rvp9_hw_session_test rwebm_hw_stream_test
THREADS ?= 1
PIXEL_FLAGS :=
PIXEL_SOURCES :=
ifeq ($(THREADS),1)
PIXEL_FLAGS += -DHAVE_THREADS -D_POSIX_C_SOURCE=200809L -pthread
PIXEL_SOURCES += ../../../rthreads/rthreads.c ../../../rthreads/retro_eventcount.c ../../../rthreads/tpool.c
endif
WEBM_SOURCES := ../../../formats/vp9/rvp9.c ../../../formats/webm/rwebm.c \
 ../../../formats/video_player.c \
 ../../../formats/image_transfer.c ../../../formats/vp8/rvp8.c \
 ../../../formats/image/image_blit_bands.c ../../../formats/image/image_hdr_blit.c \
 ../../../features/features_cpu.c ../../../time/rtime.c

ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGETS)

MP4_SOURCES := ../../../formats/mp4/rmp4.c ../../../formats/vp9/rvp9.c \
 ../../../formats/vp8/rvp8.c ../../../formats/h264/rh264.c ../../../formats/h265/rh265.c \
 ../../../formats/image/image_blit_bands.c ../../../formats/image/image_hdr_blit.c \
 ../../../features/features_cpu.c ../../../time/rtime.c
rmp4_vp9_config_test rmp4_vp9_hw_stream_test: %: %.c rvp9_mp4_fixture.h $(MP4_SOURCES) \
 ../../../formats/mp4/rmp4_video.c ../../../include/formats/rvp9_config.h ../../../include/formats/rvp9_color.h \
 ../../../include/formats/rvp9_hw.h ../../../include/formats/rmp4_video.h
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -DHAVE_RMP4 -DHAVE_RVP9 \
	 $< $(MP4_SOURCES) -o $@ $(LDFLAGS) -lm

rvp9_hw_segment_test: rvp9_hw_segment_test.c ../../../formats/vp9/rvp9.c ../../../formats/vp9/rvp9_recon.inc ../../../include/formats/rvp9_hw.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

rvp9_config_test: rvp9_config_test.c ../../../include/formats/rvp9_config.h ../../../include/formats/rvp9_color.h
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

rvp9_reference_test: rvp9_reference_test.c ../../../formats/vp9/rvp9.c ../../../formats/vp9/rvp9_recon.inc ../../../include/formats/rvp9.h
	$(CC) $(CFLAGS) rvp9_reference_test.c -o $@ $(LDFLAGS)

rvp9_hw_session_test: rvp9_hw_session_test.c ../../../formats/vp9/rvp9.c ../../../formats/vp9/rvp9_recon.inc ../../../include/formats/rvp9.h ../../../include/formats/rvp9_hw.h
	$(CC) $(CFLAGS) rvp9_hw_session_test.c ../../../formats/vp9/rvp9.c -o $@ $(LDFLAGS)

rwebm_hw_stream_test: rwebm_hw_stream_test.c $(WEBM_SOURCES) ../../../formats/webm/rwebm_video.c \
 ../../../include/formats/rvp9_hw.h ../../../include/formats/rwebm_video.h ../../../include/formats/rvp9_color.h
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -DHAVE_RWEBM -DHAVE_RVP9 -ffunction-sections -fdata-sections \
	 rwebm_hw_stream_test.c $(WEBM_SOURCES) -o $@ $(LDFLAGS) -Wl,--gc-sections -lm

check: $(TARGETS)
	./rvp9_hw_segment_test
	./rmp4_vp9_hw_stream_test
	./rmp4_vp9_config_test
	./rvp9_config_test
	./rvp9_reference_test
	./rvp9_hw_session_test
	./rwebm_hw_stream_test

stream-check: rvp9_hw_session_test
	python3 check_streams.py ./rvp9_hw_session_test

stack-check:
	CC="$(CC)" python3 check_stack.py

pixel-check: rvp9_pixel_test
	python3 check_pixels.py ./rvp9_pixel_test

rvp9_pixel_test: rvp9_pixel_test.c ../../../formats/vp9/rvp9.c ../../../formats/vp9/rvp9_recon.inc ../../../include/formats/rvp9.h $(PIXEL_SOURCES)
	$(CC) $(CFLAGS) $(PIXEL_FLAGS) rvp9_pixel_test.c ../../../formats/vp9/rvp9.c $(PIXEL_SOURCES) -o $@ $(LDFLAGS)

clean:
	rm -f rvp9_pixel_test rvp9_pixel_test.exe
	rm -f $(TARGETS) $(addsuffix .exe,$(TARGETS))

.PHONY: all check stream-check pixel-check stack-check clean

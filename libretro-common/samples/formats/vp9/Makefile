CC ?= cc
CFLAGS ?= -O2 -Wall -Wextra -std=c89
CFLAGS += -I../../../include
TARGETS := rvp9_reference_test rvp9_hw_session_test rwebm_hw_stream_test
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

rvp9_reference_test: rvp9_reference_test.c ../../../formats/vp9/rvp9.c ../../../formats/vp9/rvp9_recon.inc ../../../include/formats/rvp9.h
	$(CC) $(CFLAGS) rvp9_reference_test.c -o $@ $(LDFLAGS)

rvp9_hw_session_test: rvp9_hw_session_test.c ../../../formats/vp9/rvp9.c ../../../formats/vp9/rvp9_recon.inc ../../../include/formats/rvp9.h ../../../include/formats/rvp9_hw.h
	$(CC) $(CFLAGS) rvp9_hw_session_test.c ../../../formats/vp9/rvp9.c -o $@ $(LDFLAGS)

rwebm_hw_stream_test: rwebm_hw_stream_test.c $(WEBM_SOURCES) ../../../formats/webm/rwebm_video.c \
 ../../../include/formats/rvp9_hw.h ../../../include/formats/rwebm_video.h
	$(CC) $(CFLAGS) -D_POSIX_C_SOURCE=200809L -DHAVE_RWEBM -DHAVE_RVP9 -ffunction-sections -fdata-sections \
	 rwebm_hw_stream_test.c $(WEBM_SOURCES) -o $@ $(LDFLAGS) -Wl,--gc-sections -lm

check: $(TARGETS)
	./rvp9_reference_test
	./rvp9_hw_session_test
	./rwebm_hw_stream_test

stream-check: rvp9_hw_session_test
	python3 check_streams.py ./rvp9_hw_session_test

clean:
	rm -f $(TARGETS) $(addsuffix .exe,$(TARGETS))

.PHONY: all check stream-check clean

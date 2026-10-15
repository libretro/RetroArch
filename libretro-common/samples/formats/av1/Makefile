CC ?= cc
COMMON := ../../..
TARGET := rav1_test$(EXE)
TARGET_TEST := rav1_hw_stream_test$(EXE)
TARGET_TEST2 := rav1_mp4_stream_test$(EXE)
TARGET_TEST3 := rav1_webm_stream_test$(EXE)
HW := rav1_hw_session_test$(EXE)
BACKEND := rav1_hw_backend_test$(EXE)
STATE := rav1_reference_state_test$(EXE)
GRAIN := rav1_grain_test$(EXE)
MOTION := rav1_motion_test$(EXE)
REFERENCE := rav1_reference_test$(EXE)
PICTURE := rav1_picture_test$(EXE)
FRAME := rav1_frame_test$(EXE)
TILE_INFO := rav1_tile_info_test$(EXE)
TILES := rav1_tiles_test$(EXE)
SEQUENCE := rav1_sequence_test$(EXE)
CONTAINER := rav1_container_test$(EXE)
CFLAGS += -O1 -g1 -std=gnu89 -Wall -Wextra -Werror=declaration-after-statement -I$(COMMON)/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET) $(CONTAINER) $(SEQUENCE) $(TILES) $(TILE_INFO) $(FRAME) $(PICTURE) $(REFERENCE) $(MOTION) $(GRAIN) $(STATE) $(HW) $(BACKEND) $(TARGET_TEST) $(TARGET_TEST2) $(TARGET_TEST3)

$(TARGET): rav1_test.c $(COMMON)/include/formats/rav1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

MP4_SOURCES := $(COMMON)/formats/mp4/rmp4.c $(COMMON)/formats/mp4/rmp4_video.c \
      $(COMMON)/formats/h264/rh264.c $(COMMON)/formats/h265/rh265.c \
      $(COMMON)/formats/vp8/rvp8.c $(COMMON)/formats/image/image_blit_bands.c \
      $(COMMON)/formats/image/image_hdr_blit.c $(COMMON)/features/features_cpu.c \
      $(COMMON)/time/rtime.c $(COMMON)/formats/av1/rav1_hw.c

$(TARGET_TEST2): rav1_mp4_stream_test.c rav1_mp4_fixture.h $(MP4_SOURCES) $(wildcard $(COMMON)/include/formats/rav1*.h) $(COMMON)/include/formats/rmp4_video.h
	$(CC) $(CFLAGS) -DHAVE_RAV1 -DHAVE_RMP4 -ffunction-sections -fdata-sections -o $@ $< $(MP4_SOURCES) $(LDFLAGS) -Wl,--gc-sections -lm

WEBM_SOURCES := $(COMMON)/formats/webm/rwebm.c $(COMMON)/formats/webm/rwebm_video.c
$(TARGET_TEST3): rav1_webm_stream_test.c rav1_webm_fixture.h rav1_mp4_fixture.h $(MP4_SOURCES) $(WEBM_SOURCES) $(wildcard $(COMMON)/include/formats/rav1*.h) $(COMMON)/include/formats/rwebm_video.h
	$(CC) $(CFLAGS) -DHAVE_RAV1 -ffunction-sections -fdata-sections -o $@ $< $(MP4_SOURCES) $(WEBM_SOURCES) $(LDFLAGS) -Wl,--gc-sections -lm

$(TARGET_TEST): rav1_hw_stream_test.c $(COMMON)/formats/av1/rav1_hw.c $(wildcard $(COMMON)/include/formats/rav1*.h)
	$(CC) $(CFLAGS) -o $@ $< $(COMMON)/formats/av1/rav1_hw.c $(LDFLAGS)

$(BACKEND): rav1_hw_backend_test.c $(COMMON)/formats/av1/rav1_hw.c $(COMMON)/include/formats/rav1_hw.h
	$(CC) $(CFLAGS) -o $@ $< $(COMMON)/formats/av1/rav1_hw.c $(LDFLAGS)

$(HW): rav1_hw_session_test.c $(COMMON)/include/formats/rav1_hw.h $(COMMON)/include/formats/rav1_reference_state.h $(COMMON)/include/formats/rav1_reference.h $(COMMON)/include/formats/rav1_picture.h $(COMMON)/include/formats/rav1_motion.h $(COMMON)/include/formats/rav1_grain.h $(COMMON)/include/formats/rav1_tile_info.h $(COMMON)/include/formats/rav1_tiles.h $(COMMON)/include/formats/rav1_frame.h $(COMMON)/include/formats/rav1_sequence.h $(COMMON)/include/formats/rav1.h $(COMMON)/include/formats/video_hw_frame.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(STATE): rav1_reference_state_test.c $(COMMON)/include/formats/rav1_reference_state.h $(COMMON)/include/formats/rav1_reference.h $(COMMON)/include/formats/rav1_frame.h $(COMMON)/include/formats/rav1_sequence.h $(COMMON)/include/formats/rav1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(GRAIN): rav1_grain_test.c $(COMMON)/include/formats/rav1_grain.h $(COMMON)/include/formats/rav1_reference.h $(COMMON)/include/formats/rav1_frame.h $(COMMON)/include/formats/rav1_sequence.h $(COMMON)/include/formats/rav1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(MOTION): rav1_motion_test.c $(COMMON)/include/formats/rav1_motion.h $(COMMON)/include/formats/rav1_reference.h $(COMMON)/include/formats/rav1_frame.h $(COMMON)/include/formats/rav1_tile_info.h $(COMMON)/include/formats/rav1_tiles.h $(COMMON)/include/formats/rav1_sequence.h $(COMMON)/include/formats/rav1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(REFERENCE): rav1_reference_test.c $(COMMON)/include/formats/rav1_reference_state.h $(COMMON)/include/formats/rav1_grain.h $(COMMON)/include/formats/rav1_motion.h $(COMMON)/include/formats/rav1_reference.h $(COMMON)/include/formats/rav1_picture.h $(COMMON)/include/formats/rav1_frame.h $(COMMON)/include/formats/rav1_tile_info.h $(COMMON)/include/formats/rav1_tiles.h $(COMMON)/include/formats/rav1_sequence.h $(COMMON)/include/formats/rav1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(PICTURE): rav1_picture_test.c $(COMMON)/include/formats/rav1_picture.h $(COMMON)/include/formats/rav1_frame.h $(COMMON)/include/formats/rav1_tile_info.h $(COMMON)/include/formats/rav1_tiles.h $(COMMON)/include/formats/rav1_sequence.h $(COMMON)/include/formats/rav1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(FRAME): rav1_frame_test.c $(COMMON)/include/formats/rav1_frame.h $(COMMON)/include/formats/rav1_sequence.h $(COMMON)/include/formats/rav1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(TILE_INFO): rav1_tile_info_test.c $(COMMON)/include/formats/rav1_tile_info.h $(COMMON)/include/formats/rav1_tiles.h $(COMMON)/include/formats/rav1_sequence.h $(COMMON)/include/formats/rav1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(TILES): rav1_tiles_test.c $(COMMON)/include/formats/rav1_tiles.h $(COMMON)/include/formats/rav1_sequence.h $(COMMON)/include/formats/rav1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(SEQUENCE): rav1_sequence_test.c $(COMMON)/include/formats/rav1_sequence.h $(COMMON)/include/formats/rav1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(CONTAINER): rav1_container_test.c $(COMMON)/include/formats/rav1.h \
      $(COMMON)/include/formats/rav1_sequence.h \
      $(COMMON)/include/formats/rmp4.h $(COMMON)/include/formats/rwebm.h \
      $(COMMON)/formats/mp4/rmp4.c $(COMMON)/formats/webm/rwebm.c
	$(CC) $(CFLAGS) -o $@ rav1_container_test.c \
	   $(COMMON)/formats/mp4/rmp4.c $(COMMON)/formats/webm/rwebm.c $(LDFLAGS) -lm

check: all
	$(RUNNER) ./$(TARGET)
	$(RUNNER) ./$(CONTAINER)
	$(RUNNER) ./$(SEQUENCE)
	$(RUNNER) ./$(TILES)
	$(RUNNER) ./$(TILE_INFO)
	$(RUNNER) ./$(FRAME)
	$(RUNNER) ./$(PICTURE)
	$(RUNNER) ./$(REFERENCE)
	$(RUNNER) ./$(MOTION)
	$(RUNNER) ./$(GRAIN)
	$(RUNNER) ./$(STATE)
	$(RUNNER) ./$(HW)
	$(RUNNER) ./$(TARGET_TEST)
	$(RUNNER) ./$(TARGET_TEST2)
	$(RUNNER) ./$(TARGET_TEST3)
	$(RUNNER) ./$(BACKEND)

clean:
	rm -f rav1_webm_stream_test rav1_webm_stream_test.exe
	rm -f rav1_mp4_stream_test rav1_mp4_stream_test.exe
	rm -f rav1_hw_stream_test rav1_hw_stream_test.exe
	rm -f rav1_hw_backend_test rav1_hw_backend_test.exe
	rm -f rav1_hw_session_test rav1_hw_session_test.exe rav1_reference_state_test rav1_reference_state_test.exe rav1_grain_test rav1_grain_test.exe rav1_motion_test rav1_motion_test.exe rav1_reference_test rav1_reference_test.exe rav1_picture_test rav1_picture_test.exe rav1_frame_test rav1_frame_test.exe rav1_tile_info_test rav1_tile_info_test.exe rav1_tiles_test rav1_tiles_test.exe rav1_sequence_test rav1_sequence_test.exe rav1_test rav1_test.exe rav1_container_test rav1_container_test.exe

.PHONY: all check clean

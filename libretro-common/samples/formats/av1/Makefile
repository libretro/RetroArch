CC ?= cc
COMMON := ../../..
TARGET := rav1_test$(EXE)
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

all: $(TARGET) $(CONTAINER) $(SEQUENCE) $(TILES) $(TILE_INFO) $(FRAME) $(PICTURE) $(REFERENCE) $(MOTION)

$(TARGET): rav1_test.c $(COMMON)/include/formats/rav1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(MOTION): rav1_motion_test.c $(COMMON)/include/formats/rav1_motion.h $(COMMON)/include/formats/rav1_reference.h $(COMMON)/include/formats/rav1_frame.h $(COMMON)/include/formats/rav1_tile_info.h $(COMMON)/include/formats/rav1_tiles.h $(COMMON)/include/formats/rav1_sequence.h $(COMMON)/include/formats/rav1.h
	$(CC) $(CFLAGS) -o $@ $< $(LDFLAGS)

$(REFERENCE): rav1_reference_test.c $(COMMON)/include/formats/rav1_motion.h $(COMMON)/include/formats/rav1_reference.h $(COMMON)/include/formats/rav1_picture.h $(COMMON)/include/formats/rav1_frame.h $(COMMON)/include/formats/rav1_tile_info.h $(COMMON)/include/formats/rav1_tiles.h $(COMMON)/include/formats/rav1_sequence.h $(COMMON)/include/formats/rav1.h
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

clean:
	rm -f rav1_motion_test rav1_motion_test.exe rav1_reference_test rav1_reference_test.exe rav1_picture_test rav1_picture_test.exe rav1_frame_test rav1_frame_test.exe rav1_tile_info_test rav1_tile_info_test.exe rav1_tiles_test rav1_tiles_test.exe rav1_sequence_test rav1_sequence_test.exe rav1_test rav1_test.exe rav1_container_test rav1_container_test.exe

.PHONY: all check clean

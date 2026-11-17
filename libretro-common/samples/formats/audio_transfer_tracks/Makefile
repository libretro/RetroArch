CC ?= cc
LRC := ../../..
TARGET := audio_transfer_tracks_test
SOURCES := audio_transfer_tracks_test.c $(LRC)/formats/audio_transfer.c \
 $(LRC)/formats/mp4/rmp4.c $(LRC)/formats/webm/rwebm.c \
 $(LRC)/formats/aac/raac.c $(LRC)/formats/opus/ropus.c \
 $(LRC)/formats/vorbis/rvorbis.c $(LRC)/formats/flac/rflac.c \
 $(LRC)/compat/compat_strl.c $(LRC)/string/stdstring.c \
 $(LRC)/encodings/encoding_crc32.c $(LRC)/features/features_cpu.c \
 $(LRC)/encodings/encoding_utf.c $(LRC)/file/file_path.c $(LRC)/time/rtime.c
CFLAGS += -O2 -g1 -ffunction-sections -fdata-sections -std=gnu89 -Wall -Wextra -Werror=declaration-after-statement \
 -Wframe-larger-than=2048 -I$(LRC)/include \
 -DHAVE_RMP4 -DHAVE_RWEBM -DHAVE_RAAC -DHAVE_ROPUS -DHAVE_RVORBIS -DHAVE_RFLAC
LDFLAGS += -lm
ifeq ($(shell uname -s),Darwin)
LDFLAGS += -Wl,-dead_strip -framework CoreFoundation
else
LDFLAGS += -Wl,--gc-sections
endif
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
ifneq ($(findstring undefined,$(SANITIZER)),)
CFLAGS += -fno-sanitize-recover=undefined
endif
else
CFLAGS += -Werror=frame-larger-than=2048
endif
all: $(TARGET)
$(TARGET): $(SOURCES) $(LRC)/include/formats/audio.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS)
check: all
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean

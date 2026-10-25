TARGET := subtitle_srt_test
ROOT := ../../..
SOURCES := subtitle_srt_test.c $(ROOT)/formats/subtitle/rsubtitle_srt.c
CFLAGS += -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long \
          -Wframe-larger-than=2048 -I$(ROOT)/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET) subtitle_srt_fault_test subtitle_cues_test

subtitle_cues_test: subtitle_cues_test.c $(ROOT)/formats/subtitle/rsubtitle_srt.c
	$(CC) $(CFLAGS) $^ -o $@ $(LDFLAGS)

$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

subtitle_srt_fault_test: subtitle_srt_fault_test.c $(ROOT)/formats/subtitle/rsubtitle_srt.c
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

check: all
	./$(TARGET)
	./subtitle_srt_fault_test
	./subtitle_cues_test

clean:
	rm -f $(TARGET) $(TARGET).exe subtitle_srt_fault_test subtitle_srt_fault_test.exe
	rm -f subtitle_cues_test subtitle_cues_test.exe

.PHONY: all check clean

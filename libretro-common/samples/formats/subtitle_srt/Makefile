TARGET := subtitle_srt_test
ROOT := ../../..
SUBTITLE_SOURCES := $(ROOT)/formats/subtitle/rsubtitle_srt.c \
                    $(ROOT)/formats/subtitle/rsubtitle_vtt.c \
                    $(ROOT)/formats/subtitle/rsubtitle_vtt_css.c \
                    $(ROOT)/formats/subtitle/rsubtitle_vtt_css_tree.c \
                    $(ROOT)/formats/subtitle/rsubtitle_vtt_selector.c
HEADERS := $(ROOT)/include/formats/rsubtitle_srt.h \
           $(ROOT)/include/formats/rsubtitle_vtt.h \
           $(ROOT)/formats/subtitle/rsubtitle_vtt_internal.h \
           $(ROOT)/formats/subtitle/rsubtitle_languages.h \
           $(ROOT)/formats/subtitle/rsubtitle_entities.h
SOURCES := subtitle_srt_test.c $(SUBTITLE_SOURCES)
CFLAGS += -std=c89 -pedantic -Wall -Wextra -Werror -Wno-long-long \
          -Wframe-larger-than=2048 -I$(ROOT)/include
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif

all: $(TARGET) subtitle_srt_fault_test subtitle_cues_test subtitle_vtt_test subtitle_css_test \
     subtitle_selector_test subtitle_language_registry_test

subtitle_language_registry_test: subtitle_language_registry_test.c $(SUBTITLE_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $< $(SUBTITLE_SOURCES) -o $@ $(LDFLAGS)

subtitle_selector_test: subtitle_selector_test.c $(SUBTITLE_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) subtitle_selector_test.c $(SUBTITLE_SOURCES) -o $@ $(LDFLAGS)

subtitle_css_test: subtitle_css_test.c $(ROOT)/formats/subtitle/rsubtitle_vtt_css.c $(ROOT)/formats/subtitle/rsubtitle_vtt_css_tree.c $(HEADERS)
	$(CC) $(CFLAGS) subtitle_css_test.c $(ROOT)/formats/subtitle/rsubtitle_vtt_css.c $(ROOT)/formats/subtitle/rsubtitle_vtt_css_tree.c -o $@ $(LDFLAGS)

subtitle_cues_test: subtitle_cues_test.c $(SUBTITLE_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) subtitle_cues_test.c $(SUBTITLE_SOURCES) -o $@ $(LDFLAGS)

subtitle_vtt_test: subtitle_vtt_test.c $(SUBTITLE_SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) subtitle_vtt_test.c $(SUBTITLE_SOURCES) -o $@ $(LDFLAGS)

$(TARGET): $(SOURCES) $(HEADERS)
	$(CC) $(CFLAGS) $(SOURCES) -o $@ $(LDFLAGS)

subtitle_srt_fault_test: subtitle_srt_fault_test.c $(SUBTITLE_SOURCES) $(ROOT)/formats/subtitle/rsubtitle_vtt_css.c $(HEADERS)
	$(CC) $(CFLAGS) $< -o $@ $(LDFLAGS)

check: all
	./$(TARGET)
	./subtitle_srt_fault_test
	./subtitle_cues_test
	./subtitle_vtt_test
	./subtitle_css_test
	./subtitle_selector_test
	./subtitle_language_registry_test

clean:
	rm -f $(TARGET) $(TARGET).exe subtitle_srt_fault_test subtitle_srt_fault_test.exe
	rm -f subtitle_cues_test subtitle_cues_test.exe subtitle_vtt_test subtitle_vtt_test.exe
	rm -f subtitle_css_test subtitle_css_test.exe subtitle_selector_test subtitle_selector_test.exe
	rm -f subtitle_language_registry_test subtitle_language_registry_test.exe

.PHONY: all check clean

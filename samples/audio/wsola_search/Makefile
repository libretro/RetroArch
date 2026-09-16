CC = gcc
ROOT = ../../..
CPPFLAGS += -I$(ROOT)/libretro-common/include
CFLAGS += -O2 -std=c89 -Wall -Wextra
LDLIBS += -lm
ifneq ($(SANITIZER),)
CFLAGS += -fsanitize=$(SANITIZER) -fno-omit-frame-pointer -g
LDFLAGS += -fsanitize=$(SANITIZER)
endif
TARGET = wsola_search_test
SOURCES = wsola_search_test.c $(ROOT)/libretro-common/audio/dsp_filters/wsolapitchtempo.c

all: $(TARGET)
$(TARGET): $(SOURCES) $(ROOT)/libretro-common/include/audio/wsola_search.h
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean

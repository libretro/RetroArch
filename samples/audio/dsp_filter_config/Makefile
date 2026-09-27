CC = gcc
ROOT = ../../..
FILTERS = $(ROOT)/libretro-common/audio/dsp_filters
CPPFLAGS += -I$(ROOT)/libretro-common/include -DHAVE_FILTERS_BUILTIN
CFLAGS += -O1 -g -std=gnu89 -Wall -Wextra
LDLIBS += -lm
ifneq ($(SANITIZER),)
# -fno-sanitize-recover turns a report into a trap, so the filter dies in
# its own child and the sweep reports it instead of printing and going on.
CFLAGS += -fsanitize=$(SANITIZER) -fno-sanitize-recover=all \
	-fno-omit-frame-pointer
LDFLAGS += -fsanitize=$(SANITIZER)
endif
TARGET = dsp_filter_config_test
SOURCES = dsp_filter_config_test.c \
	$(FILTERS)/bitcrusher.c \
	$(FILTERS)/chorus.c \
	$(FILTERS)/crystalizer.c \
	$(FILTERS)/echo.c \
	$(FILTERS)/eq.c \
	$(FILTERS)/iir.c \
	$(FILTERS)/overdrive.c \
	$(FILTERS)/panning.c \
	$(FILTERS)/phaser.c \
	$(FILTERS)/reverb.c \
	$(FILTERS)/reverb_early.c \
	$(FILTERS)/tremolo.c \
	$(FILTERS)/vibrato.c \
	$(FILTERS)/wahwah.c \
	$(FILTERS)/wsolapitchtempo.c

all: $(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CPPFLAGS) $(CFLAGS) $(SOURCES) $(LDFLAGS) $(LDLIBS) -o $@
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean

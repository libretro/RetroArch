# menu/menu_screensaver.c's snow effect, compiled whole into the harness
# with the font and draw calls stubbed: an hour of frames must keep the
# flakes on screen and drifting left. No video driver needed.

TARGET    := screensaver_snow_test
REPO_ROOT := ../../..
SOURCES   := screensaver_snow_test.c
CFLAGS    += -Wall -std=gnu99 -pedantic -Wno-long-long -g1 -O1 \
             -I$(REPO_ROOT) -I$(REPO_ROOT)/libretro-common/include \
             -I$(REPO_ROOT)/gfx/include
LDLIBS    += -lm
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
$(TARGET): $(SOURCES) $(REPO_ROOT)/menu/menu_screensaver.c
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS) $(LDLIBS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET) $(TARGET).exe
.PHONY: all check clean

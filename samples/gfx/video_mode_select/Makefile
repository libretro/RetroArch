# gfx/common/video_mode_select.c: which listed mode a requested size and
# rate means, and the order the menu shows a resolution list in - the
# code every display server's mode switch now goes through.

TARGET    := video_mode_select_test
REPO_ROOT := ../../..
SOURCES   := video_mode_select_test.c $(REPO_ROOT)/gfx/common/video_mode_select.c
CFLAGS    += -Wall -std=gnu99 -g -O0 -DRARCH_INTERNAL \
             -I$(REPO_ROOT) -I$(REPO_ROOT)/libretro-common/include
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)
$(TARGET): $(SOURCES)
	$(CC) $(CFLAGS) -o $@ $^ $(LDFLAGS)
check: $(TARGET)
	./$(TARGET)
clean:
	rm -f $(TARGET)
.PHONY: all check clean

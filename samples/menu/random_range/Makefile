# libretro-common's random_range(), which the menu's Scan picks rows
# with, held to its inclusive range and an even spread across it: once
# with the host's rand() range, once with a 15-bit one as MSVC and
# mingw have.

TARGET    := random_range_test
REPO_ROOT := ../../..
SOURCES   := random_range_test.c
CFLAGS    += -Wall -std=c89 -pedantic -Wno-long-long -g1 -O1 \
             -I$(REPO_ROOT)/libretro-common/include
ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET) $(TARGET)_rand15
$(TARGET): $(SOURCES) $(REPO_ROOT)/libretro-common/include/retro_math.h
	$(CC) $(CFLAGS) -o $@ $(SOURCES) $(LDFLAGS) $(LDLIBS)
$(TARGET)_rand15: $(SOURCES) $(REPO_ROOT)/libretro-common/include/retro_math.h
	$(CC) $(CFLAGS) -DTEST_RAND_MAX=32767 -o $@ $(SOURCES) $(LDFLAGS) $(LDLIBS)
check: all
	./$(TARGET)
	./$(TARGET)_rand15
clean:
	rm -f $(TARGET) $(TARGET).exe $(TARGET)_rand15 $(TARGET)_rand15.exe
.PHONY: all check clean

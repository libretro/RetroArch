TARGET := winraw_abs_pos_test.exe

# The real winraw driver, cross-built with mingw-w64 and run under
# Wine. winraw_input.c is included by the test itself, for its static
# report handler, poll and mouse table; the frontend symbols it links
# against are stubbed in the test. Nothing here needs a display.
#    make && make check
REPO_ROOT := ../../..
CC        := x86_64-w64-mingw32-gcc

CFLAGS  += -Wall -std=gnu89 -Wdeclaration-after-statement -g -O1 \
           -DRARCH_INTERNAL -DHAVE_MENU -DHAVE_THREADS -DHAVE_WINRAWINPUT \
           -I$(REPO_ROOT) -I$(REPO_ROOT)/input/include \
           -I$(REPO_ROOT)/libretro-common/include -I$(REPO_ROOT)/deps

LDLIBS  += -lhid -lsetupapi -luser32 -lgdi32

all: $(TARGET)

$(TARGET): winraw_abs_pos_test.c $(REPO_ROOT)/input/drivers/winraw_input.c
	$(CC) $(CFLAGS) -o $@ winraw_abs_pos_test.c \
		$(REPO_ROOT)/libretro-common/compat/compat_strl.c $(LDLIBS)

check: all
	WINEDEBUG=-all wine ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

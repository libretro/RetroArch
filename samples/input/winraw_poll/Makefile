TARGET := winraw_poll_test.exe

# The real winraw driver, cross-built with mingw-w64 and run under
# Wine. winraw_input.c is included by the test itself, for its statics;
# the frontend symbols it links against are stubbed in the test. With
# a display (xvfb-run) the test also injects keys and follows them
# through a bulk read; without one that part is skipped.
#    make && make check
REPO_ROOT := ../../..
CC        := x86_64-w64-mingw32-gcc

CFLAGS  += -Wall -std=gnu89 -Wdeclaration-after-statement -g -O1 \
           -DRARCH_INTERNAL -DHAVE_MENU -DHAVE_THREADS -DHAVE_WINRAWINPUT \
           -I$(REPO_ROOT) -I$(REPO_ROOT)/input/include \
           -I$(REPO_ROOT)/libretro-common/include -I$(REPO_ROOT)/deps

LDLIBS  += -lhid -lsetupapi -luser32 -lgdi32

all: $(TARGET)

$(TARGET): winraw_poll_test.c $(REPO_ROOT)/input/drivers/winraw_input.c
	$(CC) $(CFLAGS) -o $@ winraw_poll_test.c \
		$(REPO_ROOT)/libretro-common/compat/compat_strl.c $(LDLIBS)

# wine goes by several names; the first one found is used
WINE ?= $(shell for w in wine wine64 /usr/lib/wine/wine64; do \
           command -v $$w >/dev/null 2>&1 && { echo $$w; break; }; done)
XVFB ?= $(shell command -v xvfb-run >/dev/null 2>&1 && echo "xvfb-run -a")

check: all
	@test -n "$(WINE)" || { echo "no wine" >&2; exit 1; }
	WINEDEBUG=-all $(XVFB) $(WINE) ./$(TARGET)

clean:
	rm -f $(TARGET)

.PHONY: all check clean

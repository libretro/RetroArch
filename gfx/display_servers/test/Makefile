# Regression tests for gfx/display_servers/dispserv_x11.c.
#
# Run from this directory:
#
#     make            build and run
#     make clean
#
# Needs libcheck and the X11/Xrandr headers.  It does *not* need an X
# server, a display or the RandR extension: xrandr_stub.c stands in for
# libXrandr and the handful of Xlib entry points dispserv_x11.c reaches,
# which is the only way to drive the failure modes under test.  Flags
# follow libretro-common/Makefile.test.

ROOT = ../../..

CC ?= gcc

TEST_UNIT_CFLAGS = $(CFLAGS) -I. -I$(ROOT) -I$(ROOT)/libretro-common/include \
		   -DHAVE_XRANDR \
		   $(shell pkg-config --cflags check) \
		   -Wall -Wdeclaration-after-statement \
		   -fsanitize=address -fsanitize=undefined -ggdb

TEST_LIBS = $(shell pkg-config --libs check)

TEST_DISPSERV_X11 = test_dispserv_x11
TEST_DISPSERV_X11_SRC = test_dispserv_x11.c xrandr_stub.c stubs_retroarch.c \
			$(ROOT)/gfx/display_servers/dispserv_x11.c

all: run

$(TEST_DISPSERV_X11): $(TEST_DISPSERV_X11_SRC)
	$(CC) $(TEST_UNIT_CFLAGS) $(TEST_DISPSERV_X11_SRC) $(TEST_LIBS) \
		-o $(TEST_DISPSERV_X11)

run: $(TEST_DISPSERV_X11)
	./$(TEST_DISPSERV_X11)

clean:
	rm -f $(TEST_DISPSERV_X11) *.gcda *.gcno

.PHONY: all run clean

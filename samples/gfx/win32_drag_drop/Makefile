TARGET := win32_drag_drop_test
GEN    := platform_sim.c ui_sim.c

LIBRETRO_COMM_DIR := ../../../libretro-common
PLATFORM_SRC      ?= ../../../frontend/drivers/platform_win32.c
UI_SRC            ?= ../../../ui/drivers/ui_win32.c

# The drag and drop setup and the drop handler as they ship, lifted out
# of platform_win32.c and ui_win32.c and compiled against a fake
# shell32/user32/dwmapi instead of <windows.h>.
SOURCES := win32_drag_drop_test.c \
           $(LIBRETRO_COMM_DIR)/lists/string_list.c \
           $(LIBRETRO_COMM_DIR)/compat/compat_strl.c

OBJS := $(SOURCES:.c=.o)

CFLAGS += -Wall -Wextra -Wno-unused-parameter -std=c89 -pedantic -g -O1 \
          -I. -I$(LIBRETRO_COMM_DIR)/include -DHAVE_DYLIB -DHAVE_MENU

ifneq ($(SANITIZER),)
   CFLAGS  := -fsanitize=$(SANITIZER) -fno-omit-frame-pointer $(CFLAGS)
   LDFLAGS := -fsanitize=$(SANITIZER) $(LDFLAGS)
endif

all: $(TARGET)

# gfx_dwm_shutdown() through gfx_init_dwm(), which hold the setup.
platform_sim.c: $(PLATFORM_SRC)
	awk '/^static void gfx_dwm_shutdown\(void\)$$/ { on = 1 } \
	     on { print } \
	     on && /^static bool gfx_init_dwm\(void\)$$/ { last = 1 } \
	     last && /^}$$/ { exit }' $< > $@
	grep -q 'win32_drag_drop_init' $@
	grep -q 'gfx_init_dwm' $@

# The ANSI drop handler through the #endif after the Unicode one.
ui_sim.c: $(UI_SRC)
	awk '/^static bool win32_drag_query_file_ansi\(HWND hwnd, WPARAM wparam\)$$/ { on = 1 } \
	     on { print } \
	     on && /^bool win32_drag_query_file\(HWND hwnd, WPARAM wparam\)$$/ { n++ } \
	     n == 2 && /^}$$/ { done = 1 } \
	     done && /^#endif$$/ { exit }' $< > $@
	grep -q 'DragQueryFileW' $@

win32_drag_drop_test.o: $(GEN) fake_win32.h

$(TARGET): $(OBJS)
	$(CC) -o $@ $(OBJS) $(LDFLAGS)

check: all
	./$(TARGET) win7
	./$(TARGET) vista
	./$(TARGET) xp
	./$(TARGET) win9x
	./$(TARGET) wstub

clean:
	rm -f $(OBJS) $(TARGET) $(GEN)

.PHONY: all check clean

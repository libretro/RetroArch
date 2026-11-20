/* dinput: the touches the window procedure hands the driver.
 *
 * The real driver, cross-built with mingw-w64 and run under Wine.
 * WM_POINTER* go to dinput_handle_message() as the main window's
 * procedure and a screen's own window's hand them on, and what the
 * driver publishes at its poll is read back. No device is opened.
 *
 * Checked here:
 *
 * - a touch in the main window and one in a screen's own window are
 *   published in the window each went down in, in its client
 *   coordinates, until lifted;
 * - a touch whose window loses it (WM_POINTERCAPTURECHANGED, as when
 *   the window goes from under a resting finger) is dropped;
 * - a down for a touch still held, whose lift never came, starts it
 *   again in the window and at the place it now goes down in. */
#include <stdio.h>
#include <stdlib.h>

#include "input/drivers/dinput.c"

/* The frontend, as far as the driver links against it. */
uint8_t g_win32_flags;
ui_window_win32_t main_window;
enum retro_key rarch_keysym_lut[RETROK_LAST];
const struct rarch_key_map rarch_key_map_dinput[] = { { 0, RETROK_UNKNOWN } };
bool input_config_get_nowinkey_enable(void) { return false; }
bool input_config_get_video_windows_enable(void) { return true; }
void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device)
{ (void)down; (void)code; (void)character; (void)mod; (void)device; }
void input_keymaps_init_keyboard_lut(const struct rarch_key_map *map)
{ (void)map; }
void joypad_driver_reinit(void *data, const char *name)
{ (void)data; (void)name; }
void win32_clip_window(bool grab) { (void)grab; }
HWND win32_foreground_window(void) { return NULL; }
uint16_t win32_get_keyboard_mods(void) { return 0; }
void win32_hotplug_arm(void) { }
bool win32_hotplug_due(void) { return false; }
bool win32_video_window_open(HWND hwnd) { (void)hwnd; return false; }

/* the main window: none, so its client coordinates are the screen's */
uintptr_t video_driver_window_get(void) { return 0; }

/* a screen's own window, window 1 at generation 7, by its client area
 * on the desktop */
static const RECT screen_area = { 300, 10, 460, 130 };
unsigned win32_video_window_under(POINT screen, unsigned *generation,
      POINT *client)
{
   *generation = 0;
   if (!PtInRect(&screen_area, screen))
      return 0;
   *generation = 7;
   client->x   = screen.x - screen_area.left;
   client->y   = screen.y - screen_area.top;
   return 1;
}
bool win32_video_window_point(unsigned window, unsigned generation,
      POINT screen, POINT *client)
{
   if (window != 1 || generation != 7)
      return false;
   client->x = screen.x - screen_area.left;
   client->y = screen.y - screen_area.top;
   return true;
}

/* The driver hands its mouse and touches to the frontend at its poll. */
input_pointer_frame_t *input_driver_pointers_lend(void)
{
   static input_pointer_frame_t lent[MAX_USERS];
   return lent;
}
void input_driver_publish_window_pointers(const input_pointer_frame_t *frames,
      const input_video_window_t *windows, unsigned count, unsigned flags)
{ (void)frames; (void)windows; (void)count; (void)flags; }
uint32_t *input_driver_touches_lend(void)
{
   static uint32_t lent[32];
   return lent;
}

/* the touches of the last poll */
static unsigned             pub_touch_n;
static uint32_t             pub_touch_pos[32];
static input_video_window_t pub_touch_win[32];
void input_driver_publish_window_touches(const uint32_t *pos,
      const input_video_window_t *windows, unsigned count,
      unsigned present, unsigned down)
{
   (void)present; (void)down;
   pub_touch_n = count;
   memcpy(pub_touch_pos, pos, count * sizeof(*pos));
   memcpy(pub_touch_win, windows, count * sizeof(*windows));
}

static unsigned failures;

#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

static struct dinput_input *di;

/* @msg for touch @id at @x,@y on the desktop, as a window hands it on */
static void touch(UINT msg, int id, int x, int y)
{
   dinput_handle_message(di, msg, MAKEWPARAM(id, 0), MAKELPARAM(x, y));
}

int main(void)
{
   di = (struct dinput_input*)calloc(1, sizeof(*di));
   if (!di)
      return 1;

   touch(WM_POINTERDOWN, 5, 30, 40);
   touch(WM_POINTERDOWN, 6, 312, 30);
   dinput_publish_pointers(di);
   CHECK(pub_touch_n == 2
         && pub_touch_win[0].window == 0
         && pub_touch_pos[0] == VIDEO_POS_PACK(30, 40)
         && pub_touch_win[1].window == 1
         && pub_touch_win[1].generation == 7
         && pub_touch_pos[1] == VIDEO_POS_PACK(12, 20),
         "a touch in the main window and one in a screen's own: %u"
         " published, the second at %d,%d in window %u", pub_touch_n,
         VIDEO_POS_X(pub_touch_pos[1]), VIDEO_POS_Y(pub_touch_pos[1]),
         pub_touch_win[1].window);
   touch(WM_POINTERUP, 5, 30, 40);
   dinput_publish_pointers(di);
   CHECK(pub_touch_n == 1 && pub_touch_win[0].window == 1,
         "the main window's lifted: %u left", pub_touch_n);

   /* its window gone from under a resting finger: no lift comes, only
    * the capture taken away */
   touch(WM_POINTERCAPTURECHANGED, 6, 0, 0);
   dinput_publish_pointers(di);
   CHECK(pub_touch_n == 0, "a touch whose window lost it: %u left",
         pub_touch_n);
   touch(WM_POINTERUP, 6, 312, 30);

   /* a second down for an id still held, no lift between: a new touch */
   touch(WM_POINTERDOWN, 9, 312, 30);
   touch(WM_POINTERDOWN, 9, 30, 40);
   dinput_publish_pointers(di);
   CHECK(pub_touch_n == 1 && pub_touch_win[0].window == 0
         && pub_touch_pos[0] == VIDEO_POS_PACK(30, 40),
         "a second down for a touch held: %u published, the first at"
         " %d,%d in window %u", pub_touch_n, VIDEO_POS_X(pub_touch_pos[0]),
         VIDEO_POS_Y(pub_touch_pos[0]), pub_touch_win[0].window);
   touch(WM_POINTERUP, 9, 30, 40);
   dinput_publish_pointers(di);
   CHECK(pub_touch_n == 0, "lifted again: %u left", pub_touch_n);

   free(di);
   if (failures)
   {
      printf("FAIL dinput_touch_test: %u checks\n", failures);
      return 1;
   }
   printf("PASS dinput_touch_test\n");
   return 0;
}

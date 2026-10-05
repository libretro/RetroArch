/* winraw: a mouse plugged in while running, and the Windows-key
 * setting changed while running.
 *
 * Both used to take effect only when the driver was started again,
 * which every video driver restart did. The driver, read by the poll,
 * now does them when they come up (winraw_mice_refresh(),
 * winraw_nowinkey_apply()).
 *
 * The real driver, cross-built with mingw-w64 and run under Wine, with
 * the list of raw input devices it asks Windows for replaced by one
 * the test controls.
 *
 * Checked here:
 *
 * - a report from a mouse that is not in the list has the list made
 *   again at the end of that poll, with the new mouse in it, and its
 *   next report moves it;
 * - the mice that were there keep their position and buttons;
 * - a handle that is still not a listed mouse after that is not asked
 *   about again, and a report with no handle never is;
 * - the list is not made again within a second of the last time;
 * - the Windows-key setting changed between two polls is what the
 *   keyboard is registered with after the second. */
#include <stdio.h>
#include <stdlib.h>

#include <retro_atomic.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ---- the list of devices, as the test says ---------------------- */
static HANDLE   fake_mice[8];
static unsigned fake_mice_n;
static unsigned list_calls;   /* times the list was fetched */

static UINT WINAPI fake_GetRawInputDeviceList(PRAWINPUTDEVICELIST list,
      PUINT count, UINT size)
{
   unsigned i;
   (void)size;
   if (!list)
   {
      *count = fake_mice_n;
      return 0;
   }
   list_calls++;
   for (i = 0; i < fake_mice_n && i < *count; i++)
   {
      list[i].hDevice = fake_mice[i];
      list[i].dwType  = RIM_TYPEMOUSE;
   }
   return fake_mice_n;
}
#define GetRawInputDeviceList fake_GetRawInputDeviceList

#include "input/drivers/winraw_input.c"

/* The frontend, as far as the driver links against it. */
uint8_t g_win32_flags;
ui_window_win32_t main_window;
retro_keybind_set input_config_binds[MAX_USERS];
retro_keybind_set input_autoconf_binds[MAX_USERS];
enum retro_key rarch_keysym_lut[RETROK_LAST];
const struct rarch_key_map rarch_key_map_winraw[] = { { 0, RETROK_UNKNOWN } };
static settings_t stub_settings;
static struct menu_state stub_menu;
settings_t *config_get_ptr(void) { return &stub_settings; }
/* the settings a driver asks for by name, read from the ones above */
#include "../input_config_stubs.h"

/* A port's keyboard, as the frontend works it out from the port's pin
 * and number: the rule itself (input/common/input_device_pins.h), fed
 * from the stub settings each time it is asked. */
#include "input/common/input_device_pins.h"
static char     stub_pin_ident[MAX_INPUT_DEVICES][INPUT_PIN_LEN];
static unsigned stub_pin_listed;
void input_keyboard_pins_set_devices(const char (*base)[64], unsigned n)
{
   memset(stub_pin_ident, 0, sizeof(stub_pin_ident));
   if (n)
      input_pins_identities(stub_pin_ident, base, n);
   stub_pin_listed = n;
}
int input_keyboard_port_choice(unsigned port)
{
   int8_t choice[MAX_USERS];
   input_pins_resolve(choice,
         (const char (*)[INPUT_PIN_LEN])stub_settings.arrays.input_keyboard_device,
         stub_settings.uints.input_keyboard_index, MAX_USERS,
         (const char (*)[INPUT_PIN_LEN])stub_pin_ident, stub_pin_listed);
   return choice[port];
}

/* and its mouse, the same way */
static char     stub_mouse_ident[MAX_INPUT_DEVICES][INPUT_PIN_LEN];
static unsigned stub_mouse_listed;
void input_mouse_pins_set_devices(const char (*base)[64], unsigned n)
{
   memset(stub_mouse_ident, 0, sizeof(stub_mouse_ident));
   if (n > MAX_INPUT_DEVICES)
      n = MAX_INPUT_DEVICES;
   if (n)
      input_pins_identities(stub_mouse_ident, base, n);
   stub_mouse_listed = n;
}
unsigned input_mouse_port_index(unsigned port)
{
   int16_t choice[MAX_USERS];
   input_pins_resolve_mice(choice,
         (const char (*)[INPUT_PIN_LEN])stub_settings.arrays.input_mouse_device,
         stub_settings.uints.input_mouse_index, MAX_USERS,
         (const char (*)[INPUT_PIN_LEN])stub_mouse_ident, stub_mouse_listed);
   return choice[port] < 0 ? MAX_INPUT_DEVICES : (unsigned)choice[port];
}
struct menu_state *menu_state_get_ptr(void) { return &stub_menu; }
bool menu_driver_alive(void) { return (stub_menu.flags & MENU_ST_FLAG_ALIVE) != 0; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void input_config_set_mouse_display_name(unsigned port, const char *name)
{ (void)port; (void)name; }
void input_config_set_keyboard_display_name(unsigned idx, const char *name)
{ (void)idx; (void)name; }
void input_config_clear_keyboard_display_names(void) { }
void input_config_clear_mouse_info(void) { }
void input_config_set_mouse_device(unsigned idx, const char *device,
      uint16_t vid, uint16_t pid, bool hidden)
{ (void)idx; (void)device; (void)vid; (void)pid; (void)hidden; }
void input_config_set_keyboard_ids(unsigned idx, uint16_t vid, uint16_t pid)
{ (void)idx; (void)vid; (void)pid; }
unsigned input_driver_lightgun_id_convert(unsigned id) { return id; }
bool input_driver_pointer_is_offscreen(int16_t x, int16_t y)
{ (void)x; (void)y; return false; }
void input_keymaps_init_keyboard_lut(const struct rarch_key_map *map)
{ (void)map; }
void joypad_driver_reinit(void *data, const char *name)
{ (void)data; (void)name; }
retro_task_t *task_init(void) { return NULL; }
bool task_queue_push(retro_task_t *task) { (void)task; return false; }
void task_set_flags(retro_task_t *task, uint8_t flags, bool set)
{ (void)task; (void)flags; (void)set; }
bool video_driver_get_viewport_info(struct video_viewport *vp)
{ (void)vp; return false; }
bool video_driver_translate_coord_viewport(struct video_viewport *vp,
      int mouse_x, int mouse_y, int16_t *res_x, int16_t *res_y,
      int16_t *res_screen_x, int16_t *res_screen_y, bool report_oob)
{
   (void)vp; (void)mouse_x; (void)mouse_y; (void)res_x; (void)res_y;
   (void)res_screen_x; (void)res_screen_y; (void)report_oob;
   return false;
}
uintptr_t video_driver_window_get(void) { return 0; }
void win32_clip_window(bool grab) { (void)grab; }
void win32_hotplug_arm(void) { }
bool win32_hotplug_due(void) { return false; }




void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device)
{ (void)down; (void)code; (void)character; (void)mod; (void)device; }
enum retro_key input_keymaps_translate_keysym_to_rk(unsigned sym)
{ return (enum retro_key)sym; }
uint16_t win32_get_keyboard_mods(void) { return 0; }
void winraw_joypad_take_hid(HANDLE device, const BYTE *data,
      DWORD report_size, DWORD count)
{ (void)device; (void)data; (void)report_size; (void)count; }
bool winraw_joypad_survives_video(void) { return true; }

static unsigned failures;

#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

#define M(n) ((HANDLE)(uintptr_t)(0x100 + (n)))

static winraw_input_t *wr;

static void move(HANDLE mouse, LONG dx, LONG dy)
{
   RAWMOUSE m;
   memset(&m, 0, sizeof(m));
   m.lLastX = dx;
   m.lLastY = dy;
   winraw_take(wr, RIM_TYPEMOUSE, mouse, &m);
}

static int index_of(HANDLE mouse)
{
   unsigned i;
   for (i = 0; i < wr->mouse_cnt; i++)
      if (g_mice[i].hnd == mouse)
         return (int)i;
   return -1;
}

int main(void)
{
   int a, b;
   unsigned calls;

   /* one mouse when the driver starts */
   fake_mice[0] = M(1);
   fake_mice_n  = 1;
   wr = (winraw_input_t*)winraw_init("null");
   CHECK(wr && wr->poll_drain && wr->mouse_cnt == 1, "start with one mouse");
   if (!wr)
      return 1;
   /* focused: without the focus the poll lets go of mouse buttons */
   winraw_focus = true;
   winraw_poll(wr);

   /* it has moved and has a button down */
   a = index_of(M(1));
   g_mice[a].x      = 123;
   g_mice[a].y      = 456;
   g_mice[a].flags  = WRAW_MOUSE_FLG_BTN_L;
   g_mice[a].device = RETRO_DEVICE_LIGHTGUN;

   /* ---- a second mouse is plugged in ----------------------------- */
   fake_mice[1] = M(2);
   fake_mice_n  = 2;
   calls        = list_calls;
   move(M(2), 5, 3);
   CHECK(wr->mouse_cnt == 1 && list_calls == calls,
         "the list was made again before the poll's end");
   winraw_poll(wr);
   CHECK(wr->mouse_cnt == 2 && list_calls == calls + 1,
         "a report from a new mouse: %u in the list, the list fetched %u time(s)",
         wr->mouse_cnt, list_calls - calls);
   a = index_of(M(1));
   b = index_of(M(2));
   CHECK(a >= 0 && b >= 0, "the two mice are not both in the list");
   if (a >= 0)
      CHECK(g_mice[a].x == 123 && g_mice[a].y == 456
            && g_mice[a].flags == WRAW_MOUSE_FLG_BTN_L
            && g_mice[a].device == RETRO_DEVICE_LIGHTGUN,
            "the mouse that was there lost its state: %ld,%ld flags %#x device %d",
            (long)g_mice[a].x, (long)g_mice[a].y, g_mice[a].flags, g_mice[a].device);
   if (b >= 0)
   {
      /* the device type that makes a report move by its delta */
      g_mice[b].device = RETRO_DEVICE_MOUSE;
      move(M(2), 7, -2);
      winraw_poll(wr);
      b = index_of(M(2));
      CHECK(wr->mice[b].dlt_x == 7 && wr->mice[b].dlt_y == -2,
            "the new mouse's next report read %d,%d, not 7,-2",
            (int)wr->mice[b].dlt_x, (int)wr->mice[b].dlt_y);
   }
   printf("   ok   a mouse plugged in: in the list after the poll its first report came in, the old one keeps its state\n");

   /* ---- a handle that is not a listed mouse ----------------------- */
   Sleep(1100);
   calls = list_calls;
   move(M(9), 1, 1);
   winraw_poll(wr);
   CHECK(list_calls == calls + 1 && wr->mouse_cnt == 2,
         "an unlisted handle: the list was fetched %u time(s)", list_calls - calls);
   Sleep(1100);
   move(M(9), 1, 1);
   winraw_poll(wr);
   move(M(9), 1, 1);
   winraw_poll(wr);
   CHECK(list_calls == calls + 1,
         "an unlisted handle was asked about again (%u fetches)", list_calls - calls);
   /* no handle: injected input */
   move(NULL, 1, 1);
   winraw_poll(wr);
   CHECK(list_calls == calls + 1, "a report with no handle had the list made again");
   printf("   ok   a handle that is no listed mouse is asked about once; no handle, never\n");

   /* ---- not more than once a second ------------------------------- */
   Sleep(1100);
   fake_mice[2] = M(3);
   fake_mice[3] = M(4);
   fake_mice_n  = 3;
   calls        = list_calls;
   move(M(3), 1, 0);
   winraw_poll(wr);
   fake_mice_n  = 4;
   move(M(4), 1, 0);
   winraw_poll(wr);
   CHECK(list_calls == calls + 1 && wr->mouse_cnt == 3,
         "two new mice in quick succession: %u fetches, %u in the list",
         list_calls - calls, wr->mouse_cnt);
   Sleep(1100);
   move(M(4), 1, 0);
   winraw_poll(wr);
   CHECK(wr->mouse_cnt == 4, "the fourth mouse was not picked up after the second had passed");
   printf("   ok   the list is made again at most once a second, and the mouse that had to wait is picked up after\n");

   /* ---- the Windows-key setting ----------------------------------- */
   CHECK(!wr->nowinkey, "registered with Windows keys off at the start");
   stub_settings.bools.input_nowinkey_enable = true;
   winraw_poll(wr);
   CHECK(wr->nowinkey, "the setting switched on was not registered by the next poll");
   stub_settings.bools.input_nowinkey_enable = false;
   winraw_poll(wr);
   CHECK(!wr->nowinkey, "the setting switched off was not registered by the next poll");
   printf("   ok   the Windows-key setting is registered by the poll after it changes\n");

   winraw_free(wr);

   if (failures)
   {
      printf("FAIL winraw_mouse_hotplug_test: %u\n", failures);
      return 1;
   }
   printf("PASS winraw_mouse_hotplug_test\n");
   return 0;
}

/* winraw: the keyboards are listed.
 *
 * The raw input driver reads every keyboard as one. It now also keeps
 * the list of them and gives the frontend their names, for
 * Information > Input Information - and, later, so that a port can be
 * given one of them.
 *
 * The real driver, cross-built with mingw-w64 and run under Wine, with
 * the list of raw input devices it asks Windows for replaced by one
 * the test controls. The handles are made up, so no name can be found
 * for them; what is held here is the list, not the names.
 *
 * Checked here:
 *
 * - the keyboards in the device list, and only those, are listed,
 *   oldest first, and the frontend is given a name for each after its
 *   old list is cleared;
 * - a key from a keyboard that is not in the list has the list made
 *   again at the end of that poll, with the new keyboard in it;
 * - a key with no handle - an injected one - never does, and a handle
 *   that is still no listed keyboard after that is not asked about
 *   again;
 * - the list is not made again within a second of the last time;
 * - a keyboard that has gone is out of the list and out of the
 *   frontend's names the next time the list is made - and it is made
 *   again at the poll after Windows says devices came or went, which
 *   is how an unplugged keyboard, which sends no last key, leaves it;
 * - all of them still feed the one key state. */
#include <stdio.h>
#include <stdlib.h>

#include <retro_atomic.h>

#define WIN32_LEAN_AND_MEAN
#include <windows.h>

/* ---- the list of devices, as the test says ---------------------- */
static struct { HANDLE h; DWORD type; } fake_devs[16];
static unsigned fake_devs_n;
static unsigned list_calls;   /* times the list was fetched */

static UINT WINAPI fake_GetRawInputDeviceList(PRAWINPUTDEVICELIST list,
      PUINT count, UINT size)
{
   unsigned i;
   (void)size;
   if (!list)
   {
      *count = fake_devs_n;
      return 0;
   }
   list_calls++;
   for (i = 0; i < fake_devs_n && i < *count; i++)
   {
      list[i].hDevice = fake_devs[i].h;
      list[i].dwType  = fake_devs[i].type;
   }
   return fake_devs_n;
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
struct menu_state *menu_state_get_ptr(void) { return &stub_menu; }
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
void input_config_set_mouse_display_name(unsigned port, const char *name)
{ (void)port; (void)name; }
/* the frontend's list of keyboard names, as the driver sets it */
static char     kb_names[16][64];
static unsigned kb_clears;
void input_config_set_keyboard_display_name(unsigned idx, const char *name)
{
   if (idx < 16)
      strlcpy(kb_names[idx], name, sizeof(kb_names[idx]));
}
void input_config_clear_keyboard_display_names(void)
{
   memset(kb_names, 0, sizeof(kb_names));
   kb_clears++;
}
unsigned input_driver_lightgun_id_convert(unsigned id) { return id; }
bool input_driver_pointer_is_offscreen(int16_t x, int16_t y)
{ (void)x; (void)y; return false; }
void input_keymaps_init_keyboard_lut(const struct rarch_key_map *map)
{ (void)map; }
void joypad_driver_reinit(void *data, const char *name)
{ (void)data; (void)name; }
/* A task is run to its end where it is pushed: the names are looked up
 * and set before the push returns. */
static retro_task_t the_task;
retro_task_t *task_init(void)
{
   memset(&the_task, 0, sizeof(the_task));
   return &the_task;
}
bool task_queue_push(retro_task_t *task)
{
   task->handler(task);
   if (task->callback)
      task->callback(task, NULL, NULL, NULL);
   if (task->cleanup)
      task->cleanup(task);
   return true;
}
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
bool win32_hotplug_due(void) { return true; }




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

#define K(n) ((HANDLE)(uintptr_t)(0x200 + (n)))
#define M(n) ((HANDLE)(uintptr_t)(0x100 + (n)))

static winraw_input_t *wr;

/* the device list: newest first, as Windows gives it */
static void devices(unsigned n, ...)
{
   va_list ap;
   unsigned i;
   va_start(ap, n);
   for (i = 0; i < n; i++)
   {
      fake_devs[i].h    = va_arg(ap, HANDLE);
      fake_devs[i].type = ((uintptr_t)fake_devs[i].h >= 0x200)
         ? RIM_TYPEKEYBOARD : RIM_TYPEMOUSE;
   }
   va_end(ap);
   fake_devs_n = n;
}

static void key(HANDLE kb, unsigned scancode, bool down)
{
   RAWKEYBOARD k;
   memset(&k, 0, sizeof(k));
   k.MakeCode = (USHORT)scancode;
   k.Flags    = down ? 0 : RI_KEY_BREAK;
   winraw_take(wr, RIM_TYPEKEYBOARD, kb, &k);
}

static unsigned names(void)
{
   unsigned i, n = 0;
   for (i = 0; i < 16; i++)
      if (kb_names[i][0])
         n++;
   return n;
}

int main(void)
{
   unsigned calls, clears;

   /* ---- two keyboards and a mouse when the driver starts --------- */
   devices(3, K(2), M(1), K(1));
   wr = (winraw_input_t*)winraw_init("null");
   CHECK(wr && wr->poll_drain, "the driver did not start read by the poll");
   if (!wr)
      return 1;
   winraw_focus = true;
   winraw_poll(wr);
   CHECK(wr->kb_cnt == 2 && wr->kbs[0] == K(1) && wr->kbs[1] == K(2),
         "start: %u keyboards listed, want the two, oldest first", wr->kb_cnt);
   CHECK(wr->mouse_cnt == 1, "start: %u mice, want 1", wr->mouse_cnt);
   CHECK(names() == 2 && kb_names[0][0] && kb_names[1][0] && kb_clears >= 1,
         "start: the frontend has %u keyboard names, want 2", names());
   printf("   ok   start: the two keyboards are listed, oldest first, and the mouse is not one of them\n");

   /* both feed the one key state */
   key(K(1), 0x1E, true);
   key(K(2), 0x30, true);
   winraw_poll(wr);
   CHECK(wr->kb_keys[0x1E] && wr->kb_keys[0x30], "the two keyboards' keys are not both down");
   key(K(1), 0x1E, false);
   key(K(2), 0x30, false);
   winraw_poll(wr);
   CHECK(!wr->kb_keys[0x1E] && !wr->kb_keys[0x30], "the two keyboards' keys are not both up");
   printf("   ok   both feed the one key state\n");

   /* ---- a third keyboard is plugged in --------------------------- */
   devices(4, K(3), K(2), M(1), K(1));
   calls  = list_calls;
   clears = kb_clears;
   key(K(3), 0x20, true);
   CHECK(wr->kb_cnt == 2 && list_calls == calls,
         "the list was made again before the poll's end");
   winraw_poll(wr);
   CHECK(wr->kb_cnt == 3 && wr->kbs[2] == K(3) && wr->kbs[0] == K(1),
         "a key from a new keyboard: %u listed, want 3 with the new one last", wr->kb_cnt);
   CHECK(names() == 3 && kb_clears == clears + 1,
         "the frontend has %u keyboard names after it, want 3", names());
   CHECK(wr->kb_keys[0x20], "the new keyboard's first key was lost");
   key(K(3), 0x20, false);
   winraw_poll(wr);
   printf("   ok   a keyboard plugged in: in the list after the poll its first key came in, and that key was not lost\n");

   /* ---- keys that are no keyboard's ------------------------------ */
   Sleep(1100);
   calls = list_calls;
   key(NULL, 0x21, true);
   key(NULL, 0x21, false);
   winraw_poll(wr);
   CHECK(list_calls == calls, "an injected key had the list made again");
   key(K(9), 0x22, true);
   key(K(9), 0x22, false);
   winraw_poll(wr);
   CHECK(list_calls == calls + 1 && wr->kb_cnt == 3,
         "a key from an unlisted handle: the list fetched %u time(s), want 1", list_calls - calls);
   Sleep(1100);
   calls = list_calls;
   key(K(9), 0x22, true);
   key(K(9), 0x22, false);
   winraw_poll(wr);
   CHECK(list_calls == calls, "a handle that is no listed keyboard was asked about again");
   printf("   ok   an injected key never has the list made again; a handle that is no listed keyboard is asked about once\n");

   /* ---- not within a second of the last time --------------------- */
   Sleep(1100);
   devices(5, K(4), K(3), K(2), M(1), K(1));
   calls = list_calls;
   key(K(4), 0x23, true);
   key(K(4), 0x23, false);
   winraw_poll(wr);
   CHECK(list_calls == calls + 1 && wr->kb_cnt == 4, "the fourth keyboard was not listed");
   devices(6, K(5), K(4), K(3), K(2), M(1), K(1));
   calls = list_calls;
   key(K(5), 0x24, true);
   key(K(5), 0x24, false);
   winraw_poll(wr);
   CHECK(list_calls == calls && wr->kb_cnt == 4,
         "the list was made again within a second of the last time");
   Sleep(1100);
   key(K(5), 0x24, true);
   key(K(5), 0x24, false);
   winraw_poll(wr);
   CHECK(wr->kb_cnt == 5, "the keyboard that had to wait was not picked up after: %u listed", wr->kb_cnt);
   printf("   ok   the list is made again at most once a second, and the keyboard that had to wait is picked up after\n");

   /* ---- two keyboards have gone ---------------------------------- */
   Sleep(1100);
   devices(5, K(6), K(5), K(3), M(1), K(1));
   key(K(6), 0x25, true);
   key(K(6), 0x25, false);
   winraw_poll(wr);
   CHECK(wr->kb_cnt == 4 && wr->kbs[0] == K(1) && wr->kbs[1] == K(3)
         && wr->kbs[2] == K(5) && wr->kbs[3] == K(6),
         "after two left and one came: %u listed, want 1, 3, 5 and 6", wr->kb_cnt);
   CHECK(names() == 4, "the frontend has %u keyboard names, want 4: a keyboard that left is still named", names());
   printf("   ok   keyboards that have gone are out of the list, and of the frontend's names, when it is next made\n");

   /* ---- one more is unplugged, and Windows says devices changed -- */
   devices(4, K(6), K(5), M(1), K(1));
   calls = list_calls;
   winraw_poll(wr);
   CHECK(list_calls == calls && wr->kb_cnt == 4,
         "the list was made again with nothing to say it changed");
   /* the hotplug timer's message, as the window procedure hands it on */
   winraw_handle_message(WM_TIMER, WIN32_HOTPLUG_TIMER_ID, 0);
   CHECK(list_calls == calls, "the list was made again from the window's message, not by the poll");
   winraw_poll(wr);
   CHECK(wr->kb_cnt == 3 && wr->kbs[0] == K(1) && wr->kbs[1] == K(5) && wr->kbs[2] == K(6)
         && names() == 3,
         "after a keyboard was unplugged: %u listed and %u named, want 3", wr->kb_cnt, names());
   printf("   ok   a keyboard unplugged: out of the list at the poll after Windows says devices changed\n");

   winraw_free(wr);
   if (failures)
   {
      printf("FAIL winraw_keyboards_test: %u\n", failures);
      return 1;
   }
   printf("PASS winraw_keyboards_test\n");
   return 0;
}

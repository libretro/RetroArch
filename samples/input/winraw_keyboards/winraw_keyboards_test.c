/* winraw: the keyboards are listed, and a port can be given one.
 *
 * (What Windows says of each device - its path, the physical device
 * it is part of, what it hangs off - is the test's to say as well:
 * see dev_says[]. A device the test says nothing of is a USB keyboard
 * or mouse of its own.)
 *
 * The raw input driver reads every keyboard as one. It also keeps the
 * list of them and gives the frontend their names, for Information >
 * Input Information, and keeps each one's own keys, so that a port
 * whose Keyboard Index names a keyboard reads its key binds and its
 * keyboard from that one alone.
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
 * - all of them still feed the one key state;
 * - two ports given a keyboard each: the same key bound on both is
 *   pressed for the port whose keyboard it was pressed on and not for
 *   the other, as a bind, in the bind mask and as the port's
 *   keyboard; a port with no keyboard of its own reads both;
 * - a hotkey answers to every keyboard, whatever its port was given;
 * - with the menu open every port reads every keyboard;
 * - a port given a keyboard that is not there reads every keyboard;
 * - a key held on a keyboard when it is unplugged is let go - in the
 *   one key state and with a key-up event - unless another keyboard
 *   holds it too;
 * - what Windows says of a device is read right: USB and Bluetooth
 *   ids from the path, the machine's own container taken for none,
 *   the boot keyboard interface;
 * - the raw input keyboards that are parts of one device are listed
 *   as one keyboard, and a port given it reads a key from any part,
 *   down until every part has let go;
 * - the terminal server's keyboard is not listed, and neither is one
 *   a program made;
 * - a mouse whose buttons send keys is not listed, and still feeds
 *   the ports that read every keyboard; a keyboard that is also a
 *   mouse is listed, and so is one that is also a mouse where it is
 *   not known what its keyboard part is;
 * - a keyboard with no name of its own goes by what Windows calls
 *   it, and the frontend is given each listed keyboard's ids;
 * - the mice on the desk are numbered first, in the order they had;
 *   after them, and left out of Input Information, come a keyboard's
 *   pointer part, a mouse a program made and the terminal server's.
 *   The frontend is told of each which device it is part of, its
 *   ids, and a name of last resort. */
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

/* ---- what Windows says of a device, as the test says ------------- */
#include <string.h>
static struct
{
   HANDLE h;
   const char *path, *container, *compat, *desc, *parents;
} dev_says[24];
static unsigned dev_says_n;

static void fake_device_strings(HANDLE hnd, char *path,
      char *container, char *compat, char *desc, char *parents)
{
   unsigned i;
   parents[0] = '\0';
   for (i = 0; i < dev_says_n; i++)
      if (dev_says[i].h == hnd)
      {
         strcpy(path,      dev_says[i].path);
         strcpy(container, dev_says[i].container);
         strcpy(compat,    dev_says[i].compat);
         strcpy(desc,      dev_says[i].desc);
         if (dev_says[i].parents)
            strcpy(parents, dev_says[i].parents);
         return;
      }
   /* one USB device of its own, told by its ids */
   sprintf(path, "\\\\?\\HID#VID_0001&PID_%04X#1&2&3#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}",
         (unsigned)(uintptr_t)hnd);
   container[0] = compat[0] = '\0';
   strcpy(desc, "HID Keyboard Device");
}
#define WINRAW_DEVICE_STRINGS fake_device_strings

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
void RARCH_LOG(const char *fmt, ...) { (void)fmt; }
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }
/* the frontend's list of mice, as the driver sets it */
static char     ms_names[16][64];
static char     ms_device[16][64];
static unsigned ms_vid[16], ms_pid[16];
static bool     ms_hidden[16];
void input_config_set_mouse_display_name(unsigned port, const char *name)
{
   if (port < 16)
      strlcpy(ms_names[port], name, sizeof(ms_names[port]));
}
void input_config_clear_mouse_info(void)
{
   memset(ms_names,  0, sizeof(ms_names));
   memset(ms_device, 0, sizeof(ms_device));
   memset(ms_vid,    0, sizeof(ms_vid));
   memset(ms_pid,    0, sizeof(ms_pid));
   memset(ms_hidden, 0, sizeof(ms_hidden));
}
void input_config_set_mouse_device(unsigned idx, const char *device,
      uint16_t vid, uint16_t pid, bool hidden)
{
   if (idx < 16)
   {
      strlcpy(ms_device[idx], device, sizeof(ms_device[idx]));
      ms_vid[idx]    = vid;
      ms_pid[idx]    = pid;
      ms_hidden[idx] = hidden;
   }
}
/* the frontend's list of keyboard names, as the driver sets it */
static char     kb_names[16][64];
static unsigned kb_clears;
static unsigned kb_vid[16], kb_pid[16];
void input_config_set_keyboard_ids(unsigned idx, uint16_t vid, uint16_t pid)
{
   if (idx < 16)
   {
      kb_vid[idx] = vid;
      kb_pid[idx] = pid;
   }
}
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




/* key-up events, by key */
static unsigned key_ups[RETROK_LAST];
void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device)
{
   (void)character; (void)mod; (void)device;
   if (!down && code < RETROK_LAST)
      key_ups[code]++;
}
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

/* scancodes of the two keys the tests bind */
#define SC_KEY_A 0x1E
#define SC_KEY_F 0x21

static rarch_joypad_info_t joy_info;

/* what a port's bind on RetroPad B reads */
static int pad_b(unsigned port)
{
   return winraw_input_state(wr, NULL, NULL, &joy_info, input_config_binds,
         false, port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_B);
}

static int pad_mask_b(unsigned port)
{
   return (winraw_input_state(wr, NULL, NULL, &joy_info, input_config_binds,
         false, port, RETRO_DEVICE_JOYPAD, 0, RETRO_DEVICE_ID_JOYPAD_MASK)
         >> RETRO_DEVICE_ID_JOYPAD_B) & 1;
}

static int port_key(unsigned port, unsigned rk)
{
   return winraw_input_state(wr, NULL, NULL, &joy_info, input_config_binds,
         false, port, RETRO_DEVICE_KEYBOARD, 0, rk);
}

static int hotkey(unsigned port)
{
   return winraw_input_state(wr, NULL, NULL, &joy_info, input_config_binds,
         false, port, RETRO_DEVICE_JOYPAD, 0, RARCH_FAST_FORWARD_KEY);
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

   /* ---- a port is given a keyboard ------------------------------- */
   /* three keyboards are listed now: 1, 5 and 6. Both ports bind
    * RetroPad B to the same key; the first port also has a hotkey. */
   {
      unsigned p;
      rarch_keysym_lut[RETROK_a] = SC_KEY_A;
      rarch_keysym_lut[RETROK_f] = SC_KEY_F;
      for (p = 0; p < 3; p++)
         input_config_binds[p][RETRO_DEVICE_ID_JOYPAD_B].attr =
            RETRO_KEYBIND_PACK(0, RETROK_a, 1);
      input_config_binds[0][RARCH_FAST_FORWARD_KEY].attr =
         RETRO_KEYBIND_PACK(0, RETROK_f, 1);
      input_config_binds[1][RARCH_FAST_FORWARD_KEY].attr =
         RETRO_KEYBIND_PACK(0, RETROK_f, 1);
   }
   stub_settings.uints.input_keyboard_index[0] = 1; /* K(1) */
   stub_settings.uints.input_keyboard_index[1] = 2; /* K(5) */
   stub_settings.uints.input_keyboard_index[2] = 0; /* all  */

   key(K(1), SC_KEY_A, true);
   winraw_poll(wr);
   CHECK(pad_b(0) && !pad_b(1) && pad_b(2),
         "the key on the first keyboard: ports read %d %d %d, want 1 0 1",
         pad_b(0), pad_b(1), pad_b(2));
   CHECK(pad_mask_b(0) && !pad_mask_b(1) && pad_mask_b(2),
         "the same through the bind mask: %d %d %d, want 1 0 1",
         pad_mask_b(0), pad_mask_b(1), pad_mask_b(2));
   CHECK(port_key(0, RETROK_a) && !port_key(1, RETROK_a) && port_key(2, RETROK_a),
         "the same as the ports' keyboards: %d %d %d, want 1 0 1",
         port_key(0, RETROK_a), port_key(1, RETROK_a), port_key(2, RETROK_a));
   key(K(1), SC_KEY_A, false);
   key(K(5), SC_KEY_A, true);
   winraw_poll(wr);
   CHECK(!pad_b(0) && pad_b(1) && pad_b(2),
         "the key on the second keyboard: ports read %d %d %d, want 0 1 1",
         pad_b(0), pad_b(1), pad_b(2));
   key(K(1), SC_KEY_A, true);
   winraw_poll(wr);
   CHECK(pad_b(0) && pad_b(1), "the key on both keyboards: not down on both ports");
   key(K(1), SC_KEY_A, false);
   winraw_poll(wr);
   CHECK(!pad_b(0) && pad_b(1) && pad_b(2),
         "let go on the first keyboard and still held on the second: ports read %d %d %d, want 0 1 1",
         pad_b(0), pad_b(1), pad_b(2));
   key(K(5), SC_KEY_A, false);
   winraw_poll(wr);
   CHECK(!pad_b(0) && !pad_b(1) && !pad_b(2), "the key let go on both: still down somewhere");
   printf("   ok   two ports with a keyboard each: the same key is each port's own, as a bind, in the mask and as its keyboard; a port with none reads both\n");

   /* a hotkey: every keyboard, whatever the port was given */
   key(K(5), SC_KEY_F, true);
   winraw_poll(wr);
   CHECK(hotkey(0) && hotkey(1),
         "a hotkey pressed on the second keyboard: the first port (given the first keyboard) read %d", hotkey(0));
   key(K(5), SC_KEY_F, false);
   /* an injected key is no keyboard's own: ports with a keyboard do
    * not read it, the rest do */
   key(NULL, SC_KEY_A, true);
   winraw_poll(wr);
   CHECK(!pad_b(0) && !pad_b(1) && pad_b(2),
         "an injected key: ports read %d %d %d, want 0 0 1", pad_b(0), pad_b(1), pad_b(2));
   key(NULL, SC_KEY_A, false);
   winraw_poll(wr);
   printf("   ok   a hotkey answers to every keyboard; an injected key is read by the ports that read them all\n");

   /* the menu open: every port reads every keyboard */
   key(K(5), SC_KEY_A, true);
   winraw_poll(wr);
   stub_menu.flags |= MENU_ST_FLAG_ALIVE;
   CHECK(pad_b(0) && pad_b(1), "with the menu open the first port did not read the second keyboard");
   stub_menu.flags &= ~MENU_ST_FLAG_ALIVE;
   CHECK(!pad_b(0) && pad_b(1), "with the menu closed again the first port still read the second keyboard");
   printf("   ok   with the menu open every port reads every keyboard\n");

   /* a port given a keyboard that is not there reads them all */
   stub_settings.uints.input_keyboard_index[0] = 9;
   CHECK(pad_b(0), "a port given a ninth keyboard, with three there, did not fall back to all of them");
   stub_settings.uints.input_keyboard_index[0] = 1;
   printf("   ok   a port given a keyboard that is not there reads every keyboard\n");

   /* ---- a keyboard kept by what it is ----------------------------- */
   /* (the key is still held on the second keyboard, K(5), alone)
    * The ports are pinned the other way round from their numbers: the
    * pin is what counts. */
   CHECK(stub_pin_listed >= 2 && stub_pin_ident[0][0] && stub_pin_ident[1][0]
         && strcmp(stub_pin_ident[0], stub_pin_ident[1]),
         "the driver did not say what its keyboards are known by: \"%s\" \"%s\"",
         stub_pin_ident[0], stub_pin_ident[1]);
   strcpy(stub_settings.arrays.input_keyboard_device[0], stub_pin_ident[1]);
   strcpy(stub_settings.arrays.input_keyboard_device[1], stub_pin_ident[0]);
   CHECK(pad_b(0) && !pad_b(1),
         "ports pinned to the second and the first keyboard, a key held on the second: they read %d %d, want 1 0",
         pad_b(0), pad_b(1));
   /* the first port pinned to a keyboard that is not plugged in, while
    * the second has its own: it reads none - not the second's */
   strcpy(stub_settings.arrays.input_keyboard_device[0], "dead:beef");
   strcpy(stub_settings.arrays.input_keyboard_device[1], stub_pin_ident[1]);
   CHECK(!pad_b(0) && pad_b(1) && !port_key(0, RETROK_a),
         "a port whose pinned keyboard is away, another port having its own: it read %d (bind) %d (key), want 0 0",
         pad_b(0), port_key(0, RETROK_a));
   /* and with no other port having one, it reads every keyboard */
   stub_settings.arrays.input_keyboard_device[1][0] = '\0';
   stub_settings.uints.input_keyboard_index[1]      = 0;
   CHECK(pad_b(0), "a single port whose pinned keyboard is away read no keyboard: the player has no keys");
   stub_settings.arrays.input_keyboard_device[0][0] = '\0';
   stub_settings.uints.input_keyboard_index[1]      = 2;
   key(K(5), SC_KEY_A, false);
   winraw_poll(wr);
   printf("   ok   a port reads the keyboard it is pinned to, whatever its number; with that one away it reads none while another port has its own, and every keyboard otherwise\n");

   /* ---- unplugged with a key held -------------------------------- */
   key(K(5), SC_KEY_A, true);   /* held on the second keyboard only */
   key(K(5), SC_KEY_F, true);   /* held on the second ...           */
   key(K(1), SC_KEY_F, true);   /* ... and on the first             */
   winraw_poll(wr);
   memset(key_ups, 0, sizeof(key_ups));
   devices(3, K(6), M(1), K(1));
   winraw_handle_message(WM_TIMER, WIN32_HOTPLUG_TIMER_ID, 0);
   winraw_poll(wr);
   CHECK(wr->kb_cnt == 2, "the keyboard was not taken off the list");
   CHECK(!wr->kb_keys[SC_KEY_A] && key_ups[SC_KEY_A] == 1,
         "the key held on the unplugged keyboard: down %d, key-up events %u, want 0 and 1",
         wr->kb_keys[SC_KEY_A], key_ups[SC_KEY_A]);
   CHECK(wr->kb_keys[SC_KEY_F] && key_ups[SC_KEY_F] == 0,
         "the key another keyboard still holds: down %d, key-up events %u, want 1 and 0",
         wr->kb_keys[SC_KEY_F], key_ups[SC_KEY_F]);
   /* the second port's keyboard is now K(6); the first port's is as it was */
   CHECK(!pad_b(1) && !pad_b(2), "a port still reads the key that went with the keyboard");
   key(K(1), SC_KEY_F, false);
   winraw_poll(wr);
   printf("   ok   a key held on a keyboard when it is unplugged is let go, with a key-up, unless another keyboard holds it\n");

   /* ---- what Windows says of a device ---------------------------- */
   {
      winraw_dev_ident_t id;
      winraw_dev_ident("\\\\?\\HID#VID_0951&PID_16E5&MI_01&Col02#8&2d7f0f1&0&0001#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}",
            "{11111111-2222-3333-4444-555555555555}",
            "USB\\Class_03&SubClass_00&Prot_00;USB\\Class_03&SubClass_00;USB\\Class_03",
            "USB Input Device", "", &id);
      CHECK(id.vid == 0x0951 && id.pid == 0x16E5 && id.boot == 0 && !id.remote
            && !strcmp(id.key, "{11111111-2222-3333-4444-555555555555}"),
            "a USB keyboard's second interface: ids %04x:%04x, boot %d, key \"%s\"",
            id.vid, id.pid, id.boot, id.key);
      winraw_dev_ident("\\\\?\\HID#VID_0951&PID_16E5&MI_00#8&2d7f0f1&0&0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}",
            "", "USB\\Class_03&SubClass_01&Prot_01;USB\\Class_03&SubClass_01;USB\\Class_03",
            "USB Input Device", "", &id);
      CHECK(id.boot == 1 && !strcmp(id.key, "0951:16e5"),
            "a boot keyboard with no container: boot %d, key \"%s\"", id.boot, id.key);
      winraw_dev_ident("\\\\?\\HID#{00001124-0000-1000-8000-00805f9b34fb}_VID&0002054c_PID&0df2&Col01#9&1&0&0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}",
            "{AAAAAAAA-0000-0000-0000-000000000001}", "", "", "", &id);
      CHECK(id.vid == 0x054C && id.pid == 0x0DF2 && id.boot == -1,
            "a Bluetooth device: ids %04x:%04x, boot %d", id.vid, id.pid, id.boot);
      winraw_dev_ident("\\\\?\\ACPI#PNP0303#4&1&0#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}",
            "{00000000-0000-0000-FFFF-FFFFFFFFFFFF}", "", "Standard PS/2 Keyboard",
            "ACPI\\PNP0A08\\0;ACPI_HAL\\PNP0C08\\0", &id);
      CHECK(!id.key[0] && id.boot == -1 && !id.remote && !strcmp(id.desc, "Standard PS/2 Keyboard"),
            "a built-in keyboard: key \"%s\" (the machine's own container tells nothing apart), boot %d",
            id.key, id.boot);
      winraw_dev_ident("\\\\?\\Root#RDP_KBD#0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}",
            "", "", "Terminal Server Keyboard Driver", "", &id);
      CHECK(id.remote, "the terminal server's keyboard was not known for it");
      winraw_dev_ident("\\\\?\\HID#CorsairVirtualDevice&Col01#2&1&0&0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}",
            "{CCCCCCCC-0000-0000-0000-000000000001}", "", "Corsair composite virtual input device",
            "ROOT\\SYSTEM\\0001;HTREE\\ROOT\\0", &id);
      CHECK(id.remote, "a device a program made, hanging off ROOT, was taken for one on the desk");
      winraw_dev_ident("\\\\?\\HID#VID_1B1C&PID_1B5A&MI_00#8&1&0&0000#{378de44c-56ef-11d1-bc8c-00a0c91405dd}",
            "", "USB\\Class_03&SubClass_01&Prot_02;USB\\Class_03", "USB Input Device",
            "USB\\VID_1B1C&PID_1B5A&MI_00\\7&1&0&0000;USB\\VID_1B1C&PID_1B5A\\1234", &id);
      CHECK(!id.remote && id.boot_mouse == 1 && id.boot == 0,
            "a USB mouse: made by a program %d, boot mouse %d, boot keyboard %d", id.remote, id.boot_mouse, id.boot);
      printf("   ok   what Windows says of a device: USB and Bluetooth ids, the machine's own container, the boot keyboard, the terminal server's\n");
   }

   /* ---- a desk like a real one ----------------------------------- */
   /* oldest first: the terminal server's keyboard; a keyboard and
    * mouse a program made (a vendor's virtual device); a mouse whose
    * buttons send keys; one keyboard that is two raw input keyboards,
    * and can send pointer events; a keyboard and a mouse on one
    * receiver. */
   {
      static const char kb_class[] = "{884b96c3-56ef-11d1-bc8c-00a0c91405dd}";
      static const char boot[]     = "USB\\Class_03&SubClass_01&Prot_01;USB\\Class_03";
      static const char plain[]    = "USB\\Class_03&SubClass_00&Prot_00;USB\\Class_03";
      unsigned n = 0;
      (void)kb_class;
#define SAYS(hnd, p, c, k, d) do { dev_says[n].h = (hnd); dev_says[n].path = (p); \
      dev_says[n].container = (c); dev_says[n].compat = (k); dev_says[n].desc = (d); \
      dev_says[n].parents = NULL; n++; } while (0)
      SAYS(K(30), "\\\\?\\Root#RDP_KBD#0000#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}", "", "", "Terminal Server Keyboard Driver");
      SAYS(K(31), "\\\\?\\HID#VendorVirtual&Col01#1&2&3#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}", "{VIRT}", "", "Vendor composite virtual input device");
      dev_says[n - 1].parents = "ROOT\\SYSTEM\\0001;HTREE\\ROOT\\0";
      SAYS(M(31), "\\\\?\\HID#VendorVirtual&Col02#1&2&3#{378de44c-56ef-11d1-bc8c-00a0c91405dd}", "{VIRT}", "", "Vendor composite virtual input device");
      dev_says[n - 1].parents = "ROOT\\SYSTEM\\0001;HTREE\\ROOT\\0";
      SAYS(M(40), "\\\\?\\HID#VID_1B1C&PID_1B5A&MI_00#1&2&3#{378de44c-56ef-11d1-bc8c-00a0c91405dd}", "{MOUSE}", "USB\\Class_03&SubClass_01&Prot_02;USB\\Class_03", "USB Input Device");
      SAYS(K(40), "\\\\?\\HID#VID_1B1C&PID_1B5A&MI_01&Col01#1&2&3#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}", "{MOUSE}", plain, "USB Input Device");
      SAYS(K(20), "\\\\?\\HID#VID_0951&PID_16E5&MI_00#1&2&3#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}", "{KEYB}", boot, "USB Input Device");
      SAYS(K(21), "\\\\?\\HID#VID_0951&PID_16E5&MI_01&Col02#1&2&3#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}", "{KEYB}", plain, "USB Input Device");
      SAYS(K(50), "\\\\?\\HID#VID_046D&PID_C52B&MI_00#1&2&3#{884b96c3-56ef-11d1-bc8c-00a0c91405dd}", "{COMBO}", boot, "USB Input Device");
      SAYS(M(50), "\\\\?\\HID#VID_046D&PID_C52B&MI_01&Col01#1&2&3#{378de44c-56ef-11d1-bc8c-00a0c91405dd}", "{COMBO}", "USB\\Class_03&SubClass_01&Prot_02;USB\\Class_03", "USB Input Device");
      /* the keyboard can send pointer events too */
      SAYS(M(20), "\\\\?\\HID#VID_0951&PID_16E5&MI_02&Col01#1&2&3#{378de44c-56ef-11d1-bc8c-00a0c91405dd}", "{KEYB}", plain, "USB Input Device");
      dev_says_n = n;
   }
   stub_settings.uints.input_keyboard_index[0] = 0;
   stub_settings.uints.input_keyboard_index[1] = 0;
   stub_settings.uints.input_keyboard_index[2] = 0;
   /* newest first, as Windows lists them */
   devices(10, M(50), K(50), M(20), K(21), K(20), K(40), M(40), M(31), K(31), K(30));
   winraw_handle_message(WM_TIMER, WIN32_HOTPLUG_TIMER_ID, 0);
   winraw_poll(wr);
   CHECK(wr->kb_cnt == 6, "%u raw input keyboards, want 6", wr->kb_cnt);
   CHECK(wr->kg_cnt == 2 && names() == 2,
         "%u keyboards listed and %u named, want 2: the keyboard, and the one on the receiver",
         wr->kg_cnt, names());
   CHECK(!strcmp(kb_names[0], "USB Input Device"),
         "the keyboard with no name of its own goes by \"%s\"", kb_names[0]);
   CHECK(kb_vid[0] == 0x0951 && kb_pid[0] == 0x16E5 && kb_vid[1] == 0x046D && kb_pid[1] == 0xC52B,
         "the listed keyboards' ids: %04x:%04x and %04x:%04x", kb_vid[0], kb_pid[0], kb_vid[1], kb_pid[1]);
   printf("   ok   one keyboard that is two raw input keyboards is listed once; the terminal server's, one a program made and a mouse that sends keys are not listed; a keyboard sharing a receiver with a mouse is\n");

   /* the first listed keyboard is K(20) and K(21) together */
   stub_settings.uints.input_keyboard_index[0] = 1;
   stub_settings.uints.input_keyboard_index[1] = 2;
   key(K(21), SC_KEY_A, true);
   winraw_poll(wr);
   CHECK(pad_b(0) && !pad_b(1), "a key from the keyboard's second part: ports read %d %d, want 1 0", pad_b(0), pad_b(1));
   key(K(20), SC_KEY_A, true);
   key(K(21), SC_KEY_A, false);
   winraw_poll(wr);
   CHECK(pad_b(0), "let go on one part and held on the other: the port given the keyboard read it up");
   key(K(20), SC_KEY_A, false);
   winraw_poll(wr);
   CHECK(!pad_b(0) && !wr->kb_keys[SC_KEY_A], "let go on both parts: still down");
   /* the mouse's keys: no listed keyboard's, and still every keyboard's */
   key(K(40), SC_KEY_A, true);
   winraw_poll(wr);
   CHECK(!pad_b(0) && !pad_b(1) && pad_b(2),
         "a key the mouse sent: ports read %d %d %d, want 0 0 1", pad_b(0), pad_b(1), pad_b(2));
   key(K(40), SC_KEY_A, false);
   winraw_poll(wr);
   printf("   ok   a port given a keyboard reads a key from any of its parts, down until every part lets go; a key a mouse sends is read by the ports that read every keyboard\n");

   /* ---- the mice ------------------------------------------------- */
   /* the same desk, with a second part of the mouse and the terminal
    * server's mouse. A report from a mouse not in the driver's list
    * has the list made again. */
   {
      unsigned n = dev_says_n;
      RAWMOUSE m;
      SAYS(M(41), "\\\\?\\HID#VID_1B1C&PID_1B5A&MI_01&Col03#1&2&3#{378de44c-56ef-11d1-bc8c-00a0c91405dd}", "{MOUSE}", "", "USB Input Device");
      SAYS(M(60), "\\\\?\\Root#RDP_MOU#0000#{378de44c-56ef-11d1-bc8c-00a0c91405dd}", "", "", "Terminal Server Mouse Driver");
      dev_says_n = n;
      Sleep(1100);
      /* newest first. Oldest first the mice are: the terminal
       * server's, the virtual device's, the mouse, its second part,
       * the keyboard's pointer part, the receiver's mouse */
      devices(12, M(50), K(50), M(20), K(21), K(20), M(41), K(40), M(40), M(31), K(31), K(30), M(60));
      memset(&m, 0, sizeof(m));
      m.lLastX = 1;
      winraw_take(wr, RIM_TYPEMOUSE, M(41), &m);
      winraw_poll(wr);
   }
   CHECK(wr->mouse_cnt == 6, "%u mice in the driver's list, want 6", wr->mouse_cnt);
   CHECK(g_mice[0].hnd == M(40) && g_mice[1].hnd == M(41) && g_mice[2].hnd == M(50),
         "the mice on the desk are not the first three, in the order they had");
   CHECK(g_mice[3].hnd == M(60) && g_mice[4].hnd == M(31) && g_mice[5].hnd == M(20),
         "the rest do not follow, in the order they had");
   CHECK(!ms_hidden[0] && !ms_hidden[1] && !ms_hidden[2]
         && ms_hidden[3] && ms_hidden[4] && ms_hidden[5],
         "which are left out of the list: %d %d %d %d %d %d, want 0 0 0 1 1 1",
         ms_hidden[0], ms_hidden[1], ms_hidden[2], ms_hidden[3], ms_hidden[4], ms_hidden[5]);
   CHECK(!strcmp(ms_device[0], "{MOUSE}") && !strcmp(ms_device[1], "{MOUSE}")
         && strcmp(ms_device[2], ms_device[0]),
         "mice 1 and 2 are one mouse and the third is another: \"%s\" \"%s\" \"%s\"",
         ms_device[0], ms_device[1], ms_device[2]);
   CHECK(ms_vid[0] == 0x1B1C && ms_pid[0] == 0x1B5A && ms_vid[2] == 0x046D,
         "the mice's ids: %04x:%04x and %04x", ms_vid[0], ms_pid[0], ms_vid[2]);
   CHECK(!strcmp(ms_names[4], "Vendor composite virtual input device"),
         "a mouse with no name of its own goes by \"%s\"", ms_names[4]);
   printf("   ok   the mice on the desk come first; a keyboard's pointer part, a program's mouse and the terminal server's follow and are left out of the list\n");

   /* ---- a mouse kept by what it is ------------------------------- */
   /* The driver says what each mouse is known by, in the order of
    * their numbers: the mouse's two parts are one model, told apart
    * as the first and "#2"; the receiver's mouse is another. */
   CHECK(stub_mouse_listed == 6
         && !strcmp(stub_mouse_ident[0], "1b1c:1b5a")
         && !strcmp(stub_mouse_ident[1], "1b1c:1b5a#2")
         && !strncmp(stub_mouse_ident[2], "046d:", 5),
         "what the mice are known by: %u listed, \"%s\" \"%s\" \"%s\"",
         stub_mouse_listed, stub_mouse_ident[0], stub_mouse_ident[1], stub_mouse_ident[2]);
   /* a port with no pin reads by its number, as it always did */
   stub_settings.uints.input_mouse_index[0] = 0;
   stub_settings.uints.input_mouse_index[1] = 1;
   CHECK(input_mouse_port_index(0) == 0 && input_mouse_port_index(1) == 1,
         "ports with no pin read mice %u %u, want their numbers",
         input_mouse_port_index(0), input_mouse_port_index(1));
   /* the second port pinned to the receiver's mouse: it reads that
    * one, whatever its number says */
   strcpy(stub_settings.arrays.input_mouse_device[1], stub_mouse_ident[2]);
   CHECK(input_mouse_port_index(1) == 2, "a port pinned to the third mouse reads mouse %u", input_mouse_port_index(1));
   /* the first port pinned to a mouse that is not plugged in, while
    * the second has its own: it reads none */
   strcpy(stub_settings.arrays.input_mouse_device[0], "dead:beef");
   CHECK(input_mouse_port_index(0) == MAX_INPUT_DEVICES,
         "a port whose pinned mouse is away, another port having its own, reads mouse %u: want none",
         input_mouse_port_index(0));
   stub_settings.arrays.input_mouse_device[0][0] = '\0';
   stub_settings.arrays.input_mouse_device[1][0] = '\0';
   printf("   ok   the driver says what each mouse is known by, in the order of their numbers; a pinned port reads its mouse, and none when it is away and another port has its own\n");

   /* ---- keys while another window is the active one ---------------- */
   {
      unsigned p;
      for (p = 0; p < MAX_USERS; p++)
      {
         stub_settings.uints.input_keyboard_index[p]     = 0;
         stub_settings.arrays.input_keyboard_device[p][0] = '\0';
      }
      key(K(5), SC_KEY_A, false);
      key(K(5), SC_KEY_F, false);
      winraw_poll(wr);

      /* off, as it always was: the keys held when the window stops
       * being the active one are let go */
      stub_settings.bools.input_keyboard_background = false;
      key(K(5), SC_KEY_A, true);
      winraw_poll(wr);
      CHECK(pad_b(0), "the first port does not read its key with the window active");
      winraw_focus = false;
      winraw_poll(wr);
      CHECK(!pad_b(0) && !pad_mask_b(0) && !port_key(0, RETROK_a),
            "with the setting off a key is read while another window is active");
      winraw_focus = true;
      winraw_poll(wr);

      /* on: content reads the keys; hotkeys do not, and nothing does
       * while the menu is up */
      stub_settings.bools.input_keyboard_background = true;
      winraw_poll(wr);
      CHECK(wr->kb_background, "the driver did not take the setting");
      key(K(5), SC_KEY_A, true);
      key(K(5), SC_KEY_F, true);
      winraw_focus = false;
      winraw_poll(wr);
      CHECK(pad_b(0) && pad_mask_b(0) && port_key(0, RETROK_a),
            "with the setting on content does not read a key while another window is active");
      CHECK(!hotkey(0), "a hotkey answered to a key pressed while another window was active");
      stub_menu.flags |= MENU_ST_FLAG_ALIVE;
      CHECK(!pad_b(0) && !pad_mask_b(0) && !port_key(0, RETROK_a),
            "the open menu read a key pressed while another window was active");
      stub_menu.flags &= ~MENU_ST_FLAG_ALIVE;
      /* the window active again: everything answers */
      winraw_focus = true;
      winraw_poll(wr);
      CHECK(pad_b(0) && hotkey(0), "with the window active again a key or a hotkey is not read");
      /* turned off while another window is active: the keys are let go */
      winraw_focus = false;
      winraw_poll(wr);
      stub_settings.bools.input_keyboard_background = false;
      winraw_poll(wr); /* the setting is taken at the end of a poll */
      winraw_poll(wr);
      CHECK(!wr->kb_keys[SC_KEY_A],
            "turning the setting off with another window active left a key held");
      winraw_focus = true;
      winraw_poll(wr);
      printf("   ok   with Background Keyboard Input on, content reads keys while another window is active; hotkeys and the menu do not; off, nothing does\n");
   }

   winraw_free(wr);
   if (failures)
   {
      printf("FAIL winraw_keyboards_test: %u\n", failures);
      return 1;
   }
   printf("PASS winraw_keyboards_test\n");
   return 0;
}

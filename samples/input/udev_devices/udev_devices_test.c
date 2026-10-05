/* udev input: which devices are keyboards and which are mice, what a
 * port reads of them, and a port's keyboard and mouse kept by what
 * they are.
 *
 * The udev driver lists the keyboards and mice on the desk, numbers
 * the real mice first, and gives a port the one keyboard or mouse it
 * was given. All of that went in "compiled only": it needs input
 * devices under /dev/input, which a build machine does not have and
 * nothing here can make.
 *
 * It does not need them to be real. What the driver knows of a device
 * - its name and ids, which physical device it is part of, whether it
 * has a keyboard's keys or sends pointer events, what its USB
 * interface says - is a structure in memory, filled in when a device
 * is opened. The driver is included whole, with the frontend stood in
 * for, and handed a desk of such structures: its listing, its
 * numbering and its reading are then the real code's.
 *
 * The desk: a keyboard that is three event devices (its keys, its
 * media keys, and a pointer its macros can move), a mouse that is two
 * (the mouse, and keys its buttons can send), a power button, and a
 * second keyboard.
 *
 * Checked here:
 *
 * - two keyboards are listed, by name and ids, oldest first; the
 *   mouse's keys and the power button are no keyboard;
 * - the mouse is mouse 1; the keyboard's pointer part comes after it
 *   and is marked as not a mouse;
 * - the driver says what each keyboard and mouse is known by, in the
 *   order of their numbers, for a port to be pinned to;
 * - a port given a keyboard reads a key held on any part of it and not
 *   one held on the other keyboard; a port given none reads every
 *   keyboard; a port whose own is away while another has its own
 *   reads no keyboard;
 * - a port reads the mouse the frontend says it reads, and none when
 *   it says none. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <boolean.h>

#include "input/drivers/udev_input.c"

/* ---- The frontend, as far as the driver links against it ------------ */

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }

static settings_t stub_settings;
settings_t *config_get_ptr(void) { return &stub_settings; }
/* the settings a driver asks for by name, read from the ones above */
#include "../input_config_stubs.h"

struct retro_keybind input_config_binds[MAX_USERS][RARCH_BIND_LIST_END];
struct retro_keybind input_autoconf_binds[MAX_USERS][RARCH_BIND_LIST_END];
enum retro_key rarch_keysym_lut[RETROK_LAST];
const struct rarch_key_map rarch_key_map_linux[] = { { 0, RETROK_UNKNOWN } };
void input_keymaps_init_keyboard_lut(const struct rarch_key_map *map) { (void)map; }
enum retro_key input_keymaps_translate_keysym_to_rk(unsigned sym) { (void)sym; return RETROK_UNKNOWN; }
void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device)
{ (void)down; (void)code; (void)character; (void)mod; (void)device; }
unsigned input_driver_lightgun_id_convert(unsigned id) { return id; }
bool input_driver_pointer_is_offscreen(int16_t x, int16_t y) { (void)x; (void)y; return false; }

bool linux_terminal_disable_input(void) { return true; }
bool linux_terminal_grab_stdin(void *data) { (void)data; return true; }
void linux_terminal_restore_input(void) { }
linux_illuminance_sensor_t *linux_open_illuminance_sensor(unsigned rate) { (void)rate; return NULL; }
void linux_close_illuminance_sensor(linux_illuminance_sensor_t *s) { (void)s; }
float linux_get_illuminance_reading(const linux_illuminance_sensor_t *s) { (void)s; return 0.0f; }
void linux_set_illuminance_sensor_rate(linux_illuminance_sensor_t *s, unsigned rate) { (void)s; (void)rate; }

enum rarch_display_type video_driver_display_type_get(void) { return RARCH_DISPLAY_NONE; }
bool video_driver_has_focus(void) { return true; }
bool video_driver_get_viewport_info(struct video_viewport *vp) { (void)vp; return false; }
bool video_driver_translate_coord_viewport(struct video_viewport *vp,
      int mouse_x, int mouse_y, int16_t *res_x, int16_t *res_y,
      int16_t *res_screen_x, int16_t *res_screen_y, bool report_oob)
{
   (void)vp; (void)mouse_x; (void)mouse_y; (void)res_x; (void)res_y;
   (void)res_screen_x; (void)res_screen_y; (void)report_oob;
   return false;
}

/* what the driver lists: recorded as the menu would be given it */
static char     kb_name[MAX_INPUT_DEVICES][64];
static unsigned kb_vid[MAX_INPUT_DEVICES], kb_pid[MAX_INPUT_DEVICES];
static char     ms_name[MAX_INPUT_DEVICES][64];
static bool     ms_hidden[MAX_INPUT_DEVICES];
static unsigned ms_vid[MAX_INPUT_DEVICES];

void input_config_clear_keyboard_display_names(void)
{
   memset(kb_name, 0, sizeof(kb_name));
   memset(kb_vid, 0, sizeof(kb_vid));
   memset(kb_pid, 0, sizeof(kb_pid));
}
void input_config_set_keyboard_display_name(unsigned idx, const char *name)
{
   if (idx < MAX_INPUT_DEVICES)
      strlcpy(kb_name[idx], name ? name : "", sizeof(kb_name[idx]));
}
void input_config_set_keyboard_ids(unsigned idx, uint16_t vid, uint16_t pid)
{
   if (idx < MAX_INPUT_DEVICES)
   {
      kb_vid[idx] = vid;
      kb_pid[idx] = pid;
   }
}
void input_config_set_mouse_display_name(unsigned port, const char *name)
{
   if (port < MAX_INPUT_DEVICES)
      strlcpy(ms_name[port], name ? name : "", sizeof(ms_name[port]));
}
void input_config_set_mouse_device(unsigned idx, const char *device,
      uint16_t vid, uint16_t pid, bool hidden)
{
   (void)device; (void)pid;
   if (idx < MAX_INPUT_DEVICES)
   {
      ms_vid[idx]    = vid;
      ms_hidden[idx] = hidden;
   }
}

/* what the driver says its keyboards and mice are known by, and what
 * the frontend then tells a port to read: set by the test */
static char     pin_kb[MAX_INPUT_DEVICES][64], pin_ms[MAX_INPUT_DEVICES][64];
static unsigned pin_kb_n, pin_ms_n;
static int      port_keyboard[MAX_USERS];
static unsigned port_mouse[MAX_USERS];

void input_keyboard_pins_set_devices(const char (*base)[64], unsigned n)
{
   memset(pin_kb, 0, sizeof(pin_kb));
   pin_kb_n = n;
   if (n)
      memcpy(pin_kb, base, n * sizeof(pin_kb[0]));
}
void input_mouse_pins_set_devices(const char (*base)[64], unsigned n)
{
   memset(pin_ms, 0, sizeof(pin_ms));
   pin_ms_n = n;
   if (n)
      memcpy(pin_ms, base, n * sizeof(pin_ms[0]));
}
int input_keyboard_port_choice(unsigned port) { return port_keyboard[port]; }
unsigned input_mouse_port_index(unsigned port) { return port_mouse[port]; }

/* ---- The desk ------------------------------------------------------- */

static unsigned failures;

#define CHECK(cond, ...) do { \
   if (!(cond)) { printf("   FAIL "); printf(__VA_ARGS__); printf("\n"); failures++; } \
} while (0)

#define DESK 7
static udev_input_device_t desk[DESK];
static udev_input_device_t *desk_list[DESK];

static void device(unsigned i, enum udev_input_dev_type type,
      const char *name, unsigned vid, unsigned pid,
      const char *key, bool keyboard, int boot, int boot_mouse)
{
   udev_input_device_t *d = &desk[i];
   memset(d, 0, sizeof(*d));
   d->type             = type;
   d->vid              = (uint16_t)vid;
   d->pid              = (uint16_t)pid;
   strlcpy(d->ident, name, sizeof(d->ident));
   snprintf(d->devnode, sizeof(d->devnode), "/dev/input/event%u", i);
   strlcpy(d->kbdev.key, key, sizeof(d->kbdev.key));
   d->kbdev.keyboard   = keyboard;
   d->kbdev.pointer    = (type != UDEV_INPUT_KEYBOARD);
   d->kbdev.boot       = (int8_t)boot;
   d->kbdev.boot_mouse = (int8_t)boot_mouse;
   d->kbdev.group      = INPUT_KBDEV_NONE;
   desk_list[i]        = d;
}

static void hold(unsigned i, unsigned code, bool down)
{
   if (down)
      BIT_SET(desk[i].keys, code);
   else
      BIT_CLEAR(desk[i].keys, code);
}

int main(void)
{
   udev_input_t udev;
   unsigned i;

   memset(&udev, 0, sizeof(udev));
   rarch_keysym_lut[RETROK_a] = KEY_A;

   /*      type                 name        ids             part of     keys   boot kbd, mouse */
   device(0, UDEV_INPUT_KEYBOARD, "HyperX",   0x03f0, 0x098f, "usb-1.3", true,   1, -1);
   device(1, UDEV_INPUT_KEYBOARD, "HyperX",   0x03f0, 0x098f, "usb-1.3", false,  0, -1);
   device(2, UDEV_INPUT_MOUSE,    "HyperX",   0x03f0, 0x098f, "usb-1.3", false, -1,  0);
   device(3, UDEV_INPUT_MOUSE,    "Corsair",  0x1b1c, 0x1b5a, "usb-1.4", false, -1,  1);
   device(4, UDEV_INPUT_KEYBOARD, "Corsair",  0x1b1c, 0x1b5a, "usb-1.4", true,   0, -1);
   device(5, UDEV_INPUT_KEYBOARD, "Power Button", 0, 0,       "",        false, -1, -1);
   device(6, UDEV_INPUT_KEYBOARD, "Logitech", 0x046d, 0xc31c, "usb-2.1", true,   1, -1);
   udev.devices     = desk_list;
   udev.num_devices = DESK;

   udev_input_list_pointers(&udev);
   udev_input_list_keyboards(&udev);

   /* ---- the keyboards --------------------------------------------- */
   CHECK(!strcmp(kb_name[0], "HyperX") && !strcmp(kb_name[1], "Logitech") && !kb_name[2][0],
         "keyboards listed: \"%s\" \"%s\" \"%s\", want the HyperX, the Logitech and no third",
         kb_name[0], kb_name[1], kb_name[2]);
   CHECK(kb_vid[0] == 0x03f0 && kb_pid[0] == 0x098f && kb_vid[1] == 0x046d,
         "the keyboards' ids: %04x:%04x and %04x", kb_vid[0], kb_pid[0], kb_vid[1]);
   CHECK(udev.keyboards[0] == 0 && udev.keyboards[1] == 6 && udev.keyboards[2] < 0,
         "the keyboards' devices: %d %d %d, want 0 6 and none",
         (int)udev.keyboards[0], (int)udev.keyboards[1], (int)udev.keyboards[2]);
   CHECK(desk[0].kbdev.group == 0 && desk[1].kbdev.group == 0 && desk[6].kbdev.group == 1,
         "the keyboard's two key-sending parts are not one keyboard, or the second keyboard is not the second");
   CHECK(desk[4].kbdev.group == INPUT_KBDEV_NONE && desk[5].kbdev.group == INPUT_KBDEV_NONE,
         "the mouse's keys or the power button is part of a listed keyboard");
   printf("   ok   two keyboards are listed, by name and ids; a mouse's keys and the power button are no keyboard\n");

   /* ---- the mice -------------------------------------------------- */
   CHECK(udev.pointers[0] == 3 && udev.pointers[1] == 2 && udev.pointers[2] < 0,
         "the mice's devices: %d %d %d, want the mouse (3), then the keyboard's pointer part (2), and no third",
         (int)udev.pointers[0], (int)udev.pointers[1], (int)udev.pointers[2]);
   CHECK(!strcmp(ms_name[0], "Corsair") && !ms_hidden[0] && ms_vid[0] == 0x1b1c,
         "mouse 1 is \"%s\" (%04x), left out: %d; want the Corsair, listed", ms_name[0], ms_vid[0], ms_hidden[0]);
   CHECK(!strcmp(ms_name[1], "HyperX") && ms_hidden[1],
         "mouse 2 is \"%s\", left out: %d; want the keyboard's pointer part, left out", ms_name[1], ms_hidden[1]);
   printf("   ok   the mouse is mouse 1; the keyboard's pointer part comes after it and is not listed as a mouse\n");

   /* ---- what they are known by ------------------------------------ */
   CHECK(pin_kb_n == 2 && !strcmp(pin_kb[0], "03f0:098f") && !strcmp(pin_kb[1], "046d:c31c"),
         "keyboards known by: %u, \"%s\" \"%s\"", pin_kb_n, pin_kb[0], pin_kb[1]);
   CHECK(pin_ms_n == 2 && !strcmp(pin_ms[0], "1b1c:1b5a") && !strcmp(pin_ms[1], "03f0:098f"),
         "mice known by: %u, \"%s\" \"%s\"", pin_ms_n, pin_ms[0], pin_ms[1]);
   printf("   ok   each keyboard and mouse is told to the frontend by its ids, in the order of their numbers\n");

   /* ---- what a port reads of the keyboards ------------------------ */
   port_keyboard[0] = 1;   /* the HyperX   */
   port_keyboard[1] = 2;   /* the Logitech */
   port_keyboard[2] = 0;   /* every one    */
   port_keyboard[3] = -1;  /* none: its own is away, others have theirs */
   /* A held on the HyperX's second part alone */
   hold(1, KEY_A, true);
   BIT_SET(udev.state, KEY_A);
   CHECK( udev_port_key_pressed(&udev, udev_port_keys(&udev, 0), RETROK_a)
         && !udev_port_key_pressed(&udev, udev_port_keys(&udev, 1), RETROK_a)
         &&  udev_port_key_pressed(&udev, udev_port_keys(&udev, 2), RETROK_a)
         && !udev_port_key_pressed(&udev, udev_port_keys(&udev, 3), RETROK_a),
         "a key held on the first keyboard's second part: ports read %d %d %d %d, want 1 0 1 0",
         udev_port_key_pressed(&udev, udev_port_keys(&udev, 0), RETROK_a),
         udev_port_key_pressed(&udev, udev_port_keys(&udev, 1), RETROK_a),
         udev_port_key_pressed(&udev, udev_port_keys(&udev, 2), RETROK_a),
         udev_port_key_pressed(&udev, udev_port_keys(&udev, 3), RETROK_a));
   hold(1, KEY_A, false);
   /* A held on the Logitech alone */
   hold(6, KEY_A, true);
   CHECK(!udev_port_key_pressed(&udev, udev_port_keys(&udev, 0), RETROK_a)
         &&  udev_port_key_pressed(&udev, udev_port_keys(&udev, 1), RETROK_a),
         "a key held on the second keyboard: ports read %d %d, want 0 1",
         udev_port_key_pressed(&udev, udev_port_keys(&udev, 0), RETROK_a),
         udev_port_key_pressed(&udev, udev_port_keys(&udev, 1), RETROK_a));
   hold(6, KEY_A, false);
   /* A sent by the mouse's buttons: no listed keyboard's, and still
    * every keyboard's */
   hold(4, KEY_A, true);
   CHECK(!udev_port_key_pressed(&udev, udev_port_keys(&udev, 0), RETROK_a)
         && !udev_port_key_pressed(&udev, udev_port_keys(&udev, 1), RETROK_a)
         &&  udev_port_key_pressed(&udev, udev_port_keys(&udev, 2), RETROK_a),
         "a key the mouse sent was read by a port given a keyboard, or not by the port that reads every one");
   hold(4, KEY_A, false);
   BIT_CLEAR(udev.state, KEY_A);
   /* a number past the list is no keyboard */
   port_keyboard[0] = 9;
   CHECK(udev_port_keys(&udev, 0) == -2, "a port told to read a ninth keyboard reads %d, want none", udev_port_keys(&udev, 0));
   printf("   ok   a port reads a key held on any part of its keyboard and not the other's; with none given it reads every keyboard, and with its own away none\n");

   /* ---- what a port reads of the mice ----------------------------- */
   port_mouse[0] = 0;
   port_mouse[1] = 1;
   port_mouse[2] = MAX_INPUT_DEVICES;   /* none */
   port_mouse[3] = 5;                   /* a number with no mouse */
   CHECK(udev_get_mouse(&udev, 0) == &desk[3].mouse, "the first port does not read the mouse");
   CHECK(udev_get_mouse(&udev, 1) == &desk[2].mouse, "a port told to read mouse 2 does not read the keyboard's pointer part");
   CHECK(!udev_get_mouse(&udev, 2) && !udev_get_mouse(&udev, 3), "a port told to read no mouse, or one that is not there, reads one");
   printf("   ok   a port reads the mouse the frontend says it reads, and none where it says none\n");

   /* ---- the mouse unplugged --------------------------------------- */
   /* both of its event devices go: the mouse and the keys */
   for (i = 3; i + 2 < DESK; i++)
      desk_list[i] = desk_list[i + 2];
   udev.num_devices = DESK - 2;
   udev_input_list_pointers(&udev);
   udev_input_list_keyboards(&udev);
   CHECK(pin_ms_n == 1 && ms_hidden[0] && udev.pointers[0] == 2,
         "with the mouse unplugged: %u pointer(s), the first left out: %d", pin_ms_n, ms_hidden[0]);
   CHECK(pin_kb_n == 2 && !strcmp(kb_name[0], "HyperX") && !strcmp(kb_name[1], "Logitech"),
         "with the mouse unplugged the keyboards are no longer the same two");
   printf("   ok   with the mouse unplugged the lists are made again: no mouse is listed, the keyboards are as they were\n");

   if (failures)
   {
      printf("FAIL udev_devices_test: %u\n", failures);
      return 1;
   }
   printf("PASS udev_devices_test\n");
   return 0;
}

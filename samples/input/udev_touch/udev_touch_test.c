/* udev input: a touchscreen read as the pointer.
 *
 * The udev driver turns a touchscreen's evdev contacts into the
 * pointer the menu and cores read. A runner has no touchscreen, but
 * what the driver asks of one is a handful of ioctls and a stream of
 * input_event frames, so the driver is included whole, its ioctl is
 * answered by a panel in memory, and the frames are fed to its event
 * handler as the kernel would send them.
 *
 * Two panels:
 *
 * - a multi-touch panel with slots, which also sends the kernel's
 *   single-touch emulation (ABS_X/ABS_Y/BTN_TOUCH following the
 *   oldest contact), as edt-ft5x06 and most others do;
 * - a single-touch panel (ABS_X/ABS_Y/BTN_TOUCH only) whose axes do
 *   not start at 0.
 *
 * Checked here:
 *
 * - the pointer is pressed while any contact is down and released
 *   when the last one lifts, not the other way round;
 * - the pointer device answers X, Y and pressed, on the screen and
 *   the viewport;
 * - a multi-touch panel's single-touch emulation leaves the slots
 *   alone: a new contact keeps its own position, and no contact is
 *   lifted twice;
 * - a single-touch panel's contact is slot 0: touching it presses the
 *   pointer, and nothing is written outside the slots;
 * - a panel position is taken from the axis minimum.
 *
 * Each runs with the gestures and the simulated mouse off and again
 * with both on. */
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include <errno.h>
#include <sys/ioctl.h>
#include <linux/input.h>

#include <boolean.h>

/* ---- the panel ------------------------------------------------------ */

#define PANEL_SLOTS 5

static int  panel_mt;          /* has ABS_MT_SLOT */
static int  panel_x_min, panel_x_max, panel_y_min, panel_y_max;
static int  panel_tracking[PANEL_SLOTS];

static int test_ioctl(int fd, unsigned long req, void *arg)
{
   struct input_absinfo *abs = (struct input_absinfo*)arg;
   (void)fd;

   if (_IOC_TYPE(req) == 'E' && _IOC_NR(req) == 0x0a)
   {
      /* EVIOCGMTSLOTS: the kernel refuses it without MT slots */
      int32_t *values = (int32_t*)arg + 1;
      size_t   count  = (_IOC_SIZE(req) - sizeof(int32_t)) / sizeof(int32_t);
      size_t   i;
      if (!panel_mt)
      {
         errno = EINVAL;
         return -1;
      }
      for (i = 0; i < count && i < PANEL_SLOTS; i++)
         values[i] = (*(uint32_t*)arg == ABS_MT_TRACKING_ID)
            ? panel_tracking[i] : 0;
      return 0;
   }

   if (_IOC_TYPE(req) == 'E' && _IOC_NR(req) >= 0x40
         && _IOC_NR(req) < 0x40 + ABS_CNT)
   {
      /* EVIOCGABS: an axis the device lacks reads as all zero */
      unsigned code = _IOC_NR(req) - 0x40;
      memset(abs, 0, sizeof(*abs));
      switch (code)
      {
         case ABS_MT_SLOT:
            if (panel_mt)
               abs->maximum = PANEL_SLOTS - 1;
            break;
         case ABS_X:
         case ABS_MT_POSITION_X:
            abs->minimum = panel_x_min;
            abs->maximum = panel_x_max;
            break;
         case ABS_Y:
         case ABS_MT_POSITION_Y:
            abs->minimum = panel_y_min;
            abs->maximum = panel_y_max;
            break;
         default:
            break;
      }
      return 0;
   }

   errno = ENOTTY;
   return -1;
}
#define ioctl(fd, req, arg) test_ioctl((fd), (unsigned long)(req), (void*)(arg))

#include "input/drivers/udev_input.c"

#undef ioctl

/* ---- the frontend, as far as the driver links against it ------------ */

static int errors;

void RARCH_LOG(const char *fmt, ...)  { (void)fmt; }
void RARCH_WARN(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...)  { (void)fmt; errors++; }
void RARCH_DBG(const char *fmt, ...)  { (void)fmt; }

static settings_t stub_settings;
settings_t *config_get_ptr(void) { return &stub_settings; }
#include "../input_config_stubs.h"

struct retro_keybind *input_config_binds[MAX_USERS];
struct retro_keybind *input_autoconf_binds[MAX_USERS];
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
/* the screen is the main panel; the viewport is its right half */
bool video_driver_translate_coord_viewport(struct video_viewport *vp,
      int mouse_x, int mouse_y, uint32_t *res_pos, uint32_t *res_screen_pos,
      bool report_oob)
{
   (void)vp; (void)report_oob;
   *res_screen_pos = VIDEO_POS_PACK(mouse_x, mouse_y);
   *res_pos        = VIDEO_POS_PACK(mouse_x - 512, mouse_y);
   return true;
}

void input_config_clear_keyboard_display_names(void) { }
void input_config_set_keyboard_display_name(unsigned idx, const char *name) { (void)idx; (void)name; }
void input_config_set_keyboard_ids(unsigned idx, uint16_t vid, uint16_t pid) { (void)idx; (void)vid; (void)pid; }
void input_config_set_mouse_display_name(unsigned port, const char *name) { (void)port; (void)name; }
void input_config_set_mouse_device(unsigned idx, const char *device,
      uint16_t vid, uint16_t pid, bool hidden)
{ (void)idx; (void)device; (void)vid; (void)pid; (void)hidden; }
void input_keyboard_pins_set_devices(const char (*base)[64], unsigned n) { (void)base; (void)n; }
void input_mouse_pins_set_devices(const char (*base)[64], unsigned n) { (void)base; (void)n; }
int input_keyboard_port_choice(unsigned port) { (void)port; return -1; }
unsigned input_mouse_port_index(unsigned port) { return port; }

/* ---- the test ------------------------------------------------------- */

static int failures;

static void CHECK(int cond, const char *fmt, ...)
{
   va_list ap;
   if (cond)
      return;
   va_start(ap, fmt);
   printf("   FAIL ");
   vprintf(fmt, ap);
   printf("\n");
   va_end(ap);
   failures++;
}

static udev_input_t        udev;
static udev_input_device_t panel;

static void ev(unsigned type, unsigned code, int value)
{
   struct input_event e;
   memset(&e, 0, sizeof(e));
   e.type  = type;
   e.code  = code;
   e.value = value;
   udev_handle_touch(&udev, &e, &panel);
}

#define SLOT(n)      ev(EV_ABS, ABS_MT_SLOT, (n))
#define TRACK(id)    ev(EV_ABS, ABS_MT_TRACKING_ID, (id))
#define MT_AT(x, y)  do { ev(EV_ABS, ABS_MT_POSITION_X, (x)); ev(EV_ABS, ABS_MT_POSITION_Y, (y)); } while (0)
#define ST_AT(x, y)  do { ev(EV_ABS, ABS_X, (x)); ev(EV_ABS, ABS_Y, (y)); } while (0)
#define TOUCH(v)     ev(EV_KEY, BTN_TOUCH, (v))
#define SYN()        ev(EV_SYN, SYN_REPORT, 0)

static int16_t pointer(unsigned device, unsigned id)
{
   return udev_input_touch_state(&udev, &panel, NULL, 0, device, 0, id);
}
#define PRESSED() pointer(RETRO_DEVICE_POINTER, RETRO_DEVICE_ID_POINTER_PRESSED)

static void open_panel(int mt, int x_min, int x_max, int y_min, int y_max)
{
   int i;
   panel_mt    = mt;
   panel_x_min = x_min;
   panel_x_max = x_max;
   panel_y_min = y_min;
   panel_y_max = y_max;
   for (i = 0; i < PANEL_SLOTS; i++)
      panel_tracking[i] = -1;

   memset(&udev, 0, sizeof(udev));
   memset(&panel, 0, sizeof(panel));
   panel.fd = -1;
   strlcpy(panel.ident, mt ? "mt panel" : "st panel", sizeof(panel.ident));
   udev_vp_ok       = true;
   udev_vp.full_dims = VIDEO_SCALE_PACK(1024, 600);
   errors           = 0;

   udev_init_touch_dev(&panel);
   udev_sync_touch(&panel);
}

static void close_panel(void)
{
   udev_destroy_touch_dev(&panel);
}

static void multi_touch(void)
{
   open_panel(1, 0, 1023, 0, 599);
   CHECK(panel.touch.mt_slots, "the multi-touch panel is not read by its slots");

   /* first finger, slot 0: the kernel does not announce slot 0 */
   TRACK(10); MT_AT(700, 300); TOUCH(1); ST_AT(700, 300); SYN();
   CHECK(PRESSED() == 1, "one finger down: pointer not pressed");
   CHECK(pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_X) == 699
         || pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_X) == 700,
         "screen X %d, want 700", pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_X));
   CHECK(pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_Y) >= 299
         && pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_Y) <= 300,
         "screen Y %d, want 300", pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_Y));
   CHECK(pointer(RETRO_DEVICE_POINTER, RETRO_DEVICE_ID_POINTER_X) >= 187
         && pointer(RETRO_DEVICE_POINTER, RETRO_DEVICE_ID_POINTER_X) <= 188,
         "viewport X %d, want 188", pointer(RETRO_DEVICE_POINTER, RETRO_DEVICE_ID_POINTER_X));

   /* second finger, slot 1 */
   SLOT(1); TRACK(11); MT_AT(200, 100); SYN();
   CHECK(PRESSED() == 1, "two fingers down: pointer not pressed");

   /* first finger lifts; the emulation follows the second */
   SLOT(0); TRACK(-1); ST_AT(200, 100); SYN();
   CHECK(PRESSED() == 1, "first of two fingers lifted: pointer released while a finger is down");
   CHECK(panel.touch.current_active == 1, "%d contacts after one of two lifted, want 1",
         (int)panel.touch.current_active);

   /* a new finger in slot 0 while the emulation still reports slot 1 */
   TRACK(12); MT_AT(900, 500); ST_AT(200, 100); SYN();
   CHECK(VIDEO_POS_X(panel.touch.current[0].pos) == 900
         && VIDEO_POS_Y(panel.touch.current[0].pos) == 500,
         "new contact in slot 0 is at %d,%d, want 900,500 (taken over by the single-touch emulation)",
         VIDEO_POS_X(panel.touch.current[0].pos), VIDEO_POS_Y(panel.touch.current[0].pos));
   CHECK(VIDEO_POS_X(panel.touch.current[0].td_pos) == 900,
         "new contact in slot 0 went down at x %d, want 900", VIDEO_POS_X(panel.touch.current[0].td_pos));

   /* both lift */
   TRACK(-1); SLOT(1); TRACK(-1); TOUCH(0); SYN();
   CHECK(PRESSED() == 0, "all fingers lifted: pointer still pressed");
   CHECK(panel.touch.current_active == 0, "%d contacts after all lifted, want 0",
         (int)panel.touch.current_active);
   CHECK(errors == 0, "%d errors logged (a contact lifted twice?)", errors);

   /* a plain tap */
   SLOT(0); TRACK(13); MT_AT(10, 10); TOUCH(1); ST_AT(10, 10); SYN();
   CHECK(PRESSED() == 1, "tap down: pointer not pressed");
   TRACK(-1); TOUCH(0); SYN();
   CHECK(PRESSED() == 0, "tap up: pointer still pressed");
   CHECK(errors == 0, "%d errors logged on a tap", errors);

   close_panel();
}

static void multi_touch_held_at_open(void)
{
   /* a finger already down in slot 2 when the panel is opened */
   open_panel(1, 0, 1023, 0, 599);
   close_panel();
   memset(&panel, 0, sizeof(panel));
   panel.fd          = -1;
   panel_tracking[2] = 4;
   udev_init_touch_dev(&panel);
   udev_sync_touch(&panel);
   errors = 0;
   CHECK(panel.touch.staging_active == 1, "panel opened with a finger down counts %d, want 1",
         (int)panel.touch.staging_active);
   CHECK(panel.touch.current_slot == 0, "panel opened on slot %d, want 0",
         (int)panel.touch.current_slot);
   SLOT(2); TRACK(-1); TOUCH(0); SYN();
   CHECK(errors == 0, "%d errors logged lifting a finger held since open", errors);
   CHECK(panel.touch.current_active == 0, "%d contacts after it lifted, want 0",
         (int)panel.touch.current_active);
   panel_tracking[2] = -1;
   close_panel();
}

static void single_touch(void)
{
   /* axes from 100 and 50: a panel position is from the minimum */
   open_panel(0, 100, 1123, 50, 649);
   CHECK(!panel.touch.mt_slots, "the single-touch panel is read by slots");

   ST_AT(612, 350); TOUCH(1); SYN();
   CHECK(PRESSED() == 1, "single-touch down: pointer not pressed");
   CHECK(pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_X) >= 511
         && pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_X) <= 512,
         "single-touch screen X %d, want 512", pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_X));
   CHECK(pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_Y) >= 299
         && pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_Y) <= 300,
         "single-touch screen Y %d, want 300", pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_Y));

   ST_AT(700, 400); SYN();
   CHECK(PRESSED() == 1, "single-touch move: pointer released");
   CHECK(pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_X) >= 599
         && pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_X) <= 600,
         "single-touch moved screen X %d, want 600", pointer(RARCH_DEVICE_POINTER_SCREEN, RETRO_DEVICE_ID_POINTER_X));

   TOUCH(0); SYN();
   CHECK(PRESSED() == 0, "single-touch up: pointer still pressed");

   /* position reports with nothing touching are no contact */
   ST_AT(300, 300); SYN();
   CHECK(PRESSED() == 0 && panel.touch.current_active == 0,
         "single-touch hover pressed the pointer");
   CHECK(errors == 0, "%d errors logged on the single-touch panel", errors);

   close_panel();
}

static void report(int before, const char *what, bool extras)
{
   if (failures == before)
      printf("   ok   %s%s\n", what,
            extras ? " (mouse, trackball, gestures on)" : "");
}

static void run(bool extras)
{
   int before;
   stub_settings.bools.input_touch_vmouse_pointer   = true;
   stub_settings.bools.input_touch_vmouse_mouse     = extras;
   stub_settings.bools.input_touch_vmouse_touchpad  = false;
   stub_settings.bools.input_touch_vmouse_trackball = extras;
   stub_settings.bools.input_touch_vmouse_gesture   = extras;

   before = failures;
   multi_touch();
   report(before, "multi-touch: pressed while a finger is down, released after the last; the emulation leaves the slots alone", extras);
   before = failures;
   multi_touch_held_at_open();
   report(before, "multi-touch: a finger held when the panel opens is counted and lifts cleanly", extras);
   before = failures;
   single_touch();
   report(before, "single-touch: the contact is slot 0, positions are from the axis minimum", extras);
}

int main(void)
{
   run(false);
   run(true);

   if (failures)
   {
      printf("FAIL udev_touch_test: %d checks failed\n", failures);
      return 1;
   }
   printf("PASS udev_touch_test\n");
   return 0;
}

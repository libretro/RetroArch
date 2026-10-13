/* The QNX (BlackBerry 10) input and joypad drivers
 * (input/drivers/qnx_input.c, input/drivers_joypad/qnx_joypad.c), in
 * the order the port's unity build has them, against the tree's SDK
 * stand-ins (tools/qnx_standin) with libscreen's calls defined by the
 * test. Held to:
 *
 * - each gamepad libscreen lists is announced on the port of the slot
 *   its state is kept in, and a pad is there on those ports only; a
 *   keyboard or the trackpad is no pad; the joypad driver announces
 *   nothing of its own;
 * - a gamepad unplugged lets its port go; one plugged in takes the
 *   first free slot and is announced on that slot's port;
 * - the trackpad moves the cursor the way it is swiped, all four ways. */

#include <errno.h>
#include <stdio.h>
#include <string.h>

#include "input/input_keymaps.h"
#include "verbosity.h"
#include "input/drivers/qnx_input.c"
#include "input/drivers_joypad/qnx_joypad.c"

/* ---- libscreen ---- */
struct t_dev { int type; const char *id; };
static struct t_dev t_dev[8];
static int          t_ndev;
static int          t_ev_type, t_ev_attached, t_ev_disp[2];
static struct t_dev *t_ev_device;

screen_context_t screen_ctx;

int screen_get_context_property_iv(screen_context_t c, int prop, int *v)
{
   (void)c;
   if (prop == SCREEN_PROPERTY_DEVICE_COUNT)
      *v = t_ndev;
   return 0;
}
int screen_get_context_property_pv(screen_context_t c, int prop, void **v)
{
   int i;
   (void)c;
   if (prop == SCREEN_PROPERTY_DEVICES)
      for (i = 0; i < t_ndev; i++)
         v[i] = &t_dev[i];
   return 0;
}
int screen_get_device_property_iv(screen_device_t d, int prop, int *v)
{
   struct t_dev *x = (struct t_dev*)d;
   switch (prop)
   {
      case SCREEN_PROPERTY_TYPE:         *v = x->type; return 0;
      case SCREEN_PROPERTY_BUTTON_COUNT: *v = 16;      return 0;
      default:                                         return -1;
   }
}
int screen_get_device_property_cv(screen_device_t d, int prop, int len, char *v)
{
   struct t_dev *x = (struct t_dev*)d;
   (void)prop;
   strlcpy(v, x->id, len);
   return 0;
}
screen_event_t screen_event_get_event(bps_event_t e) { (void)e; return NULL; }
int screen_get_event_property_iv(screen_event_t e, int prop, int *v)
{
   (void)e;
   switch (prop)
   {
      case SCREEN_PROPERTY_TYPE:         *v = t_ev_type;     break;
      case SCREEN_PROPERTY_ATTACHED:     *v = t_ev_attached; break;
      case SCREEN_PROPERTY_DISPLACEMENT: v[0] = t_ev_disp[0];
                                         v[1] = t_ev_disp[1]; break;
      default:                           *v = 0;             break;
   }
   return 0;
}
int screen_get_event_property_pv(screen_event_t e, int prop, void **v)
{
   (void)e;
   if (prop == SCREEN_PROPERTY_DEVICE)
      *v = t_ev_device;
   return 0;
}
int screen_get_domain(void)                         { return 1; }
int bps_get_event(bps_event_t *e, int t)            { (void)e; (void)t; return 0; }
int bps_event_get_domain(bps_event_t e)             { (void)e; return 0; }
int bps_event_get_code(bps_event_t e)               { (void)e; return 0; }
int navigator_get_domain(void)                      { return 2; }
int navigator_event_get_syskey_key(bps_event_t e)   { (void)e; return 0; }
int navigator_event_get_window_state(bps_event_t e) { (void)e; return 0; }

/* ---- the frontend ---- */
static qnx_input_t *t_qnx;
static int          t_port_name[16];   /* 1 while announced */
static unsigned     t_connects;
static unsigned     t_keys[RETROK_LAST];

const struct rarch_key_map rarch_key_map_qnx[] = { { 0, RETROK_UNKNOWN } };
enum retro_key rarch_keysym_lut[RETROK_LAST];

void *input_driver_current_data(void) { return t_qnx; }
bool input_autoconfigure_connect(const char *name, const char *display_name,
      const char *phys, const char *driver, unsigned port,
      unsigned vid, unsigned pid)
{
   (void)display_name; (void)phys; (void)driver; (void)vid; (void)pid;
   t_connects++;
   if (port < 16)
      t_port_name[port] = name && *name;
   return true;
}
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{
   (void)name;
   if (port < 16)
      t_port_name[port] = 0;
   return true;
}
const char *input_config_get_device_name(unsigned port) { (void)port; return ""; }
void input_driver_platform_request(enum input_platform_request r) { (void)r; }
bool input_driver_poll_viewport(struct video_viewport *vp) { (void)vp; return false; }
bool video_driver_translate_coord_viewport(struct video_viewport *vp,
      int x, int y, uint32_t *a, uint32_t *b, bool oob)
{
   (void)vp; (void)x; (void)y; (void)a; (void)b; (void)oob;
   return false;
}
void input_keyboard_event(bool down, unsigned code, uint32_t character,
      uint16_t mod, unsigned device)
{
   (void)character; (void)mod; (void)device;
   if (down && code < RETROK_LAST)
      t_keys[code]++;
}
void input_keymaps_init_keyboard_lut(const struct rarch_key_map *map) { (void)map; }
enum retro_key input_keymaps_translate_keysym_to_rk(unsigned sym)
{
   (void)sym;
   return RETROK_UNKNOWN;
}
void RARCH_DBG(const char *fmt, ...) { (void)fmt; }
void RARCH_ERR(const char *fmt, ...) { (void)fmt; }

/* ---- the checks ---- */
static unsigned failures = 0;

static void check(bool cond, const char *what)
{
   if (!cond)
   {
      printf("FAIL: %s\n", what);
      failures++;
   }
}

/* Ports the joypad driver has a pad on, as a mask. */
static unsigned pads_there(void)
{
   unsigned p, m = 0;
   for (p = 0; p < 16; p++)
      if (qnx_joypad.query_pad(p))
         m |= 1u << p;
   return m;
}
static unsigned announced(void)
{
   unsigned p, m = 0;
   for (p = 0; p < 16; p++)
      if (t_port_name[p])
         m |= 1u << p;
   return m;
}

static void device_event(struct t_dev *d, int attached)
{
   t_ev_type     = SCREEN_EVENT_DEVICE;
   t_ev_attached = attached;
   t_ev_device   = d;
   qnx_handle_screen_event(t_qnx, NULL);
}

static void swipe(int dx, int dy)
{
   t_ev_type    = SCREEN_EVENT_JOYSTICK;
   t_ev_disp[0] = dx;
   t_ev_disp[1] = dy;
   qnx_handle_screen_event(t_qnx, NULL);
}

int main(void)
{
   unsigned connects;

   /* A keyboard, two gamepads, the trackpad. */
   t_dev[0].type = SCREEN_EVENT_KEYBOARD; t_dev[0].id = "kbd";
   t_dev[1].type = SCREEN_EVENT_GAMEPAD;  t_dev[1].id = "padA";
   t_dev[2].type = SCREEN_EVENT_GAMEPAD;  t_dev[2].id = "padB";
   t_dev[3].type = SCREEN_EVENT_JOYSTICK; t_dev[3].id = "trackpad";
   t_ndev        = 4;

   t_qnx = (qnx_input_t*)qnx_input_init(NULL);
   check(t_qnx != NULL, "input init");
   connects = t_connects;
   check(qnx_joypad.init(NULL) != NULL, "joypad init");
   check(t_connects == connects, "joypad init announces nothing of its own");

   check(announced() == pads_there(), "found: announced where there");
   {
      unsigned m = pads_there(), n = 0;
      for (; m; m &= m - 1)
         n++;
      check(n == 2, "found: exactly the two gamepads");
   }
   {
      unsigned p;
      for (p = 0; p < DEFAULT_MAX_PADS; p++)
         if (qnx_joypad.query_pad(p))
            check(t_qnx->devices[p].handle == (screen_device_t)&t_dev[1]
               || t_qnx->devices[p].handle == (screen_device_t)&t_dev[2],
               "found: a pad's port is the slot holding it");
   }

   /* Unplug the first gamepad, plug a third. */
   {
      unsigned before = pads_there(), gone, p;
      for (p = 0; p < DEFAULT_MAX_PADS; p++)
         if (t_qnx->devices[p].handle == (screen_device_t)&t_dev[1])
            break;
      gone = 1u << p;
      device_event(&t_dev[1], 0);
      check(pads_there() == (before & ~gone), "unplugged: its port has no pad");
      check(!(announced() & gone), "unplugged: its port let go");

      t_dev[4].type = SCREEN_EVENT_GAMEPAD; t_dev[4].id = "padC";
      device_event(&t_dev[4], 1);
      for (p = 0; p < DEFAULT_MAX_PADS; p++)
         if (t_qnx->devices[p].handle == (screen_device_t)&t_dev[4])
            break;
      check(p < DEFAULT_MAX_PADS && qnx_joypad.query_pad(p),
            "plugged: there on its slot's port");
      check(announced() == pads_there(), "plugged: announced where there");
   }

   /* The trackpad, all four ways. */
   memset(t_keys, 0, sizeof(t_keys));
   swipe(-TRACKPAD_CPI, 0);
   check(t_keys[RETROK_LEFT] == 1 && !t_keys[RETROK_RIGHT], "trackpad: left");
   swipe(TRACKPAD_CPI, 0);
   check(t_keys[RETROK_RIGHT] == 1, "trackpad: right");
   swipe(0, -TRACKPAD_CPI);
   check(t_keys[RETROK_UP] == 1 && !t_keys[RETROK_DOWN], "trackpad: up");
   swipe(0, TRACKPAD_CPI);
   check(t_keys[RETROK_DOWN] == 1, "trackpad: down");

   qnx_joypad.destroy();
   free(t_qnx);

   if (failures)
   {
      printf("%u failure(s)\n", failures);
      return 1;
   }
   printf("qnx_pads: all checks passed\n");
   return 0;
}

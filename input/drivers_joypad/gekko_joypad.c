/*  RetroArch - A frontend for libretro.
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

/* GameCube controllers and (Wii) remotes with their extensions, on
 * os/gekko.  The button numbering is the libogc driver's, so the
 * built-in autoconfig profiles and saved binds carry over. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <gekko/pad.h>
#include <gekko/power.h>
#ifdef HW_RVL
#include <gekko/wiimote.h>
#endif

#include "../../config.def.h"

#include "../input_driver.h"
#ifdef HW_RVL
#include "../../gfx/video_driver.h"
#endif
#include "../../retroarch.h"
#include "../../tasks/tasks_internal.h"

enum
{
   GX_GC_A                 = 0,
   GX_GC_B                 = 1,
   GX_GC_X                 = 2,
   GX_GC_Y                 = 3,
   GX_GC_START             = 4,
   GX_GC_HOME              = 5,   /* the menu, from a button combination */
   GX_GC_Z_TRIGGER         = 6,
   GX_GC_L_TRIGGER         = 7,
   GX_GC_R_TRIGGER         = 8,
   GX_GC_UP                = 9,
   GX_GC_DOWN              = 10,
   GX_GC_LEFT              = 11,
   GX_GC_RIGHT             = 12,
   GX_CLASSIC_A            = 13,
   GX_CLASSIC_B            = 14,
   GX_CLASSIC_X            = 15,
   GX_CLASSIC_Y            = 16,
   GX_CLASSIC_PLUS         = 17,
   GX_CLASSIC_MINUS        = 18,
   GX_CLASSIC_HOME         = 19,
   GX_CLASSIC_L_TRIGGER    = 20,
   GX_CLASSIC_R_TRIGGER    = 21,
   GX_CLASSIC_ZL_TRIGGER   = 22,
   GX_CLASSIC_ZR_TRIGGER   = 23,
   GX_CLASSIC_UP           = 24,
   GX_CLASSIC_DOWN         = 25,
   GX_CLASSIC_LEFT         = 26,
   GX_CLASSIC_RIGHT        = 27,
   GX_WIIMOTE_A            = 28,
   GX_WIIMOTE_B            = 29,
   GX_WIIMOTE_1            = 30,
   GX_WIIMOTE_2            = 31,
   GX_WIIMOTE_PLUS         = 32,
   GX_WIIMOTE_MINUS        = 33,
   GX_WIIMOTE_HOME         = 34,
   GX_WIIMOTE_UP           = 35,
   GX_WIIMOTE_DOWN         = 36,
   GX_WIIMOTE_LEFT         = 37,
   GX_WIIMOTE_RIGHT        = 38,
   GX_NUNCHUK_Z            = 39,
   GX_NUNCHUK_C            = 40
};

enum pad_kind
{
   KIND_NONE = 0,
   KIND_GAMECUBE,
   KIND_WIIMOTE,
   KIND_NUNCHUK,
   KIND_CLASSIC,
   KIND_GUITAR
};

#define BIT(n) (UINT64_C(1) << (n))

/* TODO/FIXME - global referenced outside */
extern uint64_t lifecycle_state;

static uint64_t          pad_state[DEFAULT_MAX_PADS];
static int16_t           analog_state[DEFAULT_MAX_PADS][2][2];
static uint8_t           pad_kind[DEFAULT_MAX_PADS];
static volatile uint8_t  reset_pressed;
static volatile uint8_t  power_pressed;

static void on_reset(void *data) { (void)data; reset_pressed = 1; }
static void on_power(void *data) { (void)data; power_pressed = 1; }

#ifdef HW_RVL
/* Where each port's remote points, for gx_input's mouse and lightgun:
 * pixels of the full viewport, and its buttons as those devices number
 * them (B the trigger or left button, then A, 1, 2, + and -). */
static struct
{
   int      x, y;
   uint32_t buttons;
   uint8_t  valid;
} pointer[DEFAULT_MAX_PADS];

bool gxpad_mousevalid(unsigned port)
{
   return port < DEFAULT_MAX_PADS && pointer[port].valid;
}

void gx_joypad_read_mouse(unsigned port, int *irx, int *iry,
      uint32_t *button)
{
   if (port >= DEFAULT_MAX_PADS)
   {
      *irx    = 0;
      *iry    = 0;
      *button = 0;
      return;
   }
   *irx    = pointer[port].x;
   *iry    = pointer[port].y;
   *button = pointer[port].buttons;
}

static void pointer_state(unsigned port, const gk_wiimote_t *w,
      const struct video_viewport *vp)
{
   static const uint16_t mask[6] = {
      GK_WM_B, GK_WM_A, GK_WM_ONE, GK_WM_TWO, GK_WM_PLUS, GK_WM_MINUS };
   unsigned i;
   int fw = VIDEO_SCALE_W(vp->full_dims), fh = VIDEO_SCALE_H(vp->full_dims);
   pointer[port].buttons = 0;
   for (i = 0; i < 6; i++)
      if (w->buttons & mask[i])
         pointer[port].buttons |= 1u << (i + 2);
   pointer[port].valid = w->ir_valid;
   if (w->ir_dots && fw > 1 && fh > 1)
   {
      pointer[port].x = (int)(((int32_t)w->ir_x + 32767) * (fw - 1) / 65534);
      pointer[port].y = (int)(((int32_t)w->ir_y + 32767) * (fh - 1) / 65534);
   }
}
#endif

static const char *gekko_joypad_name(unsigned pad)
{
   if (pad >= DEFAULT_MAX_PADS)
      return NULL;
   switch (pad_kind[pad])
   {
      case KIND_GAMECUBE: return "GameCube Controller";
      case KIND_WIIMOTE:  return "Wiimote Controller";
      case KIND_NUNCHUK:  return "Nunchuk Controller";
      case KIND_CLASSIC:  return "Classic Controller";
      case KIND_GUITAR:   return "Guitar Hero Guitar";
   }
   return NULL;
}

static bool gekko_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS && pad < DEFAULT_MAX_PADS
      && pad_kind[pad] != KIND_NONE;
}

static int32_t gekko_joypad_button(unsigned port, uint16_t joykey)
{
   if (port >= DEFAULT_MAX_PADS || joykey >= 64)
      return 0;
   return (pad_state[port] & BIT(joykey)) != 0;
}

static void gekko_joypad_get_buttons(unsigned port, input_bits_t *state)
{
   if (port < DEFAULT_MAX_PADS)
   {
      BITS_COPY64_PTR(state, pad_state[port]);
   }
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t axis_value(unsigned port, unsigned axis)
{
   return axis < 2 ? analog_state[port][0][axis]
      : analog_state[port][1][axis - 2];
}

static int16_t gekko_joypad_axis(unsigned port, uint32_t joyaxis)
{
   int16_t v;
   if (port >= DEFAULT_MAX_PADS)
      return 0;
   if (AXIS_NEG_GET(joyaxis) < 4)
   {
      v = axis_value(port, AXIS_NEG_GET(joyaxis));
      return v < 0 ? v : 0;
   }
   if (AXIS_POS_GET(joyaxis) < 4)
   {
      v = axis_value(port, AXIS_POS_GET(joyaxis));
      return v > 0 ? v : 0;
   }
   return 0;
}

static int16_t gekko_joypad_state(rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds, unsigned port)
{
   unsigned i;
   int16_t ret       = 0;
   uint16_t port_idx = joypad_info->joy_idx;

   if (port_idx >= DEFAULT_MAX_PADS)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if ((uint16_t)joykey != NO_BTN && joykey < 64
            && (pad_state[port_idx] & BIT(joykey)))
         ret |= (1 << i);
      else if (joyaxis != AXIS_NONE
            && ((float)abs(gekko_joypad_axis(port_idx, joyaxis)) / 0x8000)
               > joypad_info->axis_threshold)
         ret |= (1 << i);
   }
   return ret;
}

static void gamecube_state(unsigned port, const gk_pad_t *p)
{
   uint64_t *s = &pad_state[port];
   const uint64_t combo = BIT(GX_GC_START) | BIT(GX_GC_Z_TRIGGER)
      | BIT(GX_GC_L_TRIGGER) | BIT(GX_GC_R_TRIGGER);
   uint16_t b = p->buttons;

   if (b & GK_PAD_A)      *s |= BIT(GX_GC_A);
   if (b & GK_PAD_B)      *s |= BIT(GX_GC_B);
   if (b & GK_PAD_X)      *s |= BIT(GX_GC_X);
   if (b & GK_PAD_Y)      *s |= BIT(GX_GC_Y);
   if (b & GK_PAD_UP)     *s |= BIT(GX_GC_UP);
   if (b & GK_PAD_DOWN)   *s |= BIT(GX_GC_DOWN);
   if (b & GK_PAD_LEFT)   *s |= BIT(GX_GC_LEFT);
   if (b & GK_PAD_RIGHT)  *s |= BIT(GX_GC_RIGHT);
   if (b & GK_PAD_START)  *s |= BIT(GX_GC_START);
   if (b & GK_PAD_Z)      *s |= BIT(GX_GC_Z_TRIGGER);
   if ((b & GK_PAD_L) || p->trigger_l > 127) *s |= BIT(GX_GC_L_TRIGGER);
   if ((b & GK_PAD_R) || p->trigger_r > 127) *s |= BIT(GX_GC_R_TRIGGER);

   analog_state[port][0][0] = (int16_t)(p->stick_x * 256);
   analog_state[port][0][1] = (int16_t)(p->stick_y * -256);
   analog_state[port][1][0] = (int16_t)(p->sub_x * 256);
   analog_state[port][1][1] = (int16_t)(p->sub_y * -256);

   /* Start, Z, L and R together open the menu. */
   if ((*s & combo) == combo)
      *s |= BIT(GX_GC_HOME);
}

#ifdef HW_RVL
static void wiimote_state(unsigned port, const gk_wiimote_t *w)
{
   uint64_t *s = &pad_state[port];
   uint16_t b  = w->buttons;
   uint16_t e  = w->ext_buttons;

   if (b & GK_WM_A)     *s |= BIT(GX_WIIMOTE_A);
   if (b & GK_WM_B)     *s |= BIT(GX_WIIMOTE_B);
   if (b & GK_WM_ONE)   *s |= BIT(GX_WIIMOTE_1);
   if (b & GK_WM_TWO)   *s |= BIT(GX_WIIMOTE_2);
   if (b & GK_WM_PLUS)  *s |= BIT(GX_WIIMOTE_PLUS);
   if (b & GK_WM_MINUS) *s |= BIT(GX_WIIMOTE_MINUS);
   if (b & GK_WM_HOME)  *s |= BIT(GX_WIIMOTE_HOME);

   if (w->ext == GK_WM_EXT_NUNCHUK)
   {
      /* Held upright beside a Nunchuk: the d-pad as it is. */
      if (b & GK_WM_UP)    *s |= BIT(GX_WIIMOTE_UP);
      if (b & GK_WM_DOWN)  *s |= BIT(GX_WIIMOTE_DOWN);
      if (b & GK_WM_LEFT)  *s |= BIT(GX_WIIMOTE_LEFT);
      if (b & GK_WM_RIGHT) *s |= BIT(GX_WIIMOTE_RIGHT);
      if (e & GK_NC_Z)     *s |= BIT(GX_NUNCHUK_Z);
      if (e & GK_NC_C)     *s |= BIT(GX_NUNCHUK_C);
      analog_state[port][0][0] = (int16_t)(w->stick[0][0] * 256);
      analog_state[port][0][1] = (int16_t)(w->stick[0][1] * -256);
      return;
   }

   if (w->ext == GK_WM_EXT_GUITAR)
   {
      /* On the Classic's numbers; the whammy bar pushes the right
       * stick right. */
      if (e & GK_GH_GREEN)  *s |= BIT(GX_CLASSIC_A);
      if (e & GK_GH_RED)    *s |= BIT(GX_CLASSIC_B);
      if (e & GK_GH_YELLOW) *s |= BIT(GX_CLASSIC_X);
      if (e & GK_GH_BLUE)   *s |= BIT(GX_CLASSIC_Y);
      if (e & GK_GH_ORANGE) *s |= BIT(GX_CLASSIC_ZL_TRIGGER);
      if (e & GK_GH_UP)     *s |= BIT(GX_CLASSIC_UP);
      if (e & GK_GH_DOWN)   *s |= BIT(GX_CLASSIC_DOWN);
      if (e & GK_GH_PLUS)   *s |= BIT(GX_CLASSIC_PLUS);
      if (e & GK_GH_MINUS)  *s |= BIT(GX_CLASSIC_MINUS);
      analog_state[port][0][0] = (int16_t)(w->stick[0][0] * 256);
      analog_state[port][0][1] = (int16_t)(w->stick[0][1] * -256);
      analog_state[port][1][0] = (int16_t)(w->trigger[1] * 128);
      return;
   }

   /* Held sideways: the d-pad turns with it. */
   if (b & GK_WM_UP)    *s |= BIT(GX_WIIMOTE_LEFT);
   if (b & GK_WM_DOWN)  *s |= BIT(GX_WIIMOTE_RIGHT);
   if (b & GK_WM_LEFT)  *s |= BIT(GX_WIIMOTE_DOWN);
   if (b & GK_WM_RIGHT) *s |= BIT(GX_WIIMOTE_UP);

   if (w->ext == GK_WM_EXT_CLASSIC)
   {
      if (e & GK_CC_A)     *s |= BIT(GX_CLASSIC_A);
      if (e & GK_CC_B)     *s |= BIT(GX_CLASSIC_B);
      if (e & GK_CC_X)     *s |= BIT(GX_CLASSIC_X);
      if (e & GK_CC_Y)     *s |= BIT(GX_CLASSIC_Y);
      if (e & GK_CC_UP)    *s |= BIT(GX_CLASSIC_UP);
      if (e & GK_CC_DOWN)  *s |= BIT(GX_CLASSIC_DOWN);
      if (e & GK_CC_LEFT)  *s |= BIT(GX_CLASSIC_LEFT);
      if (e & GK_CC_RIGHT) *s |= BIT(GX_CLASSIC_RIGHT);
      if (e & GK_CC_PLUS)  *s |= BIT(GX_CLASSIC_PLUS);
      if (e & GK_CC_MINUS) *s |= BIT(GX_CLASSIC_MINUS);
      if (e & GK_CC_HOME)  *s |= BIT(GX_CLASSIC_HOME);
      if (e & GK_CC_L)     *s |= BIT(GX_CLASSIC_L_TRIGGER);
      if (e & GK_CC_R)     *s |= BIT(GX_CLASSIC_R_TRIGGER);
      if (e & GK_CC_ZL)    *s |= BIT(GX_CLASSIC_ZL_TRIGGER);
      if (e & GK_CC_ZR)    *s |= BIT(GX_CLASSIC_ZR_TRIGGER);
      analog_state[port][0][0] = (int16_t)(w->stick[0][0] * 256);
      analog_state[port][0][1] = (int16_t)(w->stick[0][1] * -256);
      analog_state[port][1][0] = (int16_t)(w->stick[1][0] * 256);
      analog_state[port][1][1] = (int16_t)(w->stick[1][1] * -256);
   }
}
#endif

static void hotplug(unsigned port, uint8_t kind)
{
   if (kind == KIND_NONE)
      input_autoconfigure_disconnect(port, gekko_joypad_name(port));
   pad_kind[port] = kind;
   if (kind != KIND_NONE)
      input_autoconfigure_connect(gekko_joypad_name(port), NULL, NULL,
            gx_joypad.ident, port, 0, 0);
}

static void gekko_joypad_poll(void)
{
   gk_pad_t pads[GK_PAD_PORTS];
#ifdef HW_RVL
   struct video_viewport vp = {0};
#endif
   uint64_t menu_keys;
   unsigned port, i, j;

   if (power_pressed)
   {
      power_pressed = 0;
      retroarch_ctl(RARCH_CTL_SET_SHUTDOWN, NULL);
      return;
   }

   gk_pad_read(pads);
#ifdef HW_RVL
   video_driver_get_viewport_info(&vp);
#endif

   for (port = 0; port < DEFAULT_MAX_PADS; port++)
   {
      uint8_t kind = KIND_NONE;
      pad_state[port] = 0;
      memset(analog_state[port], 0, sizeof(analog_state[port]));
#ifdef HW_RVL
      pointer[port].valid   = 0;
      pointer[port].buttons = 0;
#endif

      if (port < GK_PAD_PORTS && pads[port].connected)
      {
         gamecube_state(port, &pads[port]);
         kind = KIND_GAMECUBE;
      }
#ifdef HW_RVL
      else
      {
         gk_wiimote_t w;
         if (port < GK_WIIMOTE_SLOTS && !gk_wiimote_read(port, &w))
         {
            wiimote_state(port, &w);
            kind = w.ext == GK_WM_EXT_NUNCHUK ? KIND_NUNCHUK
               : w.ext == GK_WM_EXT_CLASSIC ? KIND_CLASSIC
               : w.ext == GK_WM_EXT_GUITAR  ? KIND_GUITAR : KIND_WIIMOTE;
            /* Not with the Classic Controller or the guitar in hand. */
            if (kind != KIND_CLASSIC && kind != KIND_GUITAR)
               pointer_state(port, &w, &vp);
         }
      }
#endif

      if (kind != pad_kind[port])
         hotplug(port, kind);

      for (i = 0; i < 2; i++)
         for (j = 0; j < 2; j++)
            if (analog_state[port][i][j] == -0x8000)
               analog_state[port][i][j] = -0x7fff;
   }

   /* The reset button opens the menu, as each pad's home does. */
   if (reset_pressed)
   {
      reset_pressed = 0;
      pad_state[0] |= BIT(GX_GC_HOME);
   }

   menu_keys = BIT(GX_GC_HOME);
#ifdef HW_RVL
   menu_keys |= BIT(GX_WIIMOTE_HOME) | BIT(GX_CLASSIC_HOME);
#endif
   BIT64_CLEAR(lifecycle_state, RARCH_MENU_TOGGLE);
   if (pad_state[0] & menu_keys)
      BIT64_SET(lifecycle_state, RARCH_MENU_TOGGLE);
}

static bool gekko_joypad_set_rumble(unsigned pad,
      enum retro_rumble_effect effect, uint16_t strength)
{
   static uint16_t level[DEFAULT_MAX_PADS][2];
   if (pad >= DEFAULT_MAX_PADS)
      return false;
   level[pad][effect == RETRO_RUMBLE_STRONG ? 0 : 1] = strength;
   /* Motors that are on or off: on while either effect is asked for. */
   if (pad_kind[pad] == KIND_GAMECUBE)
      gk_pad_rumble(pad, level[pad][0] || level[pad][1]);
#ifdef HW_RVL
   else if (pad_kind[pad] != KIND_NONE)
      gk_wiimote_rumble(pad, level[pad][0] || level[pad][1]);
#endif
   else
      return false;
   return true;
}

static void *gekko_joypad_init(void *data)
{
   (void)data;
   gk_power_set_callbacks(on_power, on_reset, NULL);
   gk_pad_init();
#ifdef HW_RVL
   gk_wiimote_init();
#endif
   gekko_joypad_poll();
   return (void*)-1;
}

static void gekko_joypad_destroy(void)
{
   unsigned i;
   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      gekko_joypad_set_rumble(i, RETRO_RUMBLE_STRONG, 0);
      gekko_joypad_set_rumble(i, RETRO_RUMBLE_WEAK, 0);
   }
}

input_device_driver_t gx_joypad = {
   gekko_joypad_init,
   gekko_joypad_query_pad,
   gekko_joypad_destroy,
   gekko_joypad_button,
   gekko_joypad_state,
   gekko_joypad_get_buttons,
   gekko_joypad_axis,
   gekko_joypad_poll,
   gekko_joypad_set_rumble,
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   gekko_joypad_name,
   "gx",
};

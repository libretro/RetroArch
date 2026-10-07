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

/* Controllers through the joystick calls of the Windows multimedia
 * library (joyGetPosEx() and the rest, winmm.dll): what Windows 95 and
 * NT 4.0 have with no DirectX, and so the only controllers a build for
 * them can read. Up to six axes, thirty-two buttons and one hat a
 * controller; no rumble.
 *
 * A controller is asked for its position once a poll. One that is not
 * there is not asked every poll - on these systems the question is
 * slow when the answer is no - but looked for again once a second or
 * so, by the clock, which is how one plugged in later is found and one
 * pulled out is let go. */

#include <stdlib.h>
#include <string.h>

#include <windows.h>
#include <mmsystem.h>

#include <boolean.h>
#include <retro_miscellaneous.h>
#include <compat/strl.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../input_driver.h"
#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

/* The library numbers its controllers from 0 to 15. */
#define WINMM_JOYPAD_MAX   ((MAX_USERS < 16) ? MAX_USERS : 16)
#define WINMM_JOYPAD_AXES  6
/* how long between looks for controllers that are not there */
#define WINMM_JOYPAD_RESCAN_MS 1000

typedef struct winmm_joypad
{
   uint32_t buttons;
   int16_t  axes[WINMM_JOYPAD_AXES];
   uint16_t hat;                 /* HAT_*_MASK bits */
   bool     present;
   /* what the controller says of itself, asked when it is found */
   uint8_t  num_axes;
   bool     has_pov;
   uint32_t axis_min[WINMM_JOYPAD_AXES];
   uint32_t axis_max[WINMM_JOYPAD_AXES];
   char     name[64];
} winmm_joypad_t;

static winmm_joypad_t winmm_pads[MAX_USERS];
static DWORD          winmm_last_scan;
static bool           winmm_scanned;

/* An axis as the library reports it, between the two ends the
 * controller gave, as a stick's value: -0x7fff to 0x7fff. */
static int16_t winmm_joypad_scale(uint32_t v, uint32_t lo, uint32_t hi)
{
   double mid, half;
   int out;
   if (hi <= lo)
      return 0;
   if (v < lo)
      v = lo;
   if (v > hi)
      v = hi;
   mid  = ((double)lo + (double)hi) / 2.0;
   half = ((double)hi - (double)lo) / 2.0;
   out  = (int)(((double)v - mid) * 32767.0 / half);
   if (out >  0x7fff)
      out =  0x7fff;
   if (out < -0x7fff)
      out = -0x7fff;
   return (int16_t)out;
}

/* A hat's position, in hundredths of a degree clockwise from up, as
 * the directions held. Anything past a full turn is "centred". */
static uint16_t winmm_joypad_hat(DWORD pov)
{
   static const uint16_t dir[8] = {
      HAT_UP_MASK,   HAT_UP_MASK   | HAT_RIGHT_MASK,
      HAT_RIGHT_MASK, HAT_DOWN_MASK | HAT_RIGHT_MASK,
      HAT_DOWN_MASK, HAT_DOWN_MASK | HAT_LEFT_MASK,
      HAT_LEFT_MASK, HAT_UP_MASK   | HAT_LEFT_MASK };
   if (pov >= 36000)
      return 0;
   /* the nearest of the eight ways */
   return dir[((pov + 2250) / 4500) & 7];
}

/* Ask controller @id where it is. False if it is not there. */
static bool winmm_joypad_read(unsigned id, winmm_joypad_t *pad)
{
   JOYINFOEX info;
   DWORD v[WINMM_JOYPAD_AXES];
   unsigned i;

   memset(&info, 0, sizeof(info));
   info.dwSize  = sizeof(info);
   info.dwFlags = JOY_RETURNALL;
   if (joyGetPosEx(id, &info) != JOYERR_NOERROR)
      return false;

   v[0] = info.dwXpos;
   v[1] = info.dwYpos;
   v[2] = info.dwZpos;
   v[3] = info.dwRpos;
   v[4] = info.dwUpos;
   v[5] = info.dwVpos;
   for (i = 0; i < WINMM_JOYPAD_AXES; i++)
      pad->axes[i] = (i < pad->num_axes)
         ? winmm_joypad_scale(v[i], pad->axis_min[i], pad->axis_max[i])
         : 0;
   pad->buttons = info.dwButtons;
   pad->hat     = pad->has_pov ? winmm_joypad_hat(info.dwPOV) : 0;
   return true;
}

/* Is controller @id there? If so, what it says of itself. */
static bool winmm_joypad_open(unsigned id, winmm_joypad_t *pad)
{
   JOYCAPSA caps;
   JOYINFOEX info;

   memset(&caps, 0, sizeof(caps));
   if (joyGetDevCapsA(id, &caps, sizeof(caps)) != JOYERR_NOERROR)
      return false;
   /* the driver answers for a controller that is not plugged in, too:
    * only its position says whether it is */
   memset(&info, 0, sizeof(info));
   info.dwSize  = sizeof(info);
   info.dwFlags = JOY_RETURNBUTTONS;
   if (joyGetPosEx(id, &info) != JOYERR_NOERROR)
      return false;

   memset(pad, 0, sizeof(*pad));
   pad->axis_min[0] = caps.wXmin; pad->axis_max[0] = caps.wXmax;
   pad->axis_min[1] = caps.wYmin; pad->axis_max[1] = caps.wYmax;
   pad->axis_min[2] = caps.wZmin; pad->axis_max[2] = caps.wZmax;
   pad->axis_min[3] = caps.wRmin; pad->axis_max[3] = caps.wRmax;
   pad->axis_min[4] = caps.wUmin; pad->axis_max[4] = caps.wUmax;
   pad->axis_min[5] = caps.wVmin; pad->axis_max[5] = caps.wVmax;
   /* X and Y always; the rest as the controller says it has them */
   pad->num_axes    = 2;
   if (caps.wCaps & JOYCAPS_HASZ)
      pad->num_axes = 3;
   if (caps.wCaps & JOYCAPS_HASR)
      pad->num_axes = 4;
   if (caps.wCaps & JOYCAPS_HASU)
      pad->num_axes = 5;
   if (caps.wCaps & JOYCAPS_HASV)
      pad->num_axes = 6;
   pad->has_pov     = (caps.wCaps & JOYCAPS_HASPOV) != 0;
   strlcpy(pad->name, caps.szPname[0] ? caps.szPname : "Joystick",
         sizeof(pad->name));
   pad->present     = true;

   input_autoconfigure_connect(pad->name, NULL, NULL, "winmm",
         id, caps.wMid, caps.wPid);
   return true;
}

/* Look for the controllers that are not there, and read the ones that
 * are; one that no longer answers is let go. */
static void winmm_joypad_scan(void)
{
   unsigned id;
   unsigned n = joyGetNumDevs();
   if (n > WINMM_JOYPAD_MAX)
      n = WINMM_JOYPAD_MAX;
   for (id = 0; id < n; id++)
      if (!winmm_pads[id].present)
         winmm_joypad_open(id, &winmm_pads[id]);
}

static void winmm_joypad_poll(void)
{
   unsigned id;
   DWORD now = GetTickCount();

   for (id = 0; id < WINMM_JOYPAD_MAX; id++)
   {
      winmm_joypad_t *pad = &winmm_pads[id];
      if (!pad->present)
         continue;
      if (!winmm_joypad_read(id, pad))
      {
         input_autoconfigure_disconnect(id, pad->name);
         memset(pad, 0, sizeof(*pad));
      }
   }

   /* by the clock: the difference is right across the counter's wrap */
   if (!winmm_scanned || (DWORD)(now - winmm_last_scan) >= WINMM_JOYPAD_RESCAN_MS)
   {
      winmm_joypad_scan();
      winmm_last_scan = now;
      winmm_scanned   = true;
   }
}

static void *winmm_joypad_init(void *data)
{
   memset(winmm_pads, 0, sizeof(winmm_pads));
   winmm_joypad_scan();
   winmm_last_scan = GetTickCount();
   winmm_scanned   = true;
   return (void*)-1;
}

static void winmm_joypad_destroy(void)
{
   memset(winmm_pads, 0, sizeof(winmm_pads));
   winmm_scanned = false;
}

static bool winmm_joypad_query_pad(unsigned pad)
{
   return pad < WINMM_JOYPAD_MAX && winmm_pads[pad].present;
}

static const char *winmm_joypad_name(unsigned pad)
{
   if (pad >= WINMM_JOYPAD_MAX || !winmm_pads[pad].present)
      return NULL;
   return winmm_pads[pad].name;
}

static int32_t winmm_joypad_button_state(const winmm_joypad_t *pad,
      uint16_t joykey)
{
   unsigned hat_dir = GET_HAT_DIR(joykey);
   if (hat_dir)
      /* the one hat there is */
      return (GET_HAT(joykey) == 0 && (pad->hat & hat_dir)) ? 1 : 0;
   if (joykey < 32)
      return (pad->buttons >> joykey) & 1;
   return 0;
}

static int32_t winmm_joypad_button(unsigned port, uint16_t joykey)
{
   if (port >= WINMM_JOYPAD_MAX)
      return 0;
   return winmm_joypad_button_state(&winmm_pads[port], joykey);
}

static void winmm_joypad_get_buttons(unsigned port, input_bits_t *state)
{
   if (port < WINMM_JOYPAD_MAX)
   {
      BITS_COPY32_PTR(state, winmm_pads[port].buttons);
   }
   else
      BIT256_CLEAR_ALL_PTR(state);
}

static int16_t winmm_joypad_axis_state(const winmm_joypad_t *pad,
      uint32_t joyaxis)
{
   if (AXIS_NEG_GET(joyaxis) < WINMM_JOYPAD_AXES)
   {
      int16_t val = pad->axes[AXIS_NEG_GET(joyaxis)];
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < WINMM_JOYPAD_AXES)
   {
      int16_t val = pad->axes[AXIS_POS_GET(joyaxis)];
      if (val > 0)
         return val;
   }
   return 0;
}

static int16_t winmm_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (port >= WINMM_JOYPAD_MAX)
      return 0;
   return winmm_joypad_axis_state(&winmm_pads[port], joyaxis);
}

static int16_t winmm_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   unsigned i;
   int16_t ret                = 0;
   uint16_t port_idx          = joypad_info->joy_idx;
   const winmm_joypad_t *pad;

   if (port_idx >= WINMM_JOYPAD_MAX)
      return 0;
   pad = &winmm_pads[port_idx];

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (     (uint16_t)joykey != NO_BTN
            && winmm_joypad_button_state(pad, (uint16_t)joykey))
         ret |= (1 << i);
      else if (joyaxis != AXIS_NONE
            && ((float)abs(winmm_joypad_axis_state(pad, joyaxis))
               / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

input_device_driver_t winmm_joypad = {
   winmm_joypad_init,
   winmm_joypad_query_pad,
   winmm_joypad_destroy,
   winmm_joypad_button,
   winmm_joypad_state,
   winmm_joypad_get_buttons,
   winmm_joypad_axis,
   winmm_joypad_poll,
   NULL, /* set_rumble */
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   winmm_joypad_name,
   "winmm",
};

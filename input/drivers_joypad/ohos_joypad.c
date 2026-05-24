/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>


#include "../input_driver.h"

#include "../../tasks/tasks_internal.h"

#define NUM_BUTTONS 32
#define NUM_AXES 32

struct ohos_joypad
{
   int fd;
   uint32_t buttons;
   int16_t axes[NUM_AXES];

   char *ident;
};

/* TODO/FIXME - static globals */
static struct ohos_joypad ohos_pads[MAX_USERS];
static int ohos_epoll                              = 0;
static int ohos_inotify                            = 0;
static bool ohos_hotplug                           = false;

static const char *ohos_joypad_name(unsigned pad)
{
   if (pad >= MAX_USERS)
      return NULL;
   return input_config_get_device_name(pad);
}

static void ohos_joypad_poll(void)
{
}

static void *ohos_joypad_init(void *data)
{
    return (void*)-1;
}

static void ohos_joypad_destroy(void)
{
   int i, j;
   struct ohos_app *ohos = (struct ohos_app*)g_ohos;

   for (i = 0; i < DEFAULT_MAX_PADS; i++)
   {
      for (j = 0; j < 2; j++)
         ohos->hat_state[i][j]    = 0;
      for (j = 0; j < MAX_AXIS; j++)
         ohos->analog_state[i][j] = 0;
   }

   for (i = 0; i < MAX_USERS; i++)
   {
      ohos->rumble_last_strength_strong[i] = 0;
      ohos->rumble_last_strength_weak  [i] = 0;
      ohos->rumble_last_strength       [i] = 0;
      ohos->id                         [i] = 0;
   }
}

static int32_t ohos_joypad_button_state(
      struct ohos_app *ohos_app,
      uint8_t *buf,
      unsigned port, uint16_t joykey)
{
   unsigned hat_dir = GET_HAT_DIR(joykey);

   if (hat_dir)
   {
      unsigned h = GET_HAT(joykey);
      if (h > 0)
         return 0;

      switch (hat_dir)
      {
         case HAT_LEFT_MASK:
            return (ohos_app->hat_state[port][0] == -1);
         case HAT_RIGHT_MASK:
            return (ohos_app->hat_state[port][0] ==  1);
         case HAT_UP_MASK:
            return (ohos_app->hat_state[port][1] == -1);
         case HAT_DOWN_MASK:
            return (ohos_app->hat_state[port][1] ==  1);
         default:
            break;
      }
      /* hat requested and no hat button down */
   }
   else if (joykey < LAST_KEYCODE)
      return BIT_GET(buf, joykey);
   return 0;
}

static int32_t ohos_joypad_button(unsigned port, uint16_t joykey)
{
   struct ohos_app *ohos_app = (struct ohos_app*)g_ohos;
   uint8_t *buf                    = ohos_keyboard_state_get(port);

   if (port >= DEFAULT_MAX_PADS)
      return 0;

   return ohos_joypad_button_state(ohos_app, buf, port, joykey);
}

static void ohos_joypad_get_buttons(unsigned port, input_bits_t *state)
{
	const struct ohos_joypad *pad = (const struct ohos_joypad*)
      &ohos_pads[port];

	if (pad)
   {
		BITS_COPY16_PTR(state, pad->buttons);
	}
   else
		BIT256_CLEAR_ALL_PTR(state);
}
static int16_t ohos_joypad_axis_state(
      struct ohos_app *ohos_app,
      unsigned port, uint32_t joyaxis)
{
   if (AXIS_NEG_GET(joyaxis) < MAX_AXIS)
   {
      int16_t val = ohos_app->analog_state[port][AXIS_NEG_GET(joyaxis)];
      if (val < 0)
         return val;
   }
   else if (AXIS_POS_GET(joyaxis) < MAX_AXIS)
   {
      int16_t val = ohos_app->analog_state[port][AXIS_POS_GET(joyaxis)];
      if (val > 0)
         return val;
   }
   return 0;
}

static int16_t ohos_joypad_axis(
      const struct ohos_joypad *pad,
      unsigned port, uint32_t joyaxis)
{
   struct ohos_app *ohos_app = (struct ohos_app*)g_ohos;
   return ohos_joypad_axis_state(ohos_app, port, joyaxis);
}


static int16_t ohos_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   int i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;
   const struct ohos_joypad    *pad = (const struct ohos_joypad*)
      &ohos_pads[port_idx];

   if (port_idx >= MAX_USERS)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if ((uint16_t)joykey != NO_BTN &&
            (joykey < NUM_BUTTONS)   &&
            (BIT32_GET(pad->buttons, joykey)))
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(ohos_joypad_axis_state(pad, port_idx, joyaxis))
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static bool ohos_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS;
}

input_device_driver_t ohos_joypad = {
   ohos_joypad_init,
   ohos_joypad_query_pad,
   ohos_joypad_destroy,
   ohos_joypad_button,
   ohos_joypad_state,
   ohos_joypad_get_buttons,
   ohos_joypad_axis,
   ohos_joypad_poll,
   NULL, /* set_rumble */
   NULL, /* set_rumble_gain */
   NULL, /* set_sensor_state */
   NULL, /* get_sensor_input */
   ohos_joypad_name,
   "ohos",
};

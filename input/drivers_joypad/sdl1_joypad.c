/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Higor Euripedes
 *  Copyright (C)      2023 - Carlo Refice
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

#include "SDL.h"

#include "../input_driver.h"

#include "../../tasks/tasks_internal.h"
#include "../../verbosity.h"

typedef struct _sdl1_joypad
{
   SDL_Joystick *joypad;
   unsigned num_axes;
   unsigned num_buttons;
   unsigned num_hats;
} sdl1_joypad_t;

/* TODO/FIXME - static global */
static sdl1_joypad_t sdl1_pads[MAX_USERS];

static const char *sdl1_joypad_name(unsigned pad)
{
   if (pad >= MAX_USERS)
      return NULL;
   return SDL_JoystickName(pad);
}

static void sdl1_pad_connect(unsigned id)
{
   sdl1_joypad_t *pad = (sdl1_joypad_t*)&sdl1_pads[id];

   pad->joypad = SDL_JoystickOpen(id);

   if (!pad->joypad)
   {
      RARCH_ERR("[SDL] Couldn't open joystick #%u: %s.\n", id, SDL_GetError());

      /* Reset the whole slot so no stale handles survive, mirroring
       * sdl1_pad_disconnect(). */
      memset(pad, 0, sizeof(*pad));

      return;
   }

   input_autoconfigure_connect(
         sdl1_joypad_name(id),
         NULL, NULL,
         sdl1_joypad.ident,
         id,
         0,
         0);

   pad->num_axes    = SDL_JoystickNumAxes(pad->joypad);
   pad->num_buttons = SDL_JoystickNumButtons(pad->joypad);
   pad->num_hats    = SDL_JoystickNumHats(pad->joypad);
}

static void sdl1_pad_disconnect(unsigned id)
{
   if (sdl1_pads[id].joypad)
   {
      SDL_JoystickClose(sdl1_pads[id].joypad);
      input_autoconfigure_disconnect(id, sdl1_joypad.ident);
   }

   memset(&sdl1_pads[id], 0, sizeof(sdl1_pads[id]));
}

static void sdl1_joypad_destroy(void)
{
   int i;
   for (i = 0; i < MAX_USERS; i++)
      sdl1_pad_disconnect(i);

   memset(sdl1_pads, 0, sizeof(sdl1_pads));
}

static void *sdl1_joypad_init(void *data)
{
   unsigned i;
   unsigned num_sticks;
   uint32_t sdl_subsystem_flags = SDL_WasInit(0);

   /* Initialise joystick subsystem, if required */
   if (sdl_subsystem_flags == 0)
   {
      if (SDL_Init(SDL_INIT_JOYSTICK) < 0)
         return NULL;
   }
   else if ((sdl_subsystem_flags & SDL_INIT_JOYSTICK) == 0)
   {
      if (SDL_InitSubSystem(SDL_INIT_JOYSTICK) < 0)
         return NULL;
   }

   memset(sdl1_pads, 0, sizeof(sdl1_pads));

   num_sticks = SDL_NumJoysticks();
   if (num_sticks > MAX_USERS)
      num_sticks = MAX_USERS;

   for (i = 0; i < num_sticks; i++)
      sdl1_pad_connect(i);

   /* quit if no joypad is detected. */
   num_sticks = 0;
   for (i = 0; i < MAX_USERS; i++)
      if (sdl1_pads[i].joypad)
         num_sticks++;

   if (num_sticks == 0)
      goto error;

   return (void*)-1;

error:
   sdl1_joypad_destroy();

   return NULL;
}

static int32_t sdl1_joypad_button_state(
      sdl1_joypad_t *pad,
      unsigned port, uint16_t joykey)
{
   unsigned hat_dir = GET_HAT_DIR(joykey);
   /* Check hat. */
   if (hat_dir)
   {
      uint8_t  dir;
      uint16_t hat  = GET_HAT(joykey);

      if (hat >= pad->num_hats)
         return 0;

      dir = SDL_JoystickGetHat(pad->joypad, hat);

      switch (hat_dir)
      {
         case HAT_UP_MASK:
            return (dir & SDL_HAT_UP);
         case HAT_DOWN_MASK:
            return (dir & SDL_HAT_DOWN);
         case HAT_LEFT_MASK:
            return (dir & SDL_HAT_LEFT);
         case HAT_RIGHT_MASK:
            return (dir & SDL_HAT_RIGHT);
         default:
            break;
      }
      /* hat requested and no hat button down */
   }
   else if (joykey < pad->num_buttons)
      return SDL_JoystickGetButton(pad->joypad, joykey);
   return 0;
}

static int32_t sdl1_joypad_button(unsigned port, uint16_t joykey)
{
   sdl1_joypad_t *pad                   = (sdl1_joypad_t*)&sdl1_pads[port];
   if (!pad || !pad->joypad)
      return 0;
   if (port >= MAX_USERS)
      return 0;
   return sdl1_joypad_button_state(pad, port, joykey);
}

static int16_t sdl1_joypad_axis_state(
      sdl1_joypad_t *pad,
      unsigned port, uint32_t joyaxis)
{
   if (AXIS_NEG_GET(joyaxis) < pad->num_axes)
   {
      int16_t val  = SDL_JoystickGetAxis(pad->joypad, AXIS_NEG_GET(joyaxis));
      if (val < 0)
      {
         /* Clamp - -0x8000 can cause trouble if we later abs() it. */
         if (val < -0x7fff)
            return -0x7fff;
         return val;
      }
   }
   else if (AXIS_POS_GET(joyaxis) < pad->num_axes)
   {
      int16_t val  = SDL_JoystickGetAxis(pad->joypad, AXIS_POS_GET(joyaxis));
      if (val > 0)
         return val;
   }

   return 0;
}

static int16_t sdl1_joypad_axis(unsigned port, uint32_t joyaxis)
{
   sdl1_joypad_t *pad = (sdl1_joypad_t*)&sdl1_pads[port];
   if (!pad || !pad->joypad)
      return false;
   return sdl1_joypad_axis_state(pad, port, joyaxis);
}

static int16_t sdl1_joypad_state(
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind *binds,
      unsigned port)
{
   int i;
   int16_t ret                          = 0;
   uint16_t port_idx                    = joypad_info->joy_idx;
   sdl1_joypad_t *pad                   = (sdl1_joypad_t*)&sdl1_pads[port_idx];

   if (!pad || !pad->joypad)
      return 0;
   if (port_idx >= MAX_USERS)
      return 0;

   for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
   {
      /* Auto-binds are per joypad, not per user. */
      const uint64_t joykey  = (binds[i].joykey != NO_BTN)
         ? binds[i].joykey  : joypad_info->auto_binds[i].joykey;
      const uint32_t joyaxis = (binds[i].joyaxis != AXIS_NONE)
         ? binds[i].joyaxis : joypad_info->auto_binds[i].joyaxis;
      if (
               (uint16_t)joykey != NO_BTN
            && sdl1_joypad_button_state(pad, port_idx, (uint16_t)joykey)
         )
         ret |= ( 1 << i);
      else if (joyaxis != AXIS_NONE &&
            ((float)abs(sdl1_joypad_axis_state(pad, port_idx, joyaxis))
             / 0x8000) > joypad_info->axis_threshold)
         ret |= (1 << i);
   }

   return ret;
}

static void sdl1_joypad_poll(void)
{
   SDL_JoystickUpdate();
}

static bool sdl1_joypad_query_pad(unsigned pad)
{
   return pad < MAX_USERS && sdl1_pads[pad].joypad;
}

input_device_driver_t sdl1_joypad = {
   sdl1_joypad_init,
   sdl1_joypad_query_pad,
   sdl1_joypad_destroy,
   sdl1_joypad_button,
   sdl1_joypad_state,
   NULL,                   /* get_buttons */
   sdl1_joypad_axis,
   sdl1_joypad_poll,
   NULL,                   /* set_rumble */
   NULL,                   /* set_rumble_gain */
   NULL,                   /* set_sensor_state */
   NULL,                   /* get_sensor_input */
   sdl1_joypad_name,
   "sdl"
};

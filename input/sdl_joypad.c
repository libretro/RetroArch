/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "input_common.h"
#include "SDL.h"
#include "../general.h"

struct sdl_joypad
{
   SDL_Joystick *joypad;
   unsigned num_axes;
   unsigned num_buttons;
   unsigned num_hats;
};

static struct sdl_joypad g_pads[MAX_PLAYERS];

static void sdl_joypad_destroy(void)
{
   unsigned i;
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (g_pads[i].joypad)
         SDL_JoystickClose(g_pads[i].joypad);
   }

   SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
   memset(g_pads, 0, sizeof(g_pads));
}

static bool sdl_joypad_init(void)
{
   unsigned i;
   if (SDL_Init(SDL_INIT_JOYSTICK) < 0)
      return false;

   unsigned num_sticks = SDL_NumJoysticks();
   if (num_sticks > MAX_PLAYERS)
      num_sticks = MAX_PLAYERS;

   for (i = 0; i < num_sticks; i++)
   {
      struct sdl_joypad *pad = &g_pads[i];
      pad->joypad = SDL_JoystickOpen(i);
      if (!pad->joypad)
      {
         RARCH_ERR("Couldn't open SDL joystick #%u.\n", i);
         goto error;
      }

      RARCH_LOG("Opened Joystick: %s (#%u).\n", 
            SDL_JoystickName(i), i);

      pad->num_axes    = SDL_JoystickNumAxes(pad->joypad);
      pad->num_buttons = SDL_JoystickNumButtons(pad->joypad);
      pad->num_hats    = SDL_JoystickNumHats(pad->joypad);
      RARCH_LOG("Joypad has: %u axes, %u buttons, %u hats.\n",
            pad->num_axes, pad->num_buttons, pad->num_hats);
   }

   return true;

error:
   sdl_joypad_destroy();
   return false;
}

static bool sdl_joypad_button(unsigned port, uint16_t joykey)
{
   if (joykey == NO_BTN)
      return false;

   const struct sdl_joypad *pad = &g_pads[port];
   if (!pad->joypad)
      return false;

   // Check hat.
   if (GET_HAT_DIR(joykey))
   {
      uint16_t hat = GET_HAT(joykey);
      if (hat >= pad->num_hats)
         return false;

      Uint8 dir = SDL_JoystickGetHat(pad->joypad, hat);
      switch (GET_HAT_DIR(joykey))
      {
         case HAT_UP_MASK:
            return dir & SDL_HAT_UP;
         case HAT_DOWN_MASK:
            return dir & SDL_HAT_DOWN;
         case HAT_LEFT_MASK:
            return dir & SDL_HAT_LEFT;
         case HAT_RIGHT_MASK:
            return dir & SDL_HAT_RIGHT;
         default:
            return false;
      }
   }
   else // Check the button
   {
      if (joykey < pad->num_buttons && SDL_JoystickGetButton(pad->joypad, joykey))
         return true;

      return false;
   }
}

static int16_t sdl_joypad_axis(unsigned port, uint32_t joyaxis)
{
   if (joyaxis == AXIS_NONE)
      return 0;

   const struct sdl_joypad *pad = &g_pads[port];
   if (!pad->joypad)
      return false;

   Sint16 val = 0;
   if (AXIS_NEG_GET(joyaxis) < pad->num_axes)
   {
      val = SDL_JoystickGetAxis(pad->joypad, AXIS_NEG_GET(joyaxis));

      if (val > 0)
         val = 0;
      else if (val < -0x7fff) // -0x8000 can cause trouble if we later abs() it.
         val = -0x7fff;
   }
   else if (AXIS_POS_GET(joyaxis) < pad->num_axes)
   {
      val = SDL_JoystickGetAxis(pad->joypad, AXIS_POS_GET(joyaxis));

      if (val < 0)
         val = 0;
   }

   return val;
}

static void sdl_joypad_poll(void)
{
   SDL_JoystickUpdate();
}

static bool sdl_joypad_query_pad(unsigned pad)
{
   return pad < MAX_PLAYERS && g_pads[pad].joypad;
}

static const char *sdl_joypad_name(unsigned pad)
{
   if (pad >= MAX_PLAYERS)
      return NULL;

   return SDL_JoystickName(pad);
}

const rarch_joypad_driver_t sdl_joypad = {
   sdl_joypad_init,
   sdl_joypad_query_pad,
   sdl_joypad_destroy,
   sdl_joypad_button,
   sdl_joypad_axis,
   sdl_joypad_poll,
   NULL,
   sdl_joypad_name,
   "sdl",
};


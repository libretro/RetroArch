/*  SSNES - A Super Ninteno Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "driver.h"

#include "SDL.h"
#include <stdbool.h>
#include "general.h"
#include <stdint.h>
#include <stdlib.h>
#include <libsnes.hpp>
#include "ssnes_sdl_input.h"

static void* sdl_input_init(void)
{
   sdl_input_t *sdl = calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

   if (SDL_Init(SDL_INIT_JOYSTICK) < 0)
      return NULL;

   sdl->num_joysticks = SDL_NumJoysticks();
   if (sdl->num_joysticks > 2)
      sdl->num_joysticks = 2;
   for (unsigned i = 0; i < sdl->num_joysticks; i++)
   {
      sdl->joysticks[i] = SDL_JoystickOpen(i);
      if (!sdl->joysticks[i])
      {
         SSNES_ERR("Couldn't open SDL joystick %d\n", i);
         free(sdl);
         SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
         return NULL;
      }

      SSNES_LOG("Opened Joystick: %s\n", SDL_JoystickName(i));
      sdl->num_axes[i] = SDL_JoystickNumAxes(sdl->joysticks[i]);
      sdl->num_buttons[i] = SDL_JoystickNumButtons(sdl->joysticks[i]);
   }

   return sdl;
}


static bool sdl_key_pressed(void *data, int key)
{
   int num_keys;
   Uint8 *keymap = SDL_GetKeyState(&num_keys);

   if (key >= num_keys)
      return false;

   return keymap[key];
}

static bool sdl_is_pressed(sdl_input_t *sdl, int port_num, const struct snes_keybind *key)
{
   if (sdl_key_pressed(sdl, key->key))
      return true;
   if (port_num >= sdl->num_joysticks)
      return false;
   if (key->joykey < sdl->num_buttons[port_num] && SDL_JoystickGetButton(sdl->joysticks[port_num], key->joykey))
      return true;

   if (key->joyaxis != AXIS_NONE)
   {
      if (AXIS_NEG_GET(key->joyaxis) < sdl->num_axes[port_num])
      {
         Sint16 val = SDL_JoystickGetAxis(sdl->joysticks[port_num], AXIS_NEG_GET(key->joyaxis));
         float scaled = (float)val / 0x8000;
         if (scaled < -g_settings.input.axis_threshold)
            return true;
      }
      if (AXIS_POS_GET(key->joyaxis) < sdl->num_axes[port_num])
      {
         Sint16 val = SDL_JoystickGetAxis(sdl->joysticks[port_num], AXIS_POS_GET(key->joyaxis));
         float scaled = (float)val / 0x8000;
         if (scaled > g_settings.input.axis_threshold)
            return true;
      }
   }

   return false;
}

static bool sdl_bind_button_pressed(void *data, int key)
{
   const struct snes_keybind *binds = g_settings.input.binds[0];
   for (int i = 0; binds[i].id != -1; i++)
   {
      if (binds[i].id == key)
         return sdl_is_pressed(data, 0, &binds[i]);
   }
   return false;
}

static int16_t sdl_input_state(void *data, const struct snes_keybind **binds, bool port, unsigned device, unsigned index, unsigned id)
{
   sdl_input_t *sdl = data;
   if (device != SNES_DEVICE_JOYPAD)
      return 0;

   const struct snes_keybind *snes_keybinds = binds[port == SNES_PORT_1 ? 0 : 1];

   // Checks if button is pressed.
   int port_num = port == SNES_PORT_1 ? 0 : 1;
   for (int i = 0; snes_keybinds[i].id != -1; i++)
   {
      if (snes_keybinds[i].id == (int)id)
         return sdl_is_pressed(sdl, port_num, &snes_keybinds[i]);
   }

   return false;
}

static void sdl_input_free(void *data)
{
   if (data)
   {
      // Flush out all pending events.
      SDL_Event event;
      while (SDL_PollEvent(&event));

      sdl_input_t *sdl = data;
      for (int i = 0; i < sdl->num_joysticks; i++)
         SDL_JoystickClose(sdl->joysticks[i]);

      free(data);
      SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
   }
}

static void sdl_input_poll(void *data)
{
   SDL_PumpEvents();
   SDL_Event event;

   sdl_input_t *sdl = data;
   // Search for events...
   while (SDL_PollEvent(&event))
   {
      switch (event.type)
      {
         case SDL_QUIT:
            if (sdl->quitting)
            {
               *sdl->quitting = true;
               return;
            }
            break;

         case SDL_VIDEORESIZE:
            if (sdl->should_resize)
            {
               *sdl->new_width = event.resize.w;
               *sdl->new_height = event.resize.h;
               *sdl->should_resize = true;
            }
            break;
            
         default:
            break;
      }
   }
}

const input_driver_t input_sdl = {
   .init = sdl_input_init,
   .poll = sdl_input_poll,
   .input_state = sdl_input_state,
   .key_pressed = sdl_bind_button_pressed,
   .free = sdl_input_free,
   .ident = "sdl"
};


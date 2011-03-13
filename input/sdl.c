/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
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

   SDL_JoystickEventState(SDL_IGNORE);
   sdl->num_joysticks = SDL_NumJoysticks();

   for (unsigned i = 0; i < MAX_PLAYERS; i++)
   {
      if (g_settings.input.joypad_map[i] == SSNES_NO_JOYPAD)
         continue;

      if (sdl->num_joysticks > g_settings.input.joypad_map[i])
      {
         sdl->joysticks[i] = SDL_JoystickOpen(g_settings.input.joypad_map[i]);
         if (!sdl->joysticks[i])
         {
            SSNES_ERR("Couldn't open SDL joystick #%u on SNES port %u\n", g_settings.input.joypad_map[i], i + 1);
            free(sdl);
            SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
            return NULL;
         }

         SSNES_LOG("Opened Joystick: %s #%u on port %u\n", 
               SDL_JoystickName(g_settings.input.joypad_map[i]), g_settings.input.joypad_map[i], i + 1);
         sdl->num_axes[i] = SDL_JoystickNumAxes(sdl->joysticks[i]);
         sdl->num_buttons[i] = SDL_JoystickNumButtons(sdl->joysticks[i]);
         sdl->num_hats[i] = SDL_JoystickNumHats(sdl->joysticks[i]);
      }
   }

   sdl->use_keyboard = true;

   return sdl;
}


static bool sdl_key_pressed(int key)
{
   int num_keys;
   Uint8 *keymap = SDL_GetKeyState(&num_keys);

   if (key >= num_keys)
      return false;

   return keymap[key];
}

static bool sdl_joykey_pressed(sdl_input_t *sdl, int port_num, uint16_t joykey)
{
   // Check hat.
   if (GET_HAT_DIR(joykey))
   {
      int hat = GET_HAT(joykey);
      if (hat < sdl->num_hats[port_num])
      {
         Uint8 dir = SDL_JoystickGetHat(sdl->joysticks[port_num], hat);
         switch (GET_HAT_DIR(joykey))
         {
            case HAT_UP_MASK:
               if (dir & SDL_HAT_UP)
                  return true;
               break;
            case HAT_DOWN_MASK:
               if (dir & SDL_HAT_DOWN)
                  return true;
               break;
            case HAT_LEFT_MASK:
               if (dir & SDL_HAT_LEFT)
                  return true;
               break;
            case HAT_RIGHT_MASK:
               if (dir & SDL_HAT_RIGHT)
                  return true;
               break;
            default:
               break;
         }
      }
   }
   else // Check the button
   {
      if (joykey < sdl->num_buttons[port_num] && SDL_JoystickGetButton(sdl->joysticks[port_num], joykey))
         return true;
   }
   return false;
}

static bool sdl_axis_pressed(sdl_input_t *sdl, int port_num, uint32_t joyaxis)
{
   if (joyaxis != AXIS_NONE)
   {
      if (AXIS_NEG_GET(joyaxis) < sdl->num_axes[port_num])
      {
         Sint16 val = SDL_JoystickGetAxis(sdl->joysticks[port_num], AXIS_NEG_GET(joyaxis));
         float scaled = (float)val / 0x8000;
         if (scaled < -g_settings.input.axis_threshold)
            return true;
      }
      if (AXIS_POS_GET(joyaxis) < sdl->num_axes[port_num])
      {
         Sint16 val = SDL_JoystickGetAxis(sdl->joysticks[port_num], AXIS_POS_GET(joyaxis));
         float scaled = (float)val / 0x8000;
         if (scaled > g_settings.input.axis_threshold)
            return true;
      }
   }

   return false;
}

static bool sdl_is_pressed(sdl_input_t *sdl, int port_num, const struct snes_keybind *key)
{
   if (sdl->use_keyboard && sdl_key_pressed(key->key))
      return true;
   if (sdl->joysticks[port_num] == NULL)
      return false;
   if (sdl_joykey_pressed(sdl, port_num, key->joykey))
      return true;
   if (sdl_axis_pressed(sdl, port_num, key->joyaxis))
      return true;

   return false;
}

static bool sdl_bind_button_pressed(void *data, int key)
{
   // Only let player 1 use special binds called from main loop.
   const struct snes_keybind *binds = g_settings.input.binds[0];
   for (int i = 0; binds[i].id != -1; i++)
   {
      if (binds[i].id == key)
         return sdl_is_pressed(data, 0, &binds[i]);
   }
   return false;
}

static int16_t sdl_joypad_device_state(sdl_input_t *sdl, const struct snes_keybind **binds, 
      int port_num, unsigned device, unsigned index, unsigned id)
{
   const struct snes_keybind *snes_keybinds = binds[port_num];

   for (int i = 0; snes_keybinds[i].id != -1; i++)
   {
      if (snes_keybinds[i].id == (int)id)
         return sdl_is_pressed(sdl, port_num, &snes_keybinds[i]);
   }

   return false;
}

static int16_t sdl_mouse_device_state(sdl_input_t *sdl, bool port, unsigned id)
{
   // Might implement support for joypad mapping later.
   (void)port;
   switch (id)
   {
      case SNES_DEVICE_ID_MOUSE_LEFT:
         return sdl->mouse_l;
      case SNES_DEVICE_ID_MOUSE_RIGHT:
         return sdl->mouse_r;
      case SNES_DEVICE_ID_MOUSE_X:
         return sdl->mouse_x;
      case SNES_DEVICE_ID_MOUSE_Y:
         return sdl->mouse_y;
      default:
         return 0;
   }
}

// TODO: Missing some controllers, but hey :)
static int16_t sdl_scope_device_state(sdl_input_t *sdl, unsigned id)
{
   switch (id)
   {
      case SNES_DEVICE_ID_SUPER_SCOPE_X:
         return sdl->mouse_x;
      case SNES_DEVICE_ID_SUPER_SCOPE_Y:
         return sdl->mouse_y;
      case SNES_DEVICE_ID_SUPER_SCOPE_TRIGGER:
         return sdl->mouse_l;
      case SNES_DEVICE_ID_SUPER_SCOPE_CURSOR:
         return sdl->mouse_m;
      case SNES_DEVICE_ID_SUPER_SCOPE_TURBO:
         return sdl->mouse_r;
      default:
         return 0;
   }
}

// TODO: Support two players.
static int16_t sdl_justifier_device_state(sdl_input_t *sdl, unsigned index, unsigned id)
{
   if (index == 0)
   {
      switch (id)
      {
         case SNES_DEVICE_ID_JUSTIFIER_X:
            return sdl->mouse_x;
         case SNES_DEVICE_ID_JUSTIFIER_Y:
            return sdl->mouse_y;
         case SNES_DEVICE_ID_JUSTIFIER_TRIGGER:
            return sdl->mouse_l;
         case SNES_DEVICE_ID_JUSTIFIER_START:
            return sdl->mouse_r;
         default:
            return 0;
      }
   }
   else
      return 0;
}

static int16_t sdl_input_state(void *data, const struct snes_keybind **binds, bool port, unsigned device, unsigned index, unsigned id)
{
   switch (device)
   {
      case SNES_DEVICE_JOYPAD:
         return sdl_joypad_device_state(data, binds, (port == SNES_PORT_1) ? 0 : 1, device, index, id);
      case SNES_DEVICE_MULTITAP:
         return sdl_joypad_device_state(data, binds, (port == SNES_PORT_2) ? 1 + index : 0, device, index, id);
      case SNES_DEVICE_MOUSE:
         return sdl_mouse_device_state(data, port, id);
      case SNES_DEVICE_SUPER_SCOPE:
         return sdl_scope_device_state(data, id);
      case SNES_DEVICE_JUSTIFIER:
      case SNES_DEVICE_JUSTIFIERS:
         return sdl_justifier_device_state(data, index, id);

      default:
         return 0;
   }
}

static void sdl_input_free(void *data)
{
   if (data)
   {
      // Flush out all pending events.
      SDL_Event event;
      while (SDL_PollEvent(&event));

      sdl_input_t *sdl = data;
      for (int i = 0; i < MAX_PLAYERS; i++)
      {
         if (sdl->joysticks[i])
            SDL_JoystickClose(sdl->joysticks[i]);
      }

      free(data);
      SDL_QuitSubSystem(SDL_INIT_JOYSTICK);
   }
}

static void sdl_poll_mouse(sdl_input_t *sdl)
{
   int _x, _y;
   Uint8 btn = SDL_GetRelativeMouseState(&_x, &_y);
   sdl->mouse_x = _x;
   sdl->mouse_y = _y;
   sdl->mouse_l = SDL_BUTTON(SDL_BUTTON_LEFT) & btn ? 1 : 0;
   sdl->mouse_r = SDL_BUTTON(SDL_BUTTON_RIGHT) & btn ? 1 : 0;
   sdl->mouse_m = SDL_BUTTON(SDL_BUTTON_MIDDLE) & btn ? 1 : 0;
}

static void sdl_input_poll(void *data)
{
   SDL_PumpEvents();
   SDL_Event event;
   SDL_JoystickUpdate();
   sdl_poll_mouse(data);

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


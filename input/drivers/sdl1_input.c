/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Higor Euripedes
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

#include <boolean.h>
#include <libretro.h>

#include "SDL.h"

#include "../input_keymaps.h"

#include "../../configuration.h"
#include "../../retroarch.h"

#ifdef __linux__
#include "../common/linux_common.h"
#endif

typedef struct sdl1_input
{
#ifdef __linux__
   /* Light sensors aren't exposed through SDL, and they're not usually part of controllers */
   linux_illuminance_sensor_t *illuminance_sensor;
#endif
   int mouse_x;
   int mouse_y;
   int mouse_abs_x;
   int mouse_abs_y;
   int mouse_l;
   int mouse_r;
   int mouse_m;
   int mouse_b4;
   int mouse_b5;
   int mouse_wu;
   int mouse_wd;
} sdl1_input_t;

static void *sdl1_input_init(const char *joypad_driver)
{
   sdl1_input_t    *sdl = (sdl1_input_t*)calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

   input_keymaps_init_keyboard_lut(rarch_key_map_sdl);

   return sdl;
}

static bool sdl1_key_pressed(int key)
{
   int num_keys;
   const uint8_t *keymap = SDL_GetKeyState(&num_keys);
   unsigned sym          = rarch_keysym_lut[(enum retro_key)key];

   if (!key)
      return false;

   if (sym >= (unsigned)num_keys)
      return false;

   return keymap[sym];
}

static int16_t sdl1_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   int16_t       ret = 0;
   sdl1_input_t *sdl = (sdl1_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;

            if (!keyboard_mapping_blocked)
            {
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if (RETRO_KEYBIND_VALID(&binds[port][i]))
                  {
                     if (     (RETRO_KEYBIND_KEY(&binds[port][i]) && RETRO_KEYBIND_KEY(&binds[port][i]) < RETROK_LAST)
                           && sdl1_key_pressed(RETRO_KEYBIND_KEY(&binds[port][i])))
                        ret |= (1 << i);
                  }
               }
            }

            return ret;
         }

         if (id < RARCH_BIND_LIST_END)
         {
            if (RETRO_KEYBIND_VALID(&binds[port][id]))
            {
               if (     (RETRO_KEYBIND_KEY(&binds[port][id]) && RETRO_KEYBIND_KEY(&binds[port][id]) < RETROK_LAST)
                     && sdl1_key_pressed(RETRO_KEYBIND_KEY(&binds[port][id]))
                     && (id == RARCH_GAME_FOCUS_TOGGLE || !keyboard_mapping_blocked)
                  )
                  return 1;
            }
         }
         break;
      case RETRO_DEVICE_ANALOG:
         {
            int id_minus_key      = 0;
            int id_plus_key       = 0;
            unsigned id_minus     = 0;
            unsigned id_plus      = 0;
            bool id_plus_valid    = false;
            bool id_minus_valid   = false;

            input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

            id_minus_valid        = RETRO_KEYBIND_VALID(&binds[port][id_minus]);
            id_plus_valid         = RETRO_KEYBIND_VALID(&binds[port][id_plus]);
            id_minus_key          = RETRO_KEYBIND_KEY(&binds[port][id_minus]);
            id_plus_key           = RETRO_KEYBIND_KEY(&binds[port][id_plus]);

            if (id_plus_valid && id_plus_key && id_plus_key < RETROK_LAST)
            {
               if (sdl1_key_pressed(id_plus_key))
                  ret = 0x7fff;
            }
            if (id_minus_valid && id_minus_key && id_minus_key < RETROK_LAST)
            {
               if (sdl1_key_pressed(id_minus_key))
                  ret += -0x7fff;
            }
         }
         return ret;
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         if (config_get_ptr()->uints.input_mouse_index[ port ] == 0)
         {
            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_LEFT:
                  return sdl->mouse_l;
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
                  return sdl->mouse_r;
               case RETRO_DEVICE_ID_MOUSE_WHEELUP:
                  return sdl->mouse_wu;
               case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
                  return sdl->mouse_wd;
               case RETRO_DEVICE_ID_MOUSE_X:
                  return sdl->mouse_x;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  return sdl->mouse_y;
               case RETRO_DEVICE_ID_MOUSE_MIDDLE:
                  return sdl->mouse_m;
               case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
                  return sdl->mouse_b4;
               case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
                  return sdl->mouse_b5;
            }
         }
         break;
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         if (idx == 0)
         {
            video_viewport_t vp         = {0};
            bool screen                 = device ==
               RARCH_DEVICE_POINTER_SCREEN;
            int16_t res_x               = 0;
            int16_t res_y               = 0;
            int16_t res_screen_x        = 0;
            int16_t res_screen_y        = 0;

            if (video_driver_translate_coord_viewport_confined_wrap(
                        &vp, sdl->mouse_abs_x, sdl->mouse_abs_y,
                        &res_x, &res_y, &res_screen_x, &res_screen_y))
            {
               if (screen)
               {
                  res_x = res_screen_x;
                  res_y = res_screen_y;
               }

               switch (id)
               {
                  case RETRO_DEVICE_ID_POINTER_X:
                     return res_x;
                  case RETRO_DEVICE_ID_POINTER_Y:
                     return res_y;
                  case RETRO_DEVICE_ID_POINTER_PRESSED:
                     return sdl->mouse_l;
                  case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
                     return input_driver_pointer_is_offscreen(res_x, res_y);
               }
            }
         }
         break;
      case RETRO_DEVICE_KEYBOARD:
         return (id && id < RETROK_LAST) && sdl1_key_pressed(id);
      /* TODO: update button binds to match other input drivers */
      case RETRO_DEVICE_LIGHTGUN:
      {
         video_viewport_t vp         = {0};
         int16_t res_x               = 0;
         int16_t res_y               = 0;
         int16_t res_screen_x        = 0;
         int16_t res_screen_y        = 0;

         if (video_driver_translate_coord_viewport_wrap(
                     &vp, sdl->mouse_abs_x, sdl->mouse_abs_y,
                     &res_x, &res_y, &res_screen_x, &res_screen_y))

         switch (id)
         {
            case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
               return res_x;
            case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
               return res_y;
            case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
               return input_driver_pointer_is_offscreen(res_x, res_y);
            case RETRO_DEVICE_ID_LIGHTGUN_X:
               return sdl->mouse_x;
            case RETRO_DEVICE_ID_LIGHTGUN_Y:
               return sdl->mouse_y;
            case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
               return sdl->mouse_l;
            case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
               return sdl->mouse_m;
            case RETRO_DEVICE_ID_LIGHTGUN_START:
               return sdl->mouse_r;
            case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
               return sdl->mouse_l && sdl->mouse_r;
         }
         break;
      }
   }

   return 0;
}

static void sdl1_input_free(void *data)
{
   SDL_Event event;
   sdl1_input_t *sdl = (sdl1_input_t*)data;

   if (!sdl)
      return;

   /* Flush out all pending events. */
   while (SDL_PollEvent(&event));

#ifdef __linux__
   linux_close_illuminance_sensor(sdl->illuminance_sensor); /* noop if NULL */
#endif

   free(data);
}

static bool sdl1_set_sensor_state(void *data, unsigned port, enum retro_sensor_action action, unsigned rate)
{
   sdl1_input_t *sdl = (sdl1_input_t*)data;

   if (!sdl)
      return false;

   switch (action)
   {
      case RETRO_SENSOR_ILLUMINANCE_DISABLE:
#ifdef __linux__
         /* If already disabled, then do nothing */
         linux_close_illuminance_sensor(sdl->illuminance_sensor); /* noop if NULL */
         sdl->illuminance_sensor = NULL;
#endif
      case RETRO_SENSOR_GYROSCOPE_DISABLE:
      case RETRO_SENSOR_ACCELEROMETER_DISABLE:
         /** Unimplemented sensor actions that probably shouldn't fail */
         return true;

      case RETRO_SENSOR_ILLUMINANCE_ENABLE:
#ifdef __linux__
         /* Unsupported on non-Linux platforms */
         if (sdl->illuminance_sensor)
            /* If we already have a sensor, just set the rate */
            linux_set_illuminance_sensor_rate(sdl->illuminance_sensor, rate);
         else
            sdl->illuminance_sensor = linux_open_illuminance_sensor(rate);

         return sdl->illuminance_sensor != NULL;
#endif
      default:
         break;
   }

   return false;
}

static float sdl1_get_sensor_input(void *data, unsigned port, unsigned id)
{
   sdl1_input_t *sdl = (sdl1_input_t*)data;

   if (!sdl)
      return 0.0f;

   switch (id)
   {
      case RETRO_SENSOR_ILLUMINANCE:
#ifdef __linux__
         if (sdl->illuminance_sensor)
            return linux_get_illuminance_reading(sdl->illuminance_sensor);
#endif
      /* Unsupported on non-Linux platforms */
      default:
         break;
   }

   return 0.0f;
}

static void sdl1_poll_mouse(sdl1_input_t *sdl)
{
   Uint8 btn     = SDL_GetRelativeMouseState(&sdl->mouse_x, &sdl->mouse_y);

   SDL_GetMouseState(&sdl->mouse_abs_x, &sdl->mouse_abs_y);

   sdl->mouse_l  = (SDL_BUTTON(SDL_BUTTON_LEFT)      & btn) ? 1 : 0;
   sdl->mouse_r  = (SDL_BUTTON(SDL_BUTTON_RIGHT)     & btn) ? 1 : 0;
   sdl->mouse_m  = (SDL_BUTTON(SDL_BUTTON_MIDDLE)    & btn) ? 1 : 0;
   sdl->mouse_b4 = (SDL_BUTTON(SDL_BUTTON_X1)        & btn) ? 1 : 0;
   sdl->mouse_b5 = (SDL_BUTTON(SDL_BUTTON_X2)        & btn) ? 1 : 0;
   sdl->mouse_wu = (SDL_BUTTON(SDL_BUTTON_WHEELUP)   & btn) ? 1 : 0;
   sdl->mouse_wd = (SDL_BUTTON(SDL_BUTTON_WHEELDOWN) & btn) ? 1 : 0;
}

static void sdl1_input_poll(void *data)
{
   SDL_Event event;
   sdl1_input_t *sdl = (sdl1_input_t*)data;

   SDL_PumpEvents();

   sdl1_poll_mouse(sdl);

   while (SDL_PeepEvents(&event, 1,
            SDL_GETEVENT, SDL_KEYEVENTMASK) > 0)
   {
      if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
      {
         uint16_t mod  = 0;
         unsigned code = input_keymaps_translate_keysym_to_rk(
               event.key.keysym.sym);

         if (event.key.keysym.mod & KMOD_SHIFT)
            mod |= RETROKMOD_SHIFT;

         if (event.key.keysym.mod & KMOD_CTRL)
            mod |= RETROKMOD_CTRL;

         if (event.key.keysym.mod & KMOD_ALT)
            mod |= RETROKMOD_ALT;

         if (event.key.keysym.mod & KMOD_NUM)
            mod |= RETROKMOD_NUMLOCK;

         if (event.key.keysym.mod & KMOD_CAPS)
            mod |= RETROKMOD_CAPSLOCK;

         {
            /* Use key+mod ASCII so Shift does not leak as '?' and capitals work. */
            uint32_t character = input_keymaps_translate_rk_to_ascii(
                  (enum retro_key)code, (enum retro_mod)mod);

            input_keyboard_event(event.type == SDL_KEYDOWN, code,
                  character, mod, RETRO_DEVICE_KEYBOARD);
         }
      }
   }
}

static uint64_t sdl1_get_capabilities(void *data)
{
   return
           (1 << RETRO_DEVICE_JOYPAD)
         | (1 << RETRO_DEVICE_MOUSE)
         | (1 << RETRO_DEVICE_KEYBOARD)
         | (1 << RETRO_DEVICE_LIGHTGUN)
         | (1 << RETRO_DEVICE_POINTER)
         | (1 << RETRO_DEVICE_ANALOG);
}

input_driver_t input_sdl1 = {
   sdl1_input_init,
   sdl1_input_poll,
   sdl1_input_state,
   sdl1_input_free,
   sdl1_set_sensor_state,
   sdl1_get_sensor_input,
   sdl1_get_capabilities,
   "sdl",
   NULL,                   /* grab_mouse */
   NULL,
   NULL
};

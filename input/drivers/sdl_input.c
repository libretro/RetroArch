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
#include <string/stdstring.h>
#include <libretro.h>

#include "SDL.h"

#include "../input_keymaps.h"

#include "../../configuration.h"
#include "../../retroarch.h"
#include "../../verbosity.h"
#include "../../tasks/tasks_internal.h"

#ifdef HAVE_SDL2
#include "../../gfx/common/sdl2_common.h"
#endif

#ifdef WEBOS
#include <SDL_webOS.h>
#include <dlfcn.h>
#endif

/* TODO/FIXME -
 * fix game focus toggle */

typedef struct sdl_input
{
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
   int mouse_wl;
   int mouse_wr;
} sdl_input_t;

#ifdef WEBOS
enum sdl_webos_special_key {
   sdl_webos_spkey_back,
   sdl_webos_spkey_size,
};

static uint8_t sdl_webos_special_keymap[sdl_webos_spkey_size] = {0};
#endif

static void *sdl_input_init(const char *joypad_driver)
{
   sdl_input_t     *sdl = (sdl_input_t*)calloc(1, sizeof(*sdl));
   if (!sdl)
      return NULL;

   input_keymaps_init_keyboard_lut(rarch_key_map_sdl);

   return sdl;
}

static bool sdl_key_pressed(int key)
{
   int num_keys;
#ifdef HAVE_SDL2
   const uint8_t *keymap = SDL_GetKeyboardState(&num_keys);
   unsigned sym          = SDL_GetScancodeFromKey(rarch_keysym_lut[(enum retro_key)key]);
#else
   const uint8_t *keymap = SDL_GetKeyState(&num_keys);
   unsigned sym          = rarch_keysym_lut[(enum retro_key)key];
#endif

#ifdef WEBOS
   if (   (key == RETROK_BACKSPACE )
       && sdl_webos_special_keymap[sdl_webos_spkey_back])
   {
      /* Reset to unpressed state */
      sdl_webos_special_keymap[sdl_webos_spkey_back] = 0;
      return true;
   }
   if (key == RETROK_F1 && keymap[SDL_WEBOS_SCANCODE_EXIT])
      return true;
   if (key == RETROK_x && keymap[SDL_WEBOS_SCANCODE_RED])
      return true;
   if (key == RETROK_z && keymap[SDL_WEBOS_SCANCODE_GREEN])
      return true;
   if (key == RETROK_s && keymap[SDL_WEBOS_SCANCODE_YELLOW])
      return true;
   if (key == RETROK_a && keymap[SDL_WEBOS_SCANCODE_BLUE])
      return true;
#endif

   if (sym >= (unsigned)num_keys)
      return false;

   return keymap[sym];
}

static int16_t sdl_input_state(
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
   int16_t      ret = 0;
   sdl_input_t *sdl = (sdl_input_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;

            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               if (binds[port][i].valid)
                  if (sdl_key_pressed(binds[port][i].key))
                     ret |= (1 << i);
            }

            return ret;
         }

         if (id < RARCH_BIND_LIST_END)
         {
            if (binds[port][id].valid)
               if (sdl_key_pressed(binds[port][id].key))
                  return 1;
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

            id_minus_valid        = binds[port][id_minus].valid;
            id_plus_valid         = binds[port][id_plus].valid;
            id_minus_key          = binds[port][id_minus].key;
            id_plus_key           = binds[port][id_plus].key;

            if (id_plus_valid && id_plus_key < RETROK_LAST)
            {
               if (sdl_key_pressed(id_plus_key))
                  ret = 0x7fff;
            }
            if (id_minus_valid && id_minus_key < RETROK_LAST)
            {
               if (sdl_key_pressed(id_minus_key))
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
#ifdef WEBOS
               case RETRO_DEVICE_ID_MOUSE_WHEELUP:
                  /* Note: webOS wheel is reversed */
                  if (sdl->mouse_wd != 0)
                  {
                      sdl->mouse_wd = 0;
                      return 1;
                  }
                  return 0;
               case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
                  if (sdl->mouse_wu != 0)
                  {
                      sdl->mouse_wu = 0;
                      return 1;
                  }
                  return 0;
               case RETRO_DEVICE_ID_MOUSE_X:
                  return sdl->mouse_abs_x;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  return sdl->mouse_abs_y;
#else
               case RETRO_DEVICE_ID_MOUSE_WHEELUP:
                  return sdl->mouse_wu;
               case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
                  return sdl->mouse_wd;
               case RETRO_DEVICE_ID_MOUSE_X:
                  return sdl->mouse_x;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  return sdl->mouse_y;
#endif
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
            struct video_viewport vp;
            bool screen                 = device == 
               RARCH_DEVICE_POINTER_SCREEN;
            const int edge_detect       = 32700;
            bool inside                 = false;
            int16_t res_x               = 0;
            int16_t res_y               = 0;
            int16_t res_screen_x        = 0;
            int16_t res_screen_y        = 0;

            vp.x                        = 0;
            vp.y                        = 0;
            vp.width                    = 0;
            vp.height                   = 0;
            vp.full_width               = 0;
            vp.full_height              = 0;

            if (video_driver_translate_coord_viewport_wrap(
                        &vp, sdl->mouse_abs_x, sdl->mouse_abs_y,
                        &res_x, &res_y, &res_screen_x, &res_screen_y))
            {
               if (screen)
               {
                  res_x = res_screen_x;
                  res_y = res_screen_y;
               }

               inside =    (res_x >= -edge_detect) 
                  && (res_y >= -edge_detect)
                  && (res_x <= edge_detect)
                  && (res_y <= edge_detect);

               switch (id)
               {
                  case RETRO_DEVICE_ID_POINTER_X:
                     return res_x;
                  case RETRO_DEVICE_ID_POINTER_Y:
                     return res_y;
                  case RETRO_DEVICE_ID_POINTER_PRESSED:
                     return sdl->mouse_l;
                  case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
                     return !inside;
               }
            }
         }
         break;
      case RETRO_DEVICE_KEYBOARD:
         return (id < RETROK_LAST) && sdl_key_pressed(id);
      case RETRO_DEVICE_LIGHTGUN:
         switch (id)
         {
            case RETRO_DEVICE_ID_LIGHTGUN_X:
               return sdl->mouse_x;
            case RETRO_DEVICE_ID_LIGHTGUN_Y:
               return sdl->mouse_y;
            case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
               return sdl->mouse_l;
            case RETRO_DEVICE_ID_LIGHTGUN_CURSOR:
               return sdl->mouse_m;
            case RETRO_DEVICE_ID_LIGHTGUN_TURBO:
               return sdl->mouse_r;
            case RETRO_DEVICE_ID_LIGHTGUN_START:
               return sdl->mouse_m && sdl->mouse_r;
            case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
               return sdl->mouse_m && sdl->mouse_l;
         }
         break;
   }

   return 0;
}

static void sdl_input_free(void *data)
{
#ifndef HAVE_SDL2
   SDL_Event event;
#endif
   sdl_input_t *sdl = (sdl_input_t*)data;

   if (!data)
      return;

   /* Flush out all pending events. */
#ifdef HAVE_SDL2
   SDL_FlushEvents(SDL_FIRSTEVENT, SDL_LASTEVENT);
#else
   while (SDL_PollEvent(&event));
#endif

   free(data);
}

#ifdef HAVE_SDL2
static void sdl2_grab_mouse(void *data, bool state)
{
   sdl2_video_t *video_ptr = NULL;

   if (string_is_not_equal(video_driver_get_ident(), "sdl2"))
      return;

   video_ptr = (sdl2_video_t*)video_driver_get_ptr();

   if (!video_ptr)
      return;

   SDL_SetWindowGrab(video_ptr->window, state ? SDL_TRUE : SDL_FALSE);
}
#endif

static void sdl_poll_mouse(sdl_input_t *sdl)
{
   Uint8 btn = SDL_GetRelativeMouseState(&sdl->mouse_x, &sdl->mouse_y);

   SDL_GetMouseState(&sdl->mouse_abs_x, &sdl->mouse_abs_y);

   sdl->mouse_l  = (SDL_BUTTON(SDL_BUTTON_LEFT)      & btn) ? 1 : 0;
   sdl->mouse_r  = (SDL_BUTTON(SDL_BUTTON_RIGHT)     & btn) ? 1 : 0;
   sdl->mouse_m  = (SDL_BUTTON(SDL_BUTTON_MIDDLE)    & btn) ? 1 : 0;
   sdl->mouse_b4 = (SDL_BUTTON(SDL_BUTTON_X1)        & btn) ? 1 : 0;
   sdl->mouse_b5 = (SDL_BUTTON(SDL_BUTTON_X2)        & btn) ? 1 : 0;
#ifndef HAVE_SDL2
   sdl->mouse_wu = (SDL_BUTTON(SDL_BUTTON_WHEELUP)   & btn) ? 1 : 0;
   sdl->mouse_wd = (SDL_BUTTON(SDL_BUTTON_WHEELDOWN) & btn) ? 1 : 0;
#endif
}

static void sdl_input_poll(void *data)
{
   SDL_Event event;
   sdl_input_t *sdl = (sdl_input_t*)data;

   SDL_PumpEvents();

   sdl_poll_mouse(sdl);

#ifdef HAVE_SDL2
   while (SDL_PeepEvents(&event, 1,
            SDL_GETEVENT, SDL_KEYDOWN, SDL_MOUSEWHEEL) > 0)
#else
   while (SDL_PeepEvents(&event, 1,
            SDL_GETEVENT, SDL_KEYEVENTMASK) > 0)
#endif
   {
      if (event.type == SDL_KEYDOWN || event.type == SDL_KEYUP)
      {
         uint16_t mod  = 0;
         unsigned code = input_keymaps_translate_keysym_to_rk(
               event.key.keysym.sym);
#ifdef WEBOS
         switch ((int) event.key.keysym.scancode)
         {
            case SDL_WEBOS_SCANCODE_BACK:
               /* Because webOS is sending DOWN/UP at the same time, 
                  we save this flag for later */
               sdl_webos_special_keymap[sdl_webos_spkey_back] |= event.type == SDL_KEYDOWN;
               code = RETROK_BACKSPACE;
               break;
            case SDL_WEBOS_SCANCODE_RED:
               code = RETROK_x;
               break;
            case SDL_WEBOS_SCANCODE_GREEN:
               code = RETROK_z;
               break;
            case SDL_WEBOS_SCANCODE_YELLOW:
               code = RETROK_s;
               break;
            case SDL_WEBOS_SCANCODE_BLUE:
               code = RETROK_a;
               break;
            case SDL_WEBOS_SCANCODE_EXIT:
               code = RETROK_F1;
               break;
            default:
               break;
         }

         /* Disable cursor when using the buttons */
         if (code && code != RETROK_RETURN)
            SDL_webOSCursorVisibility(0);
#endif

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

         input_keyboard_event(event.type == SDL_KEYDOWN, code, code, mod,
               RETRO_DEVICE_KEYBOARD);
      }
#ifdef HAVE_SDL2
      else if (event.type == SDL_MOUSEWHEEL)
      {
         sdl->mouse_wu = event.wheel.y < 0;
         sdl->mouse_wd = event.wheel.y > 0;
         sdl->mouse_wl = event.wheel.x < 0;
         sdl->mouse_wr = event.wheel.x > 0;
         break;
      }
#endif
   }
}

static uint64_t sdl_get_capabilities(void *data)
{
   uint64_t caps = 0;

   caps |= (1 << RETRO_DEVICE_JOYPAD);
   caps |= (1 << RETRO_DEVICE_MOUSE);
   caps |= (1 << RETRO_DEVICE_KEYBOARD);
   caps |= (1 << RETRO_DEVICE_LIGHTGUN);
   caps |= (1 << RETRO_DEVICE_POINTER);
   caps |= (1 << RETRO_DEVICE_ANALOG);

   return caps;
}

input_driver_t input_sdl = {
   sdl_input_init,
   sdl_input_poll,
   sdl_input_state,
   sdl_input_free,
   NULL,
   NULL,
   sdl_get_capabilities,
#ifdef HAVE_SDL2
   "sdl2",
   sdl2_grab_mouse,
#else
   "sdl",
   NULL,                   /* grab_mouse */
#endif
   NULL
};

#ifdef WEBOS
SDL_bool SDL_webOSCursorVisibility(SDL_bool visible)
{
   static SDL_bool (*fn)(SDL_bool visible) = NULL;
   static bool dlsym_called = false;
   if (!dlsym_called)
   {
      fn           = dlsym(RTLD_NEXT, "SDL_webOSCursorVisibility");
      dlsym_called = true;
   }
   if (!fn)
   {
      SDL_ShowCursor(SDL_DISABLE);
      SDL_ShowCursor(SDL_ENABLE);
      return SDL_TRUE;
   }
   return fn(visible);
}
#endif

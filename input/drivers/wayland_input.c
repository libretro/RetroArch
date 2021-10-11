/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2015 - Hans-Kristian Arntzen
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
#include <string.h>

#include <fcntl.h>
#include <unistd.h>

#include <limits.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/stat.h>

#include <wayland-client.h>
#include <wayland-cursor.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <file/file_path.h>
#include <compat/strl.h>
#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#include "../input_keymaps.h"

#include "../common/linux_common.h"
#include "../common/wayland_common.h"

#include "../../retroarch.h"
#include "../../verbosity.h"

/* TODO/FIXME -
 * fix game focus toggle */

/* Forward declaration */

void flush_wayland_fd(void *data);

static bool wayland_context_gettouchpos(
      gfx_ctx_wayland_data_t *wl,
      unsigned id,
      unsigned* touch_x, unsigned* touch_y)
{
   if (id >= MAX_TOUCHES)
       return false;
   *touch_x = wl->active_touch_positions[id].x;
   *touch_y = wl->active_touch_positions[id].y;
   return wl->active_touch_positions[id].active;
}

static void input_wl_poll(void *data)
{
   int id;
   unsigned touch_x             = 0;
   unsigned touch_y             = 0;
   input_ctx_wayland_data_t *wl = (input_ctx_wayland_data_t*)data;
   if (!wl)
      return;

   flush_wayland_fd(wl);

   wl->mouse.delta_x = wl->mouse.x - wl->mouse.last_x;
   wl->mouse.delta_y = wl->mouse.y - wl->mouse.last_y;
   wl->mouse.last_x  = wl->mouse.x;
   wl->mouse.last_y  = wl->mouse.y;

   if (!wl->mouse.focus)
   {
      wl->mouse.delta_x = 0;
      wl->mouse.delta_y = 0;
   }

   for (id = 0; id < MAX_TOUCHES; id++)
   {
      if (wayland_context_gettouchpos(wl->gfx, id, &touch_x, &touch_y))
         wl->touches[id].active = true;
      else
         wl->touches[id].active = false;
      wl->touches[id].x         = touch_x;
      wl->touches[id].y         = touch_y;
   }
}

static int16_t input_wl_touch_state(input_ctx_wayland_data_t *wl,
      unsigned idx, unsigned id, bool screen)
{
   struct video_viewport vp;

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

   if (idx > MAX_TOUCHES)
      return 0;

   if (!(video_driver_translate_coord_viewport_wrap(&vp,
         wl->touches[idx].x, wl->touches[idx].y,
         &res_x, &res_y, &res_screen_x, &res_screen_y)))
      return 0;

   if (screen)
   {
      res_x = res_screen_x;
      res_y = res_screen_y;
   }

   inside = (res_x >= -0x7fff) && (res_y >= -0x7fff);

   if (!inside)
      return 0;

   switch (id)
   {
      case RETRO_DEVICE_ID_POINTER_X:
         return res_x;
      case RETRO_DEVICE_ID_POINTER_Y:
         return res_y;
      case RETRO_DEVICE_ID_POINTER_PRESSED:
         return wl->touches[idx].active;
   }

   return 0;
}

static int16_t input_wl_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const struct retro_keybind **binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned idx,
      unsigned id)
{
   input_ctx_wayland_data_t *wl = (input_ctx_wayland_data_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;

            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               if (binds[port][i].valid)
               {
                  /*if (wl_mouse_button_pressed(udev, port, binds[port][i].mbutton))
                     ret |= (1 << i);
                  */

                  /* TODO: support custom mouse-to-retropad binds */
               }
            }

            if(!keyboard_mapping_blocked)
            {
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if (binds[port][i].valid)
                  {
                     if (BIT_GET(wl->key_state,
                              rarch_keysym_lut[binds[port][i].key]) )
                        ret |= (1 << i);
                  }
               }
            }

            return ret;
         }

         if (id < RARCH_BIND_LIST_END)
         {
            if (binds[port][id].valid && binds[port][id].key < RETROK_LAST)
            {
               if(id != RARCH_GAME_FOCUS_TOGGLE && !keyboard_mapping_blocked)
               {
                  if(BIT_GET(wl->key_state, rarch_keysym_lut[binds[port][id].key]))
                     return 1;
               }
               else if(id == RARCH_GAME_FOCUS_TOGGLE)
               {
                  if(BIT_GET(wl->key_state, rarch_keysym_lut[binds[port][id].key]))
                     return 1;
               }
            
               /* TODO: support default mouse-to-retropad bindings */
               /* else if (wl_mouse_button_pressed(udev, port, binds[port][i].mbutton))
                  return 1;
               */
            }
         }
         break;
      case RETRO_DEVICE_ANALOG:
         if (binds[port])
         {
            int id_minus_key      = 0;
            int id_plus_key       = 0;
            unsigned id_minus     = 0;
            unsigned id_plus      = 0;
            int16_t ret           = 0;
            bool id_plus_valid    = false;
            bool id_minus_valid   = false;

            input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

            id_minus_valid        = binds[port][id_minus].valid;
            id_plus_valid         = binds[port][id_plus].valid;
            id_minus_key          = binds[port][id_minus].key;
            id_plus_key           = binds[port][id_plus].key;

            if (id_plus_valid && id_plus_key < RETROK_LAST)
            {
               unsigned sym = rarch_keysym_lut[(enum retro_key)id_plus_key];
               if (BIT_GET(wl->key_state, sym))
                  ret = 0x7fff;
            }
            if (id_minus_valid && id_minus_key < RETROK_LAST)
            {
               unsigned sym = rarch_keysym_lut[(enum retro_key)id_minus_key];
               if (BIT_GET(wl->key_state, sym))
                  ret += -0x7fff;
            }

            return ret;
         }
         break;
      case RETRO_DEVICE_KEYBOARD:
         return id < RETROK_LAST && 
            BIT_GET(wl->key_state, rarch_keysym_lut[(enum retro_key)id]);
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         {
            bool screen = device == RARCH_DEVICE_MOUSE_SCREEN;
            if (port > 0) return 0; /* TODO: support mouse on additional ports */
            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_X:
                  return screen ? wl->mouse.x : wl->mouse.delta_x;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  return screen ? wl->mouse.y : wl->mouse.delta_y;
               case RETRO_DEVICE_ID_MOUSE_LEFT:
                  return wl->mouse.left;
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
                  return wl->mouse.right;
               case RETRO_DEVICE_ID_MOUSE_MIDDLE:
                  return wl->mouse.middle;
                  /* TODO: Rest of the mouse inputs. */
            }
         }
         break;
      case RETRO_DEVICE_POINTER:
         /* TODO: support pointers on additional ports */
         if (idx == 0)
         {
            struct video_viewport vp;
            bool screen                 = 
               (device == RARCH_DEVICE_POINTER_SCREEN);
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

            if (video_driver_translate_coord_viewport_wrap(&vp,
                        wl->mouse.x, wl->mouse.y,
                        &res_x, &res_y, &res_screen_x, &res_screen_y))
            {
               if (screen)
               {
                  res_x = res_screen_x;
                  res_y = res_screen_y;
               }

               inside = (res_x >= -0x7fff) && (res_y >= -0x7fff);

               switch (id)
               {
                  case RETRO_DEVICE_ID_POINTER_X:
                     if (inside)
                        return res_x;
                     break;
                  case RETRO_DEVICE_ID_POINTER_Y:
                     if (inside)
                        return res_y;
                     break;
                  case RETRO_DEVICE_ID_POINTER_PRESSED:
                     return wl->mouse.left;
                  case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
                     return !inside;
                  default:
                     break;
               }
            }
         }
         break;
      case RARCH_DEVICE_POINTER_SCREEN:
         if (port > 0 ) return 0; /* TODO: support pointers on additional ports */
         if (idx < MAX_TOUCHES)
            return input_wl_touch_state(wl, idx, id,
                  device == RARCH_DEVICE_POINTER_SCREEN);
         break;
      case RETRO_DEVICE_LIGHTGUN:
         if (port > 0 ) return 0; /* TODO: support lightguns on additional ports */
         switch (id)
         { 
            case RETRO_DEVICE_ID_LIGHTGUN_X: /* TODO: migrate to RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X */
               return wl->mouse.delta_x; /* deprecated relative coordinates */
            case RETRO_DEVICE_ID_LIGHTGUN_Y: /* TODO: migrate to RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y */
               return wl->mouse.delta_y; /* deprecated relative coordinates */
            case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN: /* TODO: implement this status check*/
               return 0;
            case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
               return wl->mouse.left;
            case RETRO_DEVICE_ID_LIGHTGUN_RELOAD: /* forced/faked off-screen shot */
               return wl->mouse.middle;
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_A: /* TODO */
               return 0;
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_B: /* TODO */
               return 0;
            case RETRO_DEVICE_ID_LIGHTGUN_START:
               return wl->mouse.right;
            case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
               return wl->mouse.left && wl->mouse.right;
            case RETRO_DEVICE_ID_LIGHTGUN_AUX_C: /* TODO */
               return 0;
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP: /* TODO */
               return 0;
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN: /* TODO */
               return 0;
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT: /* TODO */
               return 0;
            case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT: /* TODO */
               return 0;
         }
         break;
   }

   return 0;
}

static void input_wl_free(void *data) { }

bool input_wl_init(void *data, const char *joypad_name)
{
   input_ctx_wayland_data_t *wl = (input_ctx_wayland_data_t*)data;

   if (!wl)
      return false;

   input_keymaps_init_keyboard_lut(rarch_key_map_linux);

   return true;
}

static uint64_t input_wl_get_capabilities(void *data)
{
   return
      (1 << RETRO_DEVICE_JOYPAD)   |
      (1 << RETRO_DEVICE_ANALOG)   |
      (1 << RETRO_DEVICE_KEYBOARD) |
      (1 << RETRO_DEVICE_MOUSE)    |
      (1 << RETRO_DEVICE_LIGHTGUN);
}

input_driver_t input_wayland = {
   NULL,
   input_wl_poll,
   input_wl_state,
   input_wl_free,
   NULL,
   NULL,
   input_wl_get_capabilities,
   "wayland",
   NULL,                         /* grab_mouse */
   NULL
};

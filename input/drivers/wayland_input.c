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

#include "../input_driver.h"
#include "../input_keymaps.h"

#include "../../gfx/video_driver.h"
#include "../common/linux_common.h"

#include "../../gfx/common/wayland_common.h"

#include "../../verbosity.h"

/* Forward declaration */

void flush_wayland_fd(void *data);

static int16_t input_wl_mouse_state(input_ctx_wayland_data_t *wl, unsigned id, bool screen)
{
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

   return 0;
}

static int16_t input_wl_lightgun_state(input_ctx_wayland_data_t *wl, unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_LIGHTGUN_X:
         return wl->mouse.delta_x;
      case RETRO_DEVICE_ID_LIGHTGUN_Y:
         return wl->mouse.delta_y;
      case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
         return wl->mouse.left;
      case RETRO_DEVICE_ID_LIGHTGUN_CURSOR:
         return wl->mouse.middle;
      case RETRO_DEVICE_ID_LIGHTGUN_TURBO:
         return wl->mouse.right;
      case RETRO_DEVICE_ID_LIGHTGUN_START:
         return wl->mouse.middle && wl->mouse.right; 
      case RETRO_DEVICE_ID_LIGHTGUN_PAUSE:
         return wl->mouse.middle && wl->mouse.left; 
   }

   return 0;
}

static void input_wl_poll(void *data)
{
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

   if (wl->joypad)
      wl->joypad->poll();
}

static int16_t input_wl_analog_pressed(input_ctx_wayland_data_t *wl,
      const struct retro_keybind *binds,
      unsigned idx, unsigned id)
{
   unsigned id_minus     = 0;
   unsigned id_plus      = 0;
   int16_t pressed_minus = 0;
   int16_t pressed_plus  = 0;

   input_conv_analog_id_to_bind_id(idx, id, &id_minus, &id_plus);

   if (binds 
         && binds[id_minus].valid 
         && (id_minus < RARCH_BIND_LIST_END)
         && BIT_GET(wl->key_state, rarch_keysym_lut[binds[id_minus].key])
      )
      pressed_minus = -0x7fff;
   if (binds 
         && binds[id_plus].valid 
         && (id_plus < RARCH_BIND_LIST_END)
         && BIT_GET(wl->key_state, rarch_keysym_lut[binds[id_plus].key])
      )
      pressed_plus = 0x7fff;

   return pressed_plus + pressed_minus;
}

static bool input_wl_state_kb(input_ctx_wayland_data_t *wl,
      const struct retro_keybind **binds,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   unsigned bit = rarch_keysym_lut[(enum retro_key)id];
   return id < RETROK_LAST && BIT_GET(wl->key_state, bit);
}

static int16_t input_wl_pointer_state(input_ctx_wayland_data_t *wl,
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

   if (!(video_driver_translate_coord_viewport_wrap(&vp,
               wl->mouse.x, wl->mouse.y,
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
         return wl->mouse.left;
   }

   return 0;
}

static int16_t input_wl_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds,
      unsigned port, unsigned device, unsigned idx, unsigned id)
{
   int16_t ret                  = 0;
   input_ctx_wayland_data_t *wl = (input_ctx_wayland_data_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id < RARCH_BIND_LIST_END)
            ret = BIT_GET(wl->key_state, rarch_keysym_lut[binds[port][id].key]);
         if (!ret && binds[port])
            ret = input_joypad_pressed(wl->joypad, joypad_info, port, binds[port], id);
         return ret;
      case RETRO_DEVICE_ANALOG:
         ret = input_wl_analog_pressed(wl, binds[port], idx, id);
         if (!ret && binds[port])
            ret = input_joypad_analog(wl->joypad, joypad_info, port, idx, id, binds[port]);
         return ret;

      case RETRO_DEVICE_KEYBOARD:
         return input_wl_state_kb(wl, binds, port, device, idx, id);
      case RETRO_DEVICE_MOUSE:
         return input_wl_mouse_state(wl, id, false);
      case RARCH_DEVICE_MOUSE_SCREEN:
         return input_wl_mouse_state(wl, id, true);

      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         if (idx == 0)
            return input_wl_pointer_state(wl, idx, id,
                  device == RARCH_DEVICE_POINTER_SCREEN);
         break;
      case RETRO_DEVICE_LIGHTGUN:
         return input_wl_lightgun_state(wl, id);
   }

   return 0;
}

static void input_wl_free(void *data)
{
   input_ctx_wayland_data_t *wl = (input_ctx_wayland_data_t*)data;
   if (!wl)
      return;

   if (wl->joypad)
      wl->joypad->destroy();
}

bool input_wl_init(void *data, const char *joypad_name)
{
   input_ctx_wayland_data_t *wl = (input_ctx_wayland_data_t*)data;

   if (!wl)
      return false;

   wl->joypad = input_joypad_init_driver(joypad_name, wl);

   if (!wl->joypad)
      return false;

   input_keymaps_init_keyboard_lut(rarch_key_map_linux);
   return true;
}

static uint64_t input_wl_get_capabilities(void *data)
{
   (void)data;

   return
      (1 << RETRO_DEVICE_JOYPAD)   |
      (1 << RETRO_DEVICE_ANALOG)   |
      (1 << RETRO_DEVICE_KEYBOARD) |
      (1 << RETRO_DEVICE_MOUSE)    |
      (1 << RETRO_DEVICE_LIGHTGUN);
}

static void input_wl_grab_mouse(void *data, bool state)
{
   /* Dummy for now. Might be useful in the future. */
   (void)data;
   (void)state;
}

static bool input_wl_set_rumble(void *data, unsigned port,
      enum retro_rumble_effect effect, uint16_t strength)
{
   input_ctx_wayland_data_t *wl = (input_ctx_wayland_data_t*)data;
   if (wl && wl->joypad)
      return input_joypad_set_rumble(wl->joypad, port, effect, strength);
   return false;
}

static const input_device_driver_t *input_wl_get_joypad_driver(void *data)
{
   input_ctx_wayland_data_t *wl = (input_ctx_wayland_data_t*)data;
   if (!wl)
      return NULL;
   return wl->joypad;
}

static bool input_wl_keyboard_mapping_is_blocked(void *data)
{
   input_ctx_wayland_data_t *wl = (input_ctx_wayland_data_t*)data;
   if (!wl)
      return false;
   return wl->blocked;
}

static void input_wl_keyboard_mapping_set_block(void *data, bool value)
{
   input_ctx_wayland_data_t *wl = (input_ctx_wayland_data_t*)data;
   if (!wl)
      return;
   wl->blocked = value;
}

static bool input_wl_meta_key_pressed(void *data, int key)
{
   (void)data;
   (void)key;
   /* FIXME: What is this supposed to do? */
   return false;
}

input_driver_t input_wayland = {
   NULL,
   input_wl_poll,
   input_wl_state,
   input_wl_meta_key_pressed,
   input_wl_free,
   NULL,
   NULL,
   input_wl_get_capabilities,
   "wayland",
   input_wl_grab_mouse,
   NULL,
   input_wl_set_rumble,
   input_wl_get_joypad_driver,
   NULL,
   input_wl_keyboard_mapping_is_blocked,
   input_wl_keyboard_mapping_set_block,
};


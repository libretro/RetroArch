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

/* Forward declaration */

void flush_wayland_fd(void *data);

static bool wayland_context_gettouchpos(
      gfx_ctx_wayland_data_t *wl,
      unsigned id,
      unsigned* touch_x, unsigned* touch_y)
{
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

   wl->mouse.last_x             = wl->mouse.x;
   wl->mouse.last_y             = wl->mouse.y;

   if (!wl->mouse.focus)
   {
      wl->mouse.delta_x         = 0;
      wl->mouse.delta_y         = 0;
   }

   if (wl->gfx->locked_pointer)
   {
      /* Clamp X */
      if (wl->mouse.x < 0)
         wl->mouse.x = 0;
      if (wl->mouse.x >= (int)wl->gfx->buffer_width)
         wl->mouse.x = ((int)wl->gfx->buffer_width - 1);

      /* Clamp Y */
      if (wl->mouse.y < 0)
         wl->mouse.y = 0;
      if (wl->mouse.y >= (int)wl->gfx->buffer_height)
         wl->mouse.y = ((int)wl->gfx->buffer_height - 1);
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
   if (idx <= MAX_TOUCHES)
   {
      struct video_viewport vp    = {0};
      int16_t res_x               = 0;
      int16_t res_y               = 0;
      int16_t res_screen_x        = 0;
      int16_t res_screen_y        = 0;

      /* Shortcut: mouse button events will be reported on desktop with 0/0 coordinates. *
       * Skip these, mouse handling will catch it elsewhere.                             */
      if (wl->touches[idx].x == 0 && wl->touches[idx].y == 0)
         return 0;

      if (video_driver_translate_coord_viewport_confined_wrap(&vp,
                  wl->touches[idx].x, wl->touches[idx].y,
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
               return wl->touches[idx].active;
         }
      }
   }

   return 0;
}

static int16_t input_wl_state(
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
   input_ctx_wayland_data_t *wl = (input_ctx_wayland_data_t*)data;
   int x, y = 0;

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

            if (!keyboard_mapping_blocked)
            {
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if (binds[port][i].valid)
                  {
                     if (     (binds[port][i].key && binds[port][i].key < RETROK_LAST)
                           && BIT_GET(wl->key_state, rarch_keysym_lut[binds[port][i].key]))
                        ret |= (1 << i);
                  }
               }
            }

            return ret;
         }

         if (id < RARCH_BIND_LIST_END)
         {
            if (binds[port][id].valid)
            {
               if (     (binds[port][id].key && binds[port][id].key < RETROK_LAST)
                     && BIT_GET(wl->key_state, rarch_keysym_lut[binds[port][id].key])
                     && (id == RARCH_GAME_FOCUS_TOGGLE || !keyboard_mapping_blocked)
                  )
                  return 1;

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

            if (id_plus_valid && id_plus_key && id_plus_key < RETROK_LAST)
            {
               unsigned sym = rarch_keysym_lut[(enum retro_key)id_plus_key];
               if (BIT_GET(wl->key_state, sym))
                  ret = 0x7fff;
            }
            if (id_minus_valid && id_minus_key && id_minus_key < RETROK_LAST)
            {
               unsigned sym = rarch_keysym_lut[(enum retro_key)id_minus_key];
               if (BIT_GET(wl->key_state, sym))
                  ret += -0x7fff;
            }

            return ret;
         }
         break;
      case RETRO_DEVICE_KEYBOARD:
         return (id && id < RETROK_LAST) && BIT_GET(wl->key_state, rarch_keysym_lut[(enum retro_key)id]);
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         /* The system-wide mouse is reported for all ports. *
          * Multi-mouse may be implemented using different wayland seats, see issue #16886 */
         {
            bool state  = false;
            bool screen = (device == RARCH_DEVICE_MOUSE_SCREEN);
            switch (id)
            {
               case RETRO_DEVICE_ID_MOUSE_WHEELUP:
                  state        = wl->mouse.wu;
                  wl->mouse.wu = false;
                  return state;
               case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
                  state        = wl->mouse.wd;
                  wl->mouse.wd = false;
                  return state;
               case RETRO_DEVICE_ID_MOUSE_X:
                  x = screen ? wl->mouse.x : wl->mouse.delta_x;
                  wl->mouse.delta_x = 0;
                  return x;
               case RETRO_DEVICE_ID_MOUSE_Y:
                  y = screen ? wl->mouse.y : wl->mouse.delta_y;
                  wl->mouse.delta_y = 0;
                  return y;
               case RETRO_DEVICE_ID_MOUSE_LEFT:
                  return wl->mouse.left;
               case RETRO_DEVICE_ID_MOUSE_RIGHT:
                  return wl->mouse.right;
               case RETRO_DEVICE_ID_MOUSE_MIDDLE:
                  return wl->mouse.middle;
               case RETRO_DEVICE_ID_MOUSE_BUTTON_4:
                  return wl->mouse.side;
               case RETRO_DEVICE_ID_MOUSE_BUTTON_5:
                  return wl->mouse.extra;
               case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
                  state        = wl->mouse.wl;
                  wl->mouse.wl = false;
                  return state;
               case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
                  state        = wl->mouse.wr;
                  wl->mouse.wr = false;
                  return state;
            }
         }
         break;
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         /* All ports report the same pointer state. See notes at mouse case. */
         if (idx < MAX_TOUCHES)
         {
            int16_t touch_state = input_wl_touch_state(wl, idx, id,
                  device == RARCH_DEVICE_POINTER_SCREEN);
            /* Touch state is only reported if it is meaningful. */
            if (touch_state)
               return touch_state;
         }
         /* Fall through to system pointer emulating max. 3 touches. */
         if (idx < 3)
         {
            struct video_viewport vp    = {0};
            bool screen                 =
               (device == RARCH_DEVICE_POINTER_SCREEN);
            int16_t res_x               = 0;
            int16_t res_y               = 0;
            int16_t res_screen_x        = 0;
            int16_t res_screen_y        = 0;

            if (video_driver_translate_coord_viewport_confined_wrap(&vp,
                        wl->mouse.x, wl->mouse.y,
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
                     if (idx == 0)
                        return (wl->mouse.left | wl->mouse.right | wl->mouse.middle);
                     else if (idx == 1)
                        return (wl->mouse.right | wl->mouse.middle);
                     else if (idx == 2)
                        return wl->mouse.middle;
                  case RETRO_DEVICE_ID_POINTER_IS_OFFSCREEN:
                     return input_driver_pointer_is_offscreen(res_x, res_y);
                  default:
                     break;
               }
            }
         }
         break;
      case RETRO_DEVICE_LIGHTGUN:
         /* All ports report the same lightgun state. See notes at mouse case. */
         {
            struct video_viewport vp = {0};
            int16_t res_x            = 0;
            int16_t res_y            = 0;
            int16_t res_screen_x     = 0;
            int16_t res_screen_y     = 0;

            if (video_driver_translate_coord_viewport_wrap(&vp,
                        wl->mouse.x, wl->mouse.y,
                        &res_x, &res_y, &res_screen_x, &res_screen_y))
            {
               switch (id)
               {
                  case RETRO_DEVICE_ID_LIGHTGUN_X:
                     return wl->mouse.delta_x;
                  case RETRO_DEVICE_ID_LIGHTGUN_Y:
                     return wl->mouse.delta_y;
                  case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_X:
                     return res_x;
                  case RETRO_DEVICE_ID_LIGHTGUN_SCREEN_Y:
                     return res_y;
                  case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
                     return wl->mouse.left;
                  case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
                     return wl->mouse.middle;
                  case RETRO_DEVICE_ID_LIGHTGUN_START:
                     return wl->mouse.right;
                  case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
                     return wl->mouse.left && wl->mouse.right;
                  case RETRO_DEVICE_ID_LIGHTGUN_IS_OFFSCREEN:
                     return input_driver_pointer_is_offscreen(res_x, res_y);
                  case RETRO_DEVICE_ID_LIGHTGUN_AUX_A:        /* TODO */
                  case RETRO_DEVICE_ID_LIGHTGUN_AUX_B:        /* TODO */
                  case RETRO_DEVICE_ID_LIGHTGUN_AUX_C:        /* TODO */
                  case RETRO_DEVICE_ID_LIGHTGUN_DPAD_UP:      /* TODO */
                  case RETRO_DEVICE_ID_LIGHTGUN_DPAD_DOWN:    /* TODO */
                  case RETRO_DEVICE_ID_LIGHTGUN_DPAD_LEFT:    /* TODO */
                  case RETRO_DEVICE_ID_LIGHTGUN_DPAD_RIGHT:   /* TODO */
                     break;
               }
            }
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
        (1 << RETRO_DEVICE_JOYPAD)
      | (1 << RETRO_DEVICE_ANALOG)
      | (1 << RETRO_DEVICE_KEYBOARD)
      | (1 << RETRO_DEVICE_MOUSE)
      | (1 << RETRO_DEVICE_LIGHTGUN);
}

static void input_wl_grab_mouse(void *data, bool state)
{
   input_ctx_wayland_data_t *wl = (input_ctx_wayland_data_t*)data;
   gfx_ctx_wayland_data_t *gfx = (gfx_ctx_wayland_data_t*)wl->gfx;

   if (gfx->pointer_constraints && gfx->wl_pointer)
   {
      if (state && !gfx->locked_pointer)
      {
         gfx->locked_pointer = zwp_pointer_constraints_v1_lock_pointer(gfx->pointer_constraints,
            gfx->surface, gfx->wl_pointer, NULL, ZWP_POINTER_CONSTRAINTS_V1_LIFETIME_PERSISTENT);
         zwp_locked_pointer_v1_add_listener(gfx->locked_pointer,
            &locked_pointer_listener, gfx);
      }
      else if (!state && gfx->locked_pointer)
      {
         zwp_locked_pointer_v1_destroy(gfx->locked_pointer);
         gfx->locked_pointer = NULL;
      }
   }
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
   input_wl_grab_mouse,          /* grab_mouse */
   NULL,
   NULL
};

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

#ifdef HAVE_WAYLAND_BACKPORT
#include "../../gfx/common/wayland_common_backport.h"
#endif

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
#ifdef WEBOS
#include "../common/wayland_common_webos.h"
#endif

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

   /* the input queue's events, here on the frontend's thread */
   wayland_input_dispatch(wl);

   wl->mouse.last_pos = wl->mouse.pos;

   if (!wl->mouse.focus)
   {
      wl->mouse.delta = 0;
   }

   if (wl->gfx->locked_pointer)
   {
      /* Clamp X */
      if (VIDEO_POS_X(wl->mouse.pos) < 0)
         VIDEO_POS_PUT_X(wl->mouse.pos, 0);
      if (VIDEO_POS_X(wl->mouse.pos) >= (int)VIDEO_SCALE_W(wl->gfx->buffer_dims))
         VIDEO_POS_PUT_X(wl->mouse.pos, ((int)VIDEO_SCALE_W(wl->gfx->buffer_dims) - 1));

      /* Clamp Y */
      if (VIDEO_POS_Y(wl->mouse.pos) < 0)
         VIDEO_POS_PUT_Y(wl->mouse.pos, 0);
      if (VIDEO_POS_Y(wl->mouse.pos) >= (int)VIDEO_SCALE_H(wl->gfx->buffer_dims))
         VIDEO_POS_PUT_Y(wl->mouse.pos, ((int)VIDEO_SCALE_H(wl->gfx->buffer_dims) - 1));
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

   /* The mouse's frame and the touches, handed to the frontend, which
    * answers for the mouse, the pointer and the lightgun's aim. The
    * motion and the wheel are taken here, once, for every reader. */
   {
      input_pointer_frame_t frame;
      uint32_t touch_pos[MAX_TOUCHES];
      unsigned down    = 0;
      unsigned buttons = 0;

      if (wl->mouse.left)
         buttons |= INPUT_POINTER_LEFT;
      if (wl->mouse.right)
         buttons |= INPUT_POINTER_RIGHT;
      if (wl->mouse.middle)
         buttons |= INPUT_POINTER_MIDDLE;
      if (wl->mouse.side)
         buttons |= INPUT_POINTER_BUTTON_4;
      if (wl->mouse.extra)
         buttons |= INPUT_POINTER_BUTTON_5;
      if (wl->mouse.wu)
         buttons |= INPUT_POINTER_WHEEL_UP;
      if (wl->mouse.wd)
         buttons |= INPUT_POINTER_WHEEL_DOWN;
      if (wl->mouse.wr)
         buttons |= INPUT_POINTER_HWHEEL_UP;
      if (wl->mouse.wl)
         buttons |= INPUT_POINTER_HWHEEL_DOWN;
      frame.pos        = wl->mouse.pos;
      frame.rel        = wl->mouse.delta;
      frame.buttons    = (uint16_t)buttons;
      wl->mouse.delta  = 0;
      wl->mouse.wu     = false;
      wl->mouse.wd     = false;
      wl->mouse.wl     = false;
      wl->mouse.wr     = false;

      for (id = 0; id < MAX_TOUCHES; id++)
      {
         touch_pos[id] = VIDEO_POS_PACK(wl->touches[id].x, wl->touches[id].y);
         if (wl->touches[id].active)
            down      |= (1 << id);
      }
      /* the one mouse is every port's; it stands for three touches */
      input_driver_publish_pointers(&frame, 1,
            INPUT_POINTERS_MOUSE_3_TOUCHES);
      input_driver_publish_touches(touch_pos, MAX_TOUCHES, down);
   }
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

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;

            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               if (RETRO_KEYBIND_VALID(&binds[port][i]))
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
                  if (RETRO_KEYBIND_VALID(&binds[port][i]))
                  {
                     if (     (RETRO_KEYBIND_KEY(&binds[port][i]) && RETRO_KEYBIND_KEY(&binds[port][i]) < RETROK_LAST)
                           && BIT_GET(wl->key_state, rarch_keysym_lut[RETRO_KEYBIND_KEY(&binds[port][i])]))
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
                     && BIT_GET(wl->key_state, rarch_keysym_lut[RETRO_KEYBIND_KEY(&binds[port][id])])
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
         if (binds)
         {
            int id_minus_key      = 0;
            int id_plus_key       = 0;
            unsigned id_minus     = 0;
            unsigned id_plus      = 0;
            int16_t ret           = 0;
            bool id_plus_valid    = false;
            bool id_minus_valid   = false;

            input_conv_analog_id_to_bind_id(idx, id, id_minus, id_plus);

            id_minus_valid        = RETRO_KEYBIND_VALID(&binds[port][id_minus]);
            id_plus_valid         = RETRO_KEYBIND_VALID(&binds[port][id_plus]);
            id_minus_key          = RETRO_KEYBIND_KEY(&binds[port][id_minus]);
            id_plus_key           = RETRO_KEYBIND_KEY(&binds[port][id_plus]);

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
#ifdef WEBOS
         if ((id && id < RETROK_LAST) && (id == RETROK_BACKSPACE) &&
             webos_wl_special_keymap[webos_wl_key_back] == WL_KEYBOARD_KEY_STATE_PRESSED)
         {
            webos_wl_special_keymap[webos_wl_key_back] = 0;
            return true;
         }
#endif
         return (id && id < RETROK_LAST) && BIT_GET(wl->key_state, rarch_keysym_lut[(enum retro_key)id]);
      /* The mouse, the pointer and the lightgun's aim are the frontend's
       * to answer: input_wl_poll() publishes the mouse and the touches.
       * The one system-wide mouse is every port's; several would be
       * several Wayland seats, see issue #16886. */
      case RETRO_DEVICE_LIGHTGUN:
         /* All ports report the same lightgun: its buttons are the
          * mouse's. */
         switch (id)
         {
            case RETRO_DEVICE_ID_LIGHTGUN_TRIGGER:
               return wl->mouse.left;
            case RETRO_DEVICE_ID_LIGHTGUN_RELOAD:
               return wl->mouse.middle;
            case RETRO_DEVICE_ID_LIGHTGUN_START:
               return wl->mouse.right;
            case RETRO_DEVICE_ID_LIGHTGUN_SELECT:
               return wl->mouse.left && wl->mouse.right;
            default:
               break;
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

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2013-2014 - Jason Fetters
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
#include <unistd.h>

#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../input_keymaps.h"

#include "cocoa_input.h"

#include "../../retroarch.h"
#include "../../driver.h"

#include "../drivers_keyboard/keyboard_event_apple.h"

/* TODO/FIXME -
 * fix game focus toggle */

/* Forward declarations */
float get_backing_scale_factor(void);

int32_t cocoa_input_find_any_key(void)
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*)input_driver_get_data();

   if (!apple)
      return 0;

   if (apple->joypad)
       apple->joypad->poll();

    if (apple->sec_joypad)
        apple->sec_joypad->poll();

   return apple_keyboard_find_any_key();
}

static int cocoa_input_find_any_button_ret(cocoa_input_data_t *apple,
   input_bits_t * state, unsigned port)
{
   unsigned i;

   if (state)
      for (i = 0; i < 256; i++)
         if (BIT256_GET_PTR(state,i))
            return i;
   return -1;
}

int32_t cocoa_input_find_any_button(uint32_t port)
{
   int ret = -1;
   cocoa_input_data_t *apple = (cocoa_input_data_t*)input_driver_get_data();

   if (!apple)
      return -1;

   if (apple->joypad)
   {
       apple->joypad->poll();

       if (apple->joypad->get_buttons)
       {
          input_bits_t state;
          apple->joypad->get_buttons(port,&state);
          ret = cocoa_input_find_any_button_ret(apple, &state, port);
       }
   }

   if (ret != -1)
      return ret;

   if (apple->sec_joypad)
   {
       apple->sec_joypad->poll();

       if (apple->sec_joypad->get_buttons)
       {
          input_bits_t state;
          apple->sec_joypad->poll();
          apple->sec_joypad->get_buttons(port,&state);
          ret = cocoa_input_find_any_button_ret(apple, &state, port);
       }
   }

   if (ret != -1)
      return ret;

   return -1;
}

int32_t cocoa_input_find_any_axis(uint32_t port)
{
   int i;
   cocoa_input_data_t *apple = (cocoa_input_data_t*)input_driver_get_data();

   if (apple && apple->joypad)
       apple->joypad->poll();

   if (apple && apple->sec_joypad)
       apple->sec_joypad->poll();

   for (i = 0; i < 6; i++)
   {
      int16_t value = apple->joypad ? apple->joypad->axis(port, i) : 0;

      if (abs(value) > 0x4000)
         return (value < 0) ? -(i + 1) : i + 1;

      value = apple->sec_joypad ? apple->sec_joypad->axis(port, i) : value;

      if (abs(value) > 0x4000)
         return (value < 0) ? -(i + 1) : i + 1;
   }

   return 0;
}

static void *cocoa_input_init(const char *joypad_driver)
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*)calloc(1, sizeof(*apple));
   if (!apple)
      return NULL;

   input_keymaps_init_keyboard_lut(rarch_key_map_apple_hid);

   apple->joypad = input_joypad_init_driver(joypad_driver, apple);

#ifdef HAVE_MFI
   apple->sec_joypad = input_joypad_init_driver("mfi", apple);
#endif

   return apple;
}

static void cocoa_input_poll(void *data)
{
   uint32_t i;
   cocoa_input_data_t *apple = (cocoa_input_data_t*)data;
#ifndef IOS
   float   backing_scale_factor = get_backing_scale_factor();
#endif

   if (!apple)
      return;

   for (i = 0; i < apple->touch_count; i++)
   {
      struct video_viewport vp;

      vp.x                        = 0;
      vp.y                        = 0;
      vp.width                    = 0;
      vp.height                   = 0;
      vp.full_width               = 0;
      vp.full_height              = 0;

#ifndef IOS
      apple->touches[i].screen_x *= backing_scale_factor;
      apple->touches[i].screen_y *= backing_scale_factor;
#endif
      video_driver_translate_coord_viewport_wrap(
            &vp,
            apple->touches[i].screen_x,
            apple->touches[i].screen_y,
            &apple->touches[i].fixed_x,
            &apple->touches[i].fixed_y,
            &apple->touches[i].full_x,
            &apple->touches[i].full_y);
   }

   if (apple->joypad)
      apple->joypad->poll();
   if (apple->sec_joypad)
       apple->sec_joypad->poll();

    apple->mouse_x_last = apple->mouse_rel_x;
    apple->mouse_y_last = apple->mouse_rel_y;
}

static int16_t cocoa_mouse_state(cocoa_input_data_t *apple,
      unsigned id)
{
   switch (id)
   {
      case RETRO_DEVICE_ID_MOUSE_X:
           return apple->mouse_rel_x - apple->mouse_x_last;
      case RETRO_DEVICE_ID_MOUSE_Y:
         return apple->mouse_rel_y - apple->mouse_y_last;
      case RETRO_DEVICE_ID_MOUSE_LEFT:
         return apple->mouse_buttons & 1;
      case RETRO_DEVICE_ID_MOUSE_RIGHT:
         return apple->mouse_buttons & 2;
       case RETRO_DEVICE_ID_MOUSE_WHEELUP:
           return apple->mouse_wu;
       case RETRO_DEVICE_ID_MOUSE_WHEELDOWN:
           return apple->mouse_wd;
       case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP:
           return apple->mouse_wl;
       case RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN:
           return apple->mouse_wr;
   }

   return 0;
}

static int16_t cocoa_mouse_state_screen(cocoa_input_data_t *apple,
                                        unsigned id)
{
   int16_t val;
#ifndef IOS
   float   backing_scale_factor = get_backing_scale_factor();
#endif

    switch (id)
    {
        case RETRO_DEVICE_ID_MOUSE_X:
            val = apple->window_pos_x;
            break;
        case RETRO_DEVICE_ID_MOUSE_Y:
            val = apple->window_pos_y;
            break;
        default:
            return cocoa_mouse_state(apple, id);
    }

#ifndef IOS
    val *= backing_scale_factor;
#endif
    return val;
}

static int16_t cocoa_pointer_state(cocoa_input_data_t *apple,
      unsigned device, unsigned idx, unsigned id)
{
   const bool want_full = (device == RARCH_DEVICE_POINTER_SCREEN);

   if (idx < apple->touch_count && (idx < MAX_TOUCHES))
   {
      int16_t x, y;
      const cocoa_touch_data_t *touch = (const cocoa_touch_data_t *)
         &apple->touches[idx];

       if (!touch)
           return 0;

      x = touch->fixed_x;
      y = touch->fixed_y;

      if (want_full)
      {
         x = touch->full_x;
         y = touch->full_y;
      }

      switch (id)
      {
         case RETRO_DEVICE_ID_POINTER_PRESSED:
            return (x != -0x8000) && (y != -0x8000);
         case RETRO_DEVICE_ID_POINTER_X:
            return x;
         case RETRO_DEVICE_ID_POINTER_Y:
            return y;
         case RETRO_DEVICE_ID_POINTER_COUNT:
            return apple->touch_count;
      }
   }

   return 0;
}

static int16_t cocoa_input_state(void *data,
      rarch_joypad_info_t joypad_info,
      const struct retro_keybind **binds, unsigned port,
      unsigned device, unsigned idx, unsigned id)
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*)data;

   switch (device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (id == RETRO_DEVICE_ID_JOYPAD_MASK)
         {
            unsigned i;
            int16_t ret = 0;
            for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
            {
               /* Auto-binds are per joypad, not per user. */
               const uint64_t joykey  = (binds[port][i].joykey != NO_BTN)
                  ? binds[port][i].joykey : joypad_info.auto_binds[i].joykey;
               const uint32_t joyaxis = (binds[port][i].joyaxis != AXIS_NONE)
                  ? binds[port][i].joyaxis : joypad_info.auto_binds[i].joyaxis;

               if (apple_key_state[rarch_keysym_lut[binds[port][i].key]])
               {
                  ret |= (1 << i);
                  continue;
               }

               if ((uint16_t)joykey != NO_BTN && apple->joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               {
                  ret |= (1 << i);
                  continue;
               }
               if (((float)abs(apple->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               {
                  ret |= (1 << i);
                  continue;
               }
#ifdef HAVE_MFI
               if ((uint16_t)joykey != NO_BTN && apple->sec_joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               {
                  ret |= (1 << i);
                  continue;
               }
               if (((float)abs(apple->sec_joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               {
                  ret |= (1 << i);
                  continue;
               }
#endif
            }
            return ret;
         }
         else
         {
            /* Auto-binds are per joypad, not per user. */
            const uint64_t joykey  = (binds[port][id].joykey != NO_BTN)
               ? binds[port][id].joykey : joypad_info.auto_binds[id].joykey;
            const uint32_t joyaxis = (binds[port][id].joyaxis != AXIS_NONE)
               ? binds[port][id].joyaxis : joypad_info.auto_binds[id].joyaxis;
            if (id < RARCH_BIND_LIST_END)
               if (apple_key_state[rarch_keysym_lut[binds[port][id].key]])
                  return true;
            if ((uint16_t)joykey != NO_BTN && apple->joypad->button(
                     joypad_info.joy_idx, (uint16_t)joykey))
               return true;
            if (((float)abs(apple->joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               return true;
#ifdef HAVE_MFI
            if ((uint16_t)joykey != NO_BTN && apple->sec_joypad->button(joypad_info.joy_idx, (uint16_t)joykey))
               return true;
            if (((float)abs(apple->sec_joypad->axis(joypad_info.joy_idx, joyaxis)) / 0x8000) > joypad_info.axis_threshold)
               return true;
#endif
         }
         break;
      case RETRO_DEVICE_ANALOG:
         {
            int16_t ret = 0;
#ifdef HAVE_MFI
            ret = input_joypad_analog(apple->sec_joypad, joypad_info, port,
                  idx, id, binds[port]);
#endif
            if (!ret && binds[port])
               ret = input_joypad_analog(apple->joypad, joypad_info, port,
                     idx, id, binds[port]);
            return ret;
         }
      case RETRO_DEVICE_KEYBOARD:
         return (id < RETROK_LAST) && apple_key_state[rarch_keysym_lut[(enum retro_key)id]];
      case RETRO_DEVICE_MOUSE:
         return cocoa_mouse_state(apple, id);
      case RARCH_DEVICE_MOUSE_SCREEN:
         return cocoa_mouse_state_screen(apple, id);
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         return cocoa_pointer_state(apple, device, idx, id);
   }

   return 0;
}

static void cocoa_input_free(void *data)
{
   unsigned i;
   cocoa_input_data_t *apple = (cocoa_input_data_t*)data;

   if (!apple || !data)
      return;

   if (apple->joypad)
      apple->joypad->destroy();

   if (apple->sec_joypad)
       apple->sec_joypad->destroy();

   for (i = 0; i < MAX_KEYS; i++)
      apple_key_state[i] = 0;

   free(apple);
}

static bool cocoa_input_set_rumble(void *data,
   unsigned port, enum retro_rumble_effect effect, uint16_t strength)
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*)data;

   if (apple && apple->joypad)
      return input_joypad_set_rumble(apple->joypad,
            port, effect, strength);
#ifdef HAVE_MFI
    if (apple && apple->sec_joypad)
        return input_joypad_set_rumble(apple->sec_joypad,
            port, effect, strength);
#endif
   return false;
}

static uint64_t cocoa_input_get_capabilities(void *data)
{
   (void)data;

   return
      (1 << RETRO_DEVICE_JOYPAD)   |
      (1 << RETRO_DEVICE_MOUSE)    |
      (1 << RETRO_DEVICE_KEYBOARD) |
      (1 << RETRO_DEVICE_POINTER)  |
      (1 << RETRO_DEVICE_ANALOG);
}

static void cocoa_input_grab_mouse(void *data, bool state)
{
   /* Dummy for now. Might be useful in the future. */
   (void)data;
   (void)state;
}

static const input_device_driver_t *cocoa_input_get_sec_joypad_driver(void *data)
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*)data;

   if (apple && apple->sec_joypad)
      return apple->sec_joypad;
   return NULL;
}

static const input_device_driver_t *cocoa_input_get_joypad_driver(void *data)
{
   cocoa_input_data_t *apple = (cocoa_input_data_t*)data;

   if (apple && apple->joypad)
      return apple->joypad;
   return NULL;
}

input_driver_t input_cocoa = {
   cocoa_input_init,
   cocoa_input_poll,
   cocoa_input_state,
   cocoa_input_free,
   NULL,
   NULL,
   cocoa_input_get_capabilities,
   "cocoa",
   cocoa_input_grab_mouse,
   NULL,
   cocoa_input_set_rumble,
   cocoa_input_get_joypad_driver,
   cocoa_input_get_sec_joypad_driver,
   false
};

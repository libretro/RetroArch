/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018-2019 - Krzysztof Haładyn
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
#define _USE_MATH_DEFINES
#include <math.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <boolean.h>
#include <libretro.h>

#include <uwp/uwp_func.h>

#include "../input_driver.h"
#include "../input_dsu.h"
#include "../../configuration.h"

/* TODO: Add support for multiple mice and multiple touch */

static void uwp_input_free_input(void *data) { }
static void *uwp_input_init(const char *a)
{
   input_keymaps_init_keyboard_lut(rarch_key_map_uwp);

   return (void*)-1;
}

static uint64_t uwp_input_get_capabilities(void *data)
{
   return
           (1 << RETRO_DEVICE_JOYPAD)
         | (1 << RETRO_DEVICE_MOUSE)
         | (1 << RETRO_DEVICE_KEYBOARD)
         | (1 << RETRO_DEVICE_POINTER)
         | (1 << RETRO_DEVICE_ANALOG);
}

static int16_t uwp_input_state(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned index,
      unsigned id)
{
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
                  if (uwp_mouse_state(port, binds[port][i].mbutton, false))
                     ret |= (1 << i);
               }
            }

            if (!keyboard_mapping_blocked)
            {
               for (i = 0; i < RARCH_FIRST_CUSTOM_BIND; i++)
               {
                  if (binds[port][i].valid)
                  {
                     if (     (binds[port][i].key && binds[port][i].key < RETROK_LAST)
                           && uwp_keyboard_pressed(binds[port][i].key))
                        ret |= (1 << i);
                  }
               }
            }

#if defined(HAVE_NETWORKING) && defined(HAVE_DSU)
            {
               input_driver_state_t *input_st = input_state_get_ptr();
               settings_t *settings = config_get_ptr();
               if (input_st && input_st->dsu && settings)
               {
                  bool is_fullpad = dsu_port_is_fullpad(input_st->dsu, port);
                  bool addon_with_gamepad = input_st->dsu->player_addon_attached[port] && input_st->dsu->player_gamepad[port];
                  if (is_fullpad || addon_with_gamepad)
                  {
                     uint16_t dsu_btns = dsu_get_buttons(input_st->dsu, port);
                     ret |= dsu_btns;

                     /* DSU analog-to-digital for menu navigation */
                     float axis_threshold = settings->floats.input_axis_threshold;
                     int16_t lx = dsu_get_analog(input_st->dsu, port, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
                     int16_t ly = dsu_get_analog(input_st->dsu, port, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
                     float norm_lx = (float)lx / 32767.0f;
                     float norm_ly = (float)ly / 32767.0f;

                     if (norm_lx < -axis_threshold)
                        ret |= (1 << RETRO_DEVICE_ID_JOYPAD_LEFT);
                     if (norm_lx > axis_threshold)
                        ret |= (1 << RETRO_DEVICE_ID_JOYPAD_RIGHT);
                     if (norm_ly < -axis_threshold)
                        ret |= (1 << RETRO_DEVICE_ID_JOYPAD_UP);
                     if (norm_ly > axis_threshold)
                        ret |= (1 << RETRO_DEVICE_ID_JOYPAD_DOWN);
                  }
               }
            }
#endif

            return ret;
         }

         if (id < RARCH_BIND_LIST_END)
         {
            /* Check local keyboard/mouse first */
            if (binds[port][id].valid)
            {
               if (     (binds[port][id].key && binds[port][id].key < RETROK_LAST)
                     && uwp_keyboard_pressed(binds[port][id].key)
                     && (id == RARCH_GAME_FOCUS_TOGGLE || !keyboard_mapping_blocked)
                  )
                  return 1;
               if (uwp_mouse_state(port, binds[port][id].mbutton, false))
                  return 1;
            }
#if defined(HAVE_NETWORKING) && defined(HAVE_DSU)
            {
               input_driver_state_t *input_st = input_state_get_ptr();
               settings_t *settings = config_get_ptr();
               if (input_st && input_st->dsu && settings)
               {
                  bool is_fullpad = dsu_port_is_fullpad(input_st->dsu, port);
                  bool addon_with_gamepad = input_st->dsu->player_addon_attached[port] && input_st->dsu->player_gamepad[port];
                  if (is_fullpad || addon_with_gamepad)
                  {
                     uint16_t dsu_btns = dsu_get_buttons(input_st->dsu, port);
                     if ((id < RARCH_FIRST_CUSTOM_BIND) && (dsu_btns & (1 << id)))
                        return 1;
                     if ((id == RARCH_MENU_TOGGLE) && dsu_get_home(input_st->dsu, port))
                        return 1;

                     /* DSU analog-to-digital for menu navigation */
                     if (id == RETRO_DEVICE_ID_JOYPAD_UP || id == RETRO_DEVICE_ID_JOYPAD_DOWN ||
                         id == RETRO_DEVICE_ID_JOYPAD_LEFT || id == RETRO_DEVICE_ID_JOYPAD_RIGHT)
                     {
                        float axis_threshold = settings->floats.input_axis_threshold;
                        int16_t lx = dsu_get_analog(input_st->dsu, port, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_X);
                        int16_t ly = dsu_get_analog(input_st->dsu, port, RETRO_DEVICE_INDEX_ANALOG_LEFT, RETRO_DEVICE_ID_ANALOG_Y);
                        float norm_lx = (float)lx / 32767.0f;
                        float norm_ly = (float)ly / 32767.0f;

                        if ((id == RETRO_DEVICE_ID_JOYPAD_LEFT) && (norm_lx < -axis_threshold))
                           return 1;
                        else if ((id == RETRO_DEVICE_ID_JOYPAD_RIGHT) && (norm_lx > axis_threshold))
                           return 1;
                        else if ((id == RETRO_DEVICE_ID_JOYPAD_UP) && (norm_ly < -axis_threshold))
                           return 1;
                        else if ((id == RETRO_DEVICE_ID_JOYPAD_DOWN) && (norm_ly > axis_threshold))
                           return 1;
                     }
                  }
               }
            }
#endif
         }
         break;
      default:
         break;
   }

   return 0;
}

static int16_t uwp_input_state_analog(
      void *data,
      const input_device_driver_t *joypad,
      const input_device_driver_t *sec_joypad,
      rarch_joypad_info_t *joypad_info,
      const retro_keybind_set *binds,
      bool keyboard_mapping_blocked,
      unsigned port,
      unsigned device,
      unsigned index,
      unsigned id)
{
   switch (device)
   {
      case RETRO_DEVICE_ANALOG:
         {
            int16_t ret = 0;

            if (binds[port])
            {
               int id_minus_key      = 0;
               int id_plus_key       = 0;
               unsigned id_minus     = 0;
               unsigned id_plus      = 0;
               bool id_plus_valid    = false;
               bool id_minus_valid   = false;

               input_conv_analog_id_to_bind_id(index, id, id_minus, id_plus);

               id_minus_valid        = binds[port][id_minus].valid;
               id_plus_valid         = binds[port][id_plus].valid;
               id_minus_key          = binds[port][id_minus].key;
               id_plus_key           = binds[port][id_plus].key;

               if (id_plus_valid && id_plus_key && id_plus_key < RETROK_LAST)
               {
                  if (uwp_keyboard_pressed(id_plus_key))
                     ret = 0x7fff;
               }
               if (id_minus_valid && id_minus_key && id_minus_key < RETROK_LAST)
               {
                  if (uwp_keyboard_pressed(id_minus_key))
                     ret += -0x7fff;
               }
            }

#if defined(HAVE_NETWORKING) && defined(HAVE_DSU)
            {
               input_driver_state_t *input_st = input_state_get_ptr();
               settings_t *settings = config_get_ptr();
               if (input_st && input_st->dsu && settings)
               {
                  bool is_fullpad = dsu_port_is_fullpad(input_st->dsu, port);
                  bool addon_with_gamepad = input_st->dsu->player_addon_attached[port] && input_st->dsu->player_gamepad[port];
                  if (is_fullpad || addon_with_gamepad)
                  {
                     int16_t dsu_val_x = dsu_get_analog(input_st->dsu, port, index, RETRO_DEVICE_ID_ANALOG_X);
                     int16_t dsu_val_y = dsu_get_analog(input_st->dsu, port, index, RETRO_DEVICE_ID_ANALOG_Y);
                     int16_t dsu_val   = (id == RETRO_DEVICE_ID_ANALOG_X) ? dsu_val_x : dsu_val_y;

                     if (dsu_val != 0)
                     {
                        float input_analog_deadzone    = settings->floats.input_analog_deadzone;
                        float input_analog_sensitivity = settings->floats.input_analog_sensitivity;

                        if (input_analog_deadzone > 0.0f)
                        {
                           float x   = (float)dsu_val_x / 0x7fff;
                           float y   = (float)dsu_val_y / 0x7fff;
                           float mag = sqrtf(x * x + y * y);

                           if (mag <= input_analog_deadzone)
                              dsu_val = 0;
                           else
                           {
                              float dz_scale = (mag - input_analog_deadzone) / (1.0f - input_analog_deadzone);
                              float inv_mag  = (mag > 1.0f) ? (1.0f / mag) : 1.0f;
                              if (dz_scale > 1.0f) dz_scale = 1.0f;
                              dsu_val = (int16_t)((float)dsu_val * inv_mag * dz_scale);
                           }
                        }

                        if (dsu_val != 0 && input_analog_sensitivity != 1.0f)
                        {
                           int new_val = (int)((float)dsu_val * input_analog_sensitivity);
                           if (new_val >  0x7fff) new_val =  0x7fff;
                           if (new_val < -0x7fff) new_val = -0x7fff;
                           dsu_val = (int16_t)new_val;
                        }

                        /* Merge with local using max magnitude */
                        if (dsu_val != 0)
                        {
                           if (ret == 0)
                              ret = dsu_val;
                           else
                           {
                              int16_t dsu_abs = (dsu_val >= 0) ? dsu_val : -dsu_val;
                              int16_t ret_abs = (ret >= 0)     ? ret     : -ret;
                              if (dsu_abs > ret_abs)
                                 ret = dsu_val;
                           }
                        }
                     }
                  }
               }
            }
#endif
            return ret;
         }
      case RETRO_DEVICE_KEYBOARD:
         {
            bool pressed = (id && id < RETROK_LAST) && uwp_keyboard_pressed(id);
            return pressed;
         }
      case RETRO_DEVICE_MOUSE:
      case RARCH_DEVICE_MOUSE_SCREEN:
         return uwp_mouse_state(port, id, device == RARCH_DEVICE_MOUSE_SCREEN);
      case RETRO_DEVICE_POINTER:
      case RARCH_DEVICE_POINTER_SCREEN:
         {
            int16_t ret = uwp_pointer_state(index, id,
                  device == RARCH_DEVICE_POINTER_SCREEN);
#if defined(HAVE_NETWORKING) && defined(HAVE_DSU)
            {
               input_driver_state_t *input_st = input_state_get_ptr();

               if (input_st && input_st->dsu
                     && dsu_port_has_touch(input_st->dsu, port)
                     && dsu_get_pointer_count(input_st->dsu, port) > 0)
               {
                  switch (id)
                  {
                     case RETRO_DEVICE_ID_POINTER_X:
                        ret = dsu_get_pointer_x(input_st->dsu, port, index);
                        break;
                     case RETRO_DEVICE_ID_POINTER_Y:
                        ret = dsu_get_pointer_y(input_st->dsu, port, index);
                        break;
                     case RETRO_DEVICE_ID_POINTER_PRESSED:
                        if (dsu_get_pointer_pressed(input_st->dsu, port, index))
                           ret = 1;
                        break;
                     case RETRO_DEVICE_ID_POINTER_COUNT:
                     {
                        int16_t dsu_cnt =
                           dsu_get_pointer_count(input_st->dsu, port);
                        if (dsu_cnt > ret)
                           ret = dsu_cnt;
                        break;
                     }
                  }
               }
            }
#endif
            return ret;
         }
   }

   return 0;
}

input_driver_t input_uwp = {
   uwp_input_init,
   uwp_input_next_frame,         /* poll       */
   uwp_input_state,
   uwp_input_free_input,
   NULL,
   NULL,
   uwp_input_get_capabilities,
   "uwp",
   NULL,                         /* grab_mouse */
   NULL,
   NULL
};

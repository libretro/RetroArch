/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "widgets/menu_entry.h"
#include "widgets/menu_input_dialog.h"

#include "menu_event.h"

#include "menu_driver.h"
#include "menu_input.h"
#include "menu_animation.h"
#include "menu_display.h"
#include "menu_navigation.h"

#include "../configuration.h"

static int menu_event_pointer(unsigned *action)
{
   const struct retro_keybind *binds[MAX_USERS] = {NULL};
   menu_input_t *menu_input                     = menu_input_get_ptr();
   unsigned fb_width                            = menu_display_get_width();
   unsigned fb_height                           = menu_display_get_height();
   int pointer_device                           =
      menu_driver_ctl(RARCH_MENU_CTL_IS_SET_TEXTURE, NULL) ?
        RETRO_DEVICE_POINTER : RARCH_DEVICE_POINTER_SCREEN;
   int pointer_x                                =
      input_driver_state(binds, 0, pointer_device,
         0, RETRO_DEVICE_ID_POINTER_X);
   int pointer_y                                =
      input_driver_state(binds, 0, pointer_device,
         0, RETRO_DEVICE_ID_POINTER_Y);

   menu_input->pointer.pressed[0]  = input_driver_state(binds,
         0, pointer_device,
         0, RETRO_DEVICE_ID_POINTER_PRESSED);
   menu_input->pointer.pressed[1]  = input_driver_state(binds,
         0, pointer_device,
         1, RETRO_DEVICE_ID_POINTER_PRESSED);
   menu_input->pointer.back  = input_driver_state(binds, 0, pointer_device,
         0, RARCH_DEVICE_ID_POINTER_BACK);

   menu_input->pointer.x = ((pointer_x + 0x7fff) * (int)fb_width) / 0xFFFF;
   menu_input->pointer.y = ((pointer_y + 0x7fff) * (int)fb_height) / 0xFFFF;

   return 0;
}

unsigned menu_event(retro_input_t input,
      retro_input_t trigger_input)
{
   menu_animation_ctx_delta_t delta;
   float delta_time;
   /* Used for key repeat */
   static float delay_timer                = 0.0f;
   static float delay_count                = 0.0f;
   unsigned ret                            = MENU_ACTION_NOOP;
   static bool initial_held                = true;
   static bool first_held                  = false;
   bool set_scroll                         = false;
   bool mouse_enabled                      = false;
   size_t new_scroll_accel                 = 0;
   menu_input_t *menu_input                = NULL;
   settings_t *settings                    = config_get_ptr();

   if (input.state)
   {
      if (!first_held)
      {
         /* don't run anything first frame, only capture held inputs
          * for old_input_state. */

         first_held  = true;
         delay_timer = initial_held ? 12 : 6;
         delay_count = 0;
      }

      if (delay_count >= delay_timer)
      {
         retro_input_t input_repeat = {0};
         BIT32_SET(input_repeat.state, RETRO_DEVICE_ID_JOYPAD_UP);
         BIT32_SET(input_repeat.state, RETRO_DEVICE_ID_JOYPAD_DOWN);
         BIT32_SET(input_repeat.state, RETRO_DEVICE_ID_JOYPAD_LEFT);
         BIT32_SET(input_repeat.state, RETRO_DEVICE_ID_JOYPAD_RIGHT);
         BIT32_SET(input_repeat.state, RETRO_DEVICE_ID_JOYPAD_B);
         BIT32_SET(input_repeat.state, RETRO_DEVICE_ID_JOYPAD_A);
         BIT32_SET(input_repeat.state, RETRO_DEVICE_ID_JOYPAD_L);
         BIT32_SET(input_repeat.state, RETRO_DEVICE_ID_JOYPAD_R);

         set_scroll           = true;
         first_held           = false;
         trigger_input.state |= input.state & input_repeat.state;

         menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL,
               &new_scroll_accel);

         new_scroll_accel = MIN(new_scroll_accel + 1, 64);
      }

      initial_held  = false;
   }
   else
   {
      set_scroll   = true;
      first_held   = false;
      initial_held = true;
   }

   if (set_scroll)
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SCROLL_ACCEL,
            &new_scroll_accel);

   menu_animation_ctl(MENU_ANIMATION_CTL_DELTA_TIME, &delta_time);

   delta.current = delta_time;

   if (menu_animation_ctl(MENU_ANIMATION_CTL_IDEAL_DELTA_TIME_GET, &delta))
      delay_count += delta.ideal;

   if (menu_input_dialog_get_display_kb())
   {
      static unsigned ti_char = 64;
      static bool ti_next     = false;

      if (trigger_input.state & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN))
      {
         if (ti_char > 32)
            ti_char--;
         if (! ti_next)
            input_keyboard_event(true, '\x7f', '\x7f', 0, RETRO_DEVICE_KEYBOARD);
         input_keyboard_event(true, ti_char, ti_char, 0, RETRO_DEVICE_KEYBOARD);
         ti_next = false;
      }

      if (trigger_input.state & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP))
      {
         if (ti_char < 125)
            ti_char++;
         if (! ti_next)
            input_keyboard_event(true, '\x7f', '\x7f', 0, RETRO_DEVICE_KEYBOARD);
         input_keyboard_event(true, ti_char, ti_char, 0, RETRO_DEVICE_KEYBOARD);
         ti_next = false;
      }

      if (trigger_input.state & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A))
      {
         ti_char = 64;
         ti_next = true;
      }

      if (trigger_input.state & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B))
      {
         input_keyboard_event(true, '\x7f', '\x7f', 0, RETRO_DEVICE_KEYBOARD);
         ti_char = 64;
         ti_next = false;
      }

      /* send return key to close keyboard input window */
      if (trigger_input.state & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START))
         input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);

      trigger_input.state = 0;
   }

   if (trigger_input.state & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP))
      ret = MENU_ACTION_UP;
   else if (trigger_input.state & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN))
      ret = MENU_ACTION_DOWN;
   else if (trigger_input.state & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT))
      ret = MENU_ACTION_LEFT;
   else if (trigger_input.state & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      ret = MENU_ACTION_RIGHT;
   else if (trigger_input.state & (UINT64_C(1) << settings->menu_scroll_up_btn))
      ret = MENU_ACTION_SCROLL_UP;
   else if (trigger_input.state & (UINT64_C(1) << settings->menu_scroll_down_btn))
      ret = MENU_ACTION_SCROLL_DOWN;
   else if (trigger_input.state & (UINT64_C(1) << settings->menu_cancel_btn))
      ret = MENU_ACTION_CANCEL;
   else if (trigger_input.state & (UINT64_C(1) << settings->menu_ok_btn))
      ret = MENU_ACTION_OK;
   else if (trigger_input.state & (UINT64_C(1) << settings->menu_search_btn))
      ret = MENU_ACTION_SEARCH;
   else if (trigger_input.state & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y))
      ret = MENU_ACTION_SCAN;
   else if (trigger_input.state & (UINT64_C(1) << settings->menu_default_btn))
      ret = MENU_ACTION_START;
   else if (trigger_input.state & (UINT64_C(1) << settings->menu_info_btn))
      ret = MENU_ACTION_INFO;
   else if (trigger_input.state & (UINT64_C(1) << RARCH_MENU_TOGGLE))
      ret = MENU_ACTION_TOGGLE;

   mouse_enabled                      = settings->menu.mouse.enable;
#ifdef HAVE_OVERLAY
   if (!mouse_enabled)
      mouse_enabled = !(settings->input.overlay_enable
            && input_overlay_is_alive(NULL));
#endif

   if (!(menu_input = menu_input_get_ptr()))
      return 0;

   if (!mouse_enabled)
      menu_input->mouse.ptr = 0;

   if (settings->menu.pointer.enable)
      menu_event_pointer(&ret);
   else
      memset(&menu_input->pointer, 0, sizeof(menu_input->pointer));

   return ret;
}

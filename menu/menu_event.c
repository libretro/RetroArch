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

#include "content.h"
#include "menu_driver.h"
#include "menu_input.h"
#include "menu_animation.h"
#include "menu_display.h"
#include "menu_navigation.h"

#include "widgets/menu_dialog.h"

#include "../configuration.h"
#include "../retroarch.h"
#include "../runloop.h"

static bool menu_keyboard_key_state[RETROK_LAST];

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
      current_input->input_state(current_input_data, binds,
            0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_X);
   int pointer_y                                =
      current_input->input_state(current_input_data, binds,
            0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_Y);

   menu_input->pointer.pressed[0]  = current_input->input_state(current_input_data, binds,
         0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
   menu_input->pointer.pressed[1]  = current_input->input_state(current_input_data, binds,
         0, pointer_device, 1, RETRO_DEVICE_ID_POINTER_PRESSED);
   menu_input->pointer.back        = current_input->input_state(current_input_data, binds,
            0, pointer_device, 0, RARCH_DEVICE_ID_POINTER_BACK);

   menu_input->pointer.x = ((pointer_x + 0x7fff) * (int)fb_width) / 0xFFFF;
   menu_input->pointer.y = ((pointer_y + 0x7fff) * (int)fb_height) / 0xFFFF;

   return 0;
}

bool menu_event_keyboard_is_set(enum retro_key key)
{
   if (menu_keyboard_key_state[key] && key == RETROK_F1)
   {
      menu_keyboard_key_state[key] = false;
      return true;
   }
   return menu_keyboard_key_state[key];
}

void menu_event_keyboard_set(enum retro_key key)
{
   menu_keyboard_key_state[key] = true;
}

unsigned menu_event(uint64_t input, uint64_t trigger_input)
{
   unsigned i;
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

   if (input)
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
         uint64_t input_repeat = 0;
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_UP);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_DOWN);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_LEFT);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_RIGHT);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_L);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_R);

         set_scroll           = true;
         first_held           = false;
         trigger_input |= input & input_repeat;

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
      if (kbd_upper)
         strlcpy(kbd_grid, "!@#$%^&*()QWERTYUIOPASDFGHJKL:ZXCVBNM <>?", sizeof(kbd_grid));
      else
         strlcpy(kbd_grid, "1234567890qwertyuiopasdfghjkl:zxcvbnm ,./", sizeof(kbd_grid));

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN))
      {
         if (kbd_index < 30)
            kbd_index = kbd_index + 10;
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP))
      {
         if (kbd_index >= 10)
            kbd_index = kbd_index - 10;
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      {
         if (kbd_index < 39)
            kbd_index = kbd_index + 1;
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT))
      {
         if (kbd_index >= 1)
            kbd_index = kbd_index - 1;
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y))
      {
         kbd_upper = ! kbd_upper;
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_A))
      {
         input_keyboard_event(true, kbd_grid[kbd_index], kbd_grid[kbd_index],
               0, RETRO_DEVICE_KEYBOARD);
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_B))
      {
         input_keyboard_event(true, '\x7f', '\x7f', 0, RETRO_DEVICE_KEYBOARD);
      }

      /* send return key to close keyboard input window */
      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START))
         input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);

      trigger_input = 0;
   }

   for (i = 0; i < RETROK_LAST; i++)
   {
      if (i == RETROK_F1)
         continue;

      if (menu_keyboard_key_state[i])
      {
         switch ((enum retro_key)i)
         {
            case RETROK_ESCAPE:
               BIT32_SET(trigger_input, RARCH_QUIT_KEY);
               break;
            case RETROK_f:
               BIT32_SET(trigger_input, RARCH_FULLSCREEN_TOGGLE_KEY);
               break;
            case RETROK_F11:
               BIT32_SET(trigger_input, RARCH_GRAB_MOUSE_TOGGLE);
               break;
            case RETROK_PAGEUP:
               BIT32_SET(trigger_input, settings->menu_scroll_up_btn);
               break;
            case RETROK_PAGEDOWN:
               BIT32_SET(trigger_input, settings->menu_scroll_down_btn);
               break;
            case RETROK_SLASH:
               BIT32_SET(trigger_input, settings->menu_search_btn);
               break;
            case RETROK_LEFT:
               BIT32_SET(trigger_input, RETRO_DEVICE_ID_JOYPAD_LEFT);
               break;
            case RETROK_RIGHT:
               BIT32_SET(trigger_input, RETRO_DEVICE_ID_JOYPAD_RIGHT);
               break;
            case RETROK_UP:
               BIT32_SET(trigger_input, RETRO_DEVICE_ID_JOYPAD_UP);
               break;
            case RETROK_DOWN:
               BIT32_SET(trigger_input, RETRO_DEVICE_ID_JOYPAD_DOWN);
               break;
            case RETROK_BACKSPACE:
               BIT32_SET(trigger_input, settings->menu_cancel_btn);
               break;
            case RETROK_RETURN:
               BIT32_SET(trigger_input, settings->menu_ok_btn);
               break;
#if 0
            default:
               break;
#endif
         }
         menu_keyboard_key_state[i] = false;
      }
   }

   if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP))
      ret = MENU_ACTION_UP;
   else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN))
      ret = MENU_ACTION_DOWN;
   else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT))
      ret = MENU_ACTION_LEFT;
   else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      ret = MENU_ACTION_RIGHT;
   else if (trigger_input & (UINT64_C(1) << settings->menu_scroll_up_btn))
      ret = MENU_ACTION_SCROLL_UP;
   else if (trigger_input & (UINT64_C(1) << settings->menu_scroll_down_btn))
      ret = MENU_ACTION_SCROLL_DOWN;
   else if (trigger_input & (UINT64_C(1) << settings->menu_cancel_btn))
      ret = MENU_ACTION_CANCEL;
   else if (trigger_input & (UINT64_C(1) << settings->menu_ok_btn))
      ret = MENU_ACTION_OK;
   else if (trigger_input & (UINT64_C(1) << settings->menu_search_btn))
      ret = MENU_ACTION_SEARCH;
   else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y))
      ret = MENU_ACTION_SCAN;
   else if (trigger_input & (UINT64_C(1) << settings->menu_default_btn))
      ret = MENU_ACTION_START;
   else if (trigger_input & (UINT64_C(1) << settings->menu_info_btn))
      ret = MENU_ACTION_INFO;
   else if (trigger_input & (UINT64_C(1) << RARCH_MENU_TOGGLE))
      ret = MENU_ACTION_TOGGLE;

   if (runloop_cmd_triggered(trigger_input, RARCH_FULLSCREEN_TOGGLE_KEY))
      command_event(CMD_EVENT_FULLSCREEN_TOGGLE, NULL);

   if (runloop_cmd_triggered(trigger_input, RARCH_GRAB_MOUSE_TOGGLE))
      command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);

   if (runloop_cmd_press(trigger_input, RARCH_QUIT_KEY))
   {
      int should_we_quit = true;

      if (!runloop_is_quit_confirm())
      {
         if (settings && settings->confirm_on_exit)
         {
            if (menu_dialog_is_active())
               should_we_quit = false;
            else if (content_is_inited())
            {
               if(menu_display_toggle_get_reason() != MENU_TOGGLE_REASON_USER)
                  menu_display_toggle_set_reason(MENU_TOGGLE_REASON_MESSAGE);
               rarch_ctl(RARCH_CTL_MENU_RUNNING, NULL);
            }

            menu_dialog_show_message(MENU_DIALOG_QUIT_CONFIRM, MENU_ENUM_LABEL_CONFIRM_ON_EXIT);

            should_we_quit = false;
         }

         if ((settings && !settings->confirm_on_exit) ||
               should_we_quit)
            return MENU_ACTION_QUIT;
      }
   }

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
   {
      menu_input->pointer.x          = 0;
      menu_input->pointer.y          = 0;
      menu_input->pointer.dx         = 0;
      menu_input->pointer.dy         = 0;
      menu_input->pointer.accel      = 0;
      menu_input->pointer.pressed[0] = false;
      menu_input->pointer.pressed[1] = false;
      menu_input->pointer.back       = false;
      menu_input->pointer.ptr        = 0;
   }

   return ret;
}

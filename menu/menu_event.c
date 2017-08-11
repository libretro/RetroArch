/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2014-2017 - Jean-André Santoni
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

#include <string/stdstring.h>

#include "widgets/menu_input_dialog.h"
#include "widgets/menu_osk.h"

#include "menu_driver.h"
#include "menu_animation.h"

#include "../configuration.h"
#include "../retroarch.h"
#include "../tasks/tasks_internal.h"

static unsigned char menu_keyboard_key_state[RETROK_LAST] = {0};

/* This function gets called for handling pointer events.
 *
 * Pointer events are touchscreen events that are spawned
 * by touchpad/touchscreen. */
static int menu_event_pointer(unsigned *action)
{
   rarch_joypad_info_t joypad_info;
   int pointer_x, pointer_y;
   size_t fb_pitch;
   unsigned fb_width, fb_height;
   const struct retro_keybind *binds[MAX_USERS] = {NULL};
   menu_input_t *menu_input                     = menu_input_get_ptr();
   int pointer_device                           = menu_driver_is_texture_set()
      ?
      RETRO_DEVICE_POINTER : RARCH_DEVICE_POINTER_SCREEN;

   menu_display_get_fb_size(&fb_width, &fb_height,
         &fb_pitch);

   joypad_info.joy_idx                          = 0;
   joypad_info.auto_binds                       = NULL;
   joypad_info.axis_threshold                   = 0.0f;

   pointer_x                                    =
      current_input->input_state(current_input_data, joypad_info, binds,
            0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_X);
   pointer_y                                    =
      current_input->input_state(current_input_data, joypad_info, binds,
            0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_Y);

   menu_input->pointer.pressed[0]  = current_input->input_state(current_input_data, 
         joypad_info,
         binds,
         0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_PRESSED);
   menu_input->pointer.pressed[1]  = current_input->input_state(current_input_data,
         joypad_info,
         binds,
         0, pointer_device, 1, RETRO_DEVICE_ID_POINTER_PRESSED);
   menu_input->pointer.back        = current_input->input_state(current_input_data,
         joypad_info,
         binds,
         0, pointer_device, 0, RARCH_DEVICE_ID_POINTER_BACK);

   menu_input->pointer.x = ((pointer_x + 0x7fff) * (int)fb_width) / 0xFFFF;
   menu_input->pointer.y = ((pointer_y + 0x7fff) * (int)fb_height) / 0xFFFF;

   return 0;
}

/* Check if a specific keyboard key has been pressed. */
unsigned char menu_event_kb_is_set(enum retro_key key)
{
   return menu_keyboard_key_state[key];
}

/* Set a specific keyboard key latch. */
static void menu_event_kb_set_internal(unsigned idx, unsigned char key)
{
   menu_keyboard_key_state[idx] = key;
}

/* Set a specific keyboard key.
 *
 * 'down' sets the latch (true would 
 * mean the key is being pressed down, while 'false' would mean that
 * the key has been released).
 **/
void menu_event_kb_set(bool down, enum retro_key key)
{
   if (key == RETROK_UNKNOWN)
   {
      unsigned i;

      for (i = 0; i < RETROK_LAST; i++)
         menu_event_kb_set_internal(i, (menu_event_kb_is_set((enum retro_key)i) & 1) << 1);
   }
   else
      menu_event_kb_set_internal(key, ((menu_event_kb_is_set(key) & 1) << 1) | down);
}

/* 
 * This function gets called in order to process all input events
 * for the current frame.
 *
 * Sends input code to menu for one frame.
 *
 * It uses as input the local variables' input' and 'trigger_input'.
 *
 * Mouse and touch input events get processed inside this function.
 *
 * NOTE: 'input' and 'trigger_input' is sourced from the keyboard and/or
 * the gamepad. It does not contain input state derived from the mouse
 * and/or touch - this gets dealt with separately within this function.
 *
 * TODO/FIXME - maybe needs to be overhauled so we can send multiple
 * events per frame if we want to, and we shouldn't send the
 * entire button state either but do a separate event per button
 * state.
 */
unsigned menu_event(uint64_t input, uint64_t trigger_input)
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
   static unsigned ok_old                  = 0;
   bool input_swap_override                = input_autoconfigure_get_swap_override();
   unsigned menu_ok_btn                    = (!input_swap_override && 
      settings->bools.input_menu_swap_ok_cancel_buttons) ?
      RETRO_DEVICE_ID_JOYPAD_B : RETRO_DEVICE_ID_JOYPAD_A;
   unsigned menu_cancel_btn                = (!input_swap_override && 
      settings->bools.input_menu_swap_ok_cancel_buttons) ?
      RETRO_DEVICE_ID_JOYPAD_A : RETRO_DEVICE_ID_JOYPAD_B;
   unsigned ok_current                     = (unsigned)(input & UINT64_C(1) << menu_ok_btn);
   unsigned ok_trigger                     = ok_current & ~ok_old;

   ok_old                                  = ok_current;

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

         menu_driver_ctl(MENU_NAVIGATION_CTL_GET_SCROLL_ACCEL,
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
      menu_driver_ctl(MENU_NAVIGATION_CTL_SET_SCROLL_ACCEL,
            &new_scroll_accel);

   menu_animation_ctl(MENU_ANIMATION_CTL_DELTA_TIME, &delta_time);

   delta.current = delta_time;

   if (menu_animation_get_ideal_delta_time(&delta))
      delay_count += delta.ideal;

   if (menu_input_dialog_get_display_kb())
   {
      menu_event_osk_iterate();

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN))
      {
         if (menu_event_get_osk_ptr() < 33)
            menu_event_set_osk_ptr(menu_event_get_osk_ptr() + OSK_CHARS_PER_LINE);
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP))
      {
         if (menu_event_get_osk_ptr() >= OSK_CHARS_PER_LINE)
            menu_event_set_osk_ptr(menu_event_get_osk_ptr() - OSK_CHARS_PER_LINE);
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      {
         if (menu_event_get_osk_ptr() < 43)
            menu_event_set_osk_ptr(menu_event_get_osk_ptr() + 1);
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT))
      {
         if (menu_event_get_osk_ptr() >= 1)
            menu_event_set_osk_ptr(menu_event_get_osk_ptr() - 1);
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L))
      {
         if (menu_event_get_osk_idx() > OSK_TYPE_UNKNOWN + 1)
            menu_event_set_osk_idx((enum osk_type)(menu_event_get_osk_idx() - 1));
         else
            menu_event_set_osk_idx((enum osk_type)(OSK_TYPE_LAST - 1));
      }

      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R))
      {
         if (menu_event_get_osk_idx() < OSK_TYPE_LAST - 1)
            menu_event_set_osk_idx((enum osk_type)(menu_event_get_osk_idx() + 1));
         else
            menu_event_set_osk_idx((enum osk_type)(OSK_TYPE_UNKNOWN + 1));
      }

      if (trigger_input & (UINT64_C(1) << menu_ok_btn))
      {
         if (menu_event_get_osk_ptr() >= 0)
            menu_event_osk_append(menu_event_get_osk_ptr());
      }

      if (trigger_input & (UINT64_C(1) << menu_cancel_btn))
      {
         input_keyboard_event(true, '\x7f', '\x7f', 0, RETRO_DEVICE_KEYBOARD);
      }

      /* send return key to close keyboard input window */
      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START))
         input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);

      trigger_input = 0;
   }
   else
   {
      if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_UP))
         ret = MENU_ACTION_UP;
      else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_DOWN))
         ret = MENU_ACTION_DOWN;
      else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_LEFT))
         ret = MENU_ACTION_LEFT;
      else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_RIGHT))
         ret = MENU_ACTION_RIGHT;
      else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_L))
         ret = MENU_ACTION_SCROLL_UP;
      else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_R))
         ret = MENU_ACTION_SCROLL_DOWN;
      else if (ok_trigger)
         ret = MENU_ACTION_OK;
      else if (trigger_input & (UINT64_C(1) << menu_cancel_btn))
         ret = MENU_ACTION_CANCEL;
      else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_X))
         ret = MENU_ACTION_SEARCH;
      else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_Y))
         ret = MENU_ACTION_SCAN;
      else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_START))
         ret = MENU_ACTION_START;
      else if (trigger_input & (UINT64_C(1) << RETRO_DEVICE_ID_JOYPAD_SELECT))
         ret = MENU_ACTION_INFO;
      else if (trigger_input & (UINT64_C(1) << RARCH_MENU_TOGGLE))
         ret = MENU_ACTION_TOGGLE;
   }

   if (menu_event_kb_is_set(RETROK_F11))
   {
      command_event(CMD_EVENT_GRAB_MOUSE_TOGGLE, NULL);
      menu_event_kb_set_internal(RETROK_F11, 0);
   }

   if (runloop_cmd_press(trigger_input, RARCH_QUIT_KEY))
      return MENU_ACTION_QUIT;

   mouse_enabled                      = settings->bools.menu_mouse_enable;
#ifdef HAVE_OVERLAY
   if (!mouse_enabled)
      mouse_enabled = !(settings->bools.input_overlay_enable
            && input_overlay_is_alive(overlay_ptr));
#endif

   if (!(menu_input = menu_input_get_ptr()))
      return 0;

   if (!mouse_enabled)
      menu_input->mouse.ptr = 0;

   if (settings->bools.menu_pointer_enable)
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

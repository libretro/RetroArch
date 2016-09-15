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

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "widgets/menu_entry.h"
#include "widgets/menu_input_dialog.h"
#include "widgets/menu_input_bind_dialog.h"

#include "menu_driver.h"
#include "menu_input.h"
#include "menu_animation.h"
#include "menu_display.h"
#include "menu_navigation.h"

#include "../configuration.h"
#include "../core.h"

enum menu_mouse_action
{
   MENU_MOUSE_ACTION_NONE = 0,
   MENU_MOUSE_ACTION_BUTTON_L,
   MENU_MOUSE_ACTION_BUTTON_L_TOGGLE,
   MENU_MOUSE_ACTION_BUTTON_L_SET_NAVIGATION,
   MENU_MOUSE_ACTION_BUTTON_R,
   MENU_MOUSE_ACTION_WHEEL_UP,
   MENU_MOUSE_ACTION_WHEEL_DOWN,
   MENU_MOUSE_ACTION_HORIZ_WHEEL_UP,
   MENU_MOUSE_ACTION_HORIZ_WHEEL_DOWN
};

typedef struct menu_input
{
   struct
   {
      unsigned ptr;
   } mouse;

   struct
   {
      int16_t x;
      int16_t y;
      int16_t dx;
      int16_t dy;
      float accel;
      bool pressed[2];
      bool back;
      unsigned ptr;
   } pointer;


   /* Used for key repeat */
   struct
   {
      float timer;
      float count;
   } delay;
} menu_input_t;

static menu_input_t *menu_input_get_ptr(void)
{
   static menu_input_t menu_input_state;
   return &menu_input_state;
}

bool menu_input_mouse_check_vector_inside_hitbox(menu_input_ctx_hitbox_t *hitbox)
{
   int16_t  mouse_x       = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
   int16_t  mouse_y       = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);
   bool     inside_hitbox =
      (mouse_x    >= hitbox->x1)
      && (mouse_x <= hitbox->x2)
      && (mouse_y >= hitbox->y1)
      && (mouse_y <= hitbox->y2)
      ;

   return inside_hitbox;
}

bool menu_input_ctl(enum menu_input_ctl_state state, void *data)
{
   static bool pointer_dragging                 = false;
   menu_input_t *menu_input                     = menu_input_get_ptr();

   if (!menu_input)
      return false;

   switch (state)
   {
      case MENU_INPUT_CTL_DEINIT:
         memset(menu_input, 0, sizeof(menu_input_t));
         pointer_dragging      = false;
         break;
      case MENU_INPUT_CTL_MOUSE_PTR:
         {
            unsigned *ptr = (unsigned*)data;
            menu_input->mouse.ptr = *ptr;
         }
         break;
      case MENU_INPUT_CTL_POINTER_PTR:
         {
            unsigned *ptr = (unsigned*)data;
            menu_input->pointer.ptr = *ptr;
         }
         break;
      case MENU_INPUT_CTL_POINTER_ACCEL_READ:
         {
            float *ptr = (float*)data;
            *ptr = menu_input->pointer.accel;
         }
         break;
      case MENU_INPUT_CTL_POINTER_ACCEL_WRITE:
         {
            float *ptr = (float*)data;
            menu_input->pointer.accel = *ptr;
         }
         break;
      case MENU_INPUT_CTL_IS_POINTER_DRAGGED:
         return pointer_dragging;
      case MENU_INPUT_CTL_SET_POINTER_DRAGGED:
         pointer_dragging = true;
         break;
      case MENU_INPUT_CTL_UNSET_POINTER_DRAGGED:
         pointer_dragging = false;
         break;
      default:
      case MENU_INPUT_CTL_NONE:
         break;
   }

   return true;
}

static int menu_input_pointer(unsigned *action)
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

static int menu_input_mouse_frame(
      menu_file_list_cbs_t *cbs, menu_entry_t *entry,
      uint64_t input_mouse, unsigned action)
{
   int ret                  = 0;
   menu_input_t *menu_input = menu_input_get_ptr();

   if (BIT64_GET(input_mouse, MENU_MOUSE_ACTION_BUTTON_L))
   {
      menu_ctx_pointer_t point;

      point.x      = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
      point.y      = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);
      point.ptr    = menu_input->mouse.ptr;
      point.cbs    = cbs;
      point.entry  = entry;
      point.action = action;

      menu_driver_ctl(RARCH_MENU_CTL_POINTER_TAP, &point);

      ret = point.retcode;
   }

   if (BIT64_GET(input_mouse, MENU_MOUSE_ACTION_BUTTON_R))
   {
      size_t selection;
      menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection);
      menu_entry_action(entry, selection, MENU_ACTION_CANCEL);
   }

   if (BIT64_GET(input_mouse, MENU_MOUSE_ACTION_WHEEL_DOWN))
   {
      unsigned increment_by = 1;
      menu_navigation_ctl(MENU_NAVIGATION_CTL_INCREMENT, &increment_by);
   }

   if (BIT64_GET(input_mouse, MENU_MOUSE_ACTION_WHEEL_UP))
   {
      unsigned decrement_by = 1;
      menu_navigation_ctl(MENU_NAVIGATION_CTL_DECREMENT, &decrement_by);
   }

   if (BIT64_GET(input_mouse, MENU_MOUSE_ACTION_HORIZ_WHEEL_UP))
   {
      /* stub */
   }

   if (BIT64_GET(input_mouse, MENU_MOUSE_ACTION_HORIZ_WHEEL_DOWN))
   {
      /* stub */
   }

   return ret;
}

static int menu_input_mouse_post_iterate(uint64_t *input_mouse,
      menu_file_list_cbs_t *cbs, unsigned action)
{
   settings_t *settings       = config_get_ptr();
   static bool mouse_oldleft  = false;
   static bool mouse_oldright = false;

   *input_mouse = MENU_MOUSE_ACTION_NONE;


   if (
         !settings->menu.mouse.enable
#ifdef HAVE_OVERLAY
         || (settings->input.overlay_enable && input_overlay_is_alive(NULL))
#endif
         )
   {
      /* HACK: Need to lie to avoid false hits if mouse is held 
       * when entering the RetroArch window. */

      /* This happens if, for example, someone double clicks the 
       * window border to maximize it.
       *
       * The proper fix is, of course, triggering on WM_LBUTTONDOWN 
       * rather than this state change. */
      mouse_oldleft   = true;
      mouse_oldright  = true;
      return 0;
   }

   if (menu_input_mouse_state(MENU_MOUSE_LEFT_BUTTON))
   {
      if (!mouse_oldleft)
      {
         size_t selection;
         menu_input_t *menu_input = menu_input_get_ptr();

         menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection);

         BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_BUTTON_L);

         mouse_oldleft = true;

         if ((menu_input->mouse.ptr == selection) && cbs && cbs->action_select)
         {
            BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_BUTTON_L_TOGGLE);
         }
         else if (menu_input->mouse.ptr <= (menu_entries_get_size() - 1))
         {
            BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_BUTTON_L_SET_NAVIGATION);
         }
      }
   }
   else
      mouse_oldleft = false;

   if (menu_input_mouse_state(MENU_MOUSE_RIGHT_BUTTON))
   {
      if (!mouse_oldright)
      {
         mouse_oldright = true;
         BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_BUTTON_R);
      }
   }
   else
      mouse_oldright = false;

   if (menu_input_mouse_state(MENU_MOUSE_WHEEL_DOWN))
   {
      BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_WHEEL_DOWN);
   }

   if (menu_input_mouse_state(MENU_MOUSE_WHEEL_UP))
   {
      BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_WHEEL_UP);
   }

   if (menu_input_mouse_state(MENU_MOUSE_HORIZ_WHEEL_DOWN))
   {
      BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_HORIZ_WHEEL_DOWN);
   }

   if (menu_input_mouse_state(MENU_MOUSE_HORIZ_WHEEL_UP))
   {
      BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_HORIZ_WHEEL_UP);
   }

   return 0;
}

int16_t menu_input_pointer_state(enum menu_input_pointer_state state)
{
   menu_input_t *menu = menu_input_get_ptr();

   if (!menu)
      return 0;

   switch (state)
   {
      case MENU_POINTER_X_AXIS:
         return menu->pointer.x;
      case MENU_POINTER_Y_AXIS:
         return menu->pointer.y;
      case MENU_POINTER_DELTA_X_AXIS:
         return menu->pointer.dx;
      case MENU_POINTER_DELTA_Y_AXIS:
         return menu->pointer.dy;
      case MENU_POINTER_PRESSED:
         return menu->pointer.pressed[0];
   }

   return 0;
}

int16_t menu_input_mouse_state(enum menu_input_mouse_state state)
{
   unsigned type   = 0;
   unsigned device = RETRO_DEVICE_MOUSE;

   switch (state)
   {
      case MENU_MOUSE_X_AXIS:
         device = RARCH_DEVICE_MOUSE_SCREEN;
         type = RETRO_DEVICE_ID_MOUSE_X;
         break;
      case MENU_MOUSE_Y_AXIS:
         device = RARCH_DEVICE_MOUSE_SCREEN;
         type = RETRO_DEVICE_ID_MOUSE_Y;
         break;
      case MENU_MOUSE_LEFT_BUTTON:
         type = RETRO_DEVICE_ID_MOUSE_LEFT;
         break;
      case MENU_MOUSE_RIGHT_BUTTON:
         type = RETRO_DEVICE_ID_MOUSE_RIGHT;
         break;
      case MENU_MOUSE_WHEEL_UP:
         type = RETRO_DEVICE_ID_MOUSE_WHEELUP;
         break;
      case MENU_MOUSE_WHEEL_DOWN:
         type = RETRO_DEVICE_ID_MOUSE_WHEELDOWN;
         break;
      case MENU_MOUSE_HORIZ_WHEEL_UP:
         type = RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP;
         break;
      case MENU_MOUSE_HORIZ_WHEEL_DOWN:
         type = RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN;
         break;
      default:
         return 0;
   }

   return input_driver_state(NULL, 0, device, 0, type);
}

static int menu_input_pointer_post_iterate(
      menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   static bool pointer_oldpressed[2];
   static bool pointer_oldback  = false;
   static int16_t start_x       = 0;
   static int16_t start_y       = 0;
   static int16_t pointer_old_x = 0;
   static int16_t pointer_old_y = 0;
   int ret                      = 0;
   menu_input_t *menu_input     = menu_input_get_ptr();
   settings_t *settings         = config_get_ptr();
   bool check_overlay           = settings ? !settings->menu.pointer.enable : false;

   if (!menu_input || !settings)
      return -1;

#ifdef HAVE_OVERLAY
   check_overlay = check_overlay ||
      (settings->input.overlay_enable && input_overlay_is_alive(NULL));
#endif

   if (check_overlay)
      return 0;

   if (menu_input->pointer.pressed[0])
   {
      gfx_ctx_metrics_t metrics;
      float dpi;
      static float accel0       = 0.0f;
      static float accel1       = 0.0f;
      int16_t pointer_x         = menu_input_pointer_state(MENU_POINTER_X_AXIS);
      int16_t pointer_y         = menu_input_pointer_state(MENU_POINTER_Y_AXIS);

      metrics.type  = DISPLAY_METRIC_DPI;
      metrics.value = &dpi;

      video_context_driver_get_metrics(&metrics);

      if (!pointer_oldpressed[0])
      {
         menu_input->pointer.accel         = 0;
         accel0                            = 0;
         accel1                            = 0;
         start_x                           = pointer_x;
         start_y                           = pointer_y;
         pointer_old_x                     = pointer_x;
         pointer_old_y                     = pointer_y;
         pointer_oldpressed[0]             = true;
      }
      else if (abs(pointer_x - start_x) > (dpi / 10)
            || abs(pointer_y - start_y) > (dpi / 10))
      {
         float s, delta_time;

         menu_input_ctl(MENU_INPUT_CTL_SET_POINTER_DRAGGED, NULL);
         menu_input->pointer.dx            = pointer_x - pointer_old_x;
         menu_input->pointer.dy            = pointer_y - pointer_old_y;
         pointer_old_x                     = pointer_x;
         pointer_old_y                     = pointer_y;

         menu_animation_ctl(MENU_ANIMATION_CTL_DELTA_TIME, &delta_time);

         s                         =  (menu_input->pointer.dy * 550000000.0 ) /
            ( dpi * delta_time );
         menu_input->pointer.accel = (accel0 + accel1 + s) / 3;
         accel0                    = accel1;
         accel1                    = menu_input->pointer.accel;
      }
   }
   else
   {
      if (pointer_oldpressed[0])
      {
         if (!menu_input_ctl(MENU_INPUT_CTL_IS_POINTER_DRAGGED, NULL))
         {
            menu_ctx_pointer_t point;

            point.x      = start_x;
            point.y      = start_y;
            point.ptr    = menu_input->pointer.ptr;
            point.cbs    = cbs;
            point.entry  = entry;
            point.action = action;

            menu_driver_ctl(RARCH_MENU_CTL_POINTER_TAP, &point);

            ret = point.retcode;
         }

         pointer_oldpressed[0]             = false;
         start_x                           = 0;
         start_y                           = 0;
         pointer_old_x                     = 0;
         pointer_old_y                     = 0;
         menu_input->pointer.dx            = 0;
         menu_input->pointer.dy            = 0;

         menu_input_ctl(MENU_INPUT_CTL_UNSET_POINTER_DRAGGED, NULL);
      }
   }

   if (menu_input->pointer.back)
   {
      if (!pointer_oldback)
      {
         size_t selection;
         pointer_oldback = true;
         menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection);
         menu_entry_action(entry, selection, MENU_ACTION_CANCEL);
      }
   }

   pointer_oldback = menu_input->pointer.back;

   return ret;
}

void menu_input_post_iterate(int *ret, unsigned action)
{
   size_t selection;
   uint64_t mouse_state       = 0;
   menu_file_list_cbs_t *cbs  = NULL;
   menu_entry_t entry         = {{0}};
   settings_t *settings       = config_get_ptr();
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return;

   if (selection_buf)
      cbs = menu_entries_get_actiondata_at_offset(selection_buf, selection);

   menu_entry_get(&entry, 0, selection, NULL, false);

   if (settings->menu.mouse.enable)
      *ret  = menu_input_mouse_post_iterate(&mouse_state, cbs, action);

   *ret = menu_input_mouse_frame(cbs, &entry, mouse_state, action);

   if (settings->menu.pointer.enable)
      *ret |= menu_input_pointer_post_iterate(cbs, &entry, action);
}

unsigned menu_event(retro_input_t input,
      retro_input_t trigger_input)
{
   menu_animation_ctx_delta_t delta;
   float delta_time;
   unsigned ret                            = MENU_ACTION_NOOP;
   static bool initial_held                = true;
   static bool first_held                  = false;
   bool set_scroll                         = false;
   bool mouse_enabled                      = false;
   size_t new_scroll_accel                 = 0;
   settings_t *settings                    = config_get_ptr();
   menu_input_t *menu_input                = menu_input_get_ptr();

   if (!menu_input)
      return 0;

   core_poll();

   /* don't run anything first frame, only capture held inputs
    * for old_input_state. */

   if (input.state)
   {
      if (!first_held)
      {
         first_held = true;
         menu_input->delay.timer = initial_held ? 12 : 6;
         menu_input->delay.count = 0;
      }

      if (menu_input->delay.count >= menu_input->delay.timer)
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
      menu_input->delay.count += delta.ideal;

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

   if (!mouse_enabled)
      menu_input->mouse.ptr = 0;

   if (settings->menu.pointer.enable)
      menu_input_pointer(&ret);
   else
      memset(&menu_input->pointer, 0, sizeof(menu_input->pointer));

   return ret;
}

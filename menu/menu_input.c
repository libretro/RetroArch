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

#define MENU_MAX_BUTTONS 219
#define MENU_MAX_AXES    32
#define MENU_MAX_HATS    4

#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include "menu_driver.h"
#include "menu_input.h"
#include "menu_animation.h"
#include "menu_display.h"
#include "menu_entry.h"
#include "menu_setting.h"
#include "menu_shader.h"
#include "menu_navigation.h"
#include "menu_hash.h"

#include "../general.h"
#include "../cheats.h"
#include "../performance.h"
#include "../core.h"
#include "../input/input_joypad_driver.h"
#include "../input/input_remapping.h"
#include "../input/input_config.h"

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

struct menu_bind_state_port
{
   bool buttons[MENU_MAX_BUTTONS];
   int16_t axes[MENU_MAX_AXES];
   uint16_t hats[MENU_MAX_HATS];
};

struct menu_bind_axis_state
{
   /* Default axis state. */
   int16_t rested_axes[MENU_MAX_AXES];
   /* Locked axis state. If we configured an axis,
    * avoid having the same axis state trigger something again right away. */
   int16_t locked_axes[MENU_MAX_AXES];
};

struct menu_bind_state
{
   struct retro_keybind *target;
   /* For keyboard binding. */
   int64_t timeout_end;
   unsigned begin;
   unsigned last;
   unsigned user;
   struct menu_bind_state_port state[MAX_USERS];
   struct menu_bind_axis_state axis_state[MAX_USERS];
   bool skip;
};

typedef struct menu_input
{
   struct menu_bind_state binds;

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

   struct
   {
      bool display;
      unsigned type;
      unsigned idx;
   } keyboard;

   /* Used for key repeat */
   struct
   {
      float timer;
      float count;
   } delay;
} menu_input_t;

static unsigned           bind_port;

static menu_input_t *menu_input_get_ptr(void)
{
   static menu_input_t menu_input_state;
   return &menu_input_state;
}

static void menu_input_key_end_line(void)
{
   bool keyboard_display    = false;

   menu_input_ctl(MENU_INPUT_CTL_SET_KEYBOARD_DISPLAY, &keyboard_display);
   menu_input_ctl(MENU_INPUT_CTL_UNSET_KEYBOARD_LABEL, NULL);
   menu_input_ctl(MENU_INPUT_CTL_UNSET_KEYBOARD_LABEL_SETTING, NULL);

   /* Avoid triggering states on pressing return. */
   input_driver_set_flushing_input();
}

static void menu_input_search_cb(void *userdata, const char *str)
{
   size_t idx = 0;
   file_list_t *selection_buf = menu_entries_get_selection_buf_ptr(0);

   if (!selection_buf)
      return;

   if (str && *str && file_list_search(selection_buf, str, &idx))
   {
      bool scroll = true;
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &idx);
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET, &scroll);
   }

   menu_input_key_end_line();
}

void menu_input_st_uint_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      rarch_setting_t         *setting = NULL;
      const char                *label = NULL;

      menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_LABEL_SETTING, &label);

      setting = menu_setting_find(label);
      menu_setting_set_with_string_representation(setting, str);
   }

   menu_input_key_end_line();
}

void menu_input_st_hex_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      rarch_setting_t         *setting = NULL;
      const char                *label = NULL;

      menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_LABEL_SETTING, &label);

      setting = menu_setting_find(label);

      if (setting)
      {
         unsigned *ptr = (unsigned*)setting_get_ptr(setting);
         if (str[0] == '#')
            str++;
         *ptr = strtoul(str, NULL, 16);
      }
   }

   menu_input_key_end_line();
}


void menu_input_st_string_cb(void *userdata, const char *str)
{
   if (str && *str)
   {
      rarch_setting_t         *setting = NULL;
      const char                *label = NULL;

      menu_input_ctl(MENU_INPUT_CTL_KEYBOARD_LABEL_SETTING, &label);

      setting = menu_setting_find(label);

      if (setting)
      {
         menu_setting_set_with_string_representation(setting, str);
         menu_setting_generic(setting, false);
      }
      else
      {
         uint32_t hash_label = menu_hash_calculate(label);

         switch (hash_label)
         {
            case MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
               menu_shader_manager_save_preset(str, false);
               break;
            case MENU_LABEL_CHEAT_FILE_SAVE_AS:
               cheat_manager_save(str);
               break;
         }
      }
   }

   menu_input_key_end_line();
}

void menu_input_st_cheat_cb(void *userdata, const char *str)
{
   menu_input_t *menu_input = menu_input_get_ptr();

   (void)userdata;

   if (!menu_input)
      return;

   if (str && *str)
   {
      unsigned cheat_index = 
         menu_input->keyboard.type - MENU_SETTINGS_CHEAT_BEGIN;
      cheat_manager_set_code(cheat_index, str);
   }

   menu_input_key_end_line();
}

static bool menu_input_key_bind_custom_bind_keyboard_cb(
      void *data, unsigned code)
{
   menu_input_t *menu_input = menu_input_get_ptr();

   if (!menu_input)
      return false;

   menu_input->binds.target->key = (enum retro_key)code;
   menu_input->binds.begin++;
   menu_input->binds.target++;
   menu_input->binds.timeout_end = retro_get_time_usec() +
      MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;

   return (menu_input->binds.begin <= menu_input->binds.last);
}

static int menu_input_key_bind_set_mode_common(
      enum menu_input_ctl_state state,
      rarch_setting_t  *setting)
{
   size_t selection;
   unsigned index_offset, bind_type;
   menu_displaylist_info_t info  = {0};
   struct retro_keybind *keybind = NULL;
   file_list_t *menu_stack       = NULL;
   settings_t     *settings      = config_get_ptr();
   menu_input_t      *menu_input = menu_input_get_ptr();

   if (!setting)
      return -1;

   index_offset = menu_setting_get_index_offset(setting);
   menu_stack   = menu_entries_get_menu_stack_ptr(0);

   menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection);

   switch (state)
   {
      case MENU_INPUT_CTL_BIND_NONE:
         return -1;
      case MENU_INPUT_CTL_BIND_SINGLE:
         keybind    = (struct retro_keybind*)setting_get_ptr(setting);

         if (!keybind)
            return -1;

         bind_type                = menu_setting_get_bind_type(setting);

         menu_input->binds.begin  = bind_type;
         menu_input->binds.last   = bind_type;
         menu_input->binds.target = keybind;
         menu_input->binds.user   = index_offset;

         info.list                = menu_stack;
         info.type                = MENU_SETTINGS_CUSTOM_BIND_KEYBOARD;
         info.directory_ptr       = selection;
         strlcpy(info.label,
               menu_hash_to_str(MENU_LABEL_CUSTOM_BIND), sizeof(info.label));

         if (menu_displaylist_ctl(DISPLAYLIST_INFO, &info))
            menu_displaylist_ctl(DISPLAYLIST_PROCESS, &info);
         break;
      case MENU_INPUT_CTL_BIND_ALL:
         menu_input->binds.target = &settings->input.binds
            [index_offset][0];
         menu_input->binds.begin  = MENU_SETTINGS_BIND_BEGIN;
         menu_input->binds.last   = MENU_SETTINGS_BIND_LAST;

         info.list                = menu_stack;
         info.type                = MENU_SETTINGS_CUSTOM_BIND_KEYBOARD;
         info.directory_ptr       = selection;
         strlcpy(info.label,
               menu_hash_to_str(MENU_LABEL_CUSTOM_BIND_ALL),
               sizeof(info.label));

         if (menu_displaylist_ctl(DISPLAYLIST_INFO, &info))
            menu_displaylist_ctl(DISPLAYLIST_PROCESS, &info);
         break;
      default:
      case MENU_INPUT_CTL_NONE:
         break;
   }

   return 0;
}

static void menu_input_key_bind_poll_bind_get_rested_axes(
      struct menu_bind_state *state, unsigned port)
{
   unsigned a;
   const input_device_driver_t     *joypad = 
      input_driver_get_joypad_driver();
   const input_device_driver_t *sec_joypad = 
      input_driver_get_sec_joypad_driver();

   if (!state || !joypad)
      return;

   /* poll only the relevant port */
   for (a = 0; a < MENU_MAX_AXES; a++)
      state->axis_state[port].rested_axes[a] = 
         input_joypad_axis_raw(joypad, port, a);
    
   if (sec_joypad)
   {
        /* poll only the relevant port */
        for (a = 0; a < MENU_MAX_AXES; a++)
            state->axis_state[port].rested_axes[a] = 
               input_joypad_axis_raw(sec_joypad, port, a);
   }
}

static void menu_input_key_bind_poll_bind_state_internal(
      const input_device_driver_t *joypad,
      struct menu_bind_state *state,
      unsigned port,
      bool timed_out)
{
   unsigned b, a, h;
    if (!joypad)
        return;
    
    if (joypad->poll)
        joypad->poll();
    
    /* poll only the relevant port */
    /* for (i = 0; i < settings->input.max_users; i++) */
    for (b = 0; b < MENU_MAX_BUTTONS; b++)
        state->state[port].buttons[b] = 
           input_joypad_button_raw(joypad, port, b);
    
    for (a = 0; a < MENU_MAX_AXES; a++)
        state->state[port].axes[a] = 
           input_joypad_axis_raw(joypad, port, a);
    
    for (h = 0; h < MENU_MAX_HATS; h++)
    {
        if (input_joypad_hat_raw(joypad, port, HAT_UP_MASK, h))
            state->state[port].hats[h] |= HAT_UP_MASK;
        if (input_joypad_hat_raw(joypad, port, HAT_DOWN_MASK, h))
            state->state[port].hats[h] |= HAT_DOWN_MASK;
        if (input_joypad_hat_raw(joypad, port, HAT_LEFT_MASK, h))
            state->state[port].hats[h] |= HAT_LEFT_MASK;
        if (input_joypad_hat_raw(joypad, port, HAT_RIGHT_MASK, h))
            state->state[port].hats[h] |= HAT_RIGHT_MASK;
    }
}

static void menu_input_key_bind_poll_bind_state(
      struct menu_bind_state *state,
      unsigned port,
      bool timed_out)
{
   const input_device_driver_t *joypad     = 
      input_driver_get_joypad_driver();
   const input_device_driver_t *sec_joypad = 
      input_driver_get_sec_joypad_driver();

   if (!state)
      return;

   memset(state->state, 0, sizeof(state->state));
   state->skip = timed_out || input_driver_state(NULL, 0,
         RETRO_DEVICE_KEYBOARD, 0, RETROK_RETURN);
    
   menu_input_key_bind_poll_bind_state_internal(
         joypad, state, port, timed_out);
    
   if (sec_joypad)
      menu_input_key_bind_poll_bind_state_internal(
            sec_joypad, state, port, timed_out);
}

static bool menu_input_key_bind_set_mode(
      enum menu_input_ctl_state state, void *data)
{
   unsigned index_offset;
   input_keyboard_ctx_wait_t keys;
   menu_handle_t       *menu = NULL;
   menu_input_t  *menu_input = menu_input_get_ptr();
   rarch_setting_t  *setting = (rarch_setting_t*)data;
   settings_t *settings      = config_get_ptr();

   if (!setting)
      return false;
   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return false;
   if (menu_input_key_bind_set_mode_common(state, setting) == -1)
      return false;

   index_offset = menu_setting_get_index_offset(setting);
   bind_port    = settings->input.joypad_map[index_offset];

   menu_input_key_bind_poll_bind_get_rested_axes(
         &menu_input->binds, bind_port);
   menu_input_key_bind_poll_bind_state(
         &menu_input->binds, bind_port, false);

   menu_input->binds.timeout_end   = retro_get_time_usec() +
      MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;

   keys.userdata = menu;
   keys.cb       = menu_input_key_bind_custom_bind_keyboard_cb;

   input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_START_WAIT_KEYS, &keys);
   return true;
}

static bool menu_input_key_bind_poll_find_trigger_pad(
      struct menu_bind_state *state,
      struct menu_bind_state *new_state,
      unsigned p)
{
   unsigned a, b, h;
   const struct menu_bind_state_port *n = (const struct menu_bind_state_port*)
      &new_state->state[p];
   const struct menu_bind_state_port *o = (const struct menu_bind_state_port*)
      &state->state[p];

   for (b = 0; b < MENU_MAX_BUTTONS; b++)
   {
      bool iterate = n->buttons[b] && !o->buttons[b];

      if (!iterate)
         continue;

      state->target->joykey = b;
      state->target->joyaxis = AXIS_NONE;
      return true;
   }

   /* Axes are a bit tricky ... */
   for (a = 0; a < MENU_MAX_AXES; a++)
   {
      int locked_distance = abs(n->axes[a] -
            new_state->axis_state[p].locked_axes[a]);
      int rested_distance = abs(n->axes[a] -
            new_state->axis_state[p].rested_axes[a]);

      if (abs(n->axes[a]) >= 20000 &&
            locked_distance >= 20000 &&
            rested_distance >= 20000)
      {
         /* Take care of case where axis rests on +/- 0x7fff
          * (e.g. 360 controller on Linux) */
         state->target->joyaxis = n->axes[a] > 0 
            ? AXIS_POS(a) : AXIS_NEG(a);
         state->target->joykey = NO_BTN;

         /* Lock the current axis */
         new_state->axis_state[p].locked_axes[a] = 
            n->axes[a] > 0 ? 
            0x7fff : -0x7fff;
         return true;
      }

      if (locked_distance >= 20000) /* Unlock the axis. */
         new_state->axis_state[p].locked_axes[a] = 0;
   }

   for (h = 0; h < MENU_MAX_HATS; h++)
   {
      uint16_t      trigged = n->hats[h] & (~o->hats[h]);
      uint16_t sane_trigger = 0;

      if (trigged & HAT_UP_MASK)
         sane_trigger = HAT_UP_MASK;
      else if (trigged & HAT_DOWN_MASK)
         sane_trigger = HAT_DOWN_MASK;
      else if (trigged & HAT_LEFT_MASK)
         sane_trigger = HAT_LEFT_MASK;
      else if (trigged & HAT_RIGHT_MASK)
         sane_trigger = HAT_RIGHT_MASK;

      if (sane_trigger)
      {
         state->target->joykey = HAT_MAP(h, sane_trigger);
         state->target->joyaxis = AXIS_NONE;
         return true;
      }
   }

   return false;
}

static bool menu_input_key_bind_poll_find_trigger(
      struct menu_bind_state *state,
      struct menu_bind_state *new_state)
{
   unsigned i;
   settings_t *settings = config_get_ptr();

   if (!state || !new_state)
      return false;

   for (i = 0; i < settings->input.max_users; i++)
   {
      if (!menu_input_key_bind_poll_find_trigger_pad(
               state, new_state, i))
         continue;

      /* Update the joypad mapping automatically.
       * More friendly that way. */
#if 0
      settings->input.joypad_map[state->user] = i;
#endif
      return true;
   }
   return false;
}


static bool menu_input_key_bind_iterate(char *s, size_t len)
{
   struct menu_bind_state binds;
   bool               timed_out = false;
   menu_input_t *menu_input     = menu_input_get_ptr();
   int64_t current              = retro_get_time_usec();
   int timeout                  = 
      (menu_input->binds.timeout_end - current) / 1000000;

   if (timeout <= 0)
   {
      input_driver_keyboard_mapping_set_block(false);

      menu_input->binds.begin++;
      menu_input->binds.target++;
      menu_input->binds.timeout_end = retro_get_time_usec() +
         MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
      timed_out = true;
   }

   snprintf(s, len,
         "[%s]\npress keyboard or joypad\n(timeout %d %s)",
         input_config_bind_map_get_desc(
         menu_input->binds.begin - MENU_SETTINGS_BIND_BEGIN),
         timeout,
         menu_hash_to_str(MENU_VALUE_SECONDS));

   /* binds.begin is updated in keyboard_press callback. */
   if (menu_input->binds.begin > menu_input->binds.last)
   {
      /* Avoid new binds triggering things right away. */
      input_driver_set_flushing_input();

      /* We won't be getting any key events, so just cancel early. */
      if (timed_out)
         input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_CANCEL_WAIT_KEYS, NULL);

      return true;
   }

   binds = menu_input->binds;

   input_driver_keyboard_mapping_set_block(true);
   menu_input_key_bind_poll_bind_state(&binds, bind_port, timed_out);

   if ((binds.skip && !menu_input->binds.skip) ||
         menu_input_key_bind_poll_find_trigger(&menu_input->binds, &binds))
   {
      input_driver_keyboard_mapping_set_block(false);

      /* Avoid new binds triggering things right away. */
      input_driver_set_flushing_input();

      binds.begin++;

      if (binds.begin > binds.last)
      {
         input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_CANCEL_WAIT_KEYS, NULL);
         return true;
      }

      binds.target++;
      binds.timeout_end = retro_get_time_usec() +
         MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
   }
   menu_input->binds = binds;

   return false;
}

bool menu_input_ctl(enum menu_input_ctl_state state, void *data)
{
   static char menu_input_keyboard_label_setting[256];
   static const char **menu_input_keyboard_buffer;
   static const char *menu_input_keyboard_label = NULL;
   static bool pointer_dragging                 = false;
   menu_input_t *menu_input                     = menu_input_get_ptr();

   if (!menu_input)
      return false;

   switch (state)
   {
      case MENU_INPUT_CTL_BIND_SET_MIN_MAX:
         {
            menu_input_ctx_bind_limits_t *lim = 
               (menu_input_ctx_bind_limits_t*)data;
            if (!lim || !menu_input)
               return false;

            menu_input->binds.begin = lim->min;
            menu_input->binds.last  = lim->max;
         }
         break;
      case MENU_INPUT_CTL_CHECK_INSIDE_HITBOX:
         {
            menu_input_ctx_hitbox_t *hitbox = (menu_input_ctx_hitbox_t*)data;
            int16_t  mouse_x       = menu_input_mouse_state(MENU_MOUSE_X_AXIS);
            int16_t  mouse_y       = menu_input_mouse_state(MENU_MOUSE_Y_AXIS);
            bool     inside_hitbox = 
                  (mouse_x    >= hitbox->x1) 
                  && (mouse_x <= hitbox->x2) 
                  && (mouse_y >= hitbox->y1) 
                  && (mouse_y <= hitbox->y2)
                  ;

            if (!inside_hitbox)
               return false;
         }
         break;
      case MENU_INPUT_CTL_DEINIT:
         memset(menu_input, 0, sizeof(menu_input_t));
         pointer_dragging      = false;
         break;
      case MENU_INPUT_CTL_SEARCH_START:
         {
            menu_handle_t      *menu = NULL;
            if (!menu_driver_ctl(
                     RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
               return false;

            menu_input->keyboard.display = true;
            menu_input_keyboard_label    = menu_hash_to_str(MENU_VALUE_SEARCH);
            menu_input_keyboard_buffer   =
               input_keyboard_start_line(menu, menu_input_search_cb);
         }
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
      case MENU_INPUT_CTL_KEYBOARD_DISPLAY:
         {
            bool *ptr = (bool*)data;
            *ptr = menu_input->keyboard.display;
         }
         break;
      case MENU_INPUT_CTL_SET_KEYBOARD_DISPLAY:
         {
            bool *ptr = (bool*)data;
            menu_input->keyboard.display = *ptr;
         }
         break;
      case MENU_INPUT_CTL_KEYBOARD_BUFF_PTR:
         {
            const char **ptr = (const char**)data;
            *ptr = *menu_input_keyboard_buffer;
         }
         break;
      case MENU_INPUT_CTL_KEYBOARD_LABEL:
         {
            const char **ptr = (const char**)data;
            *ptr = menu_input_keyboard_label;
         }
         break;
      case MENU_INPUT_CTL_SET_KEYBOARD_LABEL:
         {
            char **ptr = (char**)data;
            menu_input_keyboard_label = *ptr;
         }
         break;
      case MENU_INPUT_CTL_UNSET_KEYBOARD_LABEL:
         menu_input_keyboard_label = NULL;
         break;
      case MENU_INPUT_CTL_KEYBOARD_LABEL_SETTING:
         {
            const char **ptr = (const char**)data;
            *ptr = menu_input_keyboard_label_setting;
         }
         break;
      case MENU_INPUT_CTL_SET_KEYBOARD_LABEL_SETTING:
         {
            char **ptr = (char**)data;
            strlcpy(menu_input_keyboard_label_setting,
            *ptr, sizeof(menu_input_keyboard_label_setting));
         }
         break;
      case MENU_INPUT_CTL_UNSET_KEYBOARD_LABEL_SETTING:
         menu_input_keyboard_label_setting[0] = '\0';
         break;
      case MENU_INPUT_CTL_BIND_NONE:
      case MENU_INPUT_CTL_BIND_SINGLE:
      case MENU_INPUT_CTL_BIND_ALL:
         return menu_input_key_bind_set_mode(state, data);
      case MENU_INPUT_CTL_BIND_ITERATE:
         {
            menu_input_ctx_bind_t *bind = (menu_input_ctx_bind_t*)data;
            if (!bind)
               return false;
            return menu_input_key_bind_iterate(bind->s, bind->len);
         }
      case MENU_INPUT_CTL_START_LINE:
         {
            bool keyboard_display       = true;
            menu_handle_t    *menu      = NULL;
            menu_input_ctx_line_t *line = (menu_input_ctx_line_t*)data;
            if (!menu_input || !line)
               return false;
            if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
               return false;

            menu_input_ctl(MENU_INPUT_CTL_SET_KEYBOARD_DISPLAY,
                  &keyboard_display);
            menu_input_ctl(MENU_INPUT_CTL_SET_KEYBOARD_LABEL,
                  &line->label);
            menu_input_ctl(MENU_INPUT_CTL_SET_KEYBOARD_LABEL_SETTING,
                  &line->label_setting);

            menu_input->keyboard.type   = line->type;
            menu_input->keyboard.idx    = line->idx;
            menu_input_keyboard_buffer  = 
               input_keyboard_start_line(menu, line->cb);
         }
         break;
      default:
      case MENU_INPUT_CTL_NONE:
         break;
   }

   return true;
}

static int menu_input_pointer(unsigned *action)
{
   unsigned fb_width, fb_height;
   int pointer_device, pointer_x, pointer_y;
   const struct retro_keybind *binds[MAX_USERS] = {NULL};
   menu_input_t *menu_input                     = menu_input_get_ptr();

   fb_width  = menu_display_get_width();
   fb_height = menu_display_get_height();

   pointer_device = menu_driver_ctl(RARCH_MENU_CTL_IS_SET_TEXTURE, NULL) ?
        RETRO_DEVICE_POINTER : RARCH_DEVICE_POINTER_SCREEN;

   menu_input->pointer.pressed[0]  = input_driver_state(binds,
         0, pointer_device,
         0, RETRO_DEVICE_ID_POINTER_PRESSED);
   menu_input->pointer.pressed[1]  = input_driver_state(binds,
         0, pointer_device,
         1, RETRO_DEVICE_ID_POINTER_PRESSED);
   menu_input->pointer.back  = input_driver_state(binds, 0, pointer_device,
         0, RARCH_DEVICE_ID_POINTER_BACK);

   pointer_x = input_driver_state(binds, 0, pointer_device,
         0, RETRO_DEVICE_ID_POINTER_X);
   pointer_y = input_driver_state(binds, 0, pointer_device,
         0, RETRO_DEVICE_ID_POINTER_Y);

   menu_input->pointer.x = ((pointer_x + 0x7fff) * (int)fb_width) / 0xFFFF;
   menu_input->pointer.y = ((pointer_y + 0x7fff) * (int)fb_height) / 0xFFFF;

   return 0;
}

static int menu_input_mouse_frame(
      menu_file_list_cbs_t *cbs, menu_entry_t *entry,
      uint64_t input_mouse, unsigned action)
{
   size_t selection;
   int ret                  = 0;
   menu_input_t *menu_input = menu_input_get_ptr();

   menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection);


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
      menu_entries_pop_stack(&selection, 0, 1);
      menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &selection);
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
         || (settings->input.overlay_enable && input_overlay_is_alive())
#endif
         )
   {
      mouse_oldleft   = false;
      mouse_oldright  = false;
      return 0;
   }

   if (menu_input_mouse_state(MENU_MOUSE_LEFT_BUTTON))
   {
      if (!mouse_oldleft)
      {
         size_t selection;
         unsigned header_height;
         menu_input_t *menu_input = menu_input_get_ptr();

         menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection);
         header_height = menu_display_get_header_height();

         BIT64_SET(*input_mouse, MENU_MOUSE_ACTION_BUTTON_L);

         mouse_oldleft = true;

         /* Back button */
         if ((unsigned)menu_input_mouse_state(MENU_MOUSE_X_AXIS) < header_height)
         {
            menu_entries_pop_stack(&selection, 0, 1);
            menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &selection);
            return 0;
         }
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
   size_t selection;
   static bool pointer_oldpressed[2];
   static bool pointer_oldback  = false;
   static int16_t start_x       = 0;
   static int16_t start_y       = 0;
   static int16_t pointer_old_x = 0;
   static int16_t pointer_old_y = 0;
   int ret                      = 0;
   menu_input_t *menu_input     = menu_input_get_ptr();
   settings_t *settings         = config_get_ptr();
   bool check_overlay           = false;
   
   if (settings)
      check_overlay             = !settings->menu.pointer.enable;

   if (!menu_input)
      return -1;
   if (!menu_navigation_ctl(MENU_NAVIGATION_CTL_GET_SELECTION, &selection))
      return -1;

#ifdef HAVE_OVERLAY
   check_overlay = check_overlay || 
      (settings->input.overlay_enable && input_overlay_is_alive());
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
         pointer_oldback = true;
         menu_entries_pop_stack(&selection, 0, 1);
         menu_navigation_ctl(MENU_NAVIGATION_CTL_SET_SELECTION, &selection);
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

static unsigned menu_input_frame_pointer(unsigned *data)
{
   unsigned ret                            = *data;
   settings_t *settings                    = config_get_ptr();
   menu_input_t *menu_input                = menu_input_get_ptr();
   bool mouse_enabled                      = settings->menu.mouse.enable;
#ifdef HAVE_OVERLAY
   if (!mouse_enabled)
      mouse_enabled = !(settings->input.overlay_enable 
            && input_overlay_is_alive());
#endif
    
   if (!mouse_enabled)
      menu_input->mouse.ptr = 0;

   if (settings->menu.pointer.enable)
      menu_input_pointer(&ret);
   else
      memset(&menu_input->pointer, 0, sizeof(menu_input->pointer));

   return ret;
}

static unsigned menu_input_frame_build(retro_input_t trigger_input)
{
   settings_t *settings = config_get_ptr();
   unsigned ret         = MENU_ACTION_NOOP;

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

   return menu_input_frame_pointer(&ret);
}

unsigned menu_input_frame_retropad(retro_input_t input,
      retro_input_t trigger_input)
{
   menu_animation_ctx_delta_t delta;
   float delta_time;
   static bool initial_held                = true;
   static bool first_held                  = false;
   bool set_scroll                         = false;
   size_t new_scroll_accel                 = 0;
   menu_input_t *menu_input                = menu_input_get_ptr();

   if (!menu_input)
      return 0;

   core_poll();

   /* don't run anything first frame, only capture held inputs
    * for old_input_state. */

   if (input)
   {
      if (!first_held)
      {
         first_held = true;
         menu_input->delay.timer = initial_held ? 12 : 6;
         menu_input->delay.count = 0;
      }

      if (menu_input->delay.count >= menu_input->delay.timer)
      {
         retro_input_t input_repeat = 0;
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_UP);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_DOWN);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_LEFT);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_RIGHT);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_B);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_A);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_L);
         BIT32_SET(input_repeat, RETRO_DEVICE_ID_JOYPAD_R);

         set_scroll     = true;
         first_held     = false;
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
      menu_input->delay.count += delta.ideal;

   if (menu_input->keyboard.display)
   {
      settings_t *settings = config_get_ptr();

      /* send return key to close keyboard input window */
      if (trigger_input & (UINT64_C(1) << settings->menu_cancel_btn))
         input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);

      trigger_input = 0;
   }

   return menu_input_frame_build(trigger_input);
}

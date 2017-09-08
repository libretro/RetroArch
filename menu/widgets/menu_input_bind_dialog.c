/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <compat/strl.h>

#include <features/features_cpu.h>

#include "menu_input_bind_dialog.h"

#include "../menu_driver.h"

#include "../../input/input_driver.h"

#include "../../configuration.h"
#include "../../performance_counters.h"

#define MENU_MAX_BUTTONS 219
#define MENU_MAX_AXES    32
#define MENU_MAX_HATS    4

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

   rarch_timer_t timer;
   unsigned begin;
   unsigned last;
   unsigned user;
   struct menu_bind_state_port state[MAX_USERS];
   struct menu_bind_axis_state axis_state[MAX_USERS];
   bool skip;
};

static unsigned               menu_bind_port   = 0;
static struct menu_bind_state menu_input_binds = {0};

static bool menu_input_key_bind_custom_bind_keyboard_cb(
      void *data, unsigned code)
{
   settings_t     *settings = config_get_ptr();

   menu_input_binds.target->key = (enum retro_key)code;
   menu_input_binds.begin++;
   menu_input_binds.target++;
   
   rarch_timer_begin_new_time(&menu_input_binds.timer, settings->uints.input_bind_timeout);

   return (menu_input_binds.begin <= menu_input_binds.last);
}

static int menu_input_key_bind_set_mode_common(
      enum menu_input_binds_ctl_state state,
      rarch_setting_t  *setting)
{
   unsigned bind_type            = 0;
   menu_displaylist_info_t info  = {0};
   struct retro_keybind *keybind = NULL;
   unsigned         index_offset = setting->index_offset;
   file_list_t *menu_stack       = menu_entries_get_menu_stack_ptr(0);
   size_t selection              = menu_navigation_get_selection();

   switch (state)
   {
      case MENU_INPUT_BINDS_CTL_BIND_SINGLE:
         keybind    = (struct retro_keybind*)setting_get_ptr(setting);

         if (!keybind)
            return -1;

         bind_type                = setting_get_bind_type(setting);

         menu_input_binds.begin   = bind_type;
         menu_input_binds.last    = bind_type;
         menu_input_binds.target  = keybind;
         menu_input_binds.user    = index_offset;

         info.list                = menu_stack;
         info.type                = MENU_SETTINGS_CUSTOM_BIND_KEYBOARD;
         info.directory_ptr       = selection;
         info.enum_idx            = MENU_ENUM_LABEL_CUSTOM_BIND;
         strlcpy(info.label,
               msg_hash_to_str(MENU_ENUM_LABEL_CUSTOM_BIND), sizeof(info.label));

         if (menu_displaylist_ctl(DISPLAYLIST_INFO, &info))
            menu_displaylist_process(&info);
         break;
      case MENU_INPUT_BINDS_CTL_BIND_ALL:
         menu_input_binds.target  = &input_config_binds[index_offset][0];
         menu_input_binds.begin   = MENU_SETTINGS_BIND_BEGIN;
         menu_input_binds.last    = MENU_SETTINGS_BIND_LAST;

         info.list                = menu_stack;
         info.type                = MENU_SETTINGS_CUSTOM_BIND_KEYBOARD;
         info.directory_ptr       = selection;
         info.enum_idx            = MENU_ENUM_LABEL_CUSTOM_BIND_ALL;
         strlcpy(info.label,
               msg_hash_to_str(MENU_ENUM_LABEL_CUSTOM_BIND_ALL),
               sizeof(info.label));

         if (menu_displaylist_ctl(DISPLAYLIST_INFO, &info))
            menu_displaylist_process(&info);
         break;
      default:
      case MENU_INPUT_BINDS_CTL_BIND_NONE:
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
   rarch_joypad_info_t joypad_info;
   const input_device_driver_t *joypad     =
      input_driver_get_joypad_driver();
   const input_device_driver_t *sec_joypad =
      input_driver_get_sec_joypad_driver();

   if (!state)
      return;

   memset(state->state, 0, sizeof(state->state));

   joypad_info.joy_idx        = 0;
   joypad_info.auto_binds     = NULL;
   joypad_info.axis_threshold = 0.0f;

   state->skip = timed_out || current_input->input_state(current_input_data, joypad_info,
         NULL,
         0, RETRO_DEVICE_KEYBOARD, 0, RETROK_RETURN);

   menu_input_key_bind_poll_bind_state_internal(
         joypad, state, port, timed_out);

   if (sec_joypad)
      menu_input_key_bind_poll_bind_state_internal(
            sec_joypad, state, port, timed_out);
}

bool menu_input_key_bind_set_mode(
      enum menu_input_binds_ctl_state state, void *data)
{
   unsigned index_offset;
   input_keyboard_ctx_wait_t keys;
   menu_handle_t       *menu = NULL;
   rarch_setting_t  *setting = (rarch_setting_t*)data;
   settings_t *settings      = config_get_ptr();

   if (!setting)
      return false;
   if (!menu_driver_ctl(RARCH_MENU_CTL_DRIVER_DATA_GET, &menu))
      return false;
   if (menu_input_key_bind_set_mode_common(state, setting) == -1)
      return false;

   index_offset      = setting->index_offset;
   menu_bind_port    = settings->uints.input_joypad_map[index_offset];

   menu_input_key_bind_poll_bind_get_rested_axes(
         &menu_input_binds, menu_bind_port);
   menu_input_key_bind_poll_bind_state(
         &menu_input_binds, menu_bind_port, false);

   rarch_timer_begin_new_time(&menu_input_binds.timer, settings->uints.input_bind_timeout);

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
   unsigned max_users   = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));

   if (!state || !new_state)
      return false;

   for (i = 0; i < max_users; i++)
   {
      if (!menu_input_key_bind_poll_find_trigger_pad(
               state, new_state, i))
         continue;

      return true;
   }

   return false;
}

bool menu_input_key_bind_set_min_max(menu_input_ctx_bind_limits_t *lim)
{
   if (!lim)
      return false;

   menu_input_binds.begin = lim->min;
   menu_input_binds.last  = lim->max;

   return true;
}

bool menu_input_key_bind_iterate(menu_input_ctx_bind_t *bind)
{
   struct menu_bind_state binds;
   bool               timed_out = false;
   settings_t *settings         = config_get_ptr();

   rarch_timer_tick(&menu_input_binds.timer);

   if (!bind)
      return false;

   if (rarch_timer_has_expired(&menu_input_binds.timer))
   {
      input_driver_keyboard_mapping_set_block(false);

      menu_input_binds.begin++;
      menu_input_binds.target++;
      rarch_timer_begin_new_time(&menu_input_binds.timer, settings->uints.input_bind_timeout);
      timed_out = true;
   }

   snprintf(bind->s, bind->len,
         "[%s]\npress keyboard or joypad\n(timeout %d %s)",
         input_config_bind_map_get_desc(
            menu_input_binds.begin - MENU_SETTINGS_BIND_BEGIN),
         rarch_timer_get_timeout(&menu_input_binds.timer),
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_SECONDS));

   /* binds.begin is updated in keyboard_press callback. */
   if (menu_input_binds.begin > menu_input_binds.last)
   {
      /* Avoid new binds triggering things right away. */
      input_driver_set_flushing_input();

      /* We won't be getting any key events, so just cancel early. */
      if (timed_out)
         input_keyboard_ctl(RARCH_INPUT_KEYBOARD_CTL_CANCEL_WAIT_KEYS, NULL);

      return true;
   }

   binds = menu_input_binds;

   input_driver_keyboard_mapping_set_block(true);
   menu_input_key_bind_poll_bind_state(&binds, menu_bind_port, timed_out);

   if ((binds.skip && !menu_input_binds.skip) ||
         menu_input_key_bind_poll_find_trigger(&menu_input_binds, &binds))
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
      rarch_timer_begin_new_time(&binds.timer, settings->uints.input_bind_timeout);
   }
   menu_input_binds = binds;

   return false;
}

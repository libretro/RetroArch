/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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
#include <limits.h>
#include <ctype.h>
#include "menu_common.h"
#include "menu_action.h"
#include "../../input/keyboard_line.h"
#include "menu_input_line_cb.h"
#include "../../settings_data.h"

void menu_key_start_line(void *data, const char *label,
      const char *label_setting, input_keyboard_line_complete_t cb)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return;

   menu->keyboard.display       = true;
   menu->keyboard.label         = label;
   menu->keyboard.label_setting = label_setting;
   menu->keyboard.buffer        = input_keyboard_start_line(menu, cb);
}

static void menu_key_end_line(void *data)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return;

   menu->keyboard.display       = false;
   menu->keyboard.label         = NULL;
   menu->keyboard.label_setting = NULL;

   /* Avoid triggering states on pressing return. */
   driver.flushing_input = true;
}

static void menu_search_callback(void *userdata, const char *str)
{
   menu_handle_t *menu = (menu_handle_t*)userdata;

   if (str && *str)
      file_list_search(menu->selection_buf, str, &menu->selection_ptr);
   menu_key_end_line(menu);
}

void st_uint_callback(void *userdata, const char *str)
{
   menu_handle_t *menu = (menu_handle_t*)userdata;
   rarch_setting_t *current_setting = NULL;
   rarch_setting_t *setting_data = (rarch_setting_t *)
      setting_data_get_list(SL_FLAG_ALL_SETTINGS, false);

   if (str && *str && setting_data)
   {
      if ((current_setting = (rarch_setting_t*)
               setting_data_find_setting(
                  setting_data, menu->keyboard.label_setting)))
         *current_setting->value.unsigned_integer = strtoul(str, NULL, 0);
   }
   menu_key_end_line(menu);
}

void st_string_callback(void *userdata, const char *str)
{
   menu_handle_t *menu = (menu_handle_t*)userdata;
   rarch_setting_t *current_setting = NULL;
   rarch_setting_t *setting_data = (rarch_setting_t *)
      setting_data_get_list(SL_FLAG_ALL_SETTINGS, false);
 
   if (str && *str && setting_data)
   {
      if ((current_setting = (rarch_setting_t*)
               setting_data_find_setting(
                  setting_data, menu->keyboard.label_setting)))
         menu_action_setting_set_current_string(current_setting, str);
      else
         menu_action_set_current_string_based_on_label(
               menu->keyboard.label_setting, str);
   }
   menu_key_end_line(menu);
}

void menu_key_event(bool down, unsigned keycode, uint32_t character, uint16_t mod)
{
   if (!driver.menu)
   {
      RARCH_ERR("Cannot invoke menu key event callback, menu handle is not initialized.\n");
      return;
   }

   (void)down;
   (void)keycode;
   (void)mod;

   if (character == '/')
   {
      driver.menu->keyboard.display = true;
      driver.menu->keyboard.label = "Search: ";
      driver.menu->keyboard.buffer = 
         input_keyboard_start_line(driver.menu, menu_search_callback);
   }
}

void menu_poll_bind_state(struct menu_bind_state *state)
{
   unsigned i, b, a, h;
   const rarch_joypad_driver_t *joypad = NULL;

   if (!state)
      return;

   memset(state->state, 0, sizeof(state->state));
   state->skip = driver.input->input_state(driver.input_data, NULL, 0,
         RETRO_DEVICE_KEYBOARD, 0, RETROK_RETURN);

   if (driver.input && driver.input_data && driver.input->get_joypad_driver)
      joypad = driver.input->get_joypad_driver(driver.input_data);

   if (!joypad)
   {
      RARCH_ERR("Cannot poll raw joypad state.");
      return;
   }

   if (joypad->poll)
      joypad->poll();

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      for (b = 0; b < MENU_MAX_BUTTONS; b++)
         state->state[i].buttons[b] = input_joypad_button_raw(joypad, i, b);
      for (a = 0; a < MENU_MAX_AXES; a++)
         state->state[i].axes[a] = input_joypad_axis_raw(joypad, i, a);
      for (h = 0; h < MENU_MAX_HATS; h++)
      {
         state->state[i].hats[h] |= 
            input_joypad_hat_raw(joypad, i, HAT_UP_MASK, h) ? HAT_UP_MASK : 0;
         state->state[i].hats[h] |=
            input_joypad_hat_raw(joypad, i, HAT_DOWN_MASK, h) ? HAT_DOWN_MASK : 0;
         state->state[i].hats[h] |=
            input_joypad_hat_raw(joypad, i, HAT_LEFT_MASK, h) ? HAT_LEFT_MASK : 0;
         state->state[i].hats[h] |=
            input_joypad_hat_raw(joypad, i, HAT_RIGHT_MASK, h) ? HAT_RIGHT_MASK : 0;
      }
   }
}

void menu_poll_bind_get_rested_axes(struct menu_bind_state *state)
{
   unsigned i, a;
   const rarch_joypad_driver_t *joypad = NULL;

   if (!state)
      return;

   if (driver.input && driver.input_data && driver.input->get_joypad_driver)
      joypad = driver.input->get_joypad_driver(driver.input_data);

   if (!joypad)
   {
      RARCH_ERR("Cannot poll raw joypad state.");
      return;
   }

   for (i = 0; i < MAX_PLAYERS; i++)
      for (a = 0; a < MENU_MAX_AXES; a++)
         state->axis_state[i].rested_axes[a] =
            input_joypad_axis_raw(joypad, i, a);
}

static bool menu_poll_find_trigger_pad(struct menu_bind_state *state,
      struct menu_bind_state *new_state, unsigned p)
{
   unsigned a, b, h;
   const struct menu_bind_state_port *n = (const struct menu_bind_state_port*)
      &new_state->state[p];
   const struct menu_bind_state_port *o = (const struct menu_bind_state_port*)
      &state->state[p];

   for (b = 0; b < MENU_MAX_BUTTONS; b++)
   {
      if (n->buttons[b] && !o->buttons[b])
      {
         state->target->joykey = b;
         state->target->joyaxis = AXIS_NONE;
         return true;
      }
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
         state->target->joyaxis = n->axes[a] > 0 ? AXIS_POS(a) : AXIS_NEG(a);
         state->target->joykey = NO_BTN;

         /* Lock the current axis */
         new_state->axis_state[p].locked_axes[a] = n->axes[a] > 0 ? 0x7fff : -0x7fff;
         return true;
      }

      if (locked_distance >= 20000) /* Unlock the axis. */
         new_state->axis_state[p].locked_axes[a] = 0;
   }

   for (h = 0; h < MENU_MAX_HATS; h++)
   {
      uint16_t trigged = n->hats[h] & (~o->hats[h]);
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

bool menu_poll_find_trigger(struct menu_bind_state *state,
      struct menu_bind_state *new_state)
{
   unsigned i;

   if (!state || !new_state)
      return false;

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (menu_poll_find_trigger_pad(state, new_state, i))
      {
         /* Update the joypad mapping automatically.
          * More friendly that way. */
         g_settings.input.joypad_map[state->player] = i;
         return true;
      }
   }
   return false;
}

bool menu_custom_bind_keyboard_cb(void *data, unsigned code)
{
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return false;

   menu->binds.target->key = (enum retro_key)code;
   menu->binds.begin++;
   menu->binds.target++;
   menu->binds.timeout_end = rarch_get_time_usec() +
      MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;

   return menu->binds.begin <= menu->binds.last;
}

int menu_input_bind_iterate(void *data)
{
   char msg[PATH_MAX];
   menu_handle_t *menu = (menu_handle_t*)data;
   struct menu_bind_state binds = menu->binds;
    
   if (driver.video_data && driver.menu_ctx &&
         driver.menu_ctx->render)
      driver.menu_ctx->render();

   snprintf(msg, sizeof(msg), "[%s]\npress joypad\n(RETURN to skip)",
         input_config_bind_map[
         driver.menu->binds.begin - MENU_SETTINGS_BIND_BEGIN].desc);

   if (driver.video_data && driver.menu_ctx 
         && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   driver.block_input = true;
   menu_poll_bind_state(&binds);

   if ((binds.skip && !menu->binds.skip) ||
         menu_poll_find_trigger(&menu->binds, &binds))
   {
      driver.block_input = false;

      /* Avoid new binds triggering things right away. */
      driver.flushing_input = true;

      binds.begin++;
      if (binds.begin <= binds.last)
         binds.target++;
      else
         return 1;
   }
   menu->binds = binds;

   return 0;
}

int menu_input_bind_iterate_keyboard(void *data)
{
   char msg[PATH_MAX];
   int64_t current;
   int timeout = 0;
   bool timed_out = false;
   menu_handle_t *menu = (menu_handle_t*)data;

   if (driver.video_data && driver.menu_ctx &&
         driver.menu_ctx->render)
      driver.menu_ctx->render();

   current = rarch_get_time_usec();
   timeout = (driver.menu->binds.timeout_end - current) / 1000000;
   snprintf(msg, sizeof(msg), "[%s]\npress keyboard\n(timeout %d seconds)",
         input_config_bind_map[
         driver.menu->binds.begin - MENU_SETTINGS_BIND_BEGIN].desc,
         timeout);

   if (driver.video_data && driver.menu_ctx 
         && driver.menu_ctx->render_messagebox)
      driver.menu_ctx->render_messagebox(msg);

   if (timeout <= 0)
   {
      menu->binds.begin++;

      /* Could be unsafe, but whatever. */
      menu->binds.target->key = RETROK_UNKNOWN;

      menu->binds.target++;
      menu->binds.timeout_end = rarch_get_time_usec() +
         MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
      timed_out = true;
   }

   /* binds.begin is updated in keyboard_press callback. */
   if (menu->binds.begin > menu->binds.last)
   {
      /* Avoid new binds triggering things right away. */
      driver.flushing_input = true;

      /* We won't be getting any key events, so just cancel early. */
      if (timed_out)
         input_keyboard_wait_keys_cancel();

      return 1;
   }

   return 0;
}

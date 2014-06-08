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
#include "../../input/keyboard_line.h"
#include "menu_input_line_cb.h"

void menu_key_start_line(void *data, const char *label, input_keyboard_line_complete_t cb)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (!rgui)
      return;

   rgui->keyboard.display = true;
   rgui->keyboard.label = label;
   rgui->keyboard.buffer = input_keyboard_start_line(rgui, cb);
}

static void menu_key_end_line(void *data)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (!rgui)
      return;

   rgui->keyboard.display = false;
   rgui->keyboard.label = NULL;
   rgui->old_input_state = -1ULL; // Avoid triggering states on pressing return.
}

static void menu_search_callback(void *userdata, const char *str)
{
   rgui_handle_t *rgui = (rgui_handle_t*)userdata;

   if (str && *str)
      file_list_search(rgui->selection_buf, str, &rgui->selection_ptr);
   menu_key_end_line(rgui);
}

#ifdef HAVE_NETPLAY
void netplay_port_callback(void *userdata, const char *str)
{
   rgui_handle_t *rgui = (rgui_handle_t*)userdata;

   if (str && *str)
      g_extern.netplay_port = strtoul(str, NULL, 0);
   menu_key_end_line(rgui);
}

void netplay_ipaddress_callback(void *userdata, const char *str)
{
   rgui_handle_t *rgui = (rgui_handle_t*)userdata;

   if (str && *str)
      strlcpy(g_extern.netplay_server, str, sizeof(g_extern.netplay_server));
   menu_key_end_line(rgui);
}

void netplay_nickname_callback(void *userdata, const char *str)
{
   rgui_handle_t *rgui = (rgui_handle_t*)userdata;

   if (str && *str)
      strlcpy(g_extern.netplay_nick, str, sizeof(g_extern.netplay_nick));
   menu_key_end_line(rgui);
}
#endif

void audio_device_callback(void *userdata, const char *str)
{
   rgui_handle_t *rgui = (rgui_handle_t*)userdata;

   if (!rgui)
   {
      RARCH_ERR("Cannot invoke audio device setting callback, menu handle is not initialized.\n");
      return;
   }

   if (str && *str)
      strlcpy(g_settings.audio.device, str, sizeof(g_settings.audio.device));
   menu_key_end_line(rgui);
}

#ifdef HAVE_SHADER_MANAGER
void preset_filename_callback(void *userdata, const char *str)
{
   rgui_handle_t *rgui = (rgui_handle_t*)userdata;

   if (!rgui)
   {
      RARCH_ERR("Cannot invoke preset setting callback, menu handle is not initialized.\n");
      return;
   }

   if (driver.menu_ctx && driver.menu_ctx->backend && driver.menu_ctx->backend->shader_manager_save_preset)
      driver.menu_ctx->backend->shader_manager_save_preset(str && *str ? str : NULL, false);
   menu_key_end_line(rgui);
}
#endif

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
      driver.menu->keyboard.buffer = input_keyboard_start_line(driver.menu, menu_search_callback);
   }
}

void menu_poll_bind_state(void *data)
{
   struct rgui_bind_state *state = (struct rgui_bind_state*)data;

   if (!state)
      return;

   unsigned i, b, a, h;
   memset(state->state, 0, sizeof(state->state));
   state->skip = input_input_state_func(NULL, 0, RETRO_DEVICE_KEYBOARD, 0, RETROK_RETURN);

   const rarch_joypad_driver_t *joypad = NULL;
   if (driver.input && driver.input_data && driver.input->get_joypad_driver)
      joypad = driver.input->get_joypad_driver(driver.input_data);

   if (!joypad)
   {
      RARCH_ERR("Cannot poll raw joypad state.");
      return;
   }

   input_joypad_poll(joypad);
   for (i = 0; i < MAX_PLAYERS; i++)
   {
      for (b = 0; b < RGUI_MAX_BUTTONS; b++)
         state->state[i].buttons[b] = input_joypad_button_raw(joypad, i, b);
      for (a = 0; a < RGUI_MAX_AXES; a++)
         state->state[i].axes[a] = input_joypad_axis_raw(joypad, i, a);
      for (h = 0; h < RGUI_MAX_HATS; h++)
      {
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_UP_MASK, h) ? HAT_UP_MASK : 0;
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_DOWN_MASK, h) ? HAT_DOWN_MASK : 0;
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_LEFT_MASK, h) ? HAT_LEFT_MASK : 0;
         state->state[i].hats[h] |= input_joypad_hat_raw(joypad, i, HAT_RIGHT_MASK, h) ? HAT_RIGHT_MASK : 0;
      }
   }
}

void menu_poll_bind_get_rested_axes(void *data)
{
   unsigned i, a;
   const rarch_joypad_driver_t *joypad = NULL;
   struct rgui_bind_state *state = (struct rgui_bind_state*)data;

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
      for (a = 0; a < RGUI_MAX_AXES; a++)
         state->axis_state[i].rested_axes[a] = input_joypad_axis_raw(joypad, i, a);
}

static bool menu_poll_find_trigger_pad(struct rgui_bind_state *state, struct rgui_bind_state *new_state, unsigned p)
{
   unsigned a, b, h;
   const struct rgui_bind_state_port *n = (const struct rgui_bind_state_port*)&new_state->state[p];
   const struct rgui_bind_state_port *o = (const struct rgui_bind_state_port*)&state->state[p];

   for (b = 0; b < RGUI_MAX_BUTTONS; b++)
   {
      if (n->buttons[b] && !o->buttons[b])
      {
         state->target->joykey = b;
         state->target->joyaxis = AXIS_NONE;
         return true;
      }
   }

   // Axes are a bit tricky ...
   for (a = 0; a < RGUI_MAX_AXES; a++)
   {
      int locked_distance = abs(n->axes[a] - new_state->axis_state[p].locked_axes[a]);
      int rested_distance = abs(n->axes[a] - new_state->axis_state[p].rested_axes[a]);

      if (abs(n->axes[a]) >= 20000 &&
            locked_distance >= 20000 &&
            rested_distance >= 20000) // Take care of case where axis rests on +/- 0x7fff (e.g. 360 controller on Linux)
      {
         state->target->joyaxis = n->axes[a] > 0 ? AXIS_POS(a) : AXIS_NEG(a);
         state->target->joykey = NO_BTN;

         // Lock the current axis.
         new_state->axis_state[p].locked_axes[a] = n->axes[a] > 0 ? 0x7fff : -0x7fff;
         return true;
      }

      if (locked_distance >= 20000) // Unlock the axis.
         new_state->axis_state[p].locked_axes[a] = 0;
   }

   for (h = 0; h < RGUI_MAX_HATS; h++)
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

bool menu_poll_find_trigger(void *data1, void *data2)
{
   unsigned i;
   struct rgui_bind_state *state, *new_state;
   state     = (struct rgui_bind_state*)data1;
   new_state = (struct rgui_bind_state*)data2;

   if (!state || !new_state)
      return false;

   for (i = 0; i < MAX_PLAYERS; i++)
   {
      if (menu_poll_find_trigger_pad(state, new_state, i))
      {
         g_settings.input.joypad_map[state->player] = i; // Update the joypad mapping automatically. More friendly that way.
         return true;
      }
   }
   return false;
}

bool menu_custom_bind_keyboard_cb(void *data, unsigned code)
{
   rgui_handle_t *rgui = (rgui_handle_t*)data;

   if (!rgui)
      return false;

   rgui->binds.target->key = (enum retro_key)code;
   rgui->binds.begin++;
   rgui->binds.target++;
   rgui->binds.timeout_end = rarch_get_time_usec() + RGUI_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
   return rgui->binds.begin <= rgui->binds.last;
}

uint64_t menu_input(void)
{
   unsigned i;
   uint64_t input_state;
   static const struct retro_keybind *binds[] = { g_settings.input.binds[0] };

   if (!driver.menu)
      return 0;

   input_state = 0;


   input_push_analog_dpad((struct retro_keybind*)binds[0], (g_settings.input.analog_dpad_mode[0] == ANALOG_DPAD_NONE) ? ANALOG_DPAD_LSTICK : g_settings.input.analog_dpad_mode[0]);
   for (i = 0; i < MAX_PLAYERS; i++)
      input_push_analog_dpad(g_settings.input.autoconf_binds[i], g_settings.input.analog_dpad_mode[i]);

   for (i = 0; i < RETRO_DEVICE_ID_JOYPAD_R2; i++)
   {
      input_state |= input_input_state_func(binds,
            0, RETRO_DEVICE_JOYPAD, 0, i) ? (1ULL << i) : 0;
#ifdef HAVE_OVERLAY
      input_state |= (driver.overlay_state.buttons & (UINT64_C(1) << i)) ? (1ULL << i) : 0;
#endif
   }

   input_state |= input_key_pressed_func(RARCH_MENU_TOGGLE) ? (1ULL << RARCH_MENU_TOGGLE) : 0;

   input_pop_analog_dpad((struct retro_keybind*)binds[0]);
   for (i = 0; i < MAX_PLAYERS; i++)
      input_pop_analog_dpad(g_settings.input.autoconf_binds[i]);

   driver.menu->trigger_state = input_state & ~driver.menu->old_input_state;

   driver.menu->do_held = (input_state & (
            (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_L)
            | (1ULL << RETRO_DEVICE_ID_JOYPAD_R)
            )) && !(input_state & (1ULL << RARCH_MENU_TOGGLE));

   return input_state;
}

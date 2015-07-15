/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#include "menu_input.h"
#include "menu.h"
#include "menu_display.h"
#include "menu_entry.h"
#include "menu_setting.h"
#include "menu_shader.h"
#include "menu_navigation.h"
#include "menu_hash.h"

#include "../general.h"
#include "../cheats.h"
#include "../performance.h"
#include "../input/input_joypad.h"
#include "../input/input_remapping.h"
#include "../input/input_common.h"

menu_input_t *menu_input_get_ptr(void)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return NULL;
   return &menu->input;
}

void menu_input_key_start_line(const char *label,
      const char *label_setting, unsigned type, unsigned idx,
      input_keyboard_line_complete_t cb)
{
   menu_handle_t    *menu   = menu_driver_get_ptr();
   menu_input_t *menu_input = menu_input_get_ptr();
   if (!menu || !menu_input)
      return;

   menu_input->keyboard.display       = true;
   menu_input->keyboard.label         = label;
   menu_input->keyboard.label_setting = label_setting;
   menu_input->keyboard.type          = type;
   menu_input->keyboard.idx           = idx;
   menu_input->keyboard.buffer        = input_keyboard_start_line(menu, cb);
}

static void menu_input_key_end_line(void)
{
   driver_t *driver         = driver_get_ptr();
   menu_input_t *menu_input = menu_input_get_ptr();
   if (!menu_input)
      return;

   menu_input->keyboard.display       = false;
   menu_input->keyboard.label         = NULL;
   menu_input->keyboard.label_setting = NULL;

   /* Avoid triggering states on pressing return. */
   driver->flushing_input = true;
}

static void menu_input_search_callback(void *userdata, const char *str)
{
   size_t idx = 0;
   menu_list_t *menu_list = menu_list_get_ptr();
   menu_navigation_t *nav = menu_navigation_get_ptr();

   if (!menu_list || !nav)
      return;

   if (str && *str && file_list_search(menu_list->selection_buf, str, &idx))
         menu_navigation_set(nav, idx, true);

   menu_input_key_end_line();
}

void menu_input_st_uint_callback(void *userdata, const char *str)
{
   menu_input_t *menu_input = menu_input_get_ptr();

   if (!menu_input)
      return;

   if (str && *str)
   {
      rarch_setting_t *current_setting = NULL;
      if ((current_setting = menu_setting_find(menu_input->keyboard.label_setting)))
         setting_set_with_string_representation(current_setting, str);
   }

   menu_input_key_end_line();
}

void menu_input_st_hex_callback(void *userdata, const char *str)
{
   menu_input_t *menu_input = menu_input_get_ptr();

   if (!menu_input)
      return;

   if (str && *str)
   {
      rarch_setting_t *current_setting = NULL;
      if ((current_setting = menu_setting_find(menu_input->keyboard.label_setting)))
         if (str[0] == '#')
            str++;
         *current_setting->value.unsigned_integer = strtoul(str, NULL, 16);
   }

   menu_input_key_end_line();
}


void menu_input_st_string_callback(void *userdata, const char *str)
{
   menu_input_t *menu_input = menu_input_get_ptr();

   if (!menu_input)
      return;

   if (str && *str)
   {
      global_t *global = global_get_ptr();
      rarch_setting_t *current_setting = menu_setting_find(menu_input->keyboard.label_setting);

      if (current_setting)
      {
         setting_set_with_string_representation(current_setting, str);
         menu_setting_generic(current_setting, false);
      }
      else
      {
         uint32_t hash_label = menu_hash_calculate(menu_input->keyboard.label_setting);

         switch (hash_label)
         {
            case MENU_LABEL_VIDEO_SHADER_PRESET_SAVE_AS:
               menu_shader_manager_save_preset(str, false);
               break;
            case MENU_LABEL_REMAP_FILE_SAVE_AS:
               input_remapping_save_file(str);
               break;
            case MENU_LABEL_CHEAT_FILE_SAVE_AS:
               cheat_manager_save(global->cheat, str);
               break;
         }
      }
   }

   menu_input_key_end_line();
}

void menu_input_st_cheat_callback(void *userdata, const char *str)
{
   global_t *global         = global_get_ptr();
   cheat_manager_t *cheat   = global ? global->cheat : NULL;
   menu_input_t *menu_input = menu_input_get_ptr();

   (void)userdata;

   if (!menu_input || !cheat)
      return;

   if (cheat && str && *str)
   {
      unsigned cheat_index = menu_input->keyboard.type - MENU_SETTINGS_CHEAT_BEGIN;

      cheat->cheats[cheat_index].code  = strdup(str);
      cheat->cheats[cheat_index].state = true;
   }

   menu_input_key_end_line();
}

void menu_input_search_start(void)
{
   menu_handle_t      *menu = menu_driver_get_ptr();
   menu_input_t *menu_input = menu_input_get_ptr();
   if (!menu || !menu_input)
      return;

   menu_input->keyboard.display = true;
   menu_input->keyboard.label   = menu_hash_to_str(MENU_VALUE_SEARCH);
   menu_input->keyboard.buffer  =
      input_keyboard_start_line(menu, menu_input_search_callback);
}

void menu_input_key_event(bool down, unsigned keycode,
      uint32_t character, uint16_t mod)
{
   (void)down;
   (void)keycode;
   (void)mod;

   if (character == '/')
      menu_entry_action(NULL, 0, MENU_ACTION_SEARCH);
}

static void menu_input_poll_bind_state(struct menu_bind_state *state)
{
   unsigned i, b, a, h;
   const input_device_driver_t *joypad = input_driver_get_joypad_driver();
   settings_t *settings                = config_get_ptr();

   if (!state)
      return;

   memset(state->state, 0, sizeof(state->state));
   state->skip = input_driver_state(NULL, 0,
         RETRO_DEVICE_KEYBOARD, 0, RETROK_RETURN);

   if (!joypad)
   {
      RARCH_ERR("Cannot poll raw joypad state.");
      return;
   }

   if (joypad->poll)
      joypad->poll();

   for (i = 0; i < settings->input.max_users; i++)
   {
      for (b = 0; b < MENU_MAX_BUTTONS; b++)
         state->state[i].buttons[b] = input_joypad_button_raw(joypad, i, b);

      for (a = 0; a < MENU_MAX_AXES; a++)
         state->state[i].axes[a] = input_joypad_axis_raw(joypad, i, a);

      for (h = 0; h < MENU_MAX_HATS; h++)
      {
         if (input_joypad_hat_raw(joypad, i, HAT_UP_MASK, h))
            state->state[i].hats[h] |= HAT_UP_MASK;
         if (input_joypad_hat_raw(joypad, i, HAT_DOWN_MASK, h))
            state->state[i].hats[h] |= HAT_DOWN_MASK;
         if (input_joypad_hat_raw(joypad, i, HAT_LEFT_MASK, h))
            state->state[i].hats[h] |= HAT_LEFT_MASK;
         if (input_joypad_hat_raw(joypad, i, HAT_RIGHT_MASK, h))
            state->state[i].hats[h] |= HAT_RIGHT_MASK;
      }
   }
}

static void menu_input_poll_bind_get_rested_axes(struct menu_bind_state *state)
{
   unsigned i, a;
   const input_device_driver_t *joypad = input_driver_get_joypad_driver();
   settings_t *settings                = config_get_ptr();

   if (!state)
      return;

   if (!joypad)
   {
      RARCH_ERR("Cannot poll raw joypad state.");
      return;
   }

   for (i = 0; i < settings->input.max_users; i++)
      for (a = 0; a < MENU_MAX_AXES; a++)
         state->axis_state[i].rested_axes[a] =
            input_joypad_axis_raw(joypad, i, a);
}

static bool menu_input_poll_find_trigger_pad(struct menu_bind_state *state,
      struct menu_bind_state *new_state, unsigned p)
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

static bool menu_input_poll_find_trigger(struct menu_bind_state *state,
      struct menu_bind_state *new_state)
{
   unsigned i;
   settings_t *settings = config_get_ptr();

   if (!state || !new_state)
      return false;

   for (i = 0; i < settings->input.max_users; i++)
   {
      if (!menu_input_poll_find_trigger_pad(state, new_state, i))
         continue;

      /* Update the joypad mapping automatically.
       * More friendly that way. */
      settings->input.joypad_map[state->user] = i;
      return true;
   }
   return false;
}

static bool menu_input_custom_bind_keyboard_cb(void *data, unsigned code)
{
   menu_input_t *menu_input = menu_input_get_ptr();

   if (!menu_input)
      return false;

   menu_input->binds.target->key = (enum retro_key)code;
   menu_input->binds.begin++;
   menu_input->binds.target++;
   menu_input->binds.timeout_end = rarch_get_time_usec() +
      MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;

   return (menu_input->binds.begin <= menu_input->binds.last);
}

static int menu_input_set_bind_mode_common(rarch_setting_t  *setting,
      enum menu_input_bind_mode type)
{
   menu_displaylist_info_t info  = {0};
   struct retro_keybind *keybind = NULL;
   settings_t     *settings      = config_get_ptr();
   menu_list_t        *menu_list = menu_list_get_ptr();
   menu_input_t      *menu_input = menu_input_get_ptr();
   menu_navigation_t       *nav  = menu_navigation_get_ptr();

   if (!setting)
      return -1;

   switch (type)
   {
      case MENU_INPUT_BIND_NONE:
         return -1;
      case MENU_INPUT_BIND_SINGLE:
         keybind = (struct retro_keybind*)setting->value.keybind;

         if (!keybind)
            return -1;

         menu_input->binds.begin  = setting->bind_type;
         menu_input->binds.last   = setting->bind_type;
         menu_input->binds.target = keybind;
         menu_input->binds.user   = setting->index_offset;

         info.list          = menu_list->menu_stack;
         info.type          = MENU_SETTINGS_CUSTOM_BIND_KEYBOARD;
         info.directory_ptr = nav->selection_ptr;
         strlcpy(info.label,
               menu_hash_to_str(MENU_LABEL_CUSTOM_BIND), sizeof(info.label));

         menu_displaylist_push_list(&info, DISPLAYLIST_INFO);
         break;
      case MENU_INPUT_BIND_ALL:
         menu_input->binds.target = &settings->input.binds
            [setting->index_offset][0];
         menu_input->binds.begin  = MENU_SETTINGS_BIND_BEGIN;
         menu_input->binds.last   = MENU_SETTINGS_BIND_LAST;

         info.list          = menu_list->menu_stack;
         info.type          = MENU_SETTINGS_CUSTOM_BIND_KEYBOARD;
         info.directory_ptr = nav->selection_ptr;
         strlcpy(info.label,
               menu_hash_to_str(MENU_LABEL_CUSTOM_BIND_ALL),
               sizeof(info.label));

         menu_displaylist_push_list(&info, DISPLAYLIST_INFO);
         break;
   }
   return 0;
}

static int menu_input_set_timeout(void)
{
   menu_handle_t       *menu = menu_driver_get_ptr();
   menu_input_t  *menu_input = menu_input_get_ptr();

   menu_input->binds.timeout_end   = rarch_get_time_usec() +
      MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
   input_keyboard_wait_keys(menu,
         menu_input_custom_bind_keyboard_cb);

   return 0;
}

int menu_input_set_keyboard_bind_mode(void *data,
      enum menu_input_bind_mode type)
{
   rarch_setting_t  *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;
   if (menu_input_set_bind_mode_common(setting, type) == -1)
      return -1;

   return menu_input_set_timeout();
}

int menu_input_set_input_device_bind_mode(void *data,
      enum menu_input_bind_mode type)
{
   menu_input_t *menu_input  = menu_input_get_ptr();
   rarch_setting_t  *setting = (rarch_setting_t*)data;

   if (!setting)
      return -1;
   if (menu_input_set_bind_mode_common(setting, type) == -1)
      return -1;

   menu_input_poll_bind_get_rested_axes(&menu_input->binds);
   menu_input_poll_bind_state(&menu_input->binds);

   return 0;
}

static int menu_input_bind_iterate_keyboard(int64_t current, int timeout)
{
   bool           timed_out = false;
   menu_input_t *menu_input = menu_input_get_ptr();
   driver_t         *driver = driver_get_ptr();

   if (!menu_input)
      return -1;

   if (timeout <= 0)
   {
      /* Could be unsafe, but whatever. */
      menu_input->binds.target->key = RETROK_UNKNOWN;

      menu_input->binds.begin++;
      menu_input->binds.target++;
      menu_input->binds.timeout_end = rarch_get_time_usec() +
         MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
      timed_out = true;
   }

   /* binds.begin is updated in keyboard_press callback. */
   if (menu_input->binds.begin > menu_input->binds.last)
   {
      /* Avoid new binds triggering things right away. */
      driver->flushing_input = true;

      /* We won't be getting any key events, so just cancel early. */
      if (timed_out)
         input_keyboard_wait_keys_cancel();

      return 1;
   }

   return 0;
}

int menu_input_bind_iterate(void)
{
   int64_t current;
   struct menu_bind_state binds;
   char msg[PATH_MAX_LENGTH]    = {0};
   int timeout                  = 0;
   menu_input_t *menu_input     = menu_input_get_ptr();
   driver_t *driver             = driver_get_ptr();
   global_t *global             = global_get_ptr();
   bool bind_mode_kb            = global ? global->menu.bind_mode_keyboard : false;

   menu_driver_render();

   current = rarch_get_time_usec();
   timeout = (menu_input->binds.timeout_end - current) / 1000000;

   if (bind_mode_kb)
   snprintf(msg, sizeof(msg),
         "[%s]\npress keyboard\n(timeout %d %s)",
         input_config_bind_map[
         menu_input->binds.begin - MENU_SETTINGS_BIND_BEGIN].desc,
         timeout,
         menu_hash_to_str(MENU_VALUE_SECONDS));
    else
        snprintf(msg, sizeof(msg),
        "[%s]\npress joypad\n(RETURN to skip)",
        input_config_bind_map[
        menu_input->binds.begin - MENU_SETTINGS_BIND_BEGIN].desc);

   menu_driver_render_messagebox(msg);

   if (bind_mode_kb)
      return menu_input_bind_iterate_keyboard(current, timeout);

   binds = menu_input->binds;

   input_driver_keyboard_mapping_set_block(true);
   menu_input_poll_bind_state(&binds);

   if ((binds.skip && !menu_input->binds.skip) ||
         menu_input_poll_find_trigger(&menu_input->binds, &binds))
   {
      input_driver_keyboard_mapping_set_block(false);

      /* Avoid new binds triggering things right away. */
      driver->flushing_input = true;

      binds.begin++;

      if (binds.begin > binds.last)
         return 1;

      binds.target++;
   }
   menu_input->binds = binds;

   return 0;
}


static int menu_input_mouse(unsigned *action)
{
   video_viewport_t vp;
   const struct retro_keybind *binds[MAX_USERS];
   menu_animation_t *anim    = menu_animation_get_ptr();
   menu_input_t *menu_input  = menu_input_get_ptr();
   menu_framebuf_t *frame_buf= menu_display_fb_get_ptr();
   settings_t *settings      = config_get_ptr();

   if (!settings->menu.mouse.enable
#ifdef HAVE_OVERLAY
       || (settings->input.overlay_enable && input_overlay_is_alive())
#endif
      )
   {
      memset(&menu_input->mouse, 0, sizeof(menu_input->mouse));
      return 0;
   }

   if (!video_driver_viewport_info(&vp))
      return -1;

   if (menu_input->mouse.hwheeldown)
   {
      *action = MENU_ACTION_LEFT;
      menu_input->mouse.hwheeldown = false;
      return 0;
   }

   if (menu_input->mouse.hwheelup)
   {
      *action = MENU_ACTION_RIGHT;
      menu_input->mouse.hwheelup = false;
      return 0;
   }

   menu_input->mouse.left       = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_LEFT);
   menu_input->mouse.right      = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_RIGHT);
   menu_input->mouse.wheelup    = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_WHEELUP);
   menu_input->mouse.wheeldown  = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN);
   menu_input->mouse.hwheelup   = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP);
   menu_input->mouse.hwheeldown = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN);
   menu_input->mouse.dx         = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_X);
   menu_input->mouse.dy         = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_Y);

   menu_input->mouse.screen_x += menu_input->mouse.dx;
   menu_input->mouse.screen_y += menu_input->mouse.dy;

   menu_input->mouse.x         = ((int)menu_input->mouse.screen_x * (int)frame_buf->width) / (int)vp.width;
   menu_input->mouse.y         = ((int)menu_input->mouse.screen_y * (int)frame_buf->height) / (int)vp.height;

   if (menu_input->mouse.x < 5)
      menu_input->mouse.x       = 5;
   if (menu_input->mouse.y < 5)
      menu_input->mouse.y       = 5;
   if (menu_input->mouse.x > (int)frame_buf->width - 5)
      menu_input->mouse.x       = frame_buf->width - 5;
   if (menu_input->mouse.y > (int)frame_buf->height - 5)
      menu_input->mouse.y       = frame_buf->height - 5;

   menu_input->mouse.scrollup   = (menu_input->mouse.y == 5);
   menu_input->mouse.scrolldown = (menu_input->mouse.y == (int)frame_buf->height - 5);

   if (
         (menu_input->mouse.dx != 0)     ||
         (menu_input->mouse.dy !=0)      ||
         menu_input->mouse.left          ||
         menu_input->mouse.wheelup       ||
         menu_input->mouse.wheeldown     ||
         menu_input->mouse.hwheelup      ||
         menu_input->mouse.hwheeldown    ||
         menu_input->mouse.scrollup      ||
         menu_input->mouse.scrolldown
      )
      anim->is_active = true;

   return 0;
}

static int menu_input_pointer(unsigned *action)
{
   int pointer_device, pointer_x, pointer_y;
   const struct retro_keybind *binds[MAX_USERS] = {NULL};
   menu_input_t *menu_input  = menu_input_get_ptr();
   menu_animation_t *anim    = menu_animation_get_ptr();
   menu_framebuf_t *frame_buf= menu_display_fb_get_ptr();
   settings_t *settings      = config_get_ptr();
   driver_t *driver          = driver_get_ptr();

   if (!settings->menu.pointer.enable)
   {
      memset(&menu_input->pointer, 0, sizeof(menu_input->pointer));
      return 0;
   }

   pointer_device = driver->menu_ctx->set_texture?
        RETRO_DEVICE_POINTER : RARCH_DEVICE_POINTER_SCREEN;

   menu_input->pointer.pressed[0]  = input_driver_state(binds, 0, pointer_device,
         0, RETRO_DEVICE_ID_POINTER_PRESSED);
   menu_input->pointer.pressed[1]  = input_driver_state(binds, 0, pointer_device,
         1, RETRO_DEVICE_ID_POINTER_PRESSED);
   menu_input->pointer.back  = input_driver_state(binds, 0, pointer_device,
         0, RARCH_DEVICE_ID_POINTER_BACK);

   pointer_x = input_driver_state(binds, 0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_X);
   pointer_y = input_driver_state(binds, 0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_Y);

   menu_input->pointer.x = ((pointer_x + 0x7fff) * (int)frame_buf->width) / 0xFFFF;
   menu_input->pointer.y = ((pointer_y + 0x7fff) * (int)frame_buf->height) / 0xFFFF;

   if (
         menu_input->pointer.pressed[0]    ||
         menu_input->pointer.oldpressed[0] ||
         menu_input->pointer.back          ||
         menu_input->pointer.dragging      ||
         (menu_input->pointer.dy != 0)     ||
         (menu_input->pointer.dx != 0)
      )
      anim->is_active = true;

   return 0;
}

static int menu_input_mouse_frame(
      menu_file_list_cbs_t *cbs, menu_entry_t *entry,
      uint64_t input_mouse)
{
   menu_input_t *menu_input = menu_input_get_ptr();
   menu_list_t *menu_list   = menu_list_get_ptr();
   menu_navigation_t *nav   = menu_navigation_get_ptr();

   if (BIT64_GET(input_mouse, MOUSE_ACTION_BUTTON_L))
   {
      if (BIT64_GET(input_mouse, MOUSE_ACTION_BUTTON_L_TOGGLE))
         return menu_entry_action(entry, nav->selection_ptr, MENU_ACTION_RIGHT);

      if (BIT64_GET(input_mouse, MOUSE_ACTION_BUTTON_L_OK))
         return menu_entry_action(entry, nav->selection_ptr, MENU_ACTION_OK);

      if (BIT64_GET(input_mouse, MOUSE_ACTION_BUTTON_L_SET_NAVIGATION))
         menu_navigation_set(nav, menu_input->mouse.ptr, false);
   }

   if (BIT64_GET(input_mouse, MOUSE_ACTION_BUTTON_R))
      menu_list_pop_stack(menu_list);

   if (BIT64_GET(input_mouse, MOUSE_ACTION_WHEEL_DOWN))
      menu_navigation_increment(nav, 1);

   if (BIT64_GET(input_mouse, MOUSE_ACTION_WHEEL_UP))
      menu_navigation_decrement(nav, 1);

   return 0;
}

static int menu_input_mouse_post_iterate(uint64_t *input_mouse,
      menu_file_list_cbs_t *cbs, unsigned action)
{
   settings_t *settings     = config_get_ptr();
   menu_display_t *disp     = menu_display_get_ptr();
   menu_input_t *menu_input = menu_input_get_ptr();
   menu_list_t *menu_list   = menu_list_get_ptr();
   menu_navigation_t *nav   = menu_navigation_get_ptr();

   *input_mouse = MOUSE_ACTION_NONE;

   if (!settings->menu.mouse.enable
#ifdef HAVE_OVERLAY
       || (settings->input.overlay_enable && input_overlay_is_alive())
#endif
       )
   {
      menu_input->mouse.wheeldown = false;
      menu_input->mouse.wheelup   = false;
      menu_input->mouse.oldleft   = false;
      menu_input->mouse.oldright  = false;
      return 0;
   }

   if (menu_input->mouse.left)
   {
      if (!menu_input->mouse.oldleft)
      {
         rarch_setting_t *setting = NULL;

         BIT64_SET(*input_mouse, MOUSE_ACTION_BUTTON_L);

         setting = menu_setting_find(
               menu_list->selection_buf->list[nav->selection_ptr].label);
         menu_input->mouse.oldleft = true;

         if ((unsigned)menu_input->mouse.y < disp->header_height)
         {
            menu_list_pop_stack(menu_list);
            return 0;
         }
         if (
               (menu_input->mouse.ptr == nav->selection_ptr) &&
               cbs &&
               cbs->action_right &&
               setting &&
               (setting->type == ST_BOOL || setting->type == ST_UINT || setting->type == ST_FLOAT
                || setting->type == ST_STRING)
            )
         {
            BIT64_SET(*input_mouse, MOUSE_ACTION_BUTTON_L_TOGGLE);
         }
         else if (menu_input->mouse.ptr == nav->selection_ptr
            && cbs && cbs->action_ok)
         {
            BIT64_SET(*input_mouse, MOUSE_ACTION_BUTTON_L_OK);
         }
         else if (menu_input->mouse.ptr <= menu_list_get_size(menu_list)-1)
         {
            BIT64_SET(*input_mouse, MOUSE_ACTION_BUTTON_L_SET_NAVIGATION);
         }
      }
   }
   else
      menu_input->mouse.oldleft = false;

   if (menu_input->mouse.right)
   {
      if (!menu_input->mouse.oldright)
      {
         menu_input->mouse.oldright = true;
         BIT64_SET(*input_mouse, MOUSE_ACTION_BUTTON_R);
      }
   }
   else
      menu_input->mouse.oldright = false;

   if (menu_input->mouse.wheeldown)
   {
      BIT64_SET(*input_mouse, MOUSE_ACTION_WHEEL_DOWN);
   }

   if (menu_input->mouse.wheelup)
   {
      BIT64_SET(*input_mouse, MOUSE_ACTION_WHEEL_UP);
   }

   return 0;
}

static int pointer_tap(menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   menu_input_t *menu_input = menu_input_get_ptr();
   menu_list_t   *menu_list = menu_list_get_ptr();
   menu_navigation_t *nav   = menu_navigation_get_ptr();
   rarch_setting_t *setting =
      menu_setting_find(
            menu_list->selection_buf->list[nav->selection_ptr].label);

   if (menu_input->pointer.ptr == nav->selection_ptr
         && cbs && cbs->action_right && setting &&
         (setting->type == ST_BOOL || setting->type == ST_UINT
          || setting->type == ST_FLOAT || setting->type == ST_STRING))
      return menu_entry_action(entry, nav->selection_ptr, MENU_ACTION_RIGHT);
   else if (menu_input->pointer.ptr == nav->selection_ptr)
      return menu_entry_action(entry, nav->selection_ptr, MENU_ACTION_OK);
   else
      menu_navigation_set(nav, menu_input->pointer.ptr, false);

   return 0;
}

static int menu_input_pointer_post_iterate(menu_file_list_cbs_t *cbs,
      menu_entry_t *entry, unsigned action)
{
   int ret                  = 0;
   menu_display_t *disp     = menu_display_get_ptr();
   menu_list_t *menu_list   = menu_list_get_ptr();
   menu_input_t *menu_input = menu_input_get_ptr();
   settings_t *settings     = config_get_ptr();

   if (!menu_input)
      return -1;

   if (!settings->menu.pointer.enable
#ifdef HAVE_OVERLAY
       || (settings->input.overlay_enable && input_overlay_is_alive())
#endif
      )
      return 0;

   if (menu_input->pointer.pressed[0])
   {
      if (!menu_input->pointer.oldpressed[0])
      {
         menu_input->pointer.start_x       = menu_input->pointer.x;
         menu_input->pointer.start_y       = menu_input->pointer.y;
         menu_input->pointer.old_x         = menu_input->pointer.x;
         menu_input->pointer.old_y         = menu_input->pointer.y;
         menu_input->pointer.oldpressed[0] = true;
      }
      else if (abs(menu_input->pointer.x - menu_input->pointer.start_x) > 3
            || abs(menu_input->pointer.y - menu_input->pointer.start_y) > 3)
      {
         float s;
         menu_input->pointer.dragging = true;
         menu_input->pointer.dx       = menu_input->pointer.x - menu_input->pointer.old_x;
         menu_input->pointer.dy       = menu_input->pointer.y - menu_input->pointer.old_y;
         menu_input->pointer.old_x    = menu_input->pointer.x;
         menu_input->pointer.old_y    = menu_input->pointer.y;

         s =  menu_input->pointer.dy / disp->animation->delta_time * 1000000.0;
         menu_input->pointer.accel = (menu_input->pointer.accel0 + menu_input->pointer.accel1 + s) / 3;
         menu_input->pointer.accel0 = menu_input->pointer.accel1;
         menu_input->pointer.accel1 = menu_input->pointer.accel;
      }
   }
   else
   {
      if (menu_input->pointer.oldpressed[0])
      {
         if (!menu_input->pointer.dragging)
         {
            if ((unsigned)menu_input->pointer.start_y < disp->header_height)
               menu_list_pop_stack(menu_list);
            else if (menu_input->pointer.ptr <= menu_list_get_size(menu_list)-1)
            {
               menu_input->pointer.oldpressed[0] = false;
               ret = pointer_tap(cbs, entry, action);
            }
         }

         menu_input->pointer.oldpressed[0] = false;
         menu_input->pointer.start_x       = 0;
         menu_input->pointer.start_y       = 0;
         menu_input->pointer.old_x         = 0;
         menu_input->pointer.old_y         = 0;
         menu_input->pointer.dx            = 0;
         menu_input->pointer.dy            = 0;
         menu_input->pointer.dragging      = false;
      }
   }

   if (menu_input->pointer.back)
   {
      if (!menu_input->pointer.oldback)
      {
         menu_input->pointer.oldback = true;
         menu_list_pop_stack(menu_list);
      }
   }
   menu_input->pointer.oldback = menu_input->pointer.back;

   return ret;
}

void menu_input_post_iterate(int *ret, unsigned action)
{
   menu_entry_t entry        = {{0}};
   menu_input_t *menu_input  = menu_input_get_ptr();
   menu_list_t *menu_list    = menu_list_get_ptr();
   settings_t *settings      = config_get_ptr();
   size_t selected           = menu_navigation_get_current_selection();
   menu_file_list_cbs_t *cbs = menu_list_get_actiondata_at_offset
      (menu_list->selection_buf, selected);

   menu_entry_get(&entry, selected, NULL, false);

   if (settings->menu.mouse.enable)
      *ret  = menu_input_mouse_post_iterate  (&menu_input->mouse.state, cbs, action);

   *ret = menu_input_mouse_frame(cbs, &entry, menu_input->mouse.state);

   if (settings->menu.pointer.enable)
      *ret |= menu_input_pointer_post_iterate(cbs, &entry, action);
}

unsigned menu_input_frame(retro_input_t input, retro_input_t trigger_input)
{
   unsigned ret                            = 0;
   static bool initial_held                = true;
   static bool first_held                  = false;
   static const retro_input_t input_repeat =
        (1UL << RETRO_DEVICE_ID_JOYPAD_UP)
      | (1UL << RETRO_DEVICE_ID_JOYPAD_DOWN)
      | (1UL << RETRO_DEVICE_ID_JOYPAD_LEFT)
      | (1UL << RETRO_DEVICE_ID_JOYPAD_RIGHT)
      | (1UL << RETRO_DEVICE_ID_JOYPAD_L)
      | (1UL << RETRO_DEVICE_ID_JOYPAD_R);
   menu_navigation_t *nav      = menu_navigation_get_ptr();
   menu_handle_t *menu         = menu_driver_get_ptr();
   menu_display_t *disp        = menu_display_get_ptr();
   menu_input_t *menu_input    = menu_input_get_ptr();
   driver_t *driver            = driver_get_ptr();
   settings_t *settings        = config_get_ptr();

   if (!menu || !driver || !nav || !menu_input)
      return 0;

   driver->retro_ctx.poll_cb();

   /* don't run anything first frame, only capture held inputs
    * for old_input_state. */

   if (input & input_repeat)
   {
      if (!first_held)
      {
         first_held = true;
         menu_input->delay.timer = initial_held ? 12 : 6;
         menu_input->delay.count = 0;
      }

      if (menu_input->delay.count >= menu_input->delay.timer)
      {
         first_held = false;
         trigger_input |= input & input_repeat;
         nav->scroll.acceleration =
            min(nav->scroll.acceleration + 1, 64);
      }

      initial_held = false;
   }
   else
   {
      first_held = false;
      initial_held = true;
      nav->scroll.acceleration = 0;
   }

   menu_input->delay.count += disp->animation->delta_time / IDEAL_DT;

   if (menu_input->keyboard.display)
   {
      /* send return key to close keyboard input window */
      if (trigger_input & (UINT64_C(1) << settings->menu_cancel_btn))
         input_keyboard_event(true, '\n', '\n', 0, RETRO_DEVICE_KEYBOARD);

      trigger_input = 0;
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
   else
      ret = MENU_ACTION_NOOP;

   if (settings->menu.mouse.enable)
      menu_input_mouse(&ret);

   if (settings->menu.pointer.enable)
      menu_input_pointer(&ret);

   if (trigger_input &&
         menu_ctx_driver_get_ptr()->perform_action &&
         menu_ctx_driver_get_ptr()->perform_action(menu->userdata, ret))
      return MENU_ACTION_NOOP;

   return ret;
}

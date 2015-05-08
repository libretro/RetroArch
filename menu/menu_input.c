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
#include "menu_setting.h"
#include "menu_shader.h"
#include "menu_navigation.h"
#include "../cheats.h"
#include "../performance.h"
#include "../settings.h"
#include "../input/input_joypad.h"
#include "../input/input_remapping.h"

void menu_input_key_start_line(const char *label,
      const char *label_setting, unsigned type, unsigned idx,
      input_keyboard_line_complete_t cb)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return;

   menu->keyboard.display       = true;
   menu->keyboard.label         = label;
   menu->keyboard.label_setting = label_setting;
   menu->keyboard.type          = type;
   menu->keyboard.idx           = idx;
   menu->keyboard.buffer        = input_keyboard_start_line(menu, cb);
}

static void menu_input_key_end_line(void)
{
   driver_t *driver    = driver_get_ptr();
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return;

   menu->keyboard.display       = false;
   menu->keyboard.label         = NULL;
   menu->keyboard.label_setting = NULL;

   /* Avoid triggering states on pressing return. */
   driver->flushing_input = true;
}

static void menu_input_search_callback(void *userdata, const char *str)
{
   size_t idx;
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
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   if (str && *str)
   {
      rarch_setting_t *current_setting = NULL;
      if ((current_setting = menu_setting_find(menu->keyboard.label_setting)))
         *current_setting->value.unsigned_integer = strtoul(str, NULL, 0);
   }

   menu_input_key_end_line();
}

void menu_input_st_hex_callback(void *userdata, const char *str)
{
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;

   if (str && *str)
   {
      rarch_setting_t *current_setting = NULL;
      if ((current_setting = menu_setting_find(menu->keyboard.label_setting)))
         if (str[0] == '#')
            str++;
         *current_setting->value.unsigned_integer = strtoul(str, NULL, 16);
   }

   menu_input_key_end_line();
}


void menu_input_st_string_callback(void *userdata, const char *str)
{
   menu_handle_t *menu = menu_driver_get_ptr();

   if (!menu)
      return;
 
   if (str && *str)
   {
      rarch_setting_t *current_setting = NULL;
      global_t *global = global_get_ptr();

      if ((current_setting = menu_setting_find(menu->keyboard.label_setting)))
      {
         strlcpy(current_setting->value.string, str, current_setting->size);
         menu_setting_generic(current_setting);
      }
      else
      {
         if (!strcmp(menu->keyboard.label_setting, "video_shader_preset_save_as"))
            menu_shader_manager_save_preset(str, false);
         else if (!strcmp(menu->keyboard.label_setting, "remap_file_save_as"))
            input_remapping_save_file(str);
         else if (!strcmp(menu->keyboard.label_setting, "cheat_file_save_as"))
            cheat_manager_save(global->cheat, str);
      }
   }

   menu_input_key_end_line();
}

void menu_input_st_cheat_callback(void *userdata, const char *str)
{
   global_t *global       = global_get_ptr();
   cheat_manager_t *cheat = global->cheat;
   menu_handle_t *menu = (menu_handle_t*)userdata;

   if (!menu)
      return;
 
   if (cheat && str && *str)
   {
      unsigned cheat_index = menu->keyboard.type - MENU_SETTINGS_CHEAT_BEGIN;
      RARCH_LOG("cheat_index is: %u\n", cheat_index);

      cheat->cheats[cheat_index].code  = strdup(str);
      cheat->cheats[cheat_index].state = true;
   }

   menu_input_key_end_line();
}

void menu_input_search_start(void)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   if (!menu)
      return;

   menu->keyboard.display = true;
   menu->keyboard.label = "Search: ";
   menu->keyboard.buffer = 
      input_keyboard_start_line(menu, menu_input_search_callback);
}

void menu_input_key_event(bool down, unsigned keycode,
      uint32_t character, uint16_t mod)
{
   (void)down;
   (void)keycode;
   (void)mod;

   if (character == '/')
      menu_input_search_start();
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

bool menu_input_poll_find_trigger(struct menu_bind_state *state,
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
   menu_handle_t *menu = (menu_handle_t*)data;

   if (!menu)
      return false;

   menu->binds.target->key = (enum retro_key)code;
   menu->binds.begin++;
   menu->binds.target++;
   menu->binds.timeout_end = rarch_get_time_usec() +
      MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;

   return (menu->binds.begin <= menu->binds.last);
}

int menu_input_set_keyboard_bind_mode(void *data,
      enum menu_input_bind_mode type)
{
   struct retro_keybind *keybind = NULL;
   rarch_setting_t  *setting = (rarch_setting_t*)data;
   settings_t *settings      = config_get_ptr();
   menu_handle_t       *menu = menu_driver_get_ptr();
   menu_navigation_t   *nav  = menu_navigation_get_ptr();

   if (!menu || !setting)
      return -1;

   switch (type)
   {
      case MENU_INPUT_BIND_NONE:
         return -1;
      case MENU_INPUT_BIND_SINGLE:
         keybind = (struct retro_keybind*)setting->value.keybind;

         if (!keybind)
            return -1;

         menu->binds.begin  = setting->bind_type;
         menu->binds.last   = setting->bind_type;
         menu->binds.target = keybind;
         menu->binds.user = setting->index_offset;
         menu_list_push( menu->menu_list->menu_stack,
               "", "custom_bind",
               MENU_SETTINGS_CUSTOM_BIND_KEYBOARD,
               nav->selection_ptr);
         break;
      case MENU_INPUT_BIND_ALL:
         menu->binds.target = &settings->input.binds
            [setting->index_offset][0];
         menu->binds.begin = MENU_SETTINGS_BIND_BEGIN;
         menu->binds.last = MENU_SETTINGS_BIND_LAST;
         menu_list_push( menu->menu_list->menu_stack,
               "",
               "custom_bind_all",
               MENU_SETTINGS_CUSTOM_BIND_KEYBOARD,
               nav->selection_ptr);
         break;
   }


   menu->binds.timeout_end =
      rarch_get_time_usec() + 
      MENU_KEYBOARD_BIND_TIMEOUT_SECONDS * 1000000;
   input_keyboard_wait_keys(menu,
         menu_input_custom_bind_keyboard_cb);

   return 0;
}

int menu_input_set_input_device_bind_mode(void *data,
      enum menu_input_bind_mode type)
{
   struct retro_keybind *keybind = NULL;
   rarch_setting_t  *setting = (rarch_setting_t*)data;
   settings_t *settings      = config_get_ptr();
   menu_handle_t       *menu = menu_driver_get_ptr();
   menu_navigation_t   *nav  = menu_navigation_get_ptr();

   if (!menu || !setting)
      return -1;

   switch (type)
   {
      case MENU_INPUT_BIND_NONE:
         return -1;
      case MENU_INPUT_BIND_SINGLE:
         keybind = (struct retro_keybind*)setting->value.keybind;

         if (!keybind)
            return -1;
         menu->binds.begin  = setting->bind_type;
         menu->binds.last   = setting->bind_type;
         menu->binds.target = keybind;
         menu->binds.user   = setting->index_offset;
         menu_list_push( menu->menu_list->menu_stack,
               "",
               "custom_bind",
               MENU_SETTINGS_CUSTOM_BIND,
               nav->selection_ptr);
         break;
      case MENU_INPUT_BIND_ALL:
         menu->binds.target = &settings->input.binds
            [setting->index_offset][0];
         menu->binds.begin  = MENU_SETTINGS_BIND_BEGIN;
         menu->binds.last   = MENU_SETTINGS_BIND_LAST;
         menu_list_push( menu->menu_list->menu_stack,
               "",
               "custom_bind_all",
               MENU_SETTINGS_CUSTOM_BIND,
               nav->selection_ptr);
         break;
   }

   menu_input_poll_bind_get_rested_axes(&menu->binds);
   menu_input_poll_bind_state(&menu->binds);

   return 0;
}

int menu_input_bind_iterate(void)
{
   char msg[PATH_MAX_LENGTH];
   struct menu_bind_state binds;
   menu_handle_t *menu = menu_driver_get_ptr();
   driver_t *driver = driver_get_ptr();

   if (!menu)
      return 1;
   
   binds = menu->binds;
    
   menu_driver_render();

   snprintf(msg, sizeof(msg), "[%s]\npress joypad\n(RETURN to skip)",
         input_config_bind_map[
         menu->binds.begin - MENU_SETTINGS_BIND_BEGIN].desc);

   menu_driver_render_messagebox(msg);

   driver->block_input = true;
   menu_input_poll_bind_state(&binds);

   if ((binds.skip && !menu->binds.skip) ||
         menu_input_poll_find_trigger(&menu->binds, &binds))
   {
      driver->block_input = false;

      /* Avoid new binds triggering things right away. */
      driver->flushing_input = true;

      binds.begin++;

      if (binds.begin > binds.last)
         return 1;

      binds.target++;
   }
   menu->binds = binds;

   return 0;
}

int menu_input_bind_iterate_keyboard(void)
{
   char msg[PATH_MAX_LENGTH];
   int64_t current;
   int timeout = 0;
   bool timed_out = false;
   menu_handle_t *menu = menu_driver_get_ptr();
   driver_t *driver = driver_get_ptr();

   if (!menu)
      return -1;

   menu_driver_render();

   current = rarch_get_time_usec();
   timeout = (menu->binds.timeout_end - current) / 1000000;
   snprintf(msg, sizeof(msg), "[%s]\npress keyboard\n(timeout %d seconds)",
         input_config_bind_map[
         menu->binds.begin - MENU_SETTINGS_BIND_BEGIN].desc,
         timeout);

   menu_driver_render_messagebox(msg);

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
      driver->flushing_input = true;

      /* We won't be getting any key events, so just cancel early. */
      if (timed_out)
         input_keyboard_wait_keys_cancel();

      return 1;
   }

   return 0;
}

static int menu_input_mouse(unsigned *action)
{
   const struct retro_keybind *binds[MAX_USERS];
   driver_t *driver          = driver_get_ptr();
   menu_handle_t *menu       = menu_driver_get_ptr();
   runloop_t *runloop        = rarch_main_get_ptr();
   settings_t *settings      = config_get_ptr();
   video_viewport_t vp;

   if (!menu)
      return -1;

   if (!settings->menu.mouse.enable
#ifdef HAVE_OVERLAY
       || (settings->input.overlay_enable && driver && driver->overlay)
#endif
      )
   {
      menu->mouse.left       = 0;
      menu->mouse.right      = 0;
      menu->mouse.wheelup    = 0;
      menu->mouse.wheeldown  = 0;
      menu->mouse.hwheelup   = 0;
      menu->mouse.hwheeldown = 0;
      menu->mouse.dx         = 0;
      menu->mouse.dy         = 0;
      menu->mouse.x          = 0;
      menu->mouse.y          = 0;
      menu->mouse.screen_x   = 0;
      menu->mouse.screen_y   = 0;
      menu->mouse.scrollup   = 0;
      menu->mouse.scrolldown = 0;
      return 0;
   }

   if (!video_driver_viewport_info(&vp))
      return -1;

   if (menu->mouse.hwheeldown)
   {
      *action = MENU_ACTION_LEFT;
      menu->mouse.hwheeldown = false;
      return 0;
   }

   if (menu->mouse.hwheelup)
   {
      *action = MENU_ACTION_RIGHT;
      menu->mouse.hwheelup = false;
      return 0;
   }

   menu->mouse.left       = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_LEFT);
   menu->mouse.right      = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_RIGHT);
   menu->mouse.wheelup    = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_WHEELUP);
   menu->mouse.wheeldown  = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_WHEELDOWN);
   menu->mouse.hwheelup   = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELUP);
   menu->mouse.hwheeldown = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_HORIZ_WHEELDOWN);
   menu->mouse.dx         = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_X);
   menu->mouse.dy         = input_driver_state(binds, 0, RETRO_DEVICE_MOUSE,
         0, RETRO_DEVICE_ID_MOUSE_Y);

   menu->mouse.screen_x += menu->mouse.dx;
   menu->mouse.screen_y += menu->mouse.dy;

   menu->mouse.x         = ((int)menu->mouse.screen_x * (int)menu->frame_buf.width) / (int)vp.width;
   menu->mouse.y         = ((int)menu->mouse.screen_y * (int)menu->frame_buf.height) / (int)vp.height;

   if (menu->mouse.x < 5)
      menu->mouse.x       = 5;
   if (menu->mouse.y < 5)
      menu->mouse.y       = 5;
   if (menu->mouse.x > (int)menu->frame_buf.width - 5)
      menu->mouse.x       = menu->frame_buf.width - 5;
   if (menu->mouse.y > (int)menu->frame_buf.height - 5)
      menu->mouse.y       = menu->frame_buf.height - 5;

   menu->mouse.scrollup   = (menu->mouse.y == 5);
   menu->mouse.scrolldown = (menu->mouse.y == (int)menu->frame_buf.height - 5);

   if (menu->mouse.dx != 0 || menu->mouse.dy !=0 || menu->mouse.left
      || menu->mouse.wheelup || menu->mouse.wheeldown
      || menu->mouse.hwheelup || menu->mouse.hwheeldown
      || menu->mouse.scrollup || menu->mouse.scrolldown)
      runloop->frames.video.current.menu.animation.is_active = true;

   return 0;
}

static int menu_input_pointer(unsigned *action)
{
   int pointer_device, pointer_x, pointer_y;
   const struct retro_keybind *binds[MAX_USERS];      
   menu_handle_t *menu       = menu_driver_get_ptr();
   runloop_t *runloop        = rarch_main_get_ptr();
   settings_t *settings      = config_get_ptr();
   driver_t *driver     = driver_get_ptr();

   if (!menu)
      return -1;

   if (!settings->menu.pointer.enable)
   {
      memset(&menu->pointer, 0, sizeof(menu->pointer));
      return 0;
   }

   pointer_device = driver->menu_ctx->set_texture?
        RETRO_DEVICE_POINTER : RARCH_DEVICE_POINTER_SCREEN;

   menu->pointer.pressed[0]  = input_driver_state(binds, 0, pointer_device,
         0, RETRO_DEVICE_ID_POINTER_PRESSED);
   menu->pointer.pressed[1]  = input_driver_state(binds, 0, pointer_device,
         1, RETRO_DEVICE_ID_POINTER_PRESSED);
   menu->pointer.back  = input_driver_state(binds, 0, pointer_device,
         0, RARCH_DEVICE_ID_POINTER_BACK);

   pointer_x = input_driver_state(binds, 0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_X);
   pointer_y = input_driver_state(binds, 0, pointer_device, 0, RETRO_DEVICE_ID_POINTER_Y);

   menu->pointer.x = ((pointer_x + 0x7fff) * (int)menu->frame_buf.width) / 0xFFFF;
   menu->pointer.y = ((pointer_y + 0x7fff) * (int)menu->frame_buf.height) / 0xFFFF;

   if (menu->pointer.pressed[0] || menu->pointer.oldpressed[0]
     || menu->pointer.back || menu->pointer.dragging
     || menu->pointer.dy != 0 || menu->pointer.dx != 0)
     runloop->frames.video.current.menu.animation.is_active = true;

   return 0;
}

static int menu_input_mouse_post_iterate(menu_file_list_cbs_t *cbs,
      const char *path,
      const char *label, unsigned type, unsigned action)
{
   driver_t      *driver  = driver_get_ptr();
   settings_t *settings   = config_get_ptr();
   menu_handle_t *menu    = menu_driver_get_ptr();
   menu_navigation_t *nav = menu_navigation_get_ptr();

   if (!menu)
      return -1;

   if (!settings->menu.mouse.enable
#ifdef HAVE_OVERLAY
       || (settings->input.overlay_enable && driver && driver->overlay)
#endif
       )
   {
      menu->mouse.wheeldown = false;
      menu->mouse.wheelup   = false;
      menu->mouse.oldleft   = false;
      menu->mouse.oldright  = false;
      return 0;
   }

   if (menu->mouse.left)
   {
      if (!menu->mouse.oldleft)
      {
         rarch_setting_t *setting = menu_setting_find(
               menu->menu_list->selection_buf->list[nav->selection_ptr].label);
         menu->mouse.oldleft = true;

#if 0
         RARCH_LOG("action OK: %d\n", cbs && cbs->action_ok);
         RARCH_LOG("action toggle: %d\n", cbs && cbs->action_toggle);
         if (setting && setting->type)
            RARCH_LOG("action type: %d\n", setting->type);
#endif
         if (menu->mouse.y < menu->header_height)
         {
            menu_list_pop_stack(menu->menu_list);
            return 0;
         }
         if (menu->mouse.ptr == nav->selection_ptr
            && cbs && cbs->action_toggle && setting &&
            (setting->type == ST_BOOL || setting->type == ST_UINT || setting->type == ST_FLOAT
             || setting->type == ST_STRING))
            return cbs->action_toggle(type, label, MENU_ACTION_RIGHT, true);
         if (menu->mouse.ptr == nav->selection_ptr
            && cbs && cbs->action_ok)
            return cbs->action_ok(path, label, type, nav->selection_ptr);
         else if (menu->mouse.ptr <= menu_list_get_size(menu->menu_list)-1)
            menu_navigation_set(nav, menu->mouse.ptr, false);
      }
   }
   else
      menu->mouse.oldleft = false;

   if (menu->mouse.right)
   {
      if (!menu->mouse.oldright)
      {
         menu->mouse.oldright = true;
         menu_list_pop_stack(menu->menu_list);
      }
   }
   else
      menu->mouse.oldright = false;

   if (menu->mouse.wheeldown)
      menu_navigation_increment(nav, 1);

   if (menu->mouse.wheelup)
      menu_navigation_decrement(nav, 1);

   return 0;
}

static int pointer_tap(menu_file_list_cbs_t *cbs, const char *path,
      const char *label, unsigned type, unsigned action)
{
   menu_handle_t *menu = menu_driver_get_ptr();
   driver_t *driver = driver_get_ptr();
   rarch_setting_t *setting =
      menu_setting_find(
            driver->menu->menu_list->selection_buf->list[menu->navigation.selection_ptr].label);

   if (menu->pointer.ptr == menu->navigation.selection_ptr
         && cbs && cbs->action_toggle && setting &&
         (setting->type == ST_BOOL || setting->type == ST_UINT
          || setting->type == ST_FLOAT || setting->type == ST_STRING))
      return cbs->action_toggle(type, label, MENU_ACTION_RIGHT, true);
   else if (menu->pointer.ptr == menu->navigation.selection_ptr)
      return cbs->action_ok(path, label, type, menu->navigation.selection_ptr);
   else
      menu_navigation_set(&menu->navigation, menu->pointer.ptr, false);

   return 0;
}

static int menu_input_pointer_post_iterate(menu_file_list_cbs_t *cbs,
      const char *path,
      const char *label, unsigned type, unsigned action)
{
   int ret = 0;
   menu_handle_t *menu  = menu_driver_get_ptr();
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();

   if (!menu)
      return -1;

   if (!settings->menu.pointer.enable
#ifdef HAVE_OVERLAY
       || (settings->input.overlay_enable && driver && driver->overlay)
#endif
      )
      return 0;

   if (menu->pointer.pressed[0])
   {
      if (!menu->pointer.oldpressed[0])
      {
         menu->pointer.start_x = menu->pointer.x;
         menu->pointer.start_y = menu->pointer.y;
         menu->pointer.old_x = menu->pointer.x;
         menu->pointer.old_y = menu->pointer.y;
         menu->pointer.oldpressed[0] = true;
      }
      else if (menu->pointer.x != menu->pointer.start_x
         && menu->pointer.y != menu->pointer.start_y)
      {
         menu->pointer.dragging = true;
         menu->pointer.dx = menu->pointer.x - menu->pointer.old_x;
         menu->pointer.dy = menu->pointer.y - menu->pointer.old_y;
         menu->pointer.old_x = menu->pointer.x;
         menu->pointer.old_y = menu->pointer.y;
      }
   }
   else
   {
      if (menu->pointer.oldpressed[0])
      {
         if (!menu->pointer.dragging)
         {
            if (menu->pointer.start_y < menu->header_height)
            {
               menu_list_pop_stack(menu->menu_list);
            }
            else if (menu->pointer.ptr <= menu_list_get_size(menu->menu_list)-1)
            {
               menu->pointer.oldpressed[0] = false;
               ret = pointer_tap(cbs, path, label, type, action);
            }
         }
         menu->pointer.oldpressed[0] = false;
         menu->pointer.start_x = 0;
         menu->pointer.start_y = 0;
         menu->pointer.old_x = 0;
         menu->pointer.old_y = 0;
         menu->pointer.dx = 0;
         menu->pointer.dy = 0;
         menu->pointer.dragging = false;
      }
   }

   if (menu->pointer.back)
   {
      if (!menu->pointer.oldback)
      {
         menu->pointer.oldback = true;
         menu_list_pop_stack(menu->menu_list);
      }
   }
   menu->pointer.oldback = menu->pointer.back;

   return ret;
}

void menu_input_post_iterate(int *ret, menu_file_list_cbs_t *cbs, const char *path,
      const char *label, unsigned type, unsigned action)
{
   settings_t *settings = config_get_ptr();

   if (settings->menu.mouse.enable)
      *ret  = menu_input_mouse_post_iterate  (cbs, path, label, type, action);
   if (settings->menu.pointer.enable)
      *ret |= menu_input_pointer_post_iterate(cbs, path, label, type, action);
}

unsigned menu_input_frame(retro_input_t input, retro_input_t trigger_input)
{
   unsigned ret = 0;
   static bool initial_held = true;
   static bool first_held = false;
   static const retro_input_t input_repeat =
      (1ULL << RETRO_DEVICE_ID_JOYPAD_UP)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_L)
      | (1ULL << RETRO_DEVICE_ID_JOYPAD_R);
   menu_handle_t *menu = menu_driver_get_ptr();
   driver_t *driver    = driver_get_ptr();
   settings_t *settings = config_get_ptr();

   if (!menu || !driver)
      return 0;

   driver->retro_ctx.poll_cb();

   /* don't run anything first frame, only capture held inputs
    * for old_input_state. */

   if (input & input_repeat)
   {
      if (!first_held)
      {
         first_held = true;
         menu->delay.timer = initial_held ? 12 : 6;
         menu->delay.count = 0;
      }

      if (menu->delay.count >= menu->delay.timer)
      {
         first_held = false;
         trigger_input |= input & input_repeat;
         menu->navigation.scroll.acceleration = 
            min(menu->navigation.scroll.acceleration + 1, 64);
      }

      initial_held = false;
   }
   else
   {
      first_held = false;
      initial_held = true;
      menu->navigation.scroll.acceleration = 0;
   }

   menu->delay.count += menu->dt / IDEAL_DT;

   if (driver->block_input)
      trigger_input = 0;
   if (trigger_input & (1ULL << RETRO_DEVICE_ID_JOYPAD_UP))
      ret = MENU_ACTION_UP;
   else if (trigger_input & (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN))
      ret = MENU_ACTION_DOWN;
   else if (trigger_input & (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT))
      ret = MENU_ACTION_LEFT;
   else if (trigger_input & (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      ret = MENU_ACTION_RIGHT;
   else if (trigger_input & (1ULL << settings->menu_scroll_up_btn))
      ret = MENU_ACTION_SCROLL_UP;
   else if (trigger_input & (1ULL << settings->menu_scroll_down_btn))
      ret = MENU_ACTION_SCROLL_DOWN;
   else if (trigger_input & (1ULL << settings->menu_cancel_btn))
      ret = MENU_ACTION_CANCEL;
   else if (trigger_input & (1ULL << settings->menu_ok_btn))
      ret = MENU_ACTION_OK;
   else if (trigger_input & (1ULL << settings->menu_search_btn))
      ret = MENU_ACTION_SEARCH;
   else if (trigger_input & (1ULL << RETRO_DEVICE_ID_JOYPAD_Y))
      ret = MENU_ACTION_TEST;
   else if (trigger_input & (1ULL << settings->menu_default_btn))
      ret = MENU_ACTION_START;
   else if (trigger_input & (1ULL << settings->menu_info_btn))
      ret = MENU_ACTION_SELECT;
   else if (trigger_input & (1ULL << RARCH_MENU_TOGGLE))
      ret = MENU_ACTION_TOGGLE;
   else
      ret = MENU_ACTION_NOOP;

   if (settings->menu.mouse.enable)
      menu_input_mouse(&ret);

   if (settings->menu.pointer.enable)
      menu_input_pointer(&ret);

   return ret;
}

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include "rmenu.h"
#include "rgui.h"
#include "../../gfx/gl_common.h"

#include "../../gfx/fonts/bitmap.h"

static uint16_t menu_framebuf[400 * 240];
static rgui_handle_t *rgui;

static bool folder_cb(const char *directory, rgui_file_enum_cb_t file_cb,
      void *userdata, void *ctx)
{
   struct string_list *ext_list = string_split(g_extern.system.valid_extensions, "|");

   char _dir[PATH_MAX];
   snprintf(_dir, sizeof(_dir), "%s/", directory);
   DIR *dir = opendir(_dir);
   if (!dir)
      return false;

   struct dirent *entry;
   while ((entry = readdir(dir)))
   {
      char stat_path[PATH_MAX];
      const char *file_ext = path_get_extension(entry->d_name);
      snprintf(stat_path, sizeof(stat_path), "%s/%s", directory, entry->d_name);
      bool is_dir;

#ifdef _DIRENT_HAVE_D_TYPE
      is_dir = (entry->d_type == DT_DIR);
      if (entry->d_type != DT_REG && !is_dir)
         continue;
#else
      struct stat st;
      if (stat(stat_path, &st) < 0)
         continue;

      is_dir = S_ISDIR(st.st_mode);
      if (!S_ISREG(st.st_mode) && !is_dir)
         continue;
#endif

      if (!is_dir && ext_list && !string_list_find_elem_prefix(ext_list, ".", file_ext))
         continue;

      file_cb(ctx,
            entry->d_name,
            is_dir ? RGUI_FILE_DIRECTORY : RGUI_FILE_PLAIN, 0);
   }

   closedir(dir);
   string_list_free(ext_list);
   return true;
}

/*============================================================
RMENU API
============================================================ */

void menu_init(void)
{
   gl_t *gl = (gl_t*)driver.video_data;

   gl->menu_data = (uint32_t *) menu_framebuf;

   rgui = rgui_init("",
         menu_framebuf, RGUI_WIDTH * sizeof(uint16_t),
         NULL, bitmap_bin, folder_cb, NULL);

   rgui_iterate(rgui, RGUI_ACTION_REFRESH);
}

void menu_free(void)
{
   rgui_free(rgui);
}

static uint16_t trigger_state = 0;

int rmenu_input_process(void *data, void *state)
{
   if (g_extern.lifecycle_mode_state & (1ULL << MODE_LOAD_GAME))
   {
      if (g_extern.lifecycle_mode_state & (1ULL << MODE_INFO_DRAW))
         rmenu_settings_msg(S_MSG_LOADING_ROM, 100);

      if (g_extern.fullpath)
         g_extern.lifecycle_mode_state |= (1ULL << MODE_INIT);

      g_extern.lifecycle_mode_state &= ~(1ULL << MODE_LOAD_GAME);
      return -1;
   }

   if (!(g_extern.frame_count < g_extern.delay_timer[0]))
   {
      bool return_to_game_enable = ((g_extern.lifecycle_mode_state & (1ULL << RARCH_RMENU_TOGGLE)) && g_extern.main_is_init);

      if (return_to_game_enable)
      {
         g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
         return -1;
      }
   }

   return 0;
}

bool rmenu_iterate(void)
{
   static const struct retro_keybind *binds[] = {
      g_settings.input.binds[0]
   };

   static uint16_t old_input_state = 0;
   static bool initial_held = true;
   static bool first_held = false;

   driver.input->poll(NULL);

#ifdef HAVE_OVERLAY
   if (driver.overlay)
   {
      driver.overlay_state = 0;

      unsigned device = input_overlay_full_screen(driver.overlay) ?
         RARCH_DEVICE_POINTER_SCREEN : RETRO_DEVICE_POINTER;

      bool polled = false;
      for (unsigned i = 0;
            input_input_state_func(NULL, 0, device, i, RETRO_DEVICE_ID_POINTER_PRESSED);
            i++)
      {
         int16_t x = input_input_state_func(NULL, 0,
               device, i, RETRO_DEVICE_ID_POINTER_X);
         int16_t y = input_input_state_func(NULL, 0,
               device, i, RETRO_DEVICE_ID_POINTER_Y);

         driver.overlay_state |= input_overlay_poll(driver.overlay, x, y);
         polled = true;
      }

      if (!polled)
         input_overlay_poll_clear(driver.overlay);
   }
#endif

   if (input_key_pressed_func(RARCH_QUIT_KEY) || !video_alive_func())
   {
      g_extern.lifecycle_mode_state |= (1ULL << MODE_GAME);
      goto deinit;
   }

   g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_DRAW);

   driver.video->apply_state_changes();

   g_extern.frame_count++;

   uint16_t input_state = 0;

   for (unsigned i = 0; i < 16; i++)
   {
      input_state |= driver.input->input_state(NULL, binds, 0, RETRO_DEVICE_JOYPAD, 0, i) ? (1ULL << i) : 0;
#ifdef HAVE_OVERLAY
      input_state |= driver.overlay_state & (1ULL << i) ? (1ULL << i) : 0;
#endif
   }

   trigger_state = input_state & ~old_input_state;
   bool do_held = input_state & ((1ULL << RETRO_DEVICE_ID_JOYPAD_UP) | (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN) | (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT) | (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT));

   if(do_held)
   {
      if(!first_held)
      {
         first_held = true;
         g_extern.delay_timer[1] = g_extern.frame_count + (initial_held ? 15 : 7);
      }

      if (!(g_extern.frame_count < g_extern.delay_timer[1]))
      {
         first_held = false;
         trigger_state = input_state; //second input frame set as current frame
      }

      initial_held = false;
   }
   else
   {
      first_held = false;
      initial_held = true;
   }

   old_input_state = input_state;

   rgui_action_t action = RGUI_ACTION_NOOP;

   // don't run anything first frame, only capture held inputs for old_input_state
   if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_UP))
      action = RGUI_ACTION_UP;
   else if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_DOWN))
      action = RGUI_ACTION_DOWN;
   else if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_LEFT))
      action = RGUI_ACTION_LEFT;
   else if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_RIGHT))
      action = RGUI_ACTION_RIGHT;
   else if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_B))
      action = RGUI_ACTION_CANCEL;
   else if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_A))
      action = RGUI_ACTION_OK;
   else if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_START))
      action = RGUI_ACTION_START;
   //else if (trigger_state & (1ULL << RETRO_DEVICE_ID_JOYPAD_SELECT))
   //   action = RGUI_ACTION_SETTINGS;

   int input_entry_ret = 0;
   int input_process_ret = 0;

   input_entry_ret = rgui_iterate(rgui, action);

   // draw last frame for loading messages
   rarch_render_cached_frame();

   input_process_ret = rmenu_input_process(NULL, NULL);

   if (input_entry_ret != 0 || input_process_ret != 0)
      goto deinit;

   return true;

deinit:
   // set a timer delay so that we don't instantly switch back to the menu when
   // press and holding QUIT in the emulation loop (lasts for 30 frame ticks)
   if (!(g_extern.lifecycle_state & (1ULL << RARCH_FRAMEADVANCE)))
      g_extern.delay_timer[0] = g_extern.frame_count + 30;

   g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_DRAW);
   g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_INGAME);

   return false;
}

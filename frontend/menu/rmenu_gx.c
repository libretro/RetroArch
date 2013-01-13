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

#include "../../gfx/fonts/bitmap.h"

uint16_t menu_framebuf[400 * 240];

static const struct retro_keybind _rmenu_nav_binds[] = {
#ifdef HW_RVL
   { 0, 0, NULL, 0, GX_GC_UP | GX_GC_LSTICK_UP | GX_GC_RSTICK_UP | GX_CLASSIC_UP | GX_CLASSIC_LSTICK_UP | GX_CLASSIC_RSTICK_UP | GX_WIIMOTE_UP | GX_NUNCHUK_UP, 0 },
   { 0, 0, NULL, 0, GX_GC_DOWN | GX_GC_LSTICK_DOWN | GX_GC_RSTICK_DOWN | GX_CLASSIC_DOWN | GX_CLASSIC_LSTICK_DOWN | GX_CLASSIC_RSTICK_DOWN | GX_WIIMOTE_DOWN | GX_NUNCHUK_DOWN, 0 },
   { 0, 0, NULL, 0, GX_GC_LEFT | GX_GC_LSTICK_LEFT | GX_GC_RSTICK_LEFT | GX_CLASSIC_LEFT | GX_CLASSIC_LSTICK_LEFT | GX_CLASSIC_RSTICK_LEFT | GX_WIIMOTE_LEFT | GX_NUNCHUK_LEFT, 0 },
   { 0, 0, NULL, 0, GX_GC_RIGHT | GX_GC_LSTICK_RIGHT | GX_GC_RSTICK_RIGHT | GX_CLASSIC_RIGHT | GX_CLASSIC_LSTICK_RIGHT | GX_CLASSIC_RSTICK_RIGHT | GX_WIIMOTE_RIGHT | GX_NUNCHUK_RIGHT, 0 },
   { 0, 0, NULL, 0, GX_GC_A | GX_CLASSIC_A | GX_WIIMOTE_A | GX_WIIMOTE_2, 0 },
   { 0, 0, NULL, 0, GX_GC_B | GX_CLASSIC_B | GX_WIIMOTE_B | GX_WIIMOTE_1, 0 },
   { 0, 0, NULL, 0, GX_GC_START | GX_CLASSIC_PLUS | GX_WIIMOTE_PLUS, 0 },
   { 0, 0, NULL, 0, GX_GC_Z_TRIGGER | GX_CLASSIC_MINUS | GX_WIIMOTE_MINUS, 0 },
   { 0, 0, NULL, 0, GX_WIIMOTE_HOME | GX_CLASSIC_HOME, 0 },
#else
   { 0, 0, NULL, 0, GX_GC_UP | GX_GC_LSTICK_UP | GX_GC_RSTICK_UP, 0 },
   { 0, 0, NULL, 0, GX_GC_DOWN | GX_GC_LSTICK_DOWN | GX_GC_RSTICK_DOWN, 0 },
   { 0, 0, NULL, 0, GX_GC_LEFT | GX_GC_LSTICK_LEFT | GX_GC_RSTICK_LEFT, 0 },
   { 0, 0, NULL, 0, GX_GC_RIGHT | GX_GC_LSTICK_RIGHT | GX_GC_RSTICK_RIGHT, 0 },
   { 0, 0, NULL, 0, GX_GC_A, 0 },
   { 0, 0, NULL, 0, GX_GC_B, 0 },
   { 0, 0, NULL, 0, GX_GC_START, 0 },
   { 0, 0, NULL, 0, GX_GC_Z_TRIGGER, 0 },
   { 0, 0, NULL, 0, GX_WIIMOTE_HOME, 0 },
#endif
   { 0, 0, NULL, 0, GX_QUIT_KEY, 0 },
};

static const struct retro_keybind *rmenu_nav_binds[] = {
   _rmenu_nav_binds
};

enum
{
   GX_DEVICE_NAV_UP = 0,
   GX_DEVICE_NAV_DOWN,
   GX_DEVICE_NAV_LEFT,
   GX_DEVICE_NAV_RIGHT,
   GX_DEVICE_NAV_A,
   GX_DEVICE_NAV_B,
   GX_DEVICE_NAV_START,
   GX_DEVICE_NAV_SELECT,
   GX_DEVICE_NAV_MENU,
   GX_DEVICE_NAV_QUIT,
   RMENU_DEVICE_NAV_LAST
};

static bool folder_cb(const char *directory, rgui_file_enum_cb_t file_cb,
      void *userdata, void *ctx)
{
   bool core_chooser = (userdata) ? *(rgui_file_type_t *)userdata == RGUI_SETTINGS_CORE : false;

   if (!*directory)
   {
#ifdef HW_RVL
      file_cb(ctx, "sd:", RGUI_FILE_DEVICE, 0);
      file_cb(ctx, "usb:", RGUI_FILE_DEVICE, 0);
#endif
      file_cb(ctx, "carda:", RGUI_FILE_DEVICE, 0);
      file_cb(ctx, "cardb:", RGUI_FILE_DEVICE, 0);
      return true;
   }

#ifdef HW_RVL
   LWP_MutexLock(gx_device_mutex);
   int dev = gx_get_device_from_path(directory);

   if (dev != -1 && !gx_devices[dev].mounted && gx_devices[dev].interface->isInserted())
      fatMountSimple(gx_devices[dev].name, gx_devices[dev].interface);

   LWP_MutexUnlock(gx_device_mutex);
#endif

   char exts[256];
   if (core_chooser)
      strlcpy(exts, "dol|DOL", sizeof(exts));
   else
      strlcpy(exts, g_extern.system.valid_extensions, sizeof(exts));
   struct string_list *ext_list = string_split(exts, "|");

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

      if (core_chooser && (is_dir || strcasecmp(entry->d_name, default_paths.salamander_file) == 0))
         continue;

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
   gx_video_t *gx = (gx_video_t*)driver.video_data;

   gx->menu_data = (uint32_t *) menu_framebuf;

   rgui = rgui_init("",
         menu_framebuf, RGUI_WIDTH * sizeof(uint16_t),
         NULL /* _binary_console_font_bmp_start */, bitmap_bin, folder_cb, NULL);

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
      bool return_to_game_enable = ((trigger_state & (1ULL << GX_DEVICE_NAV_MENU)) && g_extern.main_is_init);

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
   static uint16_t old_input_state = 0;
   static bool initial_held = true;
   static bool first_held = false;

   g_extern.lifecycle_mode_state |= (1ULL << MODE_MENU_DRAW);
   driver.video->apply_state_changes();

   g_extern.frame_count++;

   rarch_render_cached_frame();

   uint16_t input_state = 0;

   driver.input->poll(NULL);

   for (unsigned i = 0; i < RMENU_DEVICE_NAV_LAST; i++)
      input_state |= driver.input->input_state(NULL, rmenu_nav_binds, 0,
            RETRO_DEVICE_JOYPAD, 0, i) ? (1ULL << i) : 0;

   trigger_state = input_state & ~old_input_state;
   bool do_held = (input_state & ((1ULL << GX_DEVICE_NAV_UP) | (1ULL << GX_DEVICE_NAV_DOWN) | (1ULL << GX_DEVICE_NAV_LEFT) | (1ULL << GX_DEVICE_NAV_RIGHT))) && !(input_state & ((1ULL << GX_DEVICE_NAV_MENU) | (1ULL << GX_DEVICE_NAV_QUIT)));

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
   if (trigger_state & (1ULL << GX_DEVICE_NAV_UP))
      action = RGUI_ACTION_UP;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_DOWN))
      action = RGUI_ACTION_DOWN;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_LEFT))
      action = RGUI_ACTION_LEFT;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_RIGHT))
      action = RGUI_ACTION_RIGHT;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_B))
      action = RGUI_ACTION_CANCEL;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_A))
      action = RGUI_ACTION_OK;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_START))
      action = RGUI_ACTION_START;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_SELECT))
      action = RGUI_ACTION_SETTINGS;
   else if (trigger_state & (1ULL << GX_DEVICE_NAV_QUIT))
   {
      g_extern.lifecycle_mode_state |= (1ULL << MODE_EXIT);
      goto deinit;
   }

   int input_entry_ret = 0;
   int input_process_ret = 0;

   input_entry_ret = rgui_iterate(rgui, action);

   input_process_ret = rmenu_input_process(NULL, NULL);

   if (input_entry_ret != 0 || input_process_ret != 0)
      goto deinit;

   return true;

deinit:
   // draw last frame for loading messages
   rarch_render_cached_frame();

   // set a timer delay so that we don't instantly switch back to the menu when
   // press and holding QUIT in the emulation loop (lasts for 30 frame ticks)
   if (!(g_extern.lifecycle_state & (1ULL << RARCH_FRAMEADVANCE)))
      g_extern.delay_timer[0] = g_extern.frame_count + 30;

   g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_DRAW);
   g_extern.lifecycle_mode_state &= ~(1ULL << MODE_MENU_INGAME);

   return false;
}

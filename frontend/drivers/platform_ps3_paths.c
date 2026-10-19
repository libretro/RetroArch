/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdio.h>
#include <string.h>

#include <compat/strl.h>
#include <file/config_file.h>
#include <file/file_path.h>

#include "platform_ps3_paths.h"
#include "../../defaults.h"
#include "../../file_path_special.h"

static const struct
{
   const char *name;
   const char *setting;
   enum default_dirs dir;
} ps3_user_dirs[] = {
   { "config", "rgui_config_directory", DEFAULT_DIR_MENU_CONFIG },
   { "remaps", "input_remapping_directory", DEFAULT_DIR_REMAP },
   { "savefiles", "savefile_directory", DEFAULT_DIR_SRAM },
   { "savestates", "savestate_directory", DEFAULT_DIR_SAVESTATE },
   { "screenshots", "screenshot_directory", DEFAULT_DIR_SCREENSHOT },
   { "playlists", "playlist_directory", DEFAULT_DIR_PLAYLIST },
   { "", "content_favorites_directory", DEFAULT_DIR_CONTENT_FAVORITES },
   { "", "content_history_directory", DEFAULT_DIR_CONTENT_HISTORY },
   { "", "content_image_history_directory", DEFAULT_DIR_CONTENT_IMAGE_HISTORY },
   { "", "content_music_history_directory", DEFAULT_DIR_CONTENT_MUSIC_HISTORY },
   { "", "content_video_history_directory", DEFAULT_DIR_CONTENT_VIDEO_HISTORY },
   { "autoconfig", "joypad_autoconfig_dir", DEFAULT_DIR_AUTOCONFIG },
   { "cache", "cache_directory", DEFAULT_DIR_CACHE },
   { "logs", "log_dir", DEFAULT_DIR_LOGS },
   { "records_config", "recording_config_directory", DEFAULT_DIR_RECORD_CONFIG },
   { "records", "recording_output_directory", DEFAULT_DIR_RECORD_OUTPUT }
};

bool ps3_user_paths_init(unsigned user_id)
{
   char user_dir[128];
   size_t i;

   if (!user_id || user_id > 99999999)
      return false;

   snprintf(user_dir, sizeof(user_dir),
         "/dev_hdd0/home/%08u/retroarch", user_id);
   for (i = 0; i < sizeof(ps3_user_dirs) / sizeof(ps3_user_dirs[0]); i++)
      fill_pathname_join(g_defaults.dirs[ps3_user_dirs[i].dir], user_dir,
            ps3_user_dirs[i].name, sizeof(g_defaults.dirs[0]));
   fill_pathname_join(g_defaults.path_config, user_dir,
         FILE_PATH_MAIN_CONFIG, sizeof(g_defaults.path_config));
   return true;
}

bool ps3_config_import(const char *path, char *dest, size_t len)
{
   config_file_t *source;
   config_file_t *config;
   struct config_file_entry entry;
   char user_dir[DIR_MAX_LENGTH];
   char name[32];
   size_t i;
   unsigned index;
   bool ret = false;

   fill_pathname_basedir(user_dir, g_defaults.path_config, sizeof(user_dir));
   if (   !*user_dir || !path || !*path
       || strlen(path) >= len
       || strlen(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]) + sizeof(name) >= len)
      return false;

   strlcpy(dest, path, len);
   if (   !strncmp(dest, user_dir, strlen(user_dir))
       && !strstr(dest, ".."))
      return true;

   if (!(source = config_file_new_from_path_to_string(dest)))
      return false;
   if (!(config = config_file_new_alloc()))
   {
      config_file_free(source);
      return false;
   }

   /* Keep imported settings independent of their source files. */
   if (config_get_entry_list_head(source, &entry))
      do
      {
         if (   entry.key && entry.value
             && !config_get_entry(config, entry.key))
            config_set_string(config, entry.key, entry.value);
      } while (config_get_entry_list_next(&entry));

   for (i = 0; i < sizeof(ps3_user_dirs) / sizeof(ps3_user_dirs[0]); i++)
      config_set_path(config, ps3_user_dirs[i].setting,
            g_defaults.dirs[ps3_user_dirs[i].dir]);

   config_set_path(config, "content_history_path", "");
   config_set_path(config, "content_favorites_path", "");
   config_set_path(config, "content_image_history_path", "");
   config_set_path(config, "content_music_history_path", "");
   config_set_path(config, "content_video_history_path", "");
   config_set_path(config, "core_options_path", "");
   config_set_path(config, "runtime_log_directory", "");

   if (path_mkdir(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]))
      for (index = 0; index < 1000; index++)
      {
         snprintf(name, sizeof(name), "imported-%u.cfg", index);
         fill_pathname_join(dest, g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
               name, len);
         if (!path_is_valid(dest))
         {
            ret = config_file_write(config, dest, true);
            break;
         }
      }

   config_file_free(config);
   config_file_free(source);
   return ret;
}

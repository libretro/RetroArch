/* Copyright (C) 2026 The RetroArch team
 * SPDX-License-Identifier: GPL-3.0-or-later */

#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>
#include <file/config_file.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "platform_ps3_paths.h"
#include "../../defaults.h"
#include "../../file_path_special.h"

/* name: below the user directory. legacy: below USRDIR in the shared
 * layout, NULL where that layout left the directory unset. migrate:
 * copied to a user on their first boot. */
static const struct
{
   const char *name;
   const char *legacy;
   const char *setting;
   enum default_dirs dir;
   bool migrate;
} ps3_user_dirs[] = {
   { "config", "config", "rgui_config_directory", DEFAULT_DIR_MENU_CONFIG, true },
   { "remaps", "remaps", "input_remapping_directory", DEFAULT_DIR_REMAP, true },
   { "savefiles", "savefiles", "savefile_directory", DEFAULT_DIR_SRAM, true },
   { "savestates", "savestates", "savestate_directory", DEFAULT_DIR_SAVESTATE, true },
   { "screenshots", NULL, "screenshot_directory", DEFAULT_DIR_SCREENSHOT, false },
   { "playlists", "playlists", "playlist_directory", DEFAULT_DIR_PLAYLIST, true },
   { "", NULL, "content_favorites_directory", DEFAULT_DIR_CONTENT_FAVORITES, false },
   { "", "", "content_history_directory", DEFAULT_DIR_CONTENT_HISTORY, false },
   { "", NULL, "content_image_history_directory", DEFAULT_DIR_CONTENT_IMAGE_HISTORY, false },
   { "", NULL, "content_music_history_directory", DEFAULT_DIR_CONTENT_MUSIC_HISTORY, false },
   { "", NULL, "content_video_history_directory", DEFAULT_DIR_CONTENT_VIDEO_HISTORY, false },
   { "autoconfig", "autoconfig", "joypad_autoconfig_dir", DEFAULT_DIR_AUTOCONFIG, true },
   { "cache", NULL, "cache_directory", DEFAULT_DIR_CACHE, false },
   { "logs", "logs", "log_dir", DEFAULT_DIR_LOGS, false },
   { "records_config", NULL, "recording_config_directory", DEFAULT_DIR_RECORD_CONFIG, false },
   { "records", NULL, "recording_output_directory", DEFAULT_DIR_RECORD_OUTPUT, false }
};

/* Set to "" so they follow the directories above */
static const char *const ps3_user_files[] = {
   "content_history_path",
   "content_favorites_path",
   "content_image_history_path",
   "content_music_history_path",
   "content_video_history_path",
   "core_options_path",
   "runtime_log_directory"
};

/* Kept beside retroarch.cfg in the shared layout */
static const char *const ps3_legacy_root_files[] = {
   FILE_PATH_CORE_OPTIONS_CONFIG,
   FILE_PATH_CONTENT_HISTORY,
   FILE_PATH_CONTENT_FAVORITES,
   FILE_PATH_CONTENT_IMAGE_HISTORY,
   FILE_PATH_CONTENT_MUSIC_HISTORY,
   FILE_PATH_CONTENT_VIDEO_HISTORY,
   "retroarch-keychain.cfg",
   "retroarch-keychain.key"
};

#define PS3_ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define PS3_COPY_DEPTH 8

bool ps3_user_paths_init(unsigned user_id)
{
   char user_dir[128];
   size_t i;

   if (!user_id || user_id > 99999999)
      return false;

   snprintf(user_dir, sizeof(user_dir),
         "/dev_hdd0/home/%08u/retroarch", user_id);
   for (i = 0; i < PS3_ARRAY_SIZE(ps3_user_dirs); i++)
      fill_pathname_join(g_defaults.dirs[ps3_user_dirs[i].dir], user_dir,
            ps3_user_dirs[i].name, sizeof(g_defaults.dirs[0]));
   fill_pathname_join(g_defaults.path_config, user_dir,
         FILE_PATH_MAIN_CONFIG, sizeof(g_defaults.path_config));
   return true;
}

void ps3_legacy_paths_init(const char *root)
{
   size_t i;

   for (i = 0; i < PS3_ARRAY_SIZE(ps3_user_dirs); i++)
   {
      char *dir = g_defaults.dirs[ps3_user_dirs[i].dir];
      if (!ps3_user_dirs[i].legacy)
         *dir = '\0';
      else if (!*ps3_user_dirs[i].legacy)
         strlcpy(dir, root, sizeof(g_defaults.dirs[0]));
      else
         fill_pathname_join(dir, root, ps3_user_dirs[i].legacy,
               sizeof(g_defaults.dirs[0]));
   }
   fill_pathname_join(g_defaults.path_config, root,
         FILE_PATH_MAIN_CONFIG, sizeof(g_defaults.path_config));
}

/* Whether a path setting points at personal data: unset, or inside
 * root. With no root, every value does. */
static bool ps3_path_is_personal(const config_file_t *config,
      const char *key, const char *root)
{
   const struct config_entry_list *entry;
   size_t len;

   if (!root)
      return true;
   if (   !(entry = config_get_entry(config, key))
       || !entry->value || !*entry->value
       || string_is_equal(entry->value, "default"))
      return true;
   len = strlen(root);
   return !strncmp(entry->value, root, len)
      && (   entry->value[len] == '/' || entry->value[len] == '\\'
          || !entry->value[len]);
}

/* A copy of path with its includes flattened and its personal paths
 * moved to the user's directories */
static config_file_t *ps3_config_rebased(const char *path, const char *root)
{
   struct config_file_entry entry;
   config_file_t *source;
   config_file_t *config;
   size_t i;

   if (!(source = config_file_new_from_path_to_string(path)))
      return NULL;
   if (!(config = config_file_new_alloc()))
   {
      config_file_free(source);
      return NULL;
   }

   if (config_get_entry_list_head(source, &entry))
      do
      {
         if (   entry.key && entry.value
             && !config_get_entry(config, entry.key))
            config_set_string(config, entry.key, entry.value);
      } while (config_get_entry_list_next(&entry));
   config_file_free(source);

   for (i = 0; i < PS3_ARRAY_SIZE(ps3_user_dirs); i++)
      if (ps3_path_is_personal(config, ps3_user_dirs[i].setting, root))
         config_set_path(config, ps3_user_dirs[i].setting,
               g_defaults.dirs[ps3_user_dirs[i].dir]);
   for (i = 0; i < PS3_ARRAY_SIZE(ps3_user_files); i++)
      if (ps3_path_is_personal(config, ps3_user_files[i], root))
         config_set_path(config, ps3_user_files[i], "");
   return config;
}

bool ps3_config_import(const char *path, char *dest, size_t len)
{
   config_file_t *config;
   char user_dir[DIR_MAX_LENGTH];
   char base[32];
   char name[64];
   const char *s;
   uint32_t hash = 5381;
   bool ret      = false;

   fill_pathname_basedir(user_dir, g_defaults.path_config, sizeof(user_dir));
   if (   !*user_dir || !path || !*path
       || strlen(path) >= len
       || strlen(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]) + sizeof(name) >= len)
      return false;

   strlcpy(dest, path, len);
   if (   !strncmp(dest, user_dir, strlen(user_dir))
       && !strstr(dest, ".."))
      return true;

   /* One copy per source: imported again, it is used as it is now */
   for (s = path; *s; s++)
      hash = hash * 33 + (unsigned char)*s;
   fill_pathname_base(base, path, sizeof(base));
   path_remove_extension(base);
   snprintf(name, sizeof(name), "imported-%s-%08lx.cfg",
         base, (unsigned long)hash);
   fill_pathname_join(dest, g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG],
         name, len);
   if (path_is_valid(dest))
      return true;

   if (!(config = ps3_config_rebased(path, NULL)))
      return false;
   if (path_mkdir(g_defaults.dirs[DEFAULT_DIR_MENU_CONFIG]))
      ret = config_file_write(config, dest, true);
   config_file_free(config);
   return ret;
}

/* A file not already at dst, copied whole or not at all */
static bool ps3_copy_file(const char *src, const char *dst)
{
   void *buf   = NULL;
   int64_t len = 0;
   bool ret;

   if (path_is_valid(dst))
      return true;
   if (!filestream_read_file(src, &buf, &len))
      return false;
   ret = filestream_write_file_atomic(dst, buf, len);
   free(buf);
   return ret;
}

static bool ps3_copy_tree(const char *src, const char *dst, unsigned depth)
{
   struct string_list *list;
   char *path;
   size_t i;
   bool ret = true;

   if (!path_is_directory(src))
      return true;
   if (   depth > PS3_COPY_DEPTH
       || !path_mkdir(dst)
       || !(list = dir_list_new(src, NULL, true, true, false, false)))
      return false;
   if (!(path = (char*)malloc(PATH_MAX_LENGTH)))
   {
      string_list_free(list);
      return false;
   }

   for (i = 0; ret && i < list->size; i++)
   {
      const char *entry = list->elems[i].data;
      fill_pathname_join(path, dst, path_basename(entry), PATH_MAX_LENGTH);
      if (list->elems[i].attr.i == RARCH_DIRECTORY)
         ret = ps3_copy_tree(entry, path, depth + 1);
      else
         ret = ps3_copy_file(entry, path);
   }

   free(path);
   string_list_free(list);
   return ret;
}

bool ps3_user_data_migrate(const char *root)
{
   char user_dir[DIR_MAX_LENGTH];
   config_file_t *config;
   char *src;
   char *dst;
   size_t i;
   bool ret = false;

   if (!(src = (char*)malloc(2 * PATH_MAX_LENGTH)))
      return false;
   dst = src + PATH_MAX_LENGTH;

   /* The user's own configuration is written last, so a boot cut
    * short copies again rather than starting the user empty */
   fill_pathname_join(src, root, FILE_PATH_MAIN_CONFIG, PATH_MAX_LENGTH);
   if (path_is_valid(g_defaults.path_config) || !path_is_valid(src))
   {
      free(src);
      return true;
   }

   for (i = 0; i < PS3_ARRAY_SIZE(ps3_user_dirs); i++)
   {
      if (!ps3_user_dirs[i].migrate)
         continue;
      fill_pathname_join(src, root, ps3_user_dirs[i].legacy,
            PATH_MAX_LENGTH);
      if (!ps3_copy_tree(src, g_defaults.dirs[ps3_user_dirs[i].dir], 0))
         goto end;
   }

   fill_pathname_basedir(user_dir, g_defaults.path_config, sizeof(user_dir));
   for (i = 0; i < PS3_ARRAY_SIZE(ps3_legacy_root_files); i++)
   {
      fill_pathname_join(src, root, ps3_legacy_root_files[i],
            PATH_MAX_LENGTH);
      fill_pathname_join(dst, user_dir, ps3_legacy_root_files[i],
            PATH_MAX_LENGTH);
      if (path_is_valid(src) && !ps3_copy_file(src, dst))
         goto end;
   }

   fill_pathname_join(src, root, FILE_PATH_MAIN_CONFIG, PATH_MAX_LENGTH);
   if ((config = ps3_config_rebased(src, root)))
   {
      ret = config_file_write(config, g_defaults.path_config, true);
      config_file_free(config);
   }

end:
   free(src);
   return ret;
}

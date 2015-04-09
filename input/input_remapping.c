/*  RetroArch - A frontend for libretro.
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

#include "input_remapping.h"

#include <file/config_file.h>
#include <file/file_path.h>

#include "../general.h"

/**
 * input_remapping_load_file:
 * @path                     : Path to remapping file (absolute path).
 *
 * Loads a remap file from disk to memory.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_remapping_load_file(const char *path)
{
   unsigned i, j;
   config_file_t *conf  = config_file_new(path);
   settings_t *settings = config_get_ptr();

   if (!conf)
      return false;

   strlcpy(settings->input.remapping_path, path,
         sizeof(settings->input.remapping_path));

   for (i = 0; i < MAX_USERS; i++)
   {
      char buf[64];
      char key_ident[RARCH_FIRST_META_KEY][128];
      char key_strings[RARCH_FIRST_META_KEY][128] = { "b", "y", "select", "start",
         "up", "down", "left", "right", "a", "x", "l", "r", "l2", "r2", "l3", "r3"};

      snprintf(buf, sizeof(buf), "input_player%u", i + 1);

      for (j = 0; j < RARCH_FIRST_META_KEY; j++)
      {
         int key_remap = -1;

         snprintf(key_ident[j], sizeof(key_ident[j]), "%s_%s", buf, key_strings[j]);
         if (config_get_int(conf, key_ident[j], &key_remap))
            settings->input.remap_ids[i][j] = key_remap;
      }
   }

   config_file_free(conf);

   return true;
}

/**
 * input_remapping_save_file:
 * @path                     : Path to remapping file (relative path).
 *
 * Saves remapping values to file.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_remapping_save_file(const char *path)
{
   bool ret;
   unsigned i, j;
   char buf[PATH_MAX_LENGTH];
   char remap_file[PATH_MAX_LENGTH];
   config_file_t *conf = NULL;
   settings_t *settings = config_get_ptr();

   fill_pathname_join(buf, settings->input_remapping_directory,
         path, sizeof(buf));

   fill_pathname_noext(remap_file, buf, ".rmp", sizeof(remap_file));

   conf = config_file_new(remap_file);

   if (!conf)
   {
      conf = config_file_new(NULL);
      return false;
   }

   for (i = 0; i < settings->input.max_users; i++)
   {
      char key_ident[RARCH_FIRST_META_KEY][128];
      char key_strings[RARCH_FIRST_META_KEY][128] = { "b", "y", "select", "start",
         "up", "down", "left", "right", "a", "x", "l", "r", "l2", "r2", "l3", "r3"};

      snprintf(buf, sizeof(buf), "input_player%u", i + 1);

      for (j = 0; j < RARCH_FIRST_META_KEY; j++)
      {
         snprintf(key_ident[j], sizeof(key_ident[j]), "%s_%s", buf, key_strings[j]);
         config_set_int(conf, key_ident[j], settings->input.remap_ids[i][j]);
      }
   }

   ret = config_file_write(conf, remap_file);
   config_file_free(conf);

   return ret;
}

void input_remapping_set_defaults(void)
{
   unsigned i, j;
   settings_t *settings = config_get_ptr();

   for (i = 0; i < MAX_USERS; i++)
   {
      for (j = 0; j < RARCH_BIND_LIST_END; j++)
         settings->input.remap_ids[i][j] = settings->input.binds[i][j].id;
   }
}

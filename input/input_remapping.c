/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Andrés Suárez
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
#include <file/config_file.h>
#include <file/file_path.h>
#include <streams/file_stream.h>
#include <string/stdstring.h>

#include "input_remapping.h"
#include "../configuration.h"
#include "../retroarch.h"

static unsigned old_analog_dpad_mode[MAX_USERS];
static unsigned old_libretro_device[MAX_USERS];

/**
 * input_remapping_load_file:
 * @data                     : Path to config file.
 *
 * Loads a remap file from disk to memory.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool input_remapping_load_file(void *data, const char *path)
{
   unsigned i, j, k;
   config_file_t *conf  = (config_file_t*)data;
   settings_t *settings = config_get_ptr();
   global_t *global     = global_get_ptr();

   if (!conf ||  string_is_empty(path))
      return false;

   if (!string_is_empty(global->name.remapfile))
      input_remapping_set_defaults(true);
   global->name.remapfile = strdup(path);

   for (i = 0; i < MAX_USERS; i++)
   {
      char s1[64], s2[64], s3[64];
      char btn_ident[RARCH_FIRST_CUSTOM_BIND][128]       = {{0}};
      char key_ident[RARCH_FIRST_CUSTOM_BIND][128]       = {{0}};
      char stk_ident[8][192]                             = {{0}};

      char key_strings[RARCH_FIRST_CUSTOM_BIND + 8][128] = {
         "b", "y", "select", "start",
         "up", "down", "left", "right",
         "a", "x", "l", "r", "l2", "r2",
         "l3", "r3", "l_x+", "l_x-", "l_y+", "l_y-", "r_x+", "r_x-", "r_y+", "r_y-" };

      old_analog_dpad_mode[i] = settings->uints.input_analog_dpad_mode[i];
      old_libretro_device[i] = settings->uints.input_libretro_device[i];

      s1[0] = '\0';
      s2[0] = '\0';
      s3[0] = '\0';

      snprintf(s1, sizeof(s1), "input_player%u_btn", i + 1);
      snprintf(s2, sizeof(s2), "input_player%u_key", i + 1);
      snprintf(s3, sizeof(s3), "input_player%u_stk", i + 1);

      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND + 8; j++)
      {
         if (j < RARCH_FIRST_CUSTOM_BIND)
         {
            int btn_remap = -1;
            int key_remap = -1;

            fill_pathname_join_delim(btn_ident[j], s1,
                  key_strings[j], '_', sizeof(btn_ident[j]));
            fill_pathname_join_delim(key_ident[j], s2,
                  key_strings[j], '_', sizeof(btn_ident[j]));

            if (config_get_int(conf, btn_ident[j], &btn_remap)
                  && btn_remap != -1)
               settings->uints.input_remap_ids[i][j] = btn_remap;
            else if (config_get_int(conf, btn_ident[j], &btn_remap)
                  && btn_remap == -1)
               settings->uints.input_remap_ids[i][j] = RARCH_UNMAPPED;
            /* else do nothing, important */

            if (config_get_int(conf, key_ident[j], &key_remap))
               settings->uints.input_keymapper_ids[i][j] = key_remap;
            else
               settings->uints.input_keymapper_ids[i][j] = RETROK_UNKNOWN;
         }
         else
         {
            int stk_remap = -1;
            k = j - RARCH_FIRST_CUSTOM_BIND;

            fill_pathname_join_delim(stk_ident[k], s3,
               key_strings[j], '$', sizeof(stk_ident[k]));

            snprintf(stk_ident[k],
                  sizeof(stk_ident[k]),
                  "%s_%s",
                  s3,
                  key_strings[j]);

            if (config_get_int(conf, stk_ident[k], &stk_remap) && stk_remap != -1)
               settings->uints.input_remap_ids[i][j] = stk_remap;
            else if (config_get_int(conf, stk_ident[k], &stk_remap) && stk_remap == -1)
               settings->uints.input_remap_ids[i][j] = RARCH_UNMAPPED;
            /* else do nothing, important */
         }
      }

      snprintf(s1, sizeof(s1), "input_player%u_analog_dpad_mode", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, uints.input_analog_dpad_mode[i], s1);

      snprintf(s1, sizeof(s1), "input_libretro_device_p%u", i + 1);
      CONFIG_GET_INT_BASE(conf, settings, uints.input_libretro_device[i], s1);
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
   unsigned i, j, k;
   size_t path_size                  = PATH_MAX_LENGTH * sizeof(char);
   char *buf                         = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   char *remap_file                  = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   config_file_t               *conf = NULL;
   unsigned max_users                = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));
   settings_t              *settings = config_get_ptr();

   buf[0] = remap_file[0]            = '\0';

   fill_pathname_join(buf, settings->paths.directory_input_remapping,
         path, path_size);

   fill_pathname_noext(remap_file, buf, ".rmp", path_size);

   free(buf);

   if (!(conf = config_file_new_from_path_to_string(remap_file)))
   {
      if (!(conf = config_file_new_alloc()))
      {
         free(remap_file);
         return false;
      }
   }

   for (i = 0; i < max_users; i++)
   {
      char s1[64], s2[64], s3[64];
      char btn_ident[RARCH_FIRST_CUSTOM_BIND][128]       = {{0}};
      char key_ident[RARCH_FIRST_CUSTOM_BIND][128]       = {{0}};
      char stk_ident[8][128]                             = {{0}};

      char key_strings[RARCH_FIRST_CUSTOM_BIND + 8][128] = {
         "b", "y", "select", "start",
         "up", "down", "left", "right",
         "a", "x", "l", "r", "l2", "r2",
         "l3", "r3", "l_x+", "l_x-", "l_y+", "l_y-", "r_x+", "r_x-", "r_y+", "r_y-" };

      s1[0] = '\0';
      s2[0] = '\0';

      snprintf(s1, sizeof(s1), "input_player%u_btn", i + 1);
      snprintf(s2, sizeof(s2), "input_player%u_key", i + 1);
      snprintf(s3, sizeof(s1), "input_player%u_stk", i + 1);

      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND + 8; j++)
      {

         if(j < RARCH_FIRST_CUSTOM_BIND)
         {
            fill_pathname_join_delim(btn_ident[j], s1,
               key_strings[j], '_', sizeof(btn_ident[j]));
            fill_pathname_join_delim(key_ident[j], s2,
               key_strings[j], '_', sizeof(btn_ident[j]));

            /* only save values that have been modified */
            if(settings->uints.input_remap_ids[i][j] != j &&
               settings->uints.input_remap_ids[i][j] != RARCH_UNMAPPED)
               config_set_int(conf, btn_ident[j], settings->uints.input_remap_ids[i][j]);
            else if (settings->uints.input_remap_ids[i][j] != j &&
                     settings->uints.input_remap_ids[i][j] == RARCH_UNMAPPED)
               config_set_int(conf, btn_ident[j], -1);
            else
               config_unset(conf,btn_ident[j]);

            if (settings->uints.input_keymapper_ids[i][j] != RETROK_UNKNOWN)
               config_set_int(conf, key_ident[j],
                  settings->uints.input_keymapper_ids[i][j]);
         }
         else
         {
            k = j - RARCH_FIRST_CUSTOM_BIND;
            fill_pathname_join_delim(stk_ident[k], s3,
               key_strings[j], '_', sizeof(stk_ident[k]));
            if(settings->uints.input_remap_ids[i][j] != j &&
               settings->uints.input_remap_ids[i][j] != RARCH_UNMAPPED)
               config_set_int(conf, stk_ident[k],
                  settings->uints.input_remap_ids[i][j]);
            else if(settings->uints.input_remap_ids[i][j] != j &&
               settings->uints.input_remap_ids[i][j] == RARCH_UNMAPPED)
               config_set_int(conf, stk_ident[k],
                  -1);
            else
               config_unset(conf, stk_ident[k]);
         }
      }
      snprintf(s1, sizeof(s1), "input_libretro_device_p%u", i + 1);
      config_set_int(conf, s1, input_config_get_device(i));
      snprintf(s1, sizeof(s1), "input_player%u_analog_dpad_mode", i + 1);
      config_set_int(conf, s1, settings->uints.input_analog_dpad_mode[i]);
   }

   ret = config_file_write(conf, remap_file, true);
   config_file_free(conf);

   free(remap_file);
   return ret;
}

bool input_remapping_remove_file(const char *path)
{
   bool ret                = false;
   size_t path_size        = PATH_MAX_LENGTH * sizeof(char);
   char *buf               = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   char *remap_file        = (char*)malloc(PATH_MAX_LENGTH * sizeof(char));
   settings_t    *settings = config_get_ptr();

   buf[0] = remap_file[0]            = '\0';

   fill_pathname_join(buf, settings->paths.directory_input_remapping,
         path, path_size);

   fill_pathname_noext(remap_file, buf, ".rmp", path_size);

   ret = filestream_delete(remap_file) == 0 ? true : false;
   free(buf);
   free(remap_file);
   return ret;
}

void input_remapping_set_defaults(bool deinit)
{
   unsigned i, j;
   settings_t *settings = config_get_ptr();
   global_t *global = global_get_ptr();

   if (!global)
      return;

   if (deinit)
   {
      if (!string_is_empty(global->name.remapfile))
         free(global->name.remapfile);
      global->name.remapfile = NULL;
      rarch_ctl(RARCH_CTL_UNSET_REMAPS_CORE_ACTIVE, NULL);
      rarch_ctl(RARCH_CTL_UNSET_REMAPS_CONTENT_DIR_ACTIVE, NULL);
      rarch_ctl(RARCH_CTL_UNSET_REMAPS_GAME_ACTIVE, NULL);
   }

   for (i = 0; i < MAX_USERS; i++)
   {
      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND + 8; j++)
      {
         if (j < RARCH_FIRST_CUSTOM_BIND)
         {
            const struct  retro_keybind *keybind  = &input_config_binds[i][j];
            if (keybind)
               settings->uints.input_remap_ids[i][j] = keybind->id;
            settings->uints.input_keymapper_ids[i][j] = RETROK_UNKNOWN;
         }
         else
            settings->uints.input_remap_ids[i][j] = j;
      }

      if (old_analog_dpad_mode[i])
         settings->uints.input_analog_dpad_mode[i] = old_analog_dpad_mode[i];
      if (old_libretro_device[i])
         settings->uints.input_libretro_device[i]  = old_libretro_device[i];
   }
}

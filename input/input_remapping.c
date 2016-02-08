/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include <file/config_file.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include "input_remapping.h"

#include "../general.h"

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
   unsigned i, j;
   config_file_t *conf  = (config_file_t*)data;
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (!conf ||  string_is_empty(path))
      return false;

   strlcpy(settings->input.remapping_path, path,
         sizeof(settings->input.remapping_path));

   for (i = 0; i < MAX_USERS; i++)
   {
      char buf[64];
      char key_ident[RARCH_FIRST_CUSTOM_BIND + 4][128]   = {{0}};
      char key_strings[RARCH_FIRST_CUSTOM_BIND + 4][128] = 
      { "b", "y", "select", "start",
         "up", "down", "left", "right", 
         "a", "x", "l", "r", "l2", "r2", 
         "l3", "r3", "l_x", "l_y", "r_x", "r_y" };

      snprintf(buf, sizeof(buf), "input_player%u", i + 1);

      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND + 4; j++)
      {
         int key_remap = -1;

         fill_pathname_join_delim(key_ident[j], buf,
               key_strings[j], '_', sizeof(key_ident[j]));
         if (config_get_int(conf, key_ident[j], &key_remap)
               && key_remap < RARCH_FIRST_CUSTOM_BIND)
            settings->input.remap_ids[i][j] = key_remap;
      }

      for (j = 0; j < 4; j++)
      {
         int key_remap = -1;

         snprintf(key_ident[RARCH_FIRST_CUSTOM_BIND + j],
               sizeof(key_ident[RARCH_FIRST_CUSTOM_BIND + j]),
               "%s_%s",
               buf,
               key_strings[RARCH_FIRST_CUSTOM_BIND + j]);

         if (config_get_int(conf, key_ident[RARCH_FIRST_CUSTOM_BIND + j],
                  &key_remap) && (key_remap < 4))
            settings->input.remap_ids[i][RARCH_FIRST_CUSTOM_BIND + j] = 
               key_remap;
      }

      for (i = 0; i < MAX_USERS; i++)
      {
         char buf[64] = {0};
         snprintf(buf, sizeof(buf), "input_player%u_joypad_index", i + 1);
         CONFIG_GET_INT_BASE(conf, settings, input.joypad_map[i], buf);

         snprintf(buf, sizeof(buf), "input_player%u_analog_dpad_mode", i + 1);
         CONFIG_GET_INT_BASE(conf, settings, input.analog_dpad_mode[i], buf);

         if (!global->has_set.libretro_device[i])
         {
            snprintf(buf, sizeof(buf), "input_libretro_device_p%u", i + 1);
            CONFIG_GET_INT_BASE(conf, settings, input.libretro_device[i], buf);
         }

         snprintf(buf, sizeof(buf), "input_player%u_joypad_index", i + 1);
         CONFIG_GET_INT_BASE(conf, settings, input.joypad_map[i], buf);

         snprintf(buf, sizeof(buf), "input_player%u_analog_dpad_mode", i + 1);
         CONFIG_GET_INT_BASE(conf, settings, input.analog_dpad_mode[i], buf);

         if (!global->has_set.libretro_device[i])
         {
            snprintf(buf, sizeof(buf), "input_libretro_device_p%u", i + 1);
            CONFIG_GET_INT_BASE(conf, settings, input.libretro_device[i], buf);
         }
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
   char buf[PATH_MAX_LENGTH]         = {0};
   char remap_file[PATH_MAX_LENGTH]  = {0};
   config_file_t               *conf = NULL;
   settings_t              *settings = config_get_ptr();

   fill_pathname_join(buf, settings->input_remapping_directory,
         path, sizeof(buf));

   fill_pathname_noext(remap_file, buf, ".rmp", sizeof(remap_file));

   conf = config_file_new(remap_file);

   if (!conf)
   {
      conf = config_file_new(NULL);
      if (!conf)
         return false;
   }

   for (i = 0; i < settings->input.max_users; i++)
   {
      char key_ident[RARCH_FIRST_CUSTOM_BIND + 4][128]   = {{0}};
      char key_strings[RARCH_FIRST_CUSTOM_BIND + 4][128] = { 
         "b", "y", "select", "start",
         "up", "down", "left", "right",
         "a", "x", "l", "r", "l2", "r2",
         "l3", "r3", "l_x", "l_y", "r_x", "r_y" };

      snprintf(buf, sizeof(buf), "input_player%u", i + 1);

      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND + 4; j++)
      {
         fill_pathname_join_delim(key_ident[j], buf,
               key_strings[j], '_', sizeof(key_ident[j]));
         config_set_int(conf, key_ident[j], settings->input.remap_ids[i][j]);
      }

      for (i = 0; i < MAX_USERS; i++)
      {
         char cfg[64] = {0};

         snprintf(cfg, sizeof(cfg), "input_libretro_device_p%u", i + 1);
         config_set_int(conf, cfg, settings->input.libretro_device[i]);
         snprintf(cfg, sizeof(cfg), "input_player%u_analog_dpad_mode", i + 1);
         config_set_int(conf, cfg, settings->input.analog_dpad_mode[i]);
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
      for (j = 0; j < RARCH_FIRST_CUSTOM_BIND; j++)
         settings->input.remap_ids[i][j] = settings->input.binds[i][j].id;
      for (j = 0; j < 4; j++)
         settings->input.remap_ids[i][RARCH_FIRST_CUSTOM_BIND + j] = j;
   }
}

void input_remapping_state(unsigned port,
      unsigned *device, unsigned *idx, unsigned *id)
{
   settings_t *settings = config_get_ptr();

   switch (*device)
   {
      case RETRO_DEVICE_JOYPAD:
         if (*id < RARCH_FIRST_CUSTOM_BIND)
            *id = settings->input.remap_ids[port][*id];
         break;
      case RETRO_DEVICE_ANALOG:
         if (*idx < 2 && *id < 2)
         {
            unsigned new_id = RARCH_FIRST_CUSTOM_BIND + (*idx * 2 + *id);

            new_id = settings->input.remap_ids[port][new_id];
            *idx   = (new_id & 2) >> 1;
            *id    = new_id & 1;
         }
         break;
   }
}

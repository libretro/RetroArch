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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <file/dir_list.h>
#include <file/file_path.h>

#include "input_common.h"
#include "input_autodetect.h"

#include "../general.h"
#include "../verbosity.h"

static bool remote_is_bound = false;

/* adds an index for devices with the same name so they can be identified in the GUI*/
static void input_reindex_devices()
{
   unsigned i;
   settings_t      *settings = config_get_ptr();

   for(i=0; i < settings->input.max_users; i++)
      settings->input.device_name_index[i]=0;

   for(i=0; i < settings->input.max_users; i++)
   {
      unsigned j;
      const char *tmp = settings->input.device_names[i];
      int k           = 1;

      for(j = 0; j < settings->input.max_users; j++)
      {
         if(!strcmp(tmp,settings->input.device_names[j]) && settings->input.device_name_index[i]==0)
            settings->input.device_name_index[j]=k++;
      }
   }
}

static void input_autoconfigure_joypad_conf(config_file_t *conf,
      struct retro_keybind *binds)
{
   unsigned i;

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      input_config_parse_joy_button(conf, "input",
            input_config_bind_map[i].base, &binds[i]);
      input_config_parse_joy_axis(conf, "input",
            input_config_bind_map[i].base, &binds[i]);
   }
}

static int input_try_autoconfigure_joypad_from_conf(config_file_t *conf,
      autoconfig_params_t *params)
{
   char ident[PATH_MAX_LENGTH]        = {0};
   char input_driver[PATH_MAX_LENGTH] = {0};
   int                      input_vid = 0;
   int                      input_pid = 0;
   int                          score = 0;

   if (!conf)
      return false;

   *ident = *input_driver = '\0';

   config_get_array(conf, "input_device", ident, sizeof(ident));
   config_get_array(conf, "input_driver", input_driver, sizeof(input_driver));
   config_get_int  (conf, "input_vendor_id", &input_vid);
   config_get_int  (conf, "input_product_id", &input_pid);

   /* Check for VID/PID */
   if (     (params->vid == input_vid)
         && (params->pid == input_pid)
         && params->vid != 0
         && params->pid != 0
         && input_vid   != 0
         && input_pid   != 0)
   {
      score += 3;
#if 0
      RARCH_LOG("Autodetect: VID/PID match score=%d\n", score);
#endif
   }

   /* Check for name match */
   if (!strcmp(ident, params->name))
   {
      score += 2;
#if 0
      RARCH_LOG("Autodetect: exact name match score=%d\n", score);
#endif
   }
   else
   {
      if (ident[0] != '\0' && !strncmp(params->name, ident, strlen(ident)))
      {
         score += 1;
#if 0
         RARCH_LOG("Autodetect: partial name match score=%d\n", score);
#endif
      }
   }
#if 0
   RARCH_LOG("Autodetect: configuration file: %s score: %d\n", conf->path, score);
#endif
   return score;
}

static void input_autoconfigure_joypad_add(
      config_file_t *conf,
      autoconfig_params_t *params)
{
   bool block_osd_spam;
   char msg[PATH_MAX_LENGTH] = {0};
   char display_name[PATH_MAX_LENGTH] = {0};
   char device_type[PATH_MAX_LENGTH] = {0};
   settings_t      *settings = config_get_ptr();

   config_get_array(conf, "input_device_display_name", display_name, sizeof(display_name));
   config_get_array(conf, "input_device_type", device_type, sizeof(device_type));

   if (!settings)
      return;

   /* This will be the case if input driver is reinitialized.
    * No reason to spam autoconfigure messages every time. */
   block_osd_spam = settings->input.autoconfigured[params->idx]
      && *params->name;

   settings->input.autoconfigured[params->idx] = true;
   input_autoconfigure_joypad_conf(conf,
         settings->input.autoconf_binds[params->idx]);

   if (!strcmp(device_type,"remote"))
   {
      if (display_name[0] != '\0' || strcmp(display_name, ""))
         snprintf(msg, sizeof(msg), "%s configured",
            display_name);
      else
         snprintf(msg, sizeof(msg), "%s configured",
            params->name);

      if(!remote_is_bound)
         rarch_main_msg_queue_push(msg, 0, 60, false);
      remote_is_bound = true;
   }
   else
   {
      if (display_name[0] != '\0' || strcmp(display_name, ""))
         snprintf(msg, sizeof(msg), "%s configured in port #%u.",
               display_name, params->idx);
      else
         snprintf(msg, sizeof(msg), "%s configured in port #%u.",
               params->name, params->idx);
      if (!block_osd_spam)
          rarch_main_msg_queue_push(msg, 0, 60, false);
   }
   input_reindex_devices();
#if 0
   RARCH_LOG("Autodetect: %s\n", msg);
#endif
}

#if defined(HAVE_BUILTIN_AUTOCONFIG)
static int input_autoconfigure_joypad_from_conf(
      config_file_t *conf, autoconfig_params_t *params)
{
   int ret = 0;

   if (!conf)
      return false;

   ret = input_try_autoconfigure_joypad_from_conf(conf,
         params);

   if (ret)
      input_autoconfigure_joypad_add(conf, params);

   config_file_free(conf);

   return ret;
}
#endif

static bool input_autoconfigure_joypad_from_conf_dir(
      autoconfig_params_t *params)
{
   size_t i;
   char path[PATH_MAX_LENGTH];
   int ret                    = 0;
   int index                  = -1;
   int current_best           = 0;
   config_file_t *conf        = NULL;
   struct string_list *list   = NULL;
   settings_t *settings       = config_get_ptr();

   if (!settings)
      return false;

   fill_pathname_join(path,
         settings->input.autoconfig_dir,
         settings->input.joypad_driver,
         sizeof(path));
   list = dir_list_new(path, "cfg", false, false);

   if (!list || !list->size)
      list = dir_list_new(settings->input.autoconfig_dir, "cfg", false, false);

   if(!list)
      return false;

   RARCH_LOG("Autodetect: %d profiles found\n", list->size);

   for (i = 0; i < list->size; i++)
   {
      conf = config_file_new(list->elems[i].data);
      ret = input_try_autoconfigure_joypad_from_conf(conf, params);
      if(ret >= current_best)
      {
         index = i;
         current_best = ret;
      }
      config_file_free(conf);
   }

   if(index >= 0 && current_best > 0)
   {
      conf = config_file_new(list->elems[index].data);

      if (conf)
      {
         char conf_path[PATH_MAX_LENGTH];

         config_get_config_path(conf, conf_path, sizeof(conf_path));

         RARCH_LOG("Autodetect: selected configuration: %s\n", conf_path);
         input_autoconfigure_joypad_add(conf, params);
         config_file_free(conf);
         ret = 1;
      }
   }
   else
      ret = 0;

   string_list_free(list);

   if (ret == 0)
      return false;
   return true;
}

#if defined(HAVE_BUILTIN_AUTOCONFIG)
static bool input_autoconfigure_joypad_from_conf_internal(
      autoconfig_params_t *params)
{
   size_t i;
   settings_t *settings = config_get_ptr();
   bool             ret = false;

   /* Load internal autoconfig files  */
   for (i = 0; input_builtin_autoconfs[i]; i++)
   {
      config_file_t *conf = config_file_new_from_string(
            input_builtin_autoconfs[i]);

      if ((ret = input_autoconfigure_joypad_from_conf(conf, params)))
         break;
   }

   if (ret || !*settings->input.autoconfig_dir)
      return true;
   return false;
}
#endif

static bool input_config_autoconfigure_joypad_init(autoconfig_params_t *params)
{
   size_t i;
   settings_t *settings = config_get_ptr();

   if (!settings || !settings->input.autodetect_enable)
      return false;

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      settings->input.autoconf_binds[params->idx][i].joykey = NO_BTN;
      settings->input.autoconf_binds[params->idx][i].joyaxis = AXIS_NONE;
      settings->input.autoconf_binds[params->idx][i].joykey_label[0] = '\0';
      settings->input.autoconf_binds[params->idx][i].joyaxis_label[0] = '\0';
   }
   settings->input.autoconfigured[params->idx] = false;

   return true;
}

bool input_config_autoconfigure_joypad(autoconfig_params_t *params)
{
   bool ret = false;

   if (!input_config_autoconfigure_joypad_init(params))
      return ret;

   if (!*params->name)
      return ret;

   if (!ret)
      ret = input_autoconfigure_joypad_from_conf_dir(params);

#if defined(HAVE_BUILTIN_AUTOCONFIG)
   if (!ret)
      ret = input_autoconfigure_joypad_from_conf_internal(params);
#endif
   if (!ret)
   {
      char msg[PATH_MAX_LENGTH];

      RARCH_LOG("Autodetect: no profiles found for %s (%d/%d)", params->name, params->vid, params->pid);
      snprintf(msg, sizeof(msg), "%s (%ld/%ld) not configured", params->name, (long)params->vid, (long)params->pid);
      rarch_main_msg_queue_push(msg, 0, 60, false);
   }
   return ret;
}

const struct retro_keybind *input_get_auto_bind(unsigned port, unsigned id)
{
   settings_t *settings = config_get_ptr();
   unsigned joy_idx     = 0;

   if (settings)
      joy_idx = settings->input.joypad_map[port];

   if (joy_idx < MAX_USERS)
      return &settings->input.autoconf_binds[joy_idx][id];
   return NULL;
}

void input_config_autoconfigure_disconnect(unsigned i, const char *ident)
{
   char msg[PATH_MAX_LENGTH] = {0};

   snprintf(msg, sizeof(msg), "Device #%u (%s) disconnected.", i, ident);
   rarch_main_msg_queue_push(msg, 0, 60, false);
   RARCH_LOG("Autodetect: %s\n", msg);
}

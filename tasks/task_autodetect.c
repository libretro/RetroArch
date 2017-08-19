/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <compat/strl.h>
#include <lists/dir_list.h>
#include <file/file_path.h>
#include <file/config_file.h>
#include <string/stdstring.h>

#include "../input/input_config.h"

#include "../configuration.h"
#include "../file_path_special.h"
#include "../list_special.h"
#include "../verbosity.h"

#include "tasks_internal.h"

typedef struct autoconfig_disconnect autoconfig_disconnect_t;
typedef struct autoconfig_params     autoconfig_params_t;

struct autoconfig_disconnect
{
   unsigned idx;
   char msg[255];
};

struct autoconfig_params
{
   char  name[255];
   char  driver[255];
   char  display_name[255];
   char autoconfig_directory[4096];
   int32_t vid;
   int32_t pid;
   unsigned idx;
   uint32_t max_users;
};

static bool input_autoconfigured[MAX_USERS];
static unsigned input_device_name_index[MAX_USERS];
static bool input_autoconfigure_swap_override;

bool input_autoconfigure_get_swap_override(void)
{
   return input_autoconfigure_swap_override;
}

/* Adds an index for devices with the same name,
 * so they can be identified in the GUI. */
static void input_autoconfigure_joypad_reindex_devices(autoconfig_params_t *params)
{
   unsigned i;

   for(i = 0; i < params->max_users; i++)
      input_device_name_index[i] = 0;

   for(i = 0; i < params->max_users; i++)
   {
      unsigned j;
      const char *tmp = input_config_get_device_name(i);
      int k           = 1;

      for(j = 0; j < params->max_users; j++)
      {
         if(string_is_equal(tmp, input_config_get_device_name(j))
               && input_device_name_index[i] == 0)
            input_device_name_index[j] = k++;
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
            input_config_bind_map_get_base(i), &binds[i]);
      input_config_parse_joy_axis(conf, "input",
            input_config_bind_map_get_base(i), &binds[i]);
   }
}

static int input_autoconfigure_joypad_try_from_conf(config_file_t *conf,
      autoconfig_params_t *params)
{
   char ident[256];
   char input_driver[32];
   int tmp_int                = 0;
   int              input_vid = 0;
   int              input_pid = 0;
   int                  score = 0;

   ident[0] = input_driver[0] = '\0';

   config_get_array(conf, "input_device", ident, sizeof(ident));
   config_get_array(conf, "input_driver", input_driver, sizeof(input_driver));

   if (config_get_int  (conf, "input_vendor_id", &tmp_int))
      input_vid = tmp_int;

   if (config_get_int  (conf, "input_product_id", &tmp_int))
      input_pid = tmp_int;

   /* Check for VID/PID */
   if (     (params->vid == input_vid)
         && (params->pid == input_pid)
         && (params->vid != 0)
         && (params->pid != 0)
         && (input_vid   != 0)
         && (input_pid   != 0))
      score += 3;

   /* Check for name match */
   if (string_is_equal(ident, params->name))
      score += 2;

   return score;
}

static void input_autoconfigure_joypad_add(config_file_t *conf,
      autoconfig_params_t *params, retro_task_t *task)
{
   char msg[128], display_name[128], device_type[128];
   /* This will be the case if input driver is reinitialized.
    * No reason to spam autoconfigure messages every time. */
   bool block_osd_spam                = 
      input_autoconfigured[params->idx]
      && !string_is_empty(params->name);

   msg[0] = display_name[0] = device_type[0] = '\0';

   config_get_array(conf, "input_device_display_name",
         display_name, sizeof(display_name));
   config_get_array(conf, "input_device_type", device_type,
         sizeof(device_type));

   input_autoconfigured[params->idx] = true;

   input_autoconfigure_joypad_conf(conf,
         input_autoconf_binds[params->idx]);

   if (string_is_equal_fast(device_type, "remote", 6))
   {
      static bool remote_is_bound        = false;

      snprintf(msg, sizeof(msg), "%s configured.",
            string_is_empty(display_name) ? params->name : display_name);

      if(!remote_is_bound)
      {
         task_free_title(task);
         task_set_title(task, strdup(msg));
      }
      remote_is_bound = true;
      if (params->idx == 0)
         input_autoconfigure_swap_override = true;
   }
   else
   {
      bool tmp = false;
      snprintf(msg, sizeof(msg), "%s %s #%u.",
            string_is_empty(display_name) ? params->name : display_name,
            msg_hash_to_str(MSG_DEVICE_CONFIGURED_IN_PORT),
            params->idx);

      /* allow overriding the swap menu controls for player 1*/
      if (params->idx == 0)
      {
         if (config_get_bool(conf, "input_swap_override", &tmp))
            input_autoconfigure_swap_override = tmp;
         else
            input_autoconfigure_swap_override = false;
      }

      if (!block_osd_spam)
      {
         task_free_title(task);
         task_set_title(task, strdup(msg));
      }
   }


   input_autoconfigure_joypad_reindex_devices(params);
}

static int input_autoconfigure_joypad_from_conf(
      config_file_t *conf, autoconfig_params_t *params, retro_task_t *task)
{
   int ret = input_autoconfigure_joypad_try_from_conf(conf,
         params);

   if (ret)
      input_autoconfigure_joypad_add(conf, params, task);

   config_file_free(conf);

   return ret;
}

static bool input_autoconfigure_joypad_from_conf_dir(
      autoconfig_params_t *params, retro_task_t *task)
{
   size_t i;
   char path[PATH_MAX_LENGTH];
   int ret                    = 0;
   int index                  = -1;
   int current_best           = 0;
   config_file_t *conf        = NULL;
   struct string_list *list   = NULL;

   path[0]                    = '\0';

   fill_pathname_application_special(path, sizeof(path),
         APPLICATION_SPECIAL_DIRECTORY_AUTOCONFIG);

   list = dir_list_new_special(path, DIR_LIST_AUTOCONFIG, "cfg");

   if (!list || !list->size)
   {
      if (list)
         string_list_free(list);
      list = dir_list_new_special(params->autoconfig_directory,
            DIR_LIST_AUTOCONFIG, "cfg");
   }

   if(!list)
      return false;

   RARCH_LOG("[Autoconf]: %d profiles found.\n", list->size);

   for (i = 0; i < list->size; i++)
   {
      conf = config_file_new(list->elems[i].data);

      if (conf)
         ret  = input_autoconfigure_joypad_try_from_conf(conf, params);

      if(ret >= current_best)
      {
         index        = (int)i;
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

         conf_path[0] = '\0';

         config_get_config_path(conf, conf_path, sizeof(conf_path));

         RARCH_LOG("[Autoconf]: selected configuration: %s\n", conf_path);
         input_autoconfigure_joypad_add(conf, params, task);
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

static bool input_autoconfigure_joypad_from_conf_internal(
      autoconfig_params_t *params, retro_task_t *task)
{
   size_t i;

   /* Load internal autoconfig files  */
   for (i = 0; input_builtin_autoconfs[i]; i++)
   {
      config_file_t *conf = config_file_new_from_string(
            input_builtin_autoconfs[i]);
      if (conf && input_autoconfigure_joypad_from_conf(conf, params, task))
         return true;
   }

   if (string_is_empty(params->autoconfig_directory))
      return true;
   return false;
}

static void input_autoconfigure_connect_handler(retro_task_t *task)
{
   autoconfig_params_t *params = (autoconfig_params_t*)task->state;

   if (!params || string_is_empty(params->name))
      goto end;

   if (     !input_autoconfigure_joypad_from_conf_dir(params, task)
         && !input_autoconfigure_joypad_from_conf_internal(params, task))
   {
      char msg[255];

      msg[0] = '\0';
#ifdef ANDROID
      strlcpy(params->name, "Android Gamepad", sizeof(params->name));
      if(input_autoconfigure_joypad_from_conf_internal(params, task))
      {
         RARCH_LOG("[Autoconf]: no profiles found for %s (%d/%d). Using fallback\n",
               params->name, params->vid, params->pid);

         snprintf(msg, sizeof(msg), "%s (%ld/%ld) %s.",
               params->name, (long)params->vid, (long)params->pid,
               msg_hash_to_str(MSG_DEVICE_NOT_CONFIGURED_FALLBACK));
      }
#else
      RARCH_LOG("[Autoconf]: no profiles found for %s (%d/%d).\n",
            params->name, params->vid, params->pid);

      snprintf(msg, sizeof(msg), "%s (%ld/%ld) %s.",
            params->name, (long)params->vid, (long)params->pid,
            msg_hash_to_str(MSG_DEVICE_NOT_CONFIGURED));
#endif
      task_free_title(task);
      task_set_title(task, strdup(msg));
   }

end:
   free(params);
   task_set_finished(task, true);
}

static void input_autoconfigure_disconnect_handler(retro_task_t *task)
{
   autoconfig_disconnect_t *params = (autoconfig_disconnect_t*)task->state;

   task_set_title(task, strdup(params->msg));

   task_set_finished(task, true);

   RARCH_LOG("%s: %s\n", msg_hash_to_str(MSG_AUTODETECT), params->msg);

   free(params);
}

bool input_autoconfigure_disconnect(unsigned i, const char *ident)
{
   char msg[255];
   retro_task_t         *task      = (retro_task_t*)calloc(1, sizeof(*task));
   autoconfig_disconnect_t *state  = (autoconfig_disconnect_t*)calloc(1, sizeof(*state));

   if (!state || !task)
      goto error;

   msg[0]      = '\0';

   state->idx  = i;

   snprintf(msg, sizeof(msg), "%s #%u (%s).", 
         msg_hash_to_str(MSG_DEVICE_DISCONNECTED_FROM_PORT),
         i, ident);

   strlcpy(state->msg, msg, sizeof(state->msg));

   input_config_clear_device_name(state->idx);

   task->state   = state;
   task->handler = input_autoconfigure_disconnect_handler;

   task_queue_push(task);

   return true;

error:
   if (state)
      free(state);
   if (task)
      free(task);

   return false;
}

void input_autoconfigure_reset(void)
{
   unsigned i, j;

   for (i = 0; i < MAX_USERS; i++)
   {
      for (j = 0; j < RARCH_BIND_LIST_END; j++)
      {
         input_autoconf_binds[i][j].joykey  = NO_BTN;
         input_autoconf_binds[i][j].joyaxis = AXIS_NONE;
      }
      input_device_name_index[i] = 0;
      input_autoconfigured[i]    = 0;
   }

   input_autoconfigure_swap_override = false;
}

bool input_is_autoconfigured(unsigned i)
{
   return input_autoconfigured[i];
}

unsigned input_autoconfigure_get_device_name_index(unsigned i)
{
   return input_device_name_index[i];
}

bool input_autoconfigure_connect(
      const char *name,
      const char *display_name,
      const char *driver,
      unsigned idx,
      unsigned vid,
      unsigned pid)
{
   unsigned i;
   retro_task_t         *task = (retro_task_t*)calloc(1, sizeof(*task));
   autoconfig_params_t *state = (autoconfig_params_t*)calloc(1, sizeof(*state));
   settings_t       *settings = config_get_ptr();
   const char *dir_autoconf   = settings->paths.directory_autoconfig;

   if (!task || !state || !settings->bools.input_autodetect_enable)
      goto error;

   if (!string_is_empty(display_name))
      strlcpy(state->display_name, display_name,
            sizeof(state->display_name));

   if (!string_is_empty(name))
      strlcpy(state->name, name, sizeof(state->name));

   if (!string_is_empty(driver))
      strlcpy(state->driver, driver, sizeof(state->driver));

   if (!string_is_empty(dir_autoconf))
      strlcpy(state->autoconfig_directory,
            dir_autoconf,
            sizeof(state->autoconfig_directory));

   state->idx       = idx;
   state->vid       = vid;
   state->pid       = pid;
   state->max_users = *(input_driver_get_uint(INPUT_ACTION_MAX_USERS));

   input_config_set_device_name(state->idx, state->name);
   input_config_set_pid(state->idx, state->pid);
   input_config_set_vid(state->idx, state->vid);

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      input_autoconf_binds[state->idx][i].joykey           = NO_BTN;
      input_autoconf_binds[state->idx][i].joyaxis          = AXIS_NONE;
      input_autoconf_binds[state->idx][i].joykey_label[0]  = '\0';
      input_autoconf_binds[state->idx][i].joyaxis_label[0] = '\0';
   }

   input_autoconfigured[state->idx] = false;

   task->state   = state;
   task->handler = input_autoconfigure_connect_handler;

   task_queue_push(task);

   return true;

error:
   if (state)
      free(state);
   if (task)
      free(task);

   return false;
}

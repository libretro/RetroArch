/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2016-2019 - Brad Parker
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

#include <string.h>
#include <stdlib.h>
#include <ctype.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include "../configuration.h"
#include "../file_path_special.h"
#include "../list_special.h"
#include "../retroarch.h"

#include "tasks_internal.h"
#ifdef HAVE_BLISSBOX
#include "../input/include/blissbox.h"
#endif

typedef struct autoconfig_disconnect autoconfig_disconnect_t;

struct autoconfig_disconnect
{
   unsigned idx;
   char *msg;
};

static bool input_autoconfigured[MAX_USERS];
static unsigned input_device_name_index[MAX_INPUT_DEVICES];
static bool input_autoconfigure_swap_override;

/* TODO/FIXME - Not thread safe to access this
 * on main thread as well in its current state -
 * menu_input.c - menu_event calls this function
 * right now, while the underlying variable can
 * be modified by a task thread. */
bool input_autoconfigure_get_swap_override(void)
{
   return input_autoconfigure_swap_override;
}

/* Adds an index for devices with the same name,
 * so they can be identified in the GUI. */
void input_autoconfigure_joypad_reindex_devices(void)
{
   unsigned i, j, k;

   for(i = 0; i < MAX_INPUT_DEVICES; i++)
      input_device_name_index[i] = 0;

   for(i = 0; i < MAX_INPUT_DEVICES; i++)
   {
      const char *tmp = input_config_get_device_name(i);
      if ( !tmp || input_device_name_index[i] )
         continue;

      k = 2; /*Additional devices start at two*/

      for(j = i+1; j < MAX_INPUT_DEVICES; j++)
      {
         const char *other = input_config_get_device_name(j);

         if (!other)
            continue;

         /*another device with the same name found, for the first time*/
         if (string_is_equal(tmp, other) &&
               input_device_name_index[j]==0 )
         {
            /*Mark the first device of the set*/
            input_device_name_index[i] = 1;
            /*count this additional device, from two up*/
            input_device_name_index[j] = k++;
         }
      }
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
   bool check_pid             = false;

   ident[0] = input_driver[0] = '\0';

   config_get_array(conf, "input_device", ident, sizeof(ident));
   config_get_array(conf, "input_driver", input_driver, sizeof(input_driver));

   if (config_get_int  (conf, "input_vendor_id", &tmp_int))
      input_vid = tmp_int;

   if (config_get_int  (conf, "input_product_id", &tmp_int))
      input_pid = tmp_int;

#ifdef HAVE_BLISSBOX
   if (params->vid == BLISSBOX_VID)
      input_pid = BLISSBOX_PID;
#endif

   check_pid = 
         (params->vid == input_vid)
      && (params->pid == input_pid)
      && (params->vid != 0)
      && (params->pid != 0);

#ifdef HAVE_BLISSBOX
   check_pid = check_pid
         && (params->vid != BLISSBOX_VID)
         && (params->pid != BLISSBOX_PID);
#endif

   /* Check for VID/PID */
   if (check_pid)
      score += 3;

   /* Check for name match */
   if (
            !string_is_empty(params->name)
         && !string_is_empty(ident)
         && string_is_equal(ident, params->name))
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
#if defined(HAVE_LIBNX) && defined(HAVE_GFX_WIDGETS)
      true;
#else
      input_autoconfigured[params->idx]
      && !string_is_empty(params->name);
#endif

   msg[0] = display_name[0] = device_type[0] = '\0';

   config_get_array(conf, "input_device_display_name",
         display_name, sizeof(display_name));
   config_get_array(conf, "input_device_type", device_type,
         sizeof(device_type));

   input_autoconfigured[params->idx] = true;

   input_autoconfigure_joypad_conf(conf,
         input_autoconf_binds[params->idx]);

   if (string_is_equal(device_type, "remote"))
   {
      static bool remote_is_bound        = false;
      const char *autoconfig_str         = (string_is_empty(display_name) &&
            !string_is_empty(params->name)) ? params->name : (!string_is_empty(display_name) ? display_name : "N/A");
      strlcpy(msg, autoconfig_str, sizeof(msg));
      strlcat(msg, " configured.", sizeof(msg));

      if (!remote_is_bound)
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
      bool tmp                    = false;
      const char *autoconfig_str  = (string_is_empty(display_name) &&
            !string_is_empty(params->name))
            ? params->name : (!string_is_empty(display_name) ? display_name : "N/A");

      snprintf(msg, sizeof(msg), "%s %s #%u.",
            autoconfig_str,
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
   if (!string_is_empty(display_name))
      input_config_set_device_display_name(params->idx, display_name);
   else
      input_config_set_device_display_name(params->idx, params->name);
   if (!string_is_empty(conf->path))
   {
      input_config_set_device_config_name(params->idx, path_basename(conf->path));
      input_config_set_device_config_path(params->idx, conf->path);
   }
   else
   {
      input_config_set_device_config_name(params->idx, "N/A");
      input_config_set_device_config_path(params->idx, "N/A");
   }

   input_autoconfigure_joypad_reindex_devices();
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
   char best_path[PATH_MAX_LENGTH];
   int ret                    = 0;
   int index                  = -1;
   int current_best           = 0;
   config_file_t *best_conf   = NULL;
   struct string_list *list   = NULL;

   best_path[0]               = '\0';
   path[0]                    = '\0';

   fill_pathname_application_special(path, sizeof(path),
         APPLICATION_SPECIAL_DIRECTORY_AUTOCONFIG);

   list = dir_list_new_special(path, DIR_LIST_AUTOCONFIG, "cfg",
         params->show_hidden_files);

   if (!list || !list->size)
   {
      if (list)
      {
         string_list_free(list);
         list = NULL;
      }
      if (!string_is_empty(params->autoconfig_directory))
         list = dir_list_new_special(params->autoconfig_directory,
               DIR_LIST_AUTOCONFIG, "cfg", params->show_hidden_files);
   }

   if (!list)
      return false;

   for (i = 0; i < list->size; i++)
   {
      int res;
      config_file_t *conf = config_file_new_from_path_to_string(list->elems[i].data);
      
      if (!conf)
         continue;

      res  = input_autoconfigure_joypad_try_from_conf(conf, params);

      if (res >= current_best)
      {
         index        = (int)i;
         current_best = res;
         if (best_conf)
            config_file_free(best_conf);
         strlcpy(best_path, list->elems[i].data, sizeof(best_path));
         best_conf    = NULL;
         best_conf    = conf;
      }
      else
         config_file_free(conf);
   }

   if (index >= 0 && current_best > 0 && best_conf)
   {
      input_autoconfigure_joypad_add(best_conf, params, task);
      ret = 1;
   }

   if (best_conf)
      config_file_free(best_conf);

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
            input_builtin_autoconfs[i], NULL);
      if (conf && input_autoconfigure_joypad_from_conf(conf, params, task))
        return true;
   }

   if (string_is_empty(params->autoconfig_directory))
      return true;
   return false;
}

static void input_autoconfigure_params_free(autoconfig_params_t *params)
{
   if (!params)
      return;
   if (!string_is_empty(params->name))
      free(params->name);
   if (!string_is_empty(params->autoconfig_directory))
      free(params->autoconfig_directory);
   params->name                 = NULL;
   params->autoconfig_directory = NULL;
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
      if (!string_is_empty(params->name))
         free(params->name);
      params->name = strdup("Android Gamepad");

      if (input_autoconfigure_joypad_from_conf_internal(params, task))
      {
         snprintf(msg, sizeof(msg), "%s (%ld/%ld) %s.",
               !string_is_empty(params->name) ? params->name : "N/A",
               (long)params->vid, (long)params->pid,
               msg_hash_to_str(MSG_DEVICE_NOT_CONFIGURED_FALLBACK));
      }
#else
      snprintf(msg, sizeof(msg), "%s (%ld/%ld) %s.",
            !string_is_empty(params->name) ? params->name : "N/A",
            (long)params->vid, (long)params->pid,
            msg_hash_to_str(MSG_DEVICE_NOT_CONFIGURED));
#endif
      task_free_title(task);
      task_set_title(task, strdup(msg));
   }

end:
   if (params)
   {
      input_autoconfigure_params_free(params);
      free(params);
   }
   task_set_finished(task, true);
}

static void input_autoconfigure_disconnect_handler(retro_task_t *task)
{
   autoconfig_disconnect_t *params = (autoconfig_disconnect_t*)task->state;

   task_set_title(task, strdup(params->msg));

   task_set_finished(task, true);

   if (!string_is_empty(params->msg))
      free(params->msg);
   free(params);
}

bool input_autoconfigure_disconnect(unsigned i, const char *ident)
{
   char msg[255];
   retro_task_t         *task      = task_init();
   autoconfig_disconnect_t *state  = (autoconfig_disconnect_t*)calloc(1, sizeof(*state));

   if (!state || !task)
      goto error;

   msg[0]      = '\0';

   state->idx  = i;

   snprintf(msg, sizeof(msg), "%s #%u (%s).",
         msg_hash_to_str(MSG_DEVICE_DISCONNECTED_FROM_PORT),
         i, ident);

   state->msg    = strdup(msg);

   input_config_clear_device_name(state->idx);
   input_config_clear_device_display_name(state->idx);
   input_config_clear_device_config_name(state->idx);
   input_config_clear_device_config_path(state->idx);

   task->state   = state;
   task->handler = input_autoconfigure_disconnect_handler;

   task_queue_push(task);

   return true;

error:
   if (state)
   {
      if (!string_is_empty(state->msg))
         free(state->msg);
      free(state);
   }
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

void input_autoconfigure_connect(
      const char *name,
      const char *display_name,
      const char *driver,
      unsigned idx,
      unsigned vid,
      unsigned pid)
{
   unsigned i;
   retro_task_t         *task = task_init();
   autoconfig_params_t *state = (autoconfig_params_t*)calloc(1, sizeof(*state));
   settings_t       *settings = config_get_ptr();
   const char *dir_autoconf   = settings ? settings->paths.directory_autoconfig : NULL;
   bool autodetect_enable     = settings ? settings->bools.input_autodetect_enable : false;

   if (!task || !state || !autodetect_enable)
   {
      if (state)
      {
         input_autoconfigure_params_free(state);
         free(state);
      }
      if (task)
         free(task);

      input_config_set_device_name(idx, name);
      return;
   }

   if (!string_is_empty(name))
      state->name                 = strdup(name);

   if (!string_is_empty(dir_autoconf))
      state->autoconfig_directory = strdup(dir_autoconf);

   state->show_hidden_files       = settings->bools.show_hidden_files;
   state->idx                     = idx;
   state->vid                     = vid;
   state->pid                     = pid;
   state->max_users               = *(
         input_driver_get_uint(INPUT_ACTION_MAX_USERS));

#ifdef HAVE_BLISSBOX
   if (state->vid == BLISSBOX_VID)
      input_autoconfigure_override_handler(state);
#endif

   if (!string_is_empty(state->name))
      input_config_set_device_name(state->idx, state->name);
   input_config_set_pid(state->idx, state->pid);
   input_config_set_vid(state->idx, state->vid);

   for (i = 0; i < RARCH_BIND_LIST_END; i++)
   {
      input_autoconf_binds[state->idx][i].joykey           = NO_BTN;
      input_autoconf_binds[state->idx][i].joyaxis          = AXIS_NONE;
      if (
            !string_is_empty(input_autoconf_binds[state->idx][i].joykey_label))
         free(input_autoconf_binds[state->idx][i].joykey_label);
      if (
            !string_is_empty(input_autoconf_binds[state->idx][i].joyaxis_label))
         free(input_autoconf_binds[state->idx][i].joyaxis_label);
      input_autoconf_binds[state->idx][i].joykey_label      = NULL;
      input_autoconf_binds[state->idx][i].joyaxis_label     = NULL;
   }

   input_autoconfigured[state->idx] = false;

   task->state                      = state;
   task->handler                    = input_autoconfigure_connect_handler;

   task_queue_push(task);
}

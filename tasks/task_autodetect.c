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
#include <file/config_file.h>

#include "../configuration.h"
#include "../file_path_special.h"
#include "../list_special.h"
#include "../retroarch.h"
#include "../input/input_driver.h"
#include "../input/input_remapping.h"

#include "tasks_internal.h"
#ifdef HAVE_BLISSBOX
#include "../input/include/blissbox.h"
#endif

typedef struct
{
   char *dir_autoconfig;
   char *dir_driver_autoconfig;
   config_file_t *autoconfig_file;
   unsigned port;
   input_device_info_t device_info; /* unsigned alignment */
   bool autoconfig_enabled;
   bool suppress_notifcations;
} autoconfig_handle_t;

/*********************/
/* Utility functions */
/*********************/

static void free_autoconfig_handle(autoconfig_handle_t *autoconfig_handle)
{
   if (!autoconfig_handle)
      return;

   if (autoconfig_handle->dir_autoconfig)
   {
      free(autoconfig_handle->dir_autoconfig);
      autoconfig_handle->dir_autoconfig = NULL;
   }

   if (autoconfig_handle->dir_driver_autoconfig)
   {
      free(autoconfig_handle->dir_driver_autoconfig);
      autoconfig_handle->dir_driver_autoconfig = NULL;
   }

   if (autoconfig_handle->autoconfig_file)
   {
      config_file_free(autoconfig_handle->autoconfig_file);
      autoconfig_handle->autoconfig_file = NULL;
   }

   free(autoconfig_handle);
   autoconfig_handle = NULL;
}

static void input_autoconfigure_free(retro_task_t *task)
{
   autoconfig_handle_t *autoconfig_handle = NULL;

   if (!task)
      return;

   autoconfig_handle = (autoconfig_handle_t*)task->state;

   free_autoconfig_handle(autoconfig_handle);
}

/******************************/
/* Autoconfig 'File' Handling */
/******************************/

/* Returns a value corresponding to the
 * 'affinity' between the connected input
 * device and the specified config file
 * > 0: No match
 * > 2: Device name matches
 * > 3: VID+PID match
 * > 5: Both device name and VID+PID match */
static unsigned input_autoconfigure_get_config_file_affinity(
      autoconfig_handle_t *autoconfig_handle,
      config_file_t *config)
{
   int tmp_int         = 0;
   uint16_t config_vid = 0;
   uint16_t config_pid = 0;
   bool pid_match      = false;
   unsigned affinity   = 0;
   struct config_entry_list 
      *entry           = NULL;

   /* Parse config file */
   if (config_get_int(config, "input_vendor_id", &tmp_int))
      config_vid = (uint16_t)tmp_int;

   if (config_get_int(config, "input_product_id", &tmp_int))
      config_pid = (uint16_t)tmp_int;

   /* > Bliss-Box shenanigans... */
#ifdef HAVE_BLISSBOX
   if (autoconfig_handle->device_info.vid == BLISSBOX_VID)
      config_pid = BLISSBOX_PID;
#endif

   /* Check for matching VID+PID */
   pid_match = (autoconfig_handle->device_info.vid == config_vid) &&
               (autoconfig_handle->device_info.pid == config_pid) &&
               (autoconfig_handle->device_info.vid != 0)          &&
               (autoconfig_handle->device_info.pid != 0);

   /* > More Bliss-Box shenanigans... */
#ifdef HAVE_BLISSBOX
   pid_match = pid_match &&
               (autoconfig_handle->device_info.vid != BLISSBOX_VID) &&
               (autoconfig_handle->device_info.pid != BLISSBOX_PID);
#endif

   if (pid_match)
      affinity += 3;

   /* Check for matching device name */
   if (      (entry  = config_get_entry(config, "input_device"))
         && !string_is_empty(entry->value)
         &&  string_is_equal(entry->value,
             autoconfig_handle->device_info.name))
      affinity += 2;

   return affinity;
}

/* 'Attaches' specified autoconfig file to autoconfig
 * handle, parsing required device info metadata */
static void input_autoconfigure_set_config_file(
      autoconfig_handle_t *autoconfig_handle,
      config_file_t *config)
{
   struct config_entry_list *entry    = NULL;

   /* Attach config file */
   autoconfig_handle->autoconfig_file = config;

   /* > Extract config file path + name */
   if (!string_is_empty(config->path))
   {
      const char *config_file_name = path_basename_nocompression(config->path);

      strlcpy(autoconfig_handle->device_info.config_path,
            config->path,
            sizeof(autoconfig_handle->device_info.config_path));

      if (!string_is_empty(config_file_name))
         strlcpy(autoconfig_handle->device_info.config_name,
               config_file_name,
               sizeof(autoconfig_handle->device_info.config_name));
   }

   /* Read device display name */
   if (  (entry = config_get_entry(config, "input_device_display_name"))
         && !string_is_empty(entry->value))
      strlcpy(autoconfig_handle->device_info.display_name,
            entry->value,
            sizeof(autoconfig_handle->device_info.display_name));

   /* Set auto-configured status to 'true' */
   autoconfig_handle->device_info.autoconfigured = true;
}

/* Attempts to find an 'external' autoconfig file
 * (in the autoconfig directory) matching the connected
 * input device
 * > Returns 'true' if successful */
static bool input_autoconfigure_scan_config_files_external(
      autoconfig_handle_t *autoconfig_handle)
{
   size_t i;
   const char *dir_autoconfig           = autoconfig_handle->dir_autoconfig;
   const char *dir_driver_autoconfig    = autoconfig_handle->dir_driver_autoconfig;
   struct string_list *config_file_list = NULL;
   config_file_t *best_config           = NULL;
   unsigned max_affinity                = 0;
   bool match_found                     = false;

   /* Attempt to fetch file listing from driver-specific
    * autoconfig directory */
   if (!string_is_empty(dir_driver_autoconfig) &&
       path_is_directory(dir_driver_autoconfig))
      config_file_list = dir_list_new_special(
            dir_driver_autoconfig, DIR_LIST_AUTOCONFIG,
            "cfg", false);

   if (!config_file_list || (config_file_list->size < 1))
   {
      /* No files found - attempt to fetch listing
       * from autoconfig base directory */
      if (config_file_list)
      {
         string_list_free(config_file_list);
         config_file_list = NULL;
      }

      if (!string_is_empty(dir_autoconfig) &&
          path_is_directory(dir_autoconfig))
         config_file_list = dir_list_new_special(
               dir_autoconfig, DIR_LIST_AUTOCONFIG,
               "cfg", false);
   }

   if (!config_file_list || (config_file_list->size < 1))
      goto end;

   /* Loop through external config files */
   for (i = 0; i < config_file_list->size; i++)
   {
      const char *config_file_path = config_file_list->elems[i].data;
      config_file_t *config        = NULL;
      unsigned affinity            = 0;

      if (string_is_empty(config_file_path))
         continue;

      /* Load autoconfig file */
      config = config_file_new_from_path_to_string(config_file_path);

      if (!config)
         continue;

      /* Check for a match */
      if (autoconfig_handle && config)
         affinity = input_autoconfigure_get_config_file_affinity(
               autoconfig_handle, config);

      if (affinity > max_affinity)
      {
         if (best_config)
         {
            config_file_free(best_config);
            best_config = NULL;
         }

         /* 'Cache' config file for later processing */
         best_config  = config;
         config       = NULL;
         max_affinity = affinity;

         /* An affinity of 5 is a 'perfect' match,
          * and means we can return immediately */
         if (affinity == 5)
            break;
      }
      /* No match - just clean up config file */
      else
      {
         config_file_free(config);
         config = NULL;
      }
   }

   /* If we reach this point and a config file has
    * been cached, then we have a match */
   if (best_config)
   {
      if (autoconfig_handle && best_config)
         input_autoconfigure_set_config_file(
               autoconfig_handle, best_config);
      match_found = true;
   }

end:
   if (config_file_list)
   {
      string_list_free(config_file_list);
      config_file_list = NULL;
   }

   return match_found;
}

/* Attempts to find an internal autoconfig definition
 * matching the connected input device
 * > Returns 'true' if successful */
static bool input_autoconfigure_scan_config_files_internal(
      autoconfig_handle_t *autoconfig_handle)
{
   size_t i;

   /* Loop through internal autoconfig files
    * > input_builtin_autoconfs is a static const,
    *   and may be read safely in any thread  */
   for (i = 0; input_builtin_autoconfs[i]; i++)
   {
      char *autoconfig_str  = NULL;
      config_file_t *config = NULL;
      unsigned affinity     = 0;

      if (string_is_empty(input_builtin_autoconfs[i]))
         continue;

      /* Load autoconfig string */
      autoconfig_str = strdup(input_builtin_autoconfs[i]);
      config         = config_file_new_from_string(
            autoconfig_str, NULL);

      /* > String no longer required - clean up */
      free(autoconfig_str);
      autoconfig_str = NULL;

      /* Check for a match */
      if (autoconfig_handle && config)
         affinity = input_autoconfigure_get_config_file_affinity(
               autoconfig_handle, config);

      /* > In the case of internal autoconfigs, any kind
       *   of match is considered to be a success */
      if (affinity > 0)
      {
         if (autoconfig_handle && config)
            input_autoconfigure_set_config_file(
                  autoconfig_handle, config);
         return true;
      }

      /* No match - clean up */
      if (config)
      {
         config_file_free(config);
         config = NULL;
      }
   }

   return false;
}

/*************************/
/* Autoconfigure Connect */
/*************************/

static void cb_input_autoconfigure_connect(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   autoconfig_handle_t *autoconfig_handle = NULL;
   unsigned port;

   if (!task)
      return;

   autoconfig_handle = (autoconfig_handle_t*)task->state;

   if (!autoconfig_handle)
      return;

   /* Use local copy of port index for brevity... */
   port = autoconfig_handle->port;

   /* We perform the actual 'connect' in this
    * callback, to ensure it occurs on the main
    * thread */

   /* Copy task handle parameters into global
    * state objects:
    * > Name */
   if (!string_is_empty(autoconfig_handle->device_info.name))
      input_config_set_device_name(port,
            autoconfig_handle->device_info.name);
   else
      input_config_clear_device_name(port);

   /* > Display name */
   if (!string_is_empty(autoconfig_handle->device_info.display_name))
      input_config_set_device_display_name(port,
            autoconfig_handle->device_info.display_name);
   else if (!string_is_empty(autoconfig_handle->device_info.name))
      input_config_set_device_display_name(port,
            autoconfig_handle->device_info.name);
   else
      input_config_clear_device_display_name(port);

   /* > Driver */
   if (!string_is_empty(autoconfig_handle->device_info.joypad_driver))
      input_config_set_device_joypad_driver(port,
            autoconfig_handle->device_info.joypad_driver);
   else
      input_config_clear_device_joypad_driver(port);

   /* > VID/PID */
   input_config_set_device_vid(port, autoconfig_handle->device_info.vid);
   input_config_set_device_pid(port, autoconfig_handle->device_info.pid);

   /* > Config file path/name */
   if (!string_is_empty(autoconfig_handle->device_info.config_path))
      input_config_set_device_config_path(port,
            autoconfig_handle->device_info.config_path);
   else
      input_config_set_device_config_path(port,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));

   if (!string_is_empty(autoconfig_handle->device_info.config_name))
      input_config_set_device_config_name(port,
            autoconfig_handle->device_info.config_name);
   else
      input_config_set_device_config_name(port,
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE));

   /* > Auto-configured state */
   input_config_set_device_autoconfigured(port,
         autoconfig_handle->device_info.autoconfigured);

   /* Reset any existing binds */
   input_config_reset_autoconfig_binds(port);

   /* If an autoconfig file is available, load its
    * bind mappings */
   if (autoconfig_handle->device_info.autoconfigured)
      input_config_set_autoconfig_binds(port,
            autoconfig_handle->autoconfig_file);
}

static void input_autoconfigure_connect_handler(retro_task_t *task)
{
   autoconfig_handle_t *autoconfig_handle = NULL;
   bool match_found                       = false;
   const char *device_display_name        = NULL;
   char task_title[NAME_MAX_LENGTH];

   task_title[0] = '\0';

   if (!task)
      goto task_finished;

   autoconfig_handle = (autoconfig_handle_t*)task->state;

   if (!autoconfig_handle ||
       string_is_empty(autoconfig_handle->device_info.name) ||
       !autoconfig_handle->autoconfig_enabled)
      goto task_finished;

   /* Annoyingly, we have to scan all the autoconfig
    * files (and in-built configs) in a single shot
    * > Would prefer to scan one config per iteration
    *   of the task, but this would render the gamepad
    *   unusable for multiple frames after loading
    *   content... */

   /* Scan in order of preference:
    * - External autoconfig files
    * - Internal autoconfig definitions */
   match_found = input_autoconfigure_scan_config_files_external(
         autoconfig_handle);

   if (!match_found)
      match_found = input_autoconfigure_scan_config_files_internal(
         autoconfig_handle);

   /* If no match was found, attempt to use
    * fallback mapping
    * > Only enabled for certain drivers */
   if (!match_found)
   {
      const char *fallback_device_name = NULL;

      /* Preset fallback device names - must match
       * those set in 'input_autodetect_builtin.c' */
      if (string_is_equal(autoconfig_handle->device_info.joypad_driver,
            "android"))
         fallback_device_name = "Android Gamepad";
      else if (string_is_equal(autoconfig_handle->device_info.joypad_driver,
            "xinput"))
         fallback_device_name = "XInput Controller";
      else if (string_is_equal(autoconfig_handle->device_info.joypad_driver,
            "sdl2"))
         fallback_device_name = "Standard Gamepad";

      if (!string_is_empty(fallback_device_name) &&
          !string_is_equal(autoconfig_handle->device_info.name,
               fallback_device_name))
      {
         char *name_backup = strdup(autoconfig_handle->device_info.name);

         strlcpy(autoconfig_handle->device_info.name,
               fallback_device_name,
               sizeof(autoconfig_handle->device_info.name));

         /* This is not a genuine match - leave
          * match_found set to 'false' regardless
          * of the outcome */
         input_autoconfigure_scan_config_files_internal(
               autoconfig_handle);

         strlcpy(autoconfig_handle->device_info.name,
               name_backup,
               sizeof(autoconfig_handle->device_info.name));

         free(name_backup);
         name_backup = NULL;
      }
   }

   /* Get display name for task status message */
   device_display_name = autoconfig_handle->device_info.display_name;
   if (string_is_empty(device_display_name))
      device_display_name = autoconfig_handle->device_info.name;
   if (string_is_empty(device_display_name))
      device_display_name = msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE);

   /* Generate task status message
    * > Note that 'connection successful' messages
    *   may be suppressed, but error messages are
    *   always shown */
   if (autoconfig_handle->device_info.autoconfigured)
   {
      if (match_found)
      {
         /* A valid autoconfig was applied */
         if (!autoconfig_handle->suppress_notifcations)
            snprintf(task_title, sizeof(task_title), "%s %s #%u",
                  device_display_name,
                  msg_hash_to_str(MSG_DEVICE_CONFIGURED_IN_PORT),
                  autoconfig_handle->port + 1);
      }
      /* Device is autoconfigured, but a (most likely
       * incorrect) fallback definition was used... */
      else
         snprintf(task_title, sizeof(task_title), "%s (%u/%u) %s",
               device_display_name,
               autoconfig_handle->device_info.vid,
               autoconfig_handle->device_info.pid,
               msg_hash_to_str(MSG_DEVICE_NOT_CONFIGURED_FALLBACK));
   }
   /* Autoconfig failed */
   else
      snprintf(task_title, sizeof(task_title), "%s (%u/%u) %s",
            device_display_name,
            autoconfig_handle->device_info.vid,
            autoconfig_handle->device_info.pid,
            msg_hash_to_str(MSG_DEVICE_NOT_CONFIGURED));

   /* Update task title */
   task_free_title(task);
   if (!string_is_empty(task_title))
      task_set_title(task, strdup(task_title));

task_finished:

   if (task)
      task_set_finished(task, true);
}

static bool autoconfigure_connect_finder(retro_task_t *task, void *user_data)
{
   autoconfig_handle_t *autoconfig_handle = NULL;
   unsigned *port                         = NULL;

   if (!task || !user_data)
      return false;

   if (task->handler != input_autoconfigure_connect_handler)
      return false;

   autoconfig_handle = (autoconfig_handle_t*)task->state;
   if (!autoconfig_handle)
      return false;

   port = (unsigned*)user_data;
   return (*port == autoconfig_handle->port);
}

void input_autoconfigure_connect(
      const char *name,
      const char *display_name,
      const char *driver,
      unsigned port,
      unsigned vid,
      unsigned pid)
{
   retro_task_t *task                     = NULL;
   autoconfig_handle_t *autoconfig_handle = NULL;
   bool driver_valid                      = false;
   settings_t *settings                   = config_get_ptr();
   bool autoconfig_enabled                = settings ?
         settings->bools.input_autodetect_enable : false;
   const char *dir_autoconfig             = settings ?
         settings->paths.directory_autoconfig : NULL;
   bool notification_show_autoconfig      = settings ?
         settings->bools.notification_show_autoconfig : true;
   task_finder_data_t find_data;

   if (port >= MAX_INPUT_DEVICES)
      goto error;

   /* Cannot connect a device that is currently
    * being connected */
   find_data.func     = autoconfigure_connect_finder;
   find_data.userdata = (void*)&port;

   if (task_queue_find(&find_data))
      goto error;

   /* Configure handle */
   autoconfig_handle = (autoconfig_handle_t*)malloc(sizeof(autoconfig_handle_t));

   if (!autoconfig_handle)
      goto error;

   autoconfig_handle->port                         = port;
   autoconfig_handle->device_info.vid              = vid;
   autoconfig_handle->device_info.pid              = pid;
   autoconfig_handle->device_info.name[0]          = '\0';
   autoconfig_handle->device_info.display_name[0]  = '\0';
   autoconfig_handle->device_info.config_path[0]   = '\0';
   autoconfig_handle->device_info.config_name[0]   = '\0';
   autoconfig_handle->device_info.joypad_driver[0] = '\0';
   autoconfig_handle->device_info.autoconfigured   = false;
   autoconfig_handle->device_info.name_index       = 0;
   autoconfig_handle->autoconfig_enabled           = autoconfig_enabled;
   autoconfig_handle->suppress_notifcations        = !notification_show_autoconfig;
   autoconfig_handle->dir_autoconfig               = NULL;
   autoconfig_handle->dir_driver_autoconfig        = NULL;
   autoconfig_handle->autoconfig_file              = NULL;

   if (!string_is_empty(name))
      strlcpy(autoconfig_handle->device_info.name, name,
            sizeof(autoconfig_handle->device_info.name));

   if (!string_is_empty(display_name))
      strlcpy(autoconfig_handle->device_info.display_name, display_name,
            sizeof(autoconfig_handle->device_info.display_name));

   driver_valid = !string_is_empty(driver);
   if (driver_valid)
      strlcpy(autoconfig_handle->device_info.joypad_driver,
            driver, sizeof(autoconfig_handle->device_info.joypad_driver));

   /* > Have to cache both the base autoconfig directory
    *   and the driver-specific autoconfig directory
    *   - Driver-specific directory is scanned by
    *     default, if available
    *   - If driver-specific directory is unavailable,
    *     we scan the base autoconfig directory as
    *     a fallback */
   if (!string_is_empty(dir_autoconfig))
   {
      autoconfig_handle->dir_autoconfig = strdup(dir_autoconfig);

      if (driver_valid)
      {
         char dir_driver_autoconfig[PATH_MAX_LENGTH];
         dir_driver_autoconfig[0] = '\0';

         /* Generate driver-specific autoconfig directory */
         fill_pathname_join(dir_driver_autoconfig, dir_autoconfig,
               autoconfig_handle->device_info.joypad_driver,
               sizeof(dir_driver_autoconfig));

         if (!string_is_empty(dir_driver_autoconfig))
            autoconfig_handle->dir_driver_autoconfig =
                  strdup(dir_driver_autoconfig);
      }
   }

   /* Bliss-Box shenanigans... */
#ifdef HAVE_BLISSBOX
   if (autoconfig_handle->device_info.vid == BLISSBOX_VID)
      input_autoconfigure_blissbox_override_handler(
            (int)autoconfig_handle->device_info.vid,
            (int)autoconfig_handle->device_info.pid,
            autoconfig_handle->device_info.name,
            sizeof(autoconfig_handle->device_info.name));
#endif

   /* If we are reconnecting a device that is already
    * connected and autoconfigured, then there is no need
    * to generate additional 'connection successful'
    * task status messages
    * > Can skip this check if autoconfig notifications
    *   have been disabled by the user */
   if (!autoconfig_handle->suppress_notifcations &&
       !string_is_empty(autoconfig_handle->device_info.name))
   {
      const char *last_device_name = input_config_get_device_name(port);
      uint16_t last_vid            = input_config_get_device_vid(port);
      uint16_t last_pid            = input_config_get_device_pid(port);
      bool last_autoconfigured     = input_config_get_device_autoconfigured(port);

      if (!string_is_empty(last_device_name) &&
          string_is_equal(autoconfig_handle->device_info.name,
               last_device_name) &&
          (autoconfig_handle->device_info.vid == last_vid) &&
          (autoconfig_handle->device_info.pid == last_pid) &&
          last_autoconfigured)
         autoconfig_handle->suppress_notifcations = true;
   }

   /* Configure task */
   task = task_init();

   if (!task)
      goto error;

   task->handler  = input_autoconfigure_connect_handler;
   task->state    = autoconfig_handle;
   task->mute     = false;
   task->title    = NULL;
   task->callback = cb_input_autoconfigure_connect;
   task->cleanup  = input_autoconfigure_free;

   task_queue_push(task);

   return;

error:

   if (task)
   {
      free(task);
      task = NULL;
   }

   free_autoconfig_handle(autoconfig_handle);
   autoconfig_handle = NULL;
}

/****************************/
/* Autoconfigure Disconnect */
/****************************/

static void cb_input_autoconfigure_disconnect(
      retro_task_t *task, void *task_data,
      void *user_data, const char *err)
{
   unsigned port;
   autoconfig_handle_t *autoconfig_handle = NULL;

   if (!task)
      return;

   autoconfig_handle = (autoconfig_handle_t*)task->state;

   if (!autoconfig_handle)
      return;

   /* Use local copy of port index for brevity... */
   port = autoconfig_handle->port;

   /* We perform the actual 'disconnect' in this
    * callback, to ensure it occurs on the main thread */
   input_config_clear_device_name(port);
   input_config_clear_device_display_name(port);
   input_config_clear_device_config_path(port);
   input_config_clear_device_config_name(port);
   input_config_clear_device_joypad_driver(port);
   input_config_set_device_vid(port, 0);
   input_config_set_device_pid(port, 0);
   input_config_set_device_autoconfigured(port, false);
   input_config_reset_autoconfig_binds(port);
}

static void input_autoconfigure_disconnect_handler(retro_task_t *task)
{
   autoconfig_handle_t *autoconfig_handle = NULL;
   char task_title[NAME_MAX_LENGTH];

   task_title[0] = '\0';

   if (!task)
      goto task_finished;

   autoconfig_handle = (autoconfig_handle_t*)task->state;

   if (!autoconfig_handle)
      goto task_finished;

   /* Set task title */
   if (!string_is_empty(autoconfig_handle->device_info.name))
      snprintf(task_title, sizeof(task_title), "%s #%u (%s)",
            msg_hash_to_str(MSG_DEVICE_DISCONNECTED_FROM_PORT),
            autoconfig_handle->port + 1,
            autoconfig_handle->device_info.name);
   else
      snprintf(task_title, sizeof(task_title), "%s #%u",
            msg_hash_to_str(MSG_DEVICE_DISCONNECTED_FROM_PORT),
            autoconfig_handle->port + 1);

   task_free_title(task);
   if (!autoconfig_handle->suppress_notifcations)
      task_set_title(task, strdup(task_title));

task_finished:

   if (task)
      task_set_finished(task, true);
}

static bool autoconfigure_disconnect_finder(retro_task_t *task, void *user_data)
{
   autoconfig_handle_t *autoconfig_handle = NULL;
   unsigned *port                         = NULL;

   if (!task || !user_data)
      return false;

   if (task->handler != input_autoconfigure_disconnect_handler)
      return false;

   autoconfig_handle = (autoconfig_handle_t*)task->state;
   if (!autoconfig_handle)
      return false;

   port = (unsigned*)user_data;
   return (*port == autoconfig_handle->port);
}

/* Note: There is no real need for autoconfigure
 * 'disconnect' to be a task - we are merely setting
 * a handful of variables. However:
 * - Making it a task means we can call
 *   input_autoconfigure_disconnect() on any thread
 *   thread, and defer the global state changes until
 *   the task queue is handled on the *main* thread
 * - By using a task for both 'connect' and 'disconnect',
 *   we ensure uniformity of OSD status messages */
bool input_autoconfigure_disconnect(unsigned port, const char *name)
{
   retro_task_t *task                     = NULL;
   autoconfig_handle_t *autoconfig_handle = NULL;
   task_finder_data_t find_data;
   settings_t *settings                   = config_get_ptr();
   bool notification_show_autoconfig      = settings ?
         settings->bools.notification_show_autoconfig : true;

   if (port >= MAX_INPUT_DEVICES)
      goto error;

   /* Cannot disconnect a device that is currently
    * being disconnected */
   find_data.func     = autoconfigure_disconnect_finder;
   find_data.userdata = (void*)&port;

   if (task_queue_find(&find_data))
      goto error;

   /* Configure handle */
   autoconfig_handle = (autoconfig_handle_t*)calloc(1, sizeof(autoconfig_handle_t));

   if (!autoconfig_handle)
      goto error;

   autoconfig_handle->port                  = port;
   autoconfig_handle->suppress_notifcations = !notification_show_autoconfig;

   if (!string_is_empty(name))
      strlcpy(autoconfig_handle->device_info.name,
            name, sizeof(autoconfig_handle->device_info.name));

   /* Configure task */
   task = task_init();

   if (!task)
      goto error;

   task->handler  = input_autoconfigure_disconnect_handler;
   task->state    = autoconfig_handle;
   task->title    = NULL;
   task->callback = cb_input_autoconfigure_disconnect;
   task->cleanup  = input_autoconfigure_free;

   task_queue_push(task);

   return true;

error:

   if (task)
   {
      free(task);
      task = NULL;
   }

   free_autoconfig_handle(autoconfig_handle);
   autoconfig_handle = NULL;

   return false;
}

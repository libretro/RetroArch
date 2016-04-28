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

#include <ctype.h>

#include <compat/strl.h>
#include <file/file_path.h>
#include <lists/dir_list.h>
#include <string/stdstring.h>

#include "command_event.h"

#include "defaults.h"
#include "frontend/frontend_driver.h"
#include "audio/audio_driver.h"
#include "record/record_driver.h"
#include "autosave.h"
#include "core_info.h"
#include "cheats.h"
#include "performance.h"
#include "dynamic.h"
#include "content.h"
#include "movie.h"
#include "screenshot.h"
#include "msg_hash.h"
#include "retroarch.h"
#include "rewind.h"
#include "system.h"
#include "ui/ui_companion_driver.h"
#include "list_special.h"

#ifdef HAVE_CHEEVOS
#include "cheevos.h"
#endif

#include "libretro_version_1.h"
#include "verbosity.h"
#include "runloop.h"
#include "configuration.h"
#include "input/input_remapping.h"

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#include "menu/menu_display.h"
#include "menu/menu_shader.h"
#endif

#ifdef HAVE_NETPLAY
#include "netplay/netplay.h"
#endif

#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#endif

/**
 * event_disk_control_set_eject:
 * @new_state            : Eject or close the virtual drive tray.
 *                         false (0) : Close
 *                         true  (1) : Eject
 * @print_log            : Show message onscreen.
 *
 * Ejects/closes of the virtual drive tray.
 **/
static void event_disk_control_set_eject(bool new_state, bool print_log)
{
   char msg[128]                                     = {0};
   bool error                                        = false;
   rarch_system_info_t *info                         = NULL;
   const struct retro_disk_control_callback *control = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info);

   if (info)
      control = (const struct retro_disk_control_callback*)&info->disk_control_cb;

   if (!control || !control->get_num_images)
      return;

   if (control->set_eject_state(new_state))
      snprintf(msg, sizeof(msg), "%s %s",
            new_state ? "Ejected" : "Closed",
            msg_hash_to_str(MSG_VIRTUAL_DISK_TRAY));
   else
   {
      error = true;
      snprintf(msg, sizeof(msg), "%s %s %s",
            msg_hash_to_str(MSG_FAILED_TO),
            new_state ? "eject" : "close",
            msg_hash_to_str(MSG_VIRTUAL_DISK_TRAY));
   }

   if (*msg)
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);

      /* Only noise in menu. */
      if (print_log)
         runloop_msg_queue_push(msg, 1, 180, true);
   }
}

/**
 * event_disk_control_set_index:
 * @idx                : Index of disk to set as current.
 *
 * Sets current disk to @index.
 **/
static void event_disk_control_set_index(unsigned idx)
{
   unsigned num_disks;
   bool error                                        = false;
   char msg[128]                                     = {0};
   rarch_system_info_t                      *info    = NULL;
   const struct retro_disk_control_callback *control = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info);

   if (info)
      control = (const struct retro_disk_control_callback*)&info->disk_control_cb;

   if (!control || !control->get_num_images)
      return;

   num_disks = control->get_num_images();

   if (control->set_image_index(idx))
   {
      if (idx < num_disks)
         snprintf(msg, sizeof(msg), "Setting disk %u of %u in tray.",
               idx + 1, num_disks);
      else
         strlcpy(msg,
               msg_hash_to_str(MSG_REMOVED_DISK_FROM_TRAY),
               sizeof(msg));
   }
   else
   {
      if (idx < num_disks)
         snprintf(msg, sizeof(msg), "Failed to set disk %u of %u.",
               idx + 1, num_disks);
      else
         strlcpy(msg,
               msg_hash_to_str(MSG_FAILED_TO_REMOVE_DISK_FROM_TRAY),
               sizeof(msg));
      error = true;
   }

   if (*msg)
   {
      if (error)
         RARCH_ERR("%s\n", msg);
      else
         RARCH_LOG("%s\n", msg);
      runloop_msg_queue_push(msg, 1, 180, true);
   }
}

/**
 * event_disk_control_append_image:
 * @path                 : Path to disk image.
 *
 * Appends disk image to disk image list.
 **/
static bool event_disk_control_append_image(const char *path)
{
   unsigned new_idx;
   char msg[128]                                      = {0};
   struct retro_game_info info                        = {0};
   global_t                                  *global  = global_get_ptr();
   const struct retro_disk_control_callback *control  = NULL;
   rarch_system_info_t                       *sysinfo = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &sysinfo);

   if (sysinfo)
      control = (const struct retro_disk_control_callback*)
         &sysinfo->disk_control_cb;

   if (!control)
      return false;

   event_disk_control_set_eject(true, false);

   control->add_image_index();
   new_idx = control->get_num_images();
   if (!new_idx)
      return false;
   new_idx--;

   info.path = path;
   control->replace_image_index(new_idx, &info);

   snprintf(msg, sizeof(msg), "%s: ", msg_hash_to_str(MSG_APPENDED_DISK));
   strlcat(msg, path, sizeof(msg));
   RARCH_LOG("%s\n", msg);
   runloop_msg_queue_push(msg, 0, 180, true);

   event_cmd_ctl(EVENT_CMD_AUTOSAVE_DEINIT, NULL);

   /* TODO: Need to figure out what to do with subsystems case. */
   if (!*global->subsystem)
   {
      /* Update paths for our new image.
       * If we actually use append_image, we assume that we
       * started out in a single disk case, and that this way
       * of doing it makes the most sense. */
      rarch_ctl(RARCH_CTL_SET_PATHS, (void*)path);
      rarch_ctl(RARCH_CTL_FILL_PATHNAMES, NULL);
   }

   event_cmd_ctl(EVENT_CMD_AUTOSAVE_INIT, NULL);
   event_disk_control_set_index(new_idx);
   event_disk_control_set_eject(false, false);

   return true;
}

/**
 * event_check_disk_prev:
 * @control              : Handle to disk control handle.
 *
 * Perform disk cycle to previous index action (Core Disk Options).
 **/
static void event_check_disk_prev(
      const struct retro_disk_control_callback *control)
{
   unsigned num_disks    = 0;
   unsigned current      = 0;
   bool disk_prev_enable = false;

   if (!control || !control->get_num_images)
      return;
   if (!control->get_image_index)
      return;

   num_disks        = control->get_num_images();
   current          = control->get_image_index();
   disk_prev_enable = num_disks && num_disks != UINT_MAX;

   if (!disk_prev_enable)
   {
      RARCH_ERR("%s.\n", msg_hash_to_str(MSG_GOT_INVALID_DISK_INDEX));
      return;
   }

   if (current > 0)
      current--;
   event_disk_control_set_index(current);
}

/**
 * event_check_disk_next:
 * @control              : Handle to disk control handle.
 *
 * Perform disk cycle to next index action (Core Disk Options).
 **/
static void event_check_disk_next(
      const struct retro_disk_control_callback *control)
{
   unsigned num_disks        = 0;
   unsigned current          = 0;
   bool     disk_next_enable = false;

   if (!control || !control->get_num_images)
      return;
   if (!control->get_image_index)
      return;

   num_disks        = control->get_num_images();
   current          = control->get_image_index();
   disk_next_enable = num_disks && num_disks != UINT_MAX;

   if (!disk_next_enable)
   {
      RARCH_ERR("%s.\n", msg_hash_to_str(MSG_GOT_INVALID_DISK_INDEX));
      return;
   }

   if (current < num_disks - 1)
      current++;
   event_disk_control_set_index(current);
}

/**
 * event_set_volume:
 * @gain      : amount of gain to be applied to current volume level.
 *
 * Adjusts the current audio volume level.
 *
 **/
static void event_set_volume(float gain)
{
   char msg[128];
   settings_t *settings      = config_get_ptr();

   settings->audio.volume += gain;
   settings->audio.volume  = MAX(settings->audio.volume, -80.0f);
   settings->audio.volume  = MIN(settings->audio.volume, 12.0f);

   snprintf(msg, sizeof(msg), "Volume: %.1f dB", settings->audio.volume);
   runloop_msg_queue_push(msg, 1, 180, true);
   RARCH_LOG("%s\n", msg);

   audio_driver_set_volume_gain(db_to_gain(settings->audio.volume));
}

/**
 * event_init_controllers:
 *
 * Initialize libretro controllers.
 **/
static void event_init_controllers(void)
{
   unsigned i;
   settings_t      *settings = config_get_ptr();
   rarch_system_info_t *info = NULL;
   
   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info);

   for (i = 0; i < MAX_USERS; i++)
   {
      retro_ctx_controller_info_t pad;
      const char *ident   = NULL;
      bool set_controller = false;
      const struct retro_controller_description *desc = NULL;
      unsigned device = settings->input.libretro_device[i];

      if (i < info->ports.size)
         desc = libretro_find_controller_description(
               &info->ports.data[i], device);

      if (desc)
         ident = desc->desc;

      if (!ident)
      {
         /* If we're trying to connect a completely unknown device,
          * revert back to JOYPAD. */

         if (device != RETRO_DEVICE_JOYPAD && device != RETRO_DEVICE_NONE)
         {
            /* Do not fix settings->input.libretro_device[i],
             * because any use of dummy core will reset this,
             * which is not a good idea. */
            RARCH_WARN("Input device ID %u is unknown to this "
                  "libretro implementation. Using RETRO_DEVICE_JOYPAD.\n",
                  device);
            device = RETRO_DEVICE_JOYPAD;
         }
         ident = "Joypad";
      }

      switch (device)
      {
         case RETRO_DEVICE_NONE:
            RARCH_LOG("Disconnecting device from port %u.\n", i + 1);
            set_controller = true;
            break;
         case RETRO_DEVICE_JOYPAD:
            break;
         default:
            /* Some cores do not properly range check port argument.
             * This is broken behavior of course, but avoid breaking
             * cores needlessly. */
            RARCH_LOG("Connecting %s (ID: %u) to port %u.\n", ident,
                  device, i + 1);
            set_controller = true;
            break;
      }

      if (set_controller)
      {
         pad.device     = device;
         pad.port       = i;
         core_ctl(CORE_CTL_RETRO_SET_CONTROLLER_PORT_DEVICE, &pad);
      }
   }
}

static void event_deinit_core(bool reinit)
{
#ifdef HAVE_CHEEVOS
   cheevos_ctl(CHEEVOS_CTL_UNLOAD, NULL);
#endif

   core_ctl(CORE_CTL_RETRO_UNLOAD_GAME, NULL);
   core_ctl(CORE_CTL_RETRO_DEINIT, NULL);

   if (reinit)
   {
      int flags = DRIVERS_CMD_ALL;
      driver_ctl(RARCH_DRIVER_CTL_UNINIT, &flags);
   }

   /* auto overrides: reload the original config */
   if (runloop_ctl(RUNLOOP_CTL_IS_OVERRIDES_ACTIVE, NULL))
   {
      config_unload_override();
      runloop_ctl(RUNLOOP_CTL_UNSET_OVERRIDES_ACTIVE, NULL);
   }
}

static void event_init_cheats(void)
{
   bool allow_cheats = true;
#ifdef HAVE_NETPLAY
   allow_cheats &= !netplay_driver_ctl(
         RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL);
#endif
   allow_cheats &= !bsv_movie_ctl(BSV_MOVIE_CTL_IS_INITED, NULL);

   if (!allow_cheats)
      return;

   /* TODO/FIXME - add some stuff here. */
}

static bool event_load_save_files(void)
{
   unsigned i;
   global_t *global = global_get_ptr();

   if (!global)
      return false;
   if (!global->savefiles || global->sram.load_disable)
      return false;

   for (i = 0; i < global->savefiles->size; i++)
   {
      ram_type_t ram;
      ram.path = global->savefiles->elems[i].data;
      ram.type = global->savefiles->elems[i].attr.i;

      content_ctl(CONTENT_CTL_LOAD_RAM_FILE, &ram);
   }

   return true;
}

static void event_load_auto_state(void)
{
   bool ret;
   char msg[128]                             = {0};
   char savestate_name_auto[PATH_MAX_LENGTH] = {0};
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

#ifdef HAVE_NETPLAY
   if (global->netplay.enable && !global->netplay.is_spectate)
      return;
#endif

#ifdef HAVE_CHEEVOS
   if (settings->cheevos.hardcore_mode_enable)
      return;
#endif

   if (!settings->savestate_auto_load)
      return;

   fill_pathname_noext(savestate_name_auto, global->name.savestate,
         ".auto", sizeof(savestate_name_auto));

   if (!path_file_exists(savestate_name_auto))
      return;

   ret = content_ctl(CONTENT_CTL_LOAD_STATE, (void*)savestate_name_auto);

   RARCH_LOG("Found auto savestate in: %s\n", savestate_name_auto);

   snprintf(msg, sizeof(msg), "Auto-loading savestate from \"%s\" %s.",
         savestate_name_auto, ret ? "succeeded" : "failed");
   runloop_msg_queue_push(msg, 1, 180, false);
   RARCH_LOG("%s\n", msg);
}

static void event_set_savestate_auto_index(void)
{
   size_t i;
   char state_dir[PATH_MAX_LENGTH]  = {0};
   char state_base[PATH_MAX_LENGTH] = {0};
   struct string_list *dir_list     = NULL;
   unsigned max_idx                 = 0;
   settings_t *settings             = config_get_ptr();
   global_t   *global               = global_get_ptr();

   if (!settings->savestate_auto_index)
      return;

   /* Find the file in the same directory as global->savestate_name
    * with the largest numeral suffix.
    *
    * E.g. /foo/path/content.state, will try to find
    * /foo/path/content.state%d, where %d is the largest number available.
    */

   fill_pathname_basedir(state_dir, global->name.savestate,
         sizeof(state_dir));
   fill_pathname_base(state_base, global->name.savestate,
         sizeof(state_base));

   if (!(dir_list = dir_list_new_special(state_dir, DIR_LIST_PLAIN, NULL)))
      return;

   for (i = 0; i < dir_list->size; i++)
   {
      unsigned idx;
      char elem_base[128]             = {0};
      const char *end                 = NULL;
      const char *dir_elem            = dir_list->elems[i].data;

      fill_pathname_base(elem_base, dir_elem, sizeof(elem_base));

      if (strstr(elem_base, state_base) != elem_base)
         continue;

      end = dir_elem + strlen(dir_elem);
      while ((end > dir_elem) && isdigit((int)end[-1]))
         end--;

      idx = strtoul(end, NULL, 0);
      if (idx > max_idx)
         max_idx = idx;
   }

   dir_list_free(dir_list);

   settings->state_slot = max_idx;
   RARCH_LOG("Found last state slot: #%d\n", settings->state_slot);
}

static bool event_init_content(void)
{
   rarch_ctl(RARCH_CTL_SET_SRAM_ENABLE, NULL);

   /* No content to be loaded for dummy core,
    * just successfully exit. */
   if (rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
      return true;

   if (!content_ctl(CONTENT_CTL_DOES_NOT_NEED_CONTENT, NULL))
      rarch_ctl(RARCH_CTL_FILL_PATHNAMES, NULL);

   if (!content_ctl(CONTENT_CTL_INIT, NULL))
      return false;

   if (content_ctl(CONTENT_CTL_DOES_NOT_NEED_CONTENT, NULL))
      return true;

   event_set_savestate_auto_index();

   if (event_load_save_files())
      RARCH_LOG("%s.\n",
            msg_hash_to_str(MSG_SKIPPING_SRAM_LOAD));

   event_load_auto_state();
   event_cmd_ctl(EVENT_CMD_BSV_MOVIE_INIT, NULL);
   event_cmd_ctl(EVENT_CMD_NETPLAY_INIT, NULL);

   return true;
}

static bool event_init_core(void *data)
{
   retro_ctx_environ_info_t info;
   settings_t *settings            = config_get_ptr();

   if (!core_ctl(CORE_CTL_RETRO_SYMBOLS_INIT, data))
      return false;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_INIT, NULL);

   /* auto overrides: apply overrides */
   if(settings->auto_overrides_enable)
   {
      if (config_load_override())
         runloop_ctl(RUNLOOP_CTL_SET_OVERRIDES_ACTIVE, NULL);
      else
         runloop_ctl(RUNLOOP_CTL_UNSET_OVERRIDES_ACTIVE, NULL);
   }

   /* reset video format to libretro's default */
   video_driver_set_pixel_format(RETRO_PIXEL_FORMAT_0RGB1555);

   info.env = rarch_environment_cb;
   core_ctl(CORE_CTL_RETRO_SET_ENVIRONMENT, &info);

   /* Auto-remap: apply remap files */
   if(settings->auto_remaps_enable)
      config_load_remap();

   /* Per-core saves: reset redirection paths */
   rarch_ctl(RARCH_CTL_SET_PATHS_REDIRECT, NULL);

   if (!core_ctl(CORE_CTL_RETRO_INIT, NULL))
      return false;

   if (!event_init_content())
      return false;

   if (!core_ctl(CORE_CTL_INIT, NULL))
      return false;

   return true;
}

static bool event_save_auto_state(void)
{
   bool ret;
   char savestate_name_auto[PATH_MAX_LENGTH] = {0};
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (!settings->savestate_auto_save)
      return false;
   if (rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL))
      return false;
   if (content_ctl(CONTENT_CTL_DOES_NOT_NEED_CONTENT, NULL))
      return false;

#ifdef HAVE_CHEEVOS
   if (settings->cheevos.hardcore_mode_enable)
      return false;
#endif

   fill_pathname_noext(savestate_name_auto, global->name.savestate,
         ".auto", sizeof(savestate_name_auto));

   ret = content_ctl(CONTENT_CTL_SAVE_STATE, (void*)savestate_name_auto);
   RARCH_LOG("Auto save state to \"%s\" %s.\n", savestate_name_auto, ret ?
         "succeeded" : "failed");

   return true;
}

/**
 * event_save_core_config:
 *
 * Saves a new (core) configuration to a file. Filename is based
 * on heuristics to avoid typing.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
static bool event_save_core_config(void)
{
   char config_dir[PATH_MAX_LENGTH]  = {0};
   char config_name[PATH_MAX_LENGTH] = {0};
   char config_path[PATH_MAX_LENGTH] = {0};
   char msg[128]                     = {0};
   bool ret                          = false;
   bool found_path                   = false;
   bool overrides_active             = false;
   settings_t *settings              = config_get_ptr();
   global_t   *global                = global_get_ptr();

   *config_dir = '\0';
   if (!string_is_empty(settings->directory.menu_config))
      strlcpy(config_dir, settings->directory.menu_config,
            sizeof(config_dir));
   else if (!string_is_empty(global->path.config)) /* Fallback */
      fill_pathname_basedir(config_dir, global->path.config,
            sizeof(config_dir));
   else
   {
      runloop_msg_queue_push(msg_hash_to_str(MSG_CONFIG_DIRECTORY_NOT_SET), 1, 180, true);
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_CONFIG_DIRECTORY_NOT_SET));
      return false;
   }

   /* Infer file name based on libretro core. */
   if (!string_is_empty(settings->path.libretro) 
   && path_file_exists(settings->path.libretro))
   {
      unsigned i;
      RARCH_LOG("Using core name for new config\n");
      /* In case of collision, find an alternative name. */
      for (i = 0; i < 16; i++)
      {
         char tmp[64];

         fill_pathname_base(
               config_name,
               settings->path.libretro,
               sizeof(config_name));

         path_remove_extension(config_name);
         fill_pathname_join(config_path, config_dir, config_name,
               sizeof(config_path));
         if (i)
            snprintf(tmp, sizeof(tmp), "-%u.cfg", i);
         else
            strlcpy(tmp, ".cfg", sizeof(tmp));

         strlcat(config_path, tmp, sizeof(config_path));
         if (!path_file_exists(config_path))
         {
            found_path = true;
            break;
         }
      }
   }

   /* Fallback to system time... */
   if (!found_path)
   {
      RARCH_WARN("Cannot infer new config path. Use current time.\n");
      fill_dated_filename(config_name, "cfg", sizeof(config_name));
      fill_pathname_join(config_path, config_dir, config_name,
            sizeof(config_path));
   }

   /* Overrides block config file saving, make it appear as overrides 
    * weren't enabled for a manual save */
   if (runloop_ctl(RUNLOOP_CTL_IS_OVERRIDES_ACTIVE, NULL))
   {
      runloop_ctl(RUNLOOP_CTL_UNSET_OVERRIDES_ACTIVE, NULL);
      overrides_active = true;
   }

   if ((ret = config_save_file(config_path)))
   {
      strlcpy(global->path.config, config_path,
            sizeof(global->path.config));
      snprintf(msg, sizeof(msg), "Saved new config to \"%s\".",
            config_path);
      RARCH_LOG("%s\n", msg);
   }
   else
   {
      snprintf(msg, sizeof(msg), "Failed saving config to \"%s\".",
            config_path);
      RARCH_ERR("%s\n", msg);
   }

   runloop_msg_queue_push(msg, 1, 180, true);

   if (overrides_active)
      runloop_ctl(RUNLOOP_CTL_SET_OVERRIDES_ACTIVE, NULL);
   else
      runloop_ctl(RUNLOOP_CTL_UNSET_OVERRIDES_ACTIVE, NULL);
   return ret;
}

/**
 * event_save_current_config:
 *
 * Saves current configuration file to disk, and (optionally)
 * autosave state.
 **/
void event_save_current_config(void)
{
   char msg[128];
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();
   bool ret             = false;

   if (settings->config_save_on_exit && !string_is_empty(global->path.config))
   {
      /* Save last core-specific config to the default config location,
       * needed on consoles for core switching and reusing last good
       * config for new cores.
       */

      /* Flush out the core specific config. */
      if (*global->path.core_specific_config &&
            settings->core_specific_config)
         ret = config_save_file(global->path.core_specific_config);
      else
         ret = config_save_file(global->path.config);
      if (ret)
      {
         snprintf(msg, sizeof(msg), "Saved new config to \"%s\".",
               global->path.config);
         RARCH_LOG("%s\n", msg);
      }
      else
      {
         snprintf(msg, sizeof(msg), "Failed saving config to \"%s\".",
               global->path.config);
         RARCH_ERR("%s\n", msg);
      }

      runloop_msg_queue_push(msg, 1, 180, true);
   }
}

/**
 * event_save_state
 * @path            : Path to state.
 * @s               : Message.
 * @len             : Size of @s.
 *
 * Saves a state with path being @path.
 **/
static void event_save_state(const char *path,
      char *s, size_t len)
{
   settings_t *settings = config_get_ptr();

   if (!content_ctl(CONTENT_CTL_SAVE_STATE, (void*)path))
   {
      snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_FAILED_TO_SAVE_STATE_TO),
            path);
      return;
   }

   if (settings->state_slot < 0)
      snprintf(s, len, "%s #-1 (auto).",
            msg_hash_to_str(MSG_SAVED_STATE_TO_SLOT));
   else
      snprintf(s, len, "%s #%d.", msg_hash_to_str(MSG_SAVED_STATE_TO_SLOT),
            settings->state_slot);
}

/**
 * event_load_state
 * @path            : Path to state.
 * @s               : Message.
 * @len             : Size of @s.
 *
 * Loads a state with path being @path.
 **/
static void event_load_state(const char *path, char *s, size_t len)
{
   settings_t *settings = config_get_ptr();

   if (!content_ctl(CONTENT_CTL_LOAD_STATE, (void*)path))
   {
      snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_FAILED_TO_LOAD_STATE),
            path);
      return;
   }

   if (settings->state_slot < 0)
      snprintf(s, len, "%s #-1 (auto).",
            msg_hash_to_str(MSG_LOADED_STATE_FROM_SLOT));
   else
      snprintf(s, len, "%s #%d.", msg_hash_to_str(MSG_LOADED_STATE_FROM_SLOT),
            settings->state_slot);
}

static void event_main_state(unsigned cmd)
{
   retro_ctx_size_info_t info;
   char path[PATH_MAX_LENGTH] = {0};
   char msg[128]              = {0};
   global_t *global           = global_get_ptr();
   settings_t *settings       = config_get_ptr();

   if (settings->state_slot > 0)
      snprintf(path, sizeof(path), "%s%d",
            global->name.savestate, settings->state_slot);
   else if (settings->state_slot < 0)
      fill_pathname_join_delim(path,
            global->name.savestate, "auto", '.', sizeof(path));
   else
      strlcpy(path, global->name.savestate, sizeof(path));

   core_ctl(CORE_CTL_RETRO_SERIALIZE_SIZE, &info);

   if (info.size)
   {
      switch (cmd)
      {
         case EVENT_CMD_SAVE_STATE:
            event_save_state(path, msg, sizeof(msg));
            break;
         case EVENT_CMD_LOAD_STATE:
            event_load_state(path, msg, sizeof(msg));
            break;
      }
   }
   else
      strlcpy(msg, msg_hash_to_str(
               MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES), sizeof(msg));

   runloop_msg_queue_push(msg, 2, 180, true);
   RARCH_LOG("%s\n", msg);
}

static bool event_cmd_exec(void *data)
{
   char *fullpath = NULL;

   runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);

   if (fullpath != data)
   {
      runloop_ctl(RUNLOOP_CTL_CLEAR_CONTENT_PATH, NULL);
      if (data)
         runloop_ctl(RUNLOOP_CTL_SET_CONTENT_PATH, data);
   }

#if defined(HAVE_DYNAMIC)
   if (!rarch_ctl(RARCH_CTL_LOAD_CONTENT, NULL))
      return false;
#else
   frontend_driver_set_fork(FRONTEND_FORK_CORE_WITH_ARGS);
#endif

   return true;
}

/**
 * event_cmd_ctl:
 * @cmd                  : Event command index.
 *
 * Performs program event command with index @cmd.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool event_cmd_ctl(enum event_command cmd, void *data)
{
   unsigned i                = 0;
   bool boolean              = false;
   settings_t *settings      = config_get_ptr();
   rarch_system_info_t *info = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &info);

   (void)i;

   switch (cmd)
   {
      case EVENT_CMD_MENU_REFRESH:
#ifdef HAVE_MENU
         menu_driver_ctl(RARCH_MENU_CTL_REFRESH, NULL);
#endif
         break;
      case EVENT_CMD_SET_PER_GAME_RESOLUTION:
#if defined(GEKKO)
         {
            unsigned width = 0, height = 0;

            event_cmd_ctl(EVENT_CMD_VIDEO_SET_ASPECT_RATIO, NULL);

            if (video_driver_get_video_output_size(&width, &height))
            {
               char msg[128] = {0};

               video_driver_set_video_mode(width, height, true);

               if (width == 0 || height == 0)
                  strlcpy(msg, "Resolution: DEFAULT", sizeof(msg));
               else
                  snprintf(msg, sizeof(msg),"Resolution: %dx%d",width, height);
               runloop_msg_queue_push(msg, 1, 100, true);
            }
         }
#endif
         break;
      case EVENT_CMD_LOAD_CONTENT_PERSIST:
#ifdef HAVE_DYNAMIC
         event_cmd_ctl(EVENT_CMD_LOAD_CORE, NULL);
#endif
         rarch_ctl(RARCH_CTL_LOAD_CONTENT, NULL);
         break;
#ifdef HAVE_FFMPEG
      case EVENT_CMD_LOAD_CONTENT_FFMPEG:
         rarch_ctl(RARCH_CTL_LOAD_CONTENT_FFMPEG, NULL);
         break;
#endif
      case EVENT_CMD_LOAD_CONTENT_IMAGEVIEWER:
         rarch_ctl(RARCH_CTL_LOAD_CONTENT_IMAGEVIEWER, NULL);
         break;
      case EVENT_CMD_LOAD_CONTENT:
         {
#ifdef HAVE_DYNAMIC
            event_cmd_ctl(EVENT_CMD_LOAD_CONTENT_PERSIST, NULL);
#else
            char *fullpath = NULL;
            runloop_ctl(RUNLOOP_CTL_GET_CONTENT_PATH, &fullpath);
            runloop_ctl(RUNLOOP_CTL_SET_LIBRETRO_PATH, settings->path.libretro);
            event_cmd_ctl(EVENT_CMD_EXEC, (void*)fullpath);
            event_cmd_ctl(EVENT_CMD_QUIT, NULL);
#endif
         }
         break;
      case EVENT_CMD_LOAD_CORE_DEINIT:
#ifdef HAVE_MENU
         menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_DEINIT, NULL);
#endif
         break;
      case EVENT_CMD_LOAD_CORE_PERSIST:
         event_cmd_ctl(EVENT_CMD_LOAD_CORE_DEINIT, NULL);
         {
#ifdef HAVE_MENU
            bool *ptr = NULL;
            struct retro_system_info *system = NULL;

            menu_driver_ctl(RARCH_MENU_CTL_SYSTEM_INFO_GET, &system);

            if (menu_driver_ctl(RARCH_MENU_CTL_LOAD_NO_CONTENT_GET, &ptr))
            {
               core_info_ctx_find_t info_find;

#if defined(HAVE_DYNAMIC)
               if (!(*settings->path.libretro))
                  return false;

               libretro_get_system_info(
                     settings->path.libretro,
                     system,
                     ptr);
#else
               libretro_get_system_info_static(system, ptr);
#endif
               info_find.path = settings->path.libretro;

               if (!core_info_ctl(CORE_INFO_CTL_LOAD, &info_find))
                  return false;
            }
#endif
         }
         break;
      case EVENT_CMD_LOAD_CORE:
         event_cmd_ctl(EVENT_CMD_LOAD_CORE_PERSIST, NULL);
#ifndef HAVE_DYNAMIC
         event_cmd_ctl(EVENT_CMD_QUIT, NULL);
#endif
         break;
      case EVENT_CMD_LOAD_STATE:
         /* Immutable - disallow savestate load when
          * we absolutely cannot change game state. */
         if (bsv_movie_ctl(BSV_MOVIE_CTL_IS_INITED, NULL))
            return false;

#ifdef HAVE_NETPLAY
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
            return false;
#endif

#ifdef HAVE_CHEEVOS
         if (settings->cheevos.hardcore_mode_enable)
            return false;
#endif

         event_main_state(cmd);
         break;
      case EVENT_CMD_RESIZE_WINDOWED_SCALE:
         {
            unsigned idx = 0;
            unsigned *window_scale = NULL;

            runloop_ctl(RUNLOOP_CTL_GET_WINDOWED_SCALE, &window_scale);

            if (*window_scale == 0)
               return false;

            settings->video.scale = *window_scale;

            if (!settings->video.fullscreen)
               event_cmd_ctl(EVENT_CMD_REINIT, NULL);

            runloop_ctl(RUNLOOP_CTL_SET_WINDOWED_SCALE, &idx);
         }
         break;
      case EVENT_CMD_MENU_TOGGLE:
#ifdef HAVE_MENU
         if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
            rarch_ctl(RARCH_CTL_MENU_RUNNING_FINISHED, NULL);
         else
            rarch_ctl(RARCH_CTL_MENU_RUNNING, NULL);
#endif
         break;
      case EVENT_CMD_CONTROLLERS_INIT:
         event_init_controllers();
         break;
      case EVENT_CMD_RESET:
         RARCH_LOG("%s.\n", msg_hash_to_str(MSG_RESET));
         runloop_msg_queue_push(msg_hash_to_str(MSG_RESET), 1, 120, true);

#ifdef HAVE_CHEEVOS
         cheevos_ctl(CHEEVOS_CTL_SET_CHEATS, NULL);
#endif
         core_ctl(CORE_CTL_RETRO_RESET, NULL);
         break;
      case EVENT_CMD_SAVE_STATE:
#ifdef HAVE_CHEEVOS
         if (settings->cheevos.hardcore_mode_enable)
            return false;
#endif

         if (settings->savestate_auto_index)
            settings->state_slot++;

         event_main_state(cmd);
         break;
      case EVENT_CMD_SAVE_STATE_DECREMENT:
         /* Slot -1 is (auto) slot. */
         if (settings->state_slot >= 0)
            settings->state_slot--;
         break;
      case EVENT_CMD_SAVE_STATE_INCREMENT:
         settings->state_slot++;
         break;
      case EVENT_CMD_TAKE_SCREENSHOT:
         if (!take_screenshot())
            return false;
         break;
      case EVENT_CMD_UNLOAD_CORE:
         runloop_ctl(RUNLOOP_CTL_PREPARE_DUMMY, NULL);
         event_cmd_ctl(EVENT_CMD_LOAD_CORE_DEINIT, NULL);
         break;
      case EVENT_CMD_QUIT:
         rarch_ctl(RARCH_CTL_QUIT, NULL);
         break;
      case EVENT_CMD_CHEEVOS_HARDCORE_MODE_TOGGLE:
#ifdef HAVE_CHEEVOS
         cheevos_ctl(CHEEVOS_CTL_TOGGLE_HARDCORE_MODE, NULL);
#endif
         break;
      case EVENT_CMD_REINIT:
         {
            struct retro_hw_render_callback *hwr = NULL;
            video_driver_ctl(RARCH_DISPLAY_CTL_HW_CONTEXT_GET, &hwr);

            if (hwr->cache_context)
               video_driver_ctl(
                     RARCH_DISPLAY_CTL_SET_VIDEO_CACHE_CONTEXT, NULL);
            else
               video_driver_ctl(
                     RARCH_DISPLAY_CTL_UNSET_VIDEO_CACHE_CONTEXT, NULL);

            video_driver_ctl(
                  RARCH_DISPLAY_CTL_UNSET_VIDEO_CACHE_CONTEXT_ACK, NULL);
            event_cmd_ctl(EVENT_CMD_RESET_CONTEXT, NULL);
            video_driver_ctl(
                  RARCH_DISPLAY_CTL_UNSET_VIDEO_CACHE_CONTEXT, NULL);

            /* Poll input to avoid possibly stale data to corrupt things. */
            input_driver_ctl(RARCH_INPUT_CTL_POLL, NULL);

#ifdef HAVE_MENU
            menu_display_ctl(
                  MENU_DISPLAY_CTL_SET_FRAMEBUFFER_DIRTY_FLAG, NULL);

            if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
               event_cmd_ctl(EVENT_CMD_VIDEO_SET_BLOCKING_STATE, NULL);
#endif
         }
         break;
      case EVENT_CMD_CHEATS_DEINIT:
         cheat_manager_state_free();
         break;
      case EVENT_CMD_CHEATS_INIT:
         event_cmd_ctl(EVENT_CMD_CHEATS_DEINIT, NULL);
         event_init_cheats();
         break;
      case EVENT_CMD_CHEATS_APPLY:
         cheat_manager_apply_cheats();
         break;
      case EVENT_CMD_REWIND_DEINIT:
#ifdef HAVE_NETPLAY
         if (netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
            return false;
#endif
#ifdef HAVE_CHEEVOS
         if (settings->cheevos.hardcore_mode_enable)
            return false;
#endif

         state_manager_event_deinit();
         break;
      case EVENT_CMD_REWIND_INIT:
#ifdef HAVE_CHEEVOS
         if (settings->cheevos.hardcore_mode_enable)
            return false;
#endif
#ifdef HAVE_NETPLAY
         if (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL))
#endif
            init_rewind();
         break;
      case EVENT_CMD_REWIND_TOGGLE:
         if (settings->rewind_enable)
            event_cmd_ctl(EVENT_CMD_REWIND_INIT, NULL);
         else
            event_cmd_ctl(EVENT_CMD_REWIND_DEINIT, NULL);
         break;
      case EVENT_CMD_AUTOSAVE_DEINIT:
#ifdef HAVE_THREADS
         {
            global_t  *global         = global_get_ptr();
            if (!global->sram.use)
               return false;
            autosave_event_deinit();
         }
#endif
         break;
      case EVENT_CMD_AUTOSAVE_INIT:
         event_cmd_ctl(EVENT_CMD_AUTOSAVE_DEINIT, NULL);
#ifdef HAVE_THREADS
         autosave_event_init();
#endif
         break;
      case EVENT_CMD_AUTOSAVE_STATE:
         event_save_auto_state();
         break;
      case EVENT_CMD_AUDIO_STOP:
         if (!audio_driver_ctl(RARCH_AUDIO_CTL_ALIVE, NULL))
            return false;

         if (!audio_driver_ctl(RARCH_AUDIO_CTL_STOP, NULL))
            return false;
         break;
      case EVENT_CMD_AUDIO_START:
         if (audio_driver_ctl(RARCH_AUDIO_CTL_ALIVE, NULL))
            return false;

         if (!settings->audio.mute_enable && 
               !audio_driver_ctl(RARCH_AUDIO_CTL_START, NULL))
         {
            RARCH_ERR("Failed to start audio driver. "
                  "Will continue without audio.\n");
            audio_driver_ctl(RARCH_AUDIO_CTL_UNSET_ACTIVE, NULL);
         }
         break;
      case EVENT_CMD_AUDIO_MUTE_TOGGLE:
         {
            const char *msg = !settings->audio.mute_enable ?
               msg_hash_to_str(MSG_AUDIO_MUTED):
               msg_hash_to_str(MSG_AUDIO_UNMUTED);

            if (!audio_driver_ctl(RARCH_AUDIO_CTL_MUTE_TOGGLE, NULL))
            {
               RARCH_ERR("%s.\n",
                     msg_hash_to_str(MSG_FAILED_TO_UNMUTE_AUDIO));
               return false;
            }

            runloop_msg_queue_push(msg, 1, 180, true);
            RARCH_LOG("%s\n", msg);
         }
         break;
      case EVENT_CMD_OVERLAY_DEINIT:
#ifdef HAVE_OVERLAY
         input_overlay_free();
#endif
         break;
      case EVENT_CMD_OVERLAY_INIT:
         event_cmd_ctl(EVENT_CMD_OVERLAY_DEINIT, NULL);
#ifdef HAVE_OVERLAY
         input_overlay_init();
#endif
         break;
      case EVENT_CMD_OVERLAY_NEXT:
#ifdef HAVE_OVERLAY
         input_overlay_next(settings->input.overlay_opacity);
#endif
         break;
      case EVENT_CMD_DSP_FILTER_DEINIT:
         audio_driver_dsp_filter_free();
         break;
      case EVENT_CMD_DSP_FILTER_INIT:
         event_cmd_ctl(EVENT_CMD_DSP_FILTER_DEINIT, NULL);
         if (!*settings->path.audio_dsp_plugin)
            break;
         audio_driver_dsp_filter_init(settings->path.audio_dsp_plugin);
         break;
      case EVENT_CMD_GPU_RECORD_DEINIT:
         video_driver_ctl(RARCH_DISPLAY_CTL_GPU_RECORD_DEINIT, NULL);
         break;
      case EVENT_CMD_RECORD_DEINIT:
         if (!recording_deinit())
            return false;
         break;
      case EVENT_CMD_RECORD_INIT:
         event_cmd_ctl(EVENT_CMD_HISTORY_DEINIT, NULL);
         if (!recording_init())
            return false;
         break;
      case EVENT_CMD_HISTORY_DEINIT:
         if (g_defaults.history)
         {
            content_playlist_write_file(g_defaults.history);
            content_playlist_free(g_defaults.history);
         }
         g_defaults.history = NULL;
         break;
      case EVENT_CMD_HISTORY_INIT:
         event_cmd_ctl(EVENT_CMD_HISTORY_DEINIT, NULL);
         if (!settings->history_list_enable)
            return false;
         RARCH_LOG("%s: [%s].\n",
               msg_hash_to_str(MSG_LOADING_HISTORY_FILE),
               settings->path.content_history);
         g_defaults.history = content_playlist_init(
               settings->path.content_history,
               settings->content_history_size);
         break;
      case EVENT_CMD_CORE_INFO_DEINIT:
         core_info_ctl(CORE_INFO_CTL_LIST_DEINIT, NULL);
         break;
      case EVENT_CMD_CORE_INFO_INIT:
         event_cmd_ctl(EVENT_CMD_CORE_INFO_DEINIT, NULL);

         if (*settings->directory.libretro)
            core_info_ctl(CORE_INFO_CTL_LIST_INIT, NULL);
         break;
      case EVENT_CMD_CORE_DEINIT:
         {
            struct retro_hw_render_callback *hwr = NULL;
            video_driver_ctl(RARCH_DISPLAY_CTL_HW_CONTEXT_GET, &hwr);
            event_deinit_core(true);

            if (hwr)
               memset(hwr, 0, sizeof(*hwr));

            break;
         }
      case EVENT_CMD_CORE_INIT:
         if (!event_init_core(data))
            return false;
         break;
      case EVENT_CMD_VIDEO_APPLY_STATE_CHANGES:
         video_driver_ctl(RARCH_DISPLAY_CTL_APPLY_STATE_CHANGES, NULL);
         break;
      case EVENT_CMD_VIDEO_SET_NONBLOCKING_STATE:
         boolean = true; /* fall-through */
      case EVENT_CMD_VIDEO_SET_BLOCKING_STATE:
         video_driver_ctl(RARCH_DISPLAY_CTL_SET_NONBLOCK_STATE, &boolean);
         break;
      case EVENT_CMD_VIDEO_SET_ASPECT_RATIO:
         video_driver_ctl(RARCH_DISPLAY_CTL_SET_ASPECT_RATIO, NULL);
         break;
      case EVENT_CMD_AUDIO_SET_NONBLOCKING_STATE:
         boolean = true; /* fall-through */
      case EVENT_CMD_AUDIO_SET_BLOCKING_STATE:
         audio_driver_set_nonblocking_state(boolean);
         break;
      case EVENT_CMD_OVERLAY_SET_SCALE_FACTOR:
#ifdef HAVE_OVERLAY
         input_overlay_set_scale_factor(settings->input.overlay_scale);
#endif
         break;
      case EVENT_CMD_OVERLAY_SET_ALPHA_MOD:
#ifdef HAVE_OVERLAY
         input_overlay_set_alpha_mod(settings->input.overlay_opacity);
#endif
         break;
      case EVENT_CMD_AUDIO_REINIT:
         {
            int flags = DRIVER_AUDIO;
            driver_ctl(RARCH_DRIVER_CTL_UNINIT, &flags);
            driver_ctl(RARCH_DRIVER_CTL_INIT, &flags);
         }
         break;
      case EVENT_CMD_RESET_CONTEXT:
         {
            /* RARCH_DRIVER_CTL_UNINIT clears the callback struct so we
             * need to make sure to keep a copy */
            struct retro_hw_render_callback *hwr = NULL;
            struct retro_hw_render_callback hwr_copy;
            int flags = DRIVERS_CMD_ALL;

            video_driver_ctl(RARCH_DISPLAY_CTL_HW_CONTEXT_GET, &hwr);

            memcpy(&hwr_copy, hwr, sizeof(hwr_copy));

            driver_ctl(RARCH_DRIVER_CTL_UNINIT, &flags);

            memcpy(hwr, &hwr_copy, sizeof(*hwr));

            driver_ctl(RARCH_DRIVER_CTL_INIT, &flags);
         }
         break;
      case EVENT_CMD_QUIT_RETROARCH:
         rarch_ctl(RARCH_CTL_FORCE_QUIT, NULL);
         break;
      case EVENT_CMD_SHUTDOWN:
#if defined(__linux__) && !defined(ANDROID)
         runloop_msg_queue_push("Shutting down...", 1, 180, true);
         rarch_ctl(RARCH_CTL_FORCE_QUIT, NULL);
         system("shutdown -P now");
#endif
         break;
      case EVENT_CMD_REBOOT:
#if defined(__linux__) && !defined(ANDROID)
         runloop_msg_queue_push("Rebooting...", 1, 180, true);
         rarch_ctl(RARCH_CTL_FORCE_QUIT, NULL);
         system("shutdown -r now");
#endif
         break;
      case EVENT_CMD_RESUME:
         rarch_ctl(RARCH_CTL_MENU_RUNNING_FINISHED, NULL);
         if (ui_companion_is_on_foreground())
            ui_companion_driver_toggle();
         break;
      case EVENT_CMD_RESTART_RETROARCH:
         if (!frontend_driver_set_fork(FRONTEND_FORK_RESTART))
            return false;
         break;
      case EVENT_CMD_MENU_SAVE_CURRENT_CONFIG:
         event_save_current_config();
         break;
      case EVENT_CMD_MENU_SAVE_CONFIG:
         if (!event_save_core_config())
            return false;
         break;
      case EVENT_CMD_SHADERS_APPLY_CHANGES:
#ifdef HAVE_MENU
         menu_shader_manager_apply_changes();
#endif
         break;
      case EVENT_CMD_PAUSE_CHECKS:
         if (runloop_ctl(RUNLOOP_CTL_IS_PAUSED, NULL))
         {
            RARCH_LOG("%s\n", msg_hash_to_str(MSG_PAUSED));
            event_cmd_ctl(EVENT_CMD_AUDIO_STOP, NULL);

            if (settings->video.black_frame_insertion)
               video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER, NULL);
         }
         else
         {
            RARCH_LOG("%s\n", msg_hash_to_str(MSG_UNPAUSED));
            event_cmd_ctl(EVENT_CMD_AUDIO_START, NULL);
         }
         break;
      case EVENT_CMD_PAUSE_TOGGLE:
         boolean = runloop_ctl(RUNLOOP_CTL_IS_PAUSED,  NULL);
         boolean = !boolean;
         runloop_ctl(RUNLOOP_CTL_SET_PAUSED, &boolean);
         event_cmd_ctl(EVENT_CMD_PAUSE_CHECKS, NULL);
         break;
      case EVENT_CMD_UNPAUSE:
         boolean = false;

         runloop_ctl(RUNLOOP_CTL_SET_PAUSED, &boolean);
         event_cmd_ctl(EVENT_CMD_PAUSE_CHECKS, NULL);
         break;
      case EVENT_CMD_PAUSE:
         boolean = true;

         runloop_ctl(RUNLOOP_CTL_SET_PAUSED, &boolean);
         event_cmd_ctl(EVENT_CMD_PAUSE_CHECKS, NULL);
         break;
      case EVENT_CMD_MENU_PAUSE_LIBRETRO:
#ifdef HAVE_MENU
         if (menu_driver_ctl(RARCH_MENU_CTL_IS_ALIVE, NULL))
         {
            if (settings->menu.pause_libretro)
               event_cmd_ctl(EVENT_CMD_AUDIO_STOP, NULL);
            else
               event_cmd_ctl(EVENT_CMD_AUDIO_START, NULL);
         }
         else
         {
            if (settings->menu.pause_libretro)
               event_cmd_ctl(EVENT_CMD_AUDIO_START, NULL);
         }
#endif
         break;
      case EVENT_CMD_SHADER_DIR_DEINIT:
         runloop_ctl(RUNLOOP_CTL_SHADER_DIR_DEINIT, NULL);
         break;
      case EVENT_CMD_SHADER_DIR_INIT:
         event_cmd_ctl(EVENT_CMD_SHADER_DIR_DEINIT, NULL);

         if (!runloop_ctl(RUNLOOP_CTL_SHADER_DIR_INIT, NULL))
            return false;
         break;
      case EVENT_CMD_SAVEFILES:
         {
            global_t  *global         = global_get_ptr();
            if (!global->savefiles || !global->sram.use)
               return false;

            for (i = 0; i < global->savefiles->size; i++)
            {
               ram_type_t ram;
               ram.type    = global->savefiles->elems[i].attr.i;
               ram.path    = global->savefiles->elems[i].data;

               RARCH_LOG("%s #%u %s \"%s\".\n",
                     msg_hash_to_str(MSG_SAVING_RAM_TYPE),
                     ram.type,
                     msg_hash_to_str(MSG_TO),
                     ram.path);
               content_ctl(CONTENT_CTL_SAVE_RAM_FILE, &ram);
            }
         }
         return true;
      case EVENT_CMD_SAVEFILES_DEINIT:
         {
            global_t  *global         = global_get_ptr();
            if (!global)
               break;

            if (global->savefiles)
               string_list_free(global->savefiles);
            global->savefiles = NULL;
         }
         break;
      case EVENT_CMD_SAVEFILES_INIT:
         {
            global_t  *global         = global_get_ptr();
            global->sram.use = global->sram.use && !global->sram.save_disable;
#ifdef HAVE_NETPLAY
            global->sram.use = global->sram.use && 
               (!netplay_driver_ctl(RARCH_NETPLAY_CTL_IS_DATA_INITED, NULL)
                || !global->netplay.is_client);
#endif

            if (!global->sram.use)
               RARCH_LOG("%s\n",
                     msg_hash_to_str(MSG_SRAM_WILL_NOT_BE_SAVED));

            if (global->sram.use)
               event_cmd_ctl(EVENT_CMD_AUTOSAVE_INIT, NULL);
         }
         break;
      case EVENT_CMD_BSV_MOVIE_DEINIT:
         bsv_movie_ctl(BSV_MOVIE_CTL_DEINIT, NULL);
         break;
      case EVENT_CMD_BSV_MOVIE_INIT:
         event_cmd_ctl(EVENT_CMD_BSV_MOVIE_DEINIT, NULL);
         bsv_movie_ctl(BSV_MOVIE_CTL_INIT, NULL);
         break;
      case EVENT_CMD_NETPLAY_DEINIT:
#ifdef HAVE_NETPLAY
         deinit_netplay();
#endif
         break;
      case EVENT_CMD_NETWORK_DEINIT:
#ifdef HAVE_NETWORKING
         network_deinit();
#endif
         break;
      case EVENT_CMD_NETWORK_INIT:
#ifdef HAVE_NETWORKING
         network_init();
#endif
         break;
      case EVENT_CMD_NETPLAY_INIT:
         event_cmd_ctl(EVENT_CMD_NETPLAY_DEINIT, NULL);
#ifdef HAVE_NETPLAY
         if (!init_netplay())
            return false;
#endif
         break;
      case EVENT_CMD_NETPLAY_FLIP_PLAYERS:
#ifdef HAVE_NETPLAY
         netplay_driver_ctl(RARCH_NETPLAY_CTL_FLIP_PLAYERS, NULL);
#endif
         break;
      case EVENT_CMD_FULLSCREEN_TOGGLE:
         if (!video_driver_ctl(RARCH_DISPLAY_CTL_HAS_WINDOWED, NULL))
            return false;

         /* If we go fullscreen we drop all drivers and
          * reinitialize to be safe. */
         settings->video.fullscreen = !settings->video.fullscreen;
         event_cmd_ctl(EVENT_CMD_REINIT, NULL);
         break;
      case EVENT_CMD_COMMAND_DEINIT:
         input_driver_ctl(RARCH_INPUT_CTL_COMMAND_DEINIT, NULL);
         break;
      case EVENT_CMD_COMMAND_INIT:
         event_cmd_ctl(EVENT_CMD_COMMAND_DEINIT, NULL);
         input_driver_ctl(RARCH_INPUT_CTL_COMMAND_INIT, NULL);
         break;
      case EVENT_CMD_REMOTE_DEINIT:
         input_driver_ctl(RARCH_INPUT_CTL_REMOTE_DEINIT, NULL);
         break;
      case EVENT_CMD_REMOTE_INIT:
         event_cmd_ctl(EVENT_CMD_REMOTE_DEINIT, NULL);
         input_driver_ctl(RARCH_INPUT_CTL_REMOTE_INIT, NULL);
         break;
      case EVENT_CMD_TEMPORARY_CONTENT_DEINIT:
         content_ctl(CONTENT_CTL_DEINIT, NULL);
         break;
      case EVENT_CMD_SUBSYSTEM_FULLPATHS_DEINIT:
         {
            global_t  *global         = global_get_ptr();
            if (!global)
               break;

            if (global->subsystem_fullpaths)
               string_list_free(global->subsystem_fullpaths);
            global->subsystem_fullpaths = NULL;
         }
         break;
      case EVENT_CMD_LOG_FILE_DEINIT:
         retro_main_log_file_deinit();
         break;
      case EVENT_CMD_DISK_APPEND_IMAGE:
         {
            const char *path = (const char*)data;
            if (string_is_empty(path))
               return false;
            return event_disk_control_append_image(path);
         }
      case EVENT_CMD_DISK_EJECT_TOGGLE:
         if (info && info->disk_control_cb.get_num_images)
         {
            const struct retro_disk_control_callback *control =
               (const struct retro_disk_control_callback*)
               &info->disk_control_cb;

            if (control)
            {
               bool new_state = !control->get_eject_state();
               event_disk_control_set_eject(new_state, true);
            }
         }
         else
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                  1, 120, true);
         break;
      case EVENT_CMD_DISK_NEXT:
         if (info && info->disk_control_cb.get_num_images)
         {
            const struct retro_disk_control_callback *control =
               (const struct retro_disk_control_callback*)
               &info->disk_control_cb;

            if (!control)
               return false;

            if (!control->get_eject_state())
               return false;

            event_check_disk_next(control);
         }
         else
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                  1, 120, true);
         break;
      case EVENT_CMD_DISK_PREV:
         if (info && info->disk_control_cb.get_num_images)
         {
            const struct retro_disk_control_callback *control =
               (const struct retro_disk_control_callback*)
               &info->disk_control_cb;

            if (!control)
               return false;

            if (!control->get_eject_state())
               return false;

            event_check_disk_prev(control);
         }
         else
            runloop_msg_queue_push(
                  msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS),
                  1, 120, true);
         break;
      case EVENT_CMD_RUMBLE_STOP:
         for (i = 0; i < MAX_USERS; i++)
         {
            input_driver_set_rumble_state(i, RETRO_RUMBLE_STRONG, 0);
            input_driver_set_rumble_state(i, RETRO_RUMBLE_WEAK, 0);
         }
         break;
      case EVENT_CMD_GRAB_MOUSE_TOGGLE:
         {
            bool ret = false;
            static bool grab_mouse_state  = false;

            grab_mouse_state = !grab_mouse_state;

            if (grab_mouse_state)
               ret = input_driver_ctl(RARCH_INPUT_CTL_GRAB_MOUSE, NULL);
            else
               ret = input_driver_ctl(RARCH_INPUT_CTL_UNGRAB_MOUSE, NULL);

            if (!ret)
               return false;

            RARCH_LOG("%s: %s.\n",
                  msg_hash_to_str(MSG_GRAB_MOUSE_STATE),
                  grab_mouse_state ? "yes" : "no");

            if (grab_mouse_state)
               video_driver_ctl(RARCH_DISPLAY_CTL_HIDE_MOUSE, NULL);
            else
               video_driver_ctl(RARCH_DISPLAY_CTL_SHOW_MOUSE, NULL);
         }
         break;
      case EVENT_CMD_PERFCNT_REPORT_FRONTEND_LOG:
         rarch_perf_log();
         break;
      case EVENT_CMD_VOLUME_UP:
         event_set_volume(0.5f);
         break;
      case EVENT_CMD_VOLUME_DOWN:
         event_set_volume(-0.5f);
         break;
      case EVENT_CMD_SET_FRAME_LIMIT:
         runloop_ctl(RUNLOOP_CTL_SET_FRAME_LIMIT, NULL);
         break;
      case EVENT_CMD_EXEC:
         return event_cmd_exec(data);
      case EVENT_CMD_NONE:
      default:
         return false;
   }

   return true;
}

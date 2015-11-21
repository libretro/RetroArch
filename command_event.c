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

#include <ctype.h>

#include <compat/strl.h>

#include "command_event.h"

#include "general.h"
#include "performance.h"
#include "dynamic.h"
#include "content.h"
#include "screenshot.h"
#include "msg_hash.h"
#include "retroarch.h"
#include "dir_list_special.h"

#ifdef HAVE_CHEEVOS
#include "cheevos.h"
#endif

#include "runloop_data.h"
#include "configuration.h"
#include "input/input_remapping.h"

#ifdef HAVE_MENU
#include "menu/menu.h"
#include "menu/menu_display.h"
#include "menu/menu_shader.h"
#include "menu/menu_input.h"
#endif

#ifdef HAVE_NETPLAY
#include "netplay.h"
#endif

#ifdef HAVE_NETWORKING
#include <net/net_compat.h>
#endif

#ifdef HAVE_COMMAND
static void event_init_command(void)
{
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();

   if (!settings->stdin_cmd_enable && !settings->network_cmd_enable)
      return;

   if (settings->stdin_cmd_enable && input_driver_grab_stdin())
   {
      RARCH_WARN("stdin command interface is desired, but input driver has already claimed stdin.\n"
            "Cannot use this command interface.\n");
   }

   if (!(driver->command = rarch_cmd_new(settings->stdin_cmd_enable
               && !input_driver_grab_stdin(),
               settings->network_cmd_enable, settings->network_cmd_port)))
      RARCH_ERR("Failed to initialize command interface.\n");
}
#endif

/**
 * event_free_temporary_content:
 *
 * Frees temporary content handle.
 **/
static void event_free_temporary_content(void)
{
   unsigned i;
   global_t *global = global_get_ptr();

   for (i = 0; i < global->temporary_content->size; i++)
   {
      const char *path = global->temporary_content->elems[i].data;

      RARCH_LOG("%s: %s.\n",
            msg_hash_to_str(MSG_REMOVING_TEMPORARY_CONTENT_FILE), path);
      if (remove(path) < 0)
         RARCH_ERR("%s: %s.\n",
               msg_hash_to_str(MSG_FAILED_TO_REMOVE_TEMPORARY_FILE),
               path);
   }
   string_list_free(global->temporary_content);
}

#if defined(HAVE_THREADS)
static void event_init_autosave(void)
{
   unsigned i;
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (settings->autosave_interval < 1 || !global->savefiles)
      return;

   if (!(global->autosave.list = (autosave_t**)calloc(global->savefiles->size,
               sizeof(*global->autosave.list))))
      return;

   global->autosave.num = global->savefiles->size;

   for (i = 0; i < global->savefiles->size; i++)
   {
      const char *path = global->savefiles->elems[i].data;
      unsigned    type = global->savefiles->elems[i].attr.i;

      if (core.retro_get_memory_size(type) <= 0)
         continue;

      global->autosave.list[i] = autosave_new(path,
            core.retro_get_memory_data(type),
            core.retro_get_memory_size(type),
            settings->autosave_interval);

      if (!global->autosave.list[i])
         RARCH_WARN("%s\n", msg_hash_to_str(MSG_AUTOSAVE_FAILED));
   }
}

static void event_deinit_autosave(void)
{
   unsigned i;
   global_t *global = global_get_ptr();

   for (i = 0; i < global->autosave.num; i++)
      autosave_free(global->autosave.list[i]);

   if (global->autosave.list)
      free(global->autosave.list);

   global->autosave.list     = NULL;
   global->autosave.num      = 0;
}
#endif

static void event_save_files(void)
{
   unsigned i;
   global_t *global = global_get_ptr();

   if (!global->savefiles || !global->sram.use)
      return;

   for (i = 0; i < global->savefiles->size; i++)
   {
      unsigned type    = global->savefiles->elems[i].attr.i;
      const char *path = global->savefiles->elems[i].data;
      RARCH_LOG("%s #%u %s \"%s\".\n",
            msg_hash_to_str(MSG_SAVING_RAM_TYPE),
            type,
            msg_hash_to_str(MSG_TO),
            path);
      save_ram_file(path, type);
   }
}

static void event_init_movie(void)
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (global->bsv.movie_start_playback)
   {
      if (!(global->bsv.movie = bsv_movie_init(global->bsv.movie_start_path,
                  RARCH_MOVIE_PLAYBACK)))
      {
         RARCH_ERR("%s: \"%s\".\n",
               msg_hash_to_str(MSG_FAILED_TO_LOAD_MOVIE_FILE),
               global->bsv.movie_start_path);
         retro_fail(1, "event_init_movie()");
      }

      global->bsv.movie_playback = true;
      rarch_main_msg_queue_push_new(MSG_STARTING_MOVIE_PLAYBACK, 2, 180, false);
      RARCH_LOG("%s.\n", msg_hash_to_str(MSG_STARTING_MOVIE_PLAYBACK));
      settings->rewind_granularity = 1;
   }
   else if (global->bsv.movie_start_recording)
   {
      char msg[PATH_MAX_LENGTH] = {0};
      snprintf(msg, sizeof(msg),
            "%s \"%s\".",
            msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
            global->bsv.movie_start_path);

      if (!(global->bsv.movie = bsv_movie_init(global->bsv.movie_start_path,
                  RARCH_MOVIE_RECORD)))
      {
         rarch_main_msg_queue_push_new(MSG_FAILED_TO_START_MOVIE_RECORD, 1, 180, true);
         RARCH_ERR("%s.\n", msg_hash_to_str(MSG_FAILED_TO_START_MOVIE_RECORD));
         return;
      }

      rarch_main_msg_queue_push(msg, 1, 180, true);
      RARCH_LOG("%s \"%s\".\n",
            msg_hash_to_str(MSG_STARTING_MOVIE_RECORD_TO),
            global->bsv.movie_start_path);
      settings->rewind_granularity = 1;
   }
}

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
   char msg[PATH_MAX_LENGTH] = {0};
   bool error                = false;
   rarch_system_info_t *info = rarch_system_info_get_ptr();
   const struct retro_disk_control_callback *control =
      info ? (const struct retro_disk_control_callback*)&info->disk_control : NULL;

   if (!control || !control->get_num_images)
      return;

   *msg = '\0';

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
         rarch_main_msg_queue_push(msg, 1, 180, true);
   }
}

/**
 * event_check_disk_eject:
 * @control              : Handle to disk control handle.
 *
 * Perform disk eject (Core Disk Options).
 **/
static void event_check_disk_eject(
      const struct retro_disk_control_callback *control)
{
   bool new_state = !control->get_eject_state();
   event_disk_control_set_eject(new_state, true);
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
   char msg[PATH_MAX_LENGTH] = {0};
   rarch_system_info_t                      *info    = rarch_system_info_get_ptr();
   const struct retro_disk_control_callback *control =
      info ? (const struct retro_disk_control_callback*)&info->disk_control : NULL;
   bool error = false;

   if (!control || !control->get_num_images)
      return;

   *msg = '\0';

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
      rarch_main_msg_queue_push(msg, 1, 180, true);
   }
}

/**
 * event_disk_control_append_image:
 * @path                 : Path to disk image.
 *
 * Appends disk image to disk image list.
 **/
void event_disk_control_append_image(const char *path)
{
   unsigned new_idx;
   char msg[PATH_MAX_LENGTH]                         = {0};
   struct retro_game_info info                       = {0};
   global_t                                  *global = global_get_ptr();
   rarch_system_info_t                       *sysinfo = rarch_system_info_get_ptr();
   const struct retro_disk_control_callback *control =
      sysinfo ? (const struct retro_disk_control_callback*)&sysinfo->disk_control
      : NULL;

   if (!control)
      return;

   event_disk_control_set_eject(true, false);

   control->add_image_index();
   new_idx = control->get_num_images();
   if (!new_idx)
      return;
   new_idx--;

   info.path = path;
   control->replace_image_index(new_idx, &info);

   snprintf(msg, sizeof(msg), "%s: ", msg_hash_to_str(MSG_APPENDED_DISK));
   strlcat(msg, path, sizeof(msg));
   RARCH_LOG("%s\n", msg);
   rarch_main_msg_queue_push(msg, 0, 180, true);

   event_command(EVENT_CMD_AUTOSAVE_DEINIT);

   /* TODO: Need to figure out what to do with subsystems case. */
   if (!*global->subsystem)
   {
      /* Update paths for our new image.
       * If we actually use append_image, we assume that we
       * started out in a single disk case, and that this way
       * of doing it makes the most sense. */
      rarch_set_paths(path);
      rarch_ctl(RARCH_ACTION_STATE_FILL_PATHNAMES, NULL);
   }

   event_command(EVENT_CMD_AUTOSAVE_INIT);
   event_disk_control_set_index(new_idx);
   event_disk_control_set_eject(false, false);
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

   if (!control)
      return;
   if (!control->get_num_images)
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

   if (!control)
      return;
   if (!control->get_num_images)
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
   char msg[PATH_MAX_LENGTH] = {0};
   settings_t *settings      = config_get_ptr();

   settings->audio.volume += gain;
   settings->audio.volume  = max(settings->audio.volume, -80.0f);
   settings->audio.volume  = min(settings->audio.volume, 12.0f);

   snprintf(msg, sizeof(msg), "Volume: %.1f dB", settings->audio.volume);
   rarch_main_msg_queue_push(msg, 1, 180, true);
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
   settings_t *settings = config_get_ptr();
   rarch_system_info_t *info = rarch_system_info_get_ptr();

   for (i = 0; i < MAX_USERS; i++)
   {
      const char *ident = NULL;
      const struct retro_controller_description *desc = NULL;
      unsigned device = settings->input.libretro_device[i];

      if (i < info->num_ports)
         desc = libretro_find_controller_description(
               &info->ports[i], device);

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
            RARCH_WARN("Input device ID %u is unknown to this libretro implementation. Using RETRO_DEVICE_JOYPAD.\n", device);
            device = RETRO_DEVICE_JOYPAD;
         }
         ident = "Joypad";
      }

      switch (device)
      {
         case RETRO_DEVICE_NONE:
            RARCH_LOG("Disconnecting device from port %u.\n", i + 1);
            core.retro_set_controller_port_device(i, device);
            break;
         case RETRO_DEVICE_JOYPAD:
            break;
         default:
            /* Some cores do not properly range check port argument.
             * This is broken behavior of course, but avoid breaking
             * cores needlessly. */
            RARCH_LOG("Connecting %s (ID: %u) to port %u.\n", ident,
                  device, i + 1);
            core.retro_set_controller_port_device(i, device);
            break;
      }
   }
}

static void event_deinit_core(bool reinit)
{
   global_t *global     = global_get_ptr();

#ifdef HAVE_CHEEVOS
   /* Unload the achievements from memory. */
   cheevos_unload();
#endif

   core.retro_unload_game();
   core.retro_deinit();

   if (reinit)
      event_command(EVENT_CMD_DRIVERS_DEINIT);

  /* auto overrides: reload the original config */
   if(global->overrides_active)
   {
      config_unload_override();
      global->overrides_active = false;
   }

   uninit_libretro_sym();
}

static void event_init_cheats(void)
{
   bool allow_cheats = true;
   driver_t *driver  = driver_get_ptr();
   global_t *global  = global_get_ptr();

   (void)driver;

#ifdef HAVE_NETPLAY
   allow_cheats &= !driver->netplay_data;
#endif
   allow_cheats &= !global->bsv.movie;

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
      load_ram_file(global->savefiles->elems[i].data,
            global->savefiles->elems[i].attr.i);

   return true;
}

static void event_load_auto_state(void)
{
   bool ret;
   char msg[PATH_MAX_LENGTH]                 = {0};
   char savestate_name_auto[PATH_MAX_LENGTH] = {0};
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

#ifdef HAVE_NETPLAY
   if (global->netplay.enable && !global->netplay.is_spectate)
      return;
#endif

   if (!settings->savestate_auto_load)
      return;

   fill_pathname_noext(savestate_name_auto, global->name.savestate,
         ".auto", sizeof(savestate_name_auto));

   if (!path_file_exists(savestate_name_auto))
      return;

   ret = load_state(savestate_name_auto);

   RARCH_LOG("Found auto savestate in: %s\n", savestate_name_auto);

   snprintf(msg, sizeof(msg), "Auto-loading savestate from \"%s\" %s.",
         savestate_name_auto, ret ? "succeeded" : "failed");
   rarch_main_msg_queue_push(msg, 1, 180, false);
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
      char elem_base[PATH_MAX_LENGTH] = {0};
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
   global_t *global = global_get_ptr();

   /* No content to be loaded for dummy core,
    * just successfully exit. */
   if (global->inited.core.type == CORE_TYPE_DUMMY)
      return true;

   if (!global->inited.core.no_content)
      rarch_ctl(RARCH_ACTION_STATE_FILL_PATHNAMES, NULL);

   if (!init_content_file())
      return false;

   if (global->inited.core.no_content)
      return true;

   event_set_savestate_auto_index();

   if (event_load_save_files())
      RARCH_LOG("%s.\n",
            msg_hash_to_str(MSG_SKIPPING_SRAM_LOAD));

   event_load_auto_state();
   event_command(EVENT_CMD_BSV_MOVIE_INIT);
   event_command(EVENT_CMD_NETPLAY_INIT);

   return true;
}

static bool event_init_core(void)
{
   global_t *global     = global_get_ptr();
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();

   /* auto overrides: apply overrides */
   if(settings->auto_overrides_enable)
   {
      if (config_load_override())
         global->overrides_active = true;
      else
         global->overrides_active = false;
   }

   /* reset video format to libretro's default */
   video_driver_set_pixel_format(RETRO_PIXEL_FORMAT_0RGB1555);

   core.retro_set_environment(rarch_environment_cb);

   /* auto-remap: apply remap files */
   if(settings->auto_remaps_enable)
      config_load_remap();

   /* per-core saves: reset redirection paths */
   if((settings->sort_savestates_enable || settings->sort_savefiles_enable) && !global->inited.core.no_content)
      set_paths_redirect(global->name.base);

   rarch_ctl(RARCH_ACTION_STATE_VERIFY_API_VERSION, NULL);
   core.retro_init();

   global->sram.use = (global->inited.core.type == CORE_TYPE_PLAIN) &&
      !global->inited.core.no_content;

   if (!event_init_content())
      return false;

   retro_init_libretro_cbs(&driver->retro_ctx);
   rarch_init_system_av_info();

   return true;
}

static bool event_save_auto_state(void)
{
   bool ret;
   char savestate_name_auto[PATH_MAX_LENGTH] = {0};
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (!settings->savestate_auto_save ||
         (global->inited.core.type == CORE_TYPE_DUMMY) ||
       global->inited.core.no_content)
       return false;

   fill_pathname_noext(savestate_name_auto, global->name.savestate,
         ".auto", sizeof(savestate_name_auto));

   ret = save_state(savestate_name_auto);
   RARCH_LOG("Auto save state to \"%s\" %s.\n", savestate_name_auto, ret ?
         "succeeded" : "failed");

   return true;
}

static void event_init_remapping(void)
{
   settings_t *settings = config_get_ptr();
   const char   *path   = settings->input.remapping_path;
   config_file_t *conf  = config_file_new(path);

   if (!settings->input.remap_binds_enable || !conf)
      return;

   input_remapping_load_file(conf, path);
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
   char msg[PATH_MAX_LENGTH]         = {0};
   bool ret                          = false;
   bool found_path                   = false;
   bool overrides_active             = false;
   settings_t *settings              = config_get_ptr();
   global_t   *global                = global_get_ptr();

   *config_dir = '\0';

   if (*settings->menu_config_directory)
      strlcpy(config_dir, settings->menu_config_directory,
            sizeof(config_dir));
   else if (*global->path.config) /* Fallback */
      fill_pathname_basedir(config_dir, global->path.config,
            sizeof(config_dir));
   else
   {
      rarch_main_msg_queue_push_new(MSG_CONFIG_DIRECTORY_NOT_SET, 1, 180, true);
      RARCH_ERR("%s\n", msg_hash_to_str(MSG_CONFIG_DIRECTORY_NOT_SET));
      return false;
   }

   /* Infer file name based on libretro core. */
   if (*settings->libretro && path_file_exists(settings->libretro))
   {
      unsigned i;

      /* In case of collision, find an alternative name. */
      for (i = 0; i < 16; i++)
      {
         char tmp[64] = {0};

         fill_pathname_base(config_name, settings->libretro,
               sizeof(config_name));
         path_remove_extension(config_name);
         fill_pathname_join(config_path, config_dir, config_name,
               sizeof(config_path));

         *tmp = '\0';

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

   /* Overrides block config file saving, make it appear as overrides weren't enabled for a manual save */
   if (global->overrides_active)
   {
      global->overrides_active = false;
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

   rarch_main_msg_queue_push(msg, 1, 180, true);
   global->overrides_active = overrides_active;
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
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

   if (settings->config_save_on_exit && *global->path.config)
   {
      /* Save last core-specific config to the default config location,
       * needed on consoles for core switching and reusing last good
       * config for new cores.
       */
      config_save_file(global->path.config);

      /* Flush out the core specific config. */
      if (*global->path.core_specific_config &&
            settings->core_specific_config)
         config_save_file(global->path.core_specific_config);
   }

   event_command(EVENT_CMD_AUTOSAVE_STATE);
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

   if (!save_state(path))
   {
      snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_FAILED_TO_SAVE_STATE_TO),
            path);
      return;
   }

   if (settings->state_slot < 0)
      snprintf(s, len, "%s #-1 (auto).", msg_hash_to_str(MSG_SAVED_STATE_TO_SLOT));
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

   if (!load_state(path))
   {
      snprintf(s, len, "%s \"%s\".",
            msg_hash_to_str(MSG_FAILED_TO_LOAD_STATE),
            path);
      return;
   }

   if (settings->state_slot < 0)
      snprintf(s, len, "%s #-1 (auto).", msg_hash_to_str(MSG_LOADED_STATE_FROM_SLOT));
   else
      snprintf(s, len, "%s #%d.", msg_hash_to_str(MSG_LOADED_STATE_FROM_SLOT),
            settings->state_slot);
}

static void event_main_state(unsigned cmd)
{
   char path[PATH_MAX_LENGTH] = {0};
   char msg[PATH_MAX_LENGTH]  = {0};
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

   if (core.retro_serialize_size())
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
      strlcpy(msg, msg_hash_to_str(MSG_CORE_DOES_NOT_SUPPORT_SAVESTATES), sizeof(msg));

   rarch_main_msg_queue_push(msg, 2, 180, true);
   RARCH_LOG("%s\n", msg);
}

#ifdef HAVE_MENU
static bool event_update_system_info(struct retro_system_info *_info,
      bool *load_no_content)
{
   settings_t *settings = config_get_ptr();
   global_t   *global   = global_get_ptr();

#if defined(HAVE_DYNAMIC)
   if (!(*settings->libretro))
      return false;

   libretro_get_system_info(settings->libretro, _info,
         load_no_content);
#endif
   if (!global->core_info.list)
      return false;

   if (!core_info_list_get_info(global->core_info.list,
            global->core_info.current, settings->libretro))
      return false;

   return true;
}
#endif

/**
 * event_command:
 * @cmd                  : Event command index.
 *
 * Performs program event command with index @cmd.
 *
 * Returns: true (1) on success, otherwise false (0).
 **/
bool event_command(enum event_command cmd)
{
   unsigned i           = 0;
   bool boolean         = false;
   driver_t  *driver    = driver_get_ptr();
   global_t  *global    = global_get_ptr();
   settings_t *settings = config_get_ptr();
   rarch_system_info_t *info = rarch_system_info_get_ptr();

   (void)i;

   switch (cmd)
   {
      case EVENT_CMD_SET_PER_GAME_RESOLUTION:
#if defined(GEKKO)
         {
            unsigned width = 0, height = 0;

            event_command(EVENT_CMD_VIDEO_SET_ASPECT_RATIO);

            if (video_driver_get_video_output_size(&width, &height))
            {
               video_driver_set_video_mode(width, height, true);

               char msg[PATH_MAX_LENGTH] = {0};
               snprintf(msg, sizeof(msg),"Resolution: %dx%d",width, height);
               rarch_main_msg_queue_push(msg, 1, 100, true);
            }
         }
#endif
         break;
      case EVENT_CMD_LOAD_CONTENT_PERSIST:
#ifdef HAVE_DYNAMIC
         event_command(EVENT_CMD_LOAD_CORE);
#endif
         rarch_ctl(RARCH_ACTION_STATE_LOAD_CONTENT, NULL);
         break;
#ifdef HAVE_FFMPEG
      case EVENT_CMD_LOAD_CONTENT_FFMPEG:
         rarch_ctl(RARCH_ACTION_STATE_LOAD_CONTENT_FFMPEG, NULL);
         break;
#endif
      case EVENT_CMD_LOAD_CONTENT_IMAGEVIEWER:
         rarch_ctl(RARCH_ACTION_STATE_LOAD_CONTENT_IMAGEVIEWER, NULL);
         break;
      case EVENT_CMD_LOAD_CONTENT:
         {
#ifdef HAVE_DYNAMIC
            event_command(EVENT_CMD_LOAD_CONTENT_PERSIST);
#else
            char *fullpath = NULL;
            rarch_main_ctl(RARCH_MAIN_CTL_GET_CONTENT_PATH, &fullpath);
            rarch_environment_cb(RETRO_ENVIRONMENT_SET_LIBRETRO_PATH,
                  (void*)settings->libretro);
            rarch_environment_cb(RETRO_ENVIRONMENT_EXEC, (void*)fullpath);
            event_command(EVENT_CMD_QUIT);
#endif
         }
         break;
      case EVENT_CMD_LOAD_CORE_DEINIT:
#ifdef HAVE_DYNAMIC
#ifdef HAVE_MENU
         libretro_free_system_info(&global->menu.info);
#endif
#endif
         break;
      case EVENT_CMD_LOAD_CORE_PERSIST:
         event_command(EVENT_CMD_LOAD_CORE_DEINIT);
         {
#ifdef HAVE_MENU
            menu_handle_t *menu = menu_driver_get_ptr();
            if (menu)
               event_update_system_info(&global->menu.info,
                     &menu->load_no_content);
#endif
         }
         break;
      case EVENT_CMD_LOAD_CORE:
         event_command(EVENT_CMD_LOAD_CORE_PERSIST);
#ifndef HAVE_DYNAMIC
         event_command(EVENT_CMD_QUIT);
#endif
         break;
      case EVENT_CMD_LOAD_STATE:
         /* Immutable - disallow savestate load when
          * we absolutely cannot change game state. */
         if (global->bsv.movie)
            return false;

#ifdef HAVE_NETPLAY
         if (driver->netplay_data)
            return false;
#endif
         event_main_state(cmd);
         break;
      case EVENT_CMD_RESIZE_WINDOWED_SCALE:
         {
            unsigned idx = 0;
            if (global->pending.windowed_scale == 0)
               return false;

            settings->video.scale = global->pending.windowed_scale;

            if (!settings->video.fullscreen)
               event_command(EVENT_CMD_REINIT);

            rarch_main_ctl(RARCH_MAIN_CTL_SET_WINDOWED_SCALE, &idx);
         }
         break;
      case EVENT_CMD_MENU_TOGGLE:
#ifdef HAVE_MENU
         if (menu_driver_alive())
            rarch_ctl(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED, NULL);
         else
            rarch_ctl(RARCH_ACTION_STATE_MENU_RUNNING, NULL);
#endif
         break;
      case EVENT_CMD_CONTROLLERS_INIT:
         event_init_controllers();
         break;
      case EVENT_CMD_RESET:
         RARCH_LOG("%s.\n", msg_hash_to_str(MSG_RESET));
         rarch_main_msg_queue_push_new(MSG_RESET, 1, 120, true);

#ifdef HAVE_CHEEVOS
         cheevos_globals.cheats_were_enabled = cheevos_globals.cheats_are_enabled;
#endif
         core.retro_reset();

         /* bSNES since v073r01 resets controllers to JOYPAD
          * after a reset, so just enforce it here. */
         event_command(EVENT_CMD_CONTROLLERS_INIT);
         break;
      case EVENT_CMD_SAVE_STATE:
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
      case EVENT_CMD_PREPARE_DUMMY:
         {
#ifdef HAVE_MENU
            menu_handle_t *menu = menu_driver_get_ptr();
            if (menu)
               menu->load_no_content = false;
#endif
            rarch_main_data_deinit();

            rarch_main_ctl(RARCH_MAIN_CTL_CLEAR_CONTENT_PATH, NULL);

            rarch_ctl(RARCH_ACTION_STATE_LOAD_CONTENT, NULL);
         }
         break;
      case EVENT_CMD_UNLOAD_CORE:
         event_command(EVENT_CMD_PREPARE_DUMMY);
         event_command(EVENT_CMD_LOAD_CORE_DEINIT);
         break;
      case EVENT_CMD_QUIT:
         rarch_ctl(RARCH_ACTION_STATE_QUIT, NULL);
         break;
      case EVENT_CMD_REINIT:
         {
            const struct retro_hw_render_callback *hw_render =
               (const struct retro_hw_render_callback*)video_driver_callback();
            const input_driver_t *input     = driver ?
               (const input_driver_t*)driver->input : NULL;

            driver->video_cache_context     = hw_render->cache_context;
            driver->video_cache_context_ack = false;
            event_command(EVENT_CMD_RESET_CONTEXT);
            driver->video_cache_context     = false;

            /* Poll input to avoid possibly stale data to corrupt things. */
            input->poll(driver->input_data);

#ifdef HAVE_MENU
            menu_display_ctl(MENU_DISPLAY_CTL_SET_FRAMEBUFFER_DIRTY_FLAG, NULL);

            if (menu_driver_alive())
               event_command(EVENT_CMD_VIDEO_SET_BLOCKING_STATE);
#endif
         }
         break;
      case EVENT_CMD_CHEATS_DEINIT:
         if (!global)
            break;

         if (global->cheat)
            cheat_manager_free(global->cheat);
         global->cheat = NULL;
         break;
      case EVENT_CMD_CHEATS_INIT:
         event_command(EVENT_CMD_CHEATS_DEINIT);
         event_init_cheats();
         break;
      case EVENT_CMD_CHEATS_APPLY:
         if (!global->cheat)
            break;

         cheat_manager_apply_cheats(global->cheat);
         break;
      case EVENT_CMD_REMAPPING_DEINIT:
         break;
      case EVENT_CMD_REMAPPING_INIT:
         event_command(EVENT_CMD_REMAPPING_DEINIT);
         event_init_remapping();
         break;
      case EVENT_CMD_REWIND_DEINIT:
         if (!global)
            break;
#ifdef HAVE_NETPLAY
         if (driver->netplay_data)
            return false;
#endif
         if (global->rewind.state)
            state_manager_free(global->rewind.state);
         global->rewind.state = NULL;
         break;
      case EVENT_CMD_REWIND_INIT:
         if (!driver->netplay_data)
            init_rewind();
         break;
      case EVENT_CMD_REWIND_TOGGLE:
         if (settings->rewind_enable)
            event_command(EVENT_CMD_REWIND_INIT);
         else
            event_command(EVENT_CMD_REWIND_DEINIT);
         break;
      case EVENT_CMD_AUTOSAVE_DEINIT:
#ifdef HAVE_THREADS
         event_deinit_autosave();
#endif
         break;
      case EVENT_CMD_AUTOSAVE_INIT:
         event_command(EVENT_CMD_AUTOSAVE_DEINIT);
#ifdef HAVE_THREADS
         event_init_autosave();
#endif
         break;
      case EVENT_CMD_AUTOSAVE_STATE:
         event_save_auto_state();
         break;
      case EVENT_CMD_AUDIO_STOP:
         if (!driver->audio_data)
            return false;
         if (!audio_driver_alive())
            return false;

         if (!audio_driver_stop())
            return false;
         break;
      case EVENT_CMD_AUDIO_START:
         if (!driver->audio_data || audio_driver_alive())
            return false;

         if (!settings->audio.mute_enable && !audio_driver_start())
         {
            RARCH_ERR("Failed to start audio driver. Will continue without audio.\n");
            driver->audio_active = false;
         }
         break;
      case EVENT_CMD_AUDIO_MUTE_TOGGLE:
         {
            const char *msg = !settings->audio.mute_enable ?
               msg_hash_to_str(MSG_AUDIO_MUTED):
               msg_hash_to_str(MSG_AUDIO_UNMUTED);

            if (!audio_driver_mute_toggle())
            {
               RARCH_ERR("%s.\n",
                     msg_hash_to_str(MSG_FAILED_TO_UNMUTE_AUDIO));
               return false;
            }

            rarch_main_msg_queue_push(msg, 1, 180, true);
            RARCH_LOG("%s\n", msg);
         }
         break;
      case EVENT_CMD_OVERLAY_DEINIT:
#ifdef HAVE_OVERLAY
         input_overlay_free_ptr();
#endif
         break;
      case EVENT_CMD_OVERLAY_INIT:
         event_command(EVENT_CMD_OVERLAY_DEINIT);
#ifdef HAVE_OVERLAY
         if (input_overlay_new_ptr() == -1)
            RARCH_ERR("%s.\n", msg_hash_to_str(MSG_FAILED_TO_LOAD_OVERLAY));
#endif
         break;
      case EVENT_CMD_OVERLAY_NEXT:
#ifdef HAVE_OVERLAY
         input_overlay_next(settings->input.overlay_opacity);
#endif
         break;
      case EVENT_CMD_DSP_FILTER_DEINIT:
         if (!global)
            break;

         audio_driver_dsp_filter_free();
         break;
      case EVENT_CMD_DSP_FILTER_INIT:
         event_command(EVENT_CMD_DSP_FILTER_DEINIT);
         if (!*settings->audio.dsp_plugin)
            break;
         audio_driver_dsp_filter_init(settings->audio.dsp_plugin);
         break;
      case EVENT_CMD_GPU_RECORD_DEINIT:
         if (!global)
            break;

         if (global->record.gpu_buffer)
            free(global->record.gpu_buffer);
         global->record.gpu_buffer = NULL;
         break;
      case EVENT_CMD_RECORD_DEINIT:
         if (!recording_deinit())
            return false;
         break;
      case EVENT_CMD_RECORD_INIT:
         event_command(EVENT_CMD_HISTORY_DEINIT);
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
         event_command(EVENT_CMD_HISTORY_DEINIT);
         if (!settings->history_list_enable)
            return false;
         RARCH_LOG("%s: [%s].\n",
               msg_hash_to_str(MSG_LOADING_HISTORY_FILE),
               settings->content_history_path);
         g_defaults.history = content_playlist_init(
               settings->content_history_path,
               settings->content_history_size);
         break;
      case EVENT_CMD_CORE_INFO_DEINIT:
         if (!global)
            break;

         if (global->core_info.list)
            core_info_list_free(global->core_info.list);
         global->core_info.list = NULL;
         break;
      case EVENT_CMD_DATA_RUNLOOP_FREE:
         rarch_main_data_free();
         break;
      case EVENT_CMD_CORE_INFO_INIT:
         event_command(EVENT_CMD_CORE_INFO_DEINIT);

         if (*settings->libretro_directory)
            global->core_info.list = core_info_list_new();
         break;
      case EVENT_CMD_CORE_DEINIT:
         {
            struct retro_hw_render_callback *cb = video_driver_callback();
            event_deinit_core(true);

            if (cb)
               memset(cb, 0, sizeof(*cb));

            break;
         }
      case EVENT_CMD_CORE_INIT:
         if (!event_init_core())
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
         audio_driver_set_nonblock_state(boolean);
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
      case EVENT_CMD_DRIVERS_DEINIT:
         uninit_drivers(DRIVERS_CMD_ALL);
         break;
      case EVENT_CMD_DRIVERS_INIT:
         init_drivers(DRIVERS_CMD_ALL);
         break;
      case EVENT_CMD_AUDIO_REINIT:
         uninit_drivers(DRIVER_AUDIO);
         init_drivers(DRIVER_AUDIO);
         break;
      case EVENT_CMD_RESET_CONTEXT:
         event_command(EVENT_CMD_DRIVERS_DEINIT);
         event_command(EVENT_CMD_DRIVERS_INIT);
         break;
      case EVENT_CMD_QUIT_RETROARCH:
         rarch_ctl(RARCH_ACTION_STATE_FORCE_QUIT, NULL);
         break;
      case EVENT_CMD_SHUTDOWN:
#if defined(__linux__) && !defined(ANDROID)
         rarch_main_msg_queue_push("Shutting down...", 1, 180, true);
         rarch_ctl(RARCH_ACTION_STATE_FORCE_QUIT, NULL);
         system("shutdown -P now");
#endif
         break;
      case EVENT_CMD_RESUME:
         rarch_ctl(RARCH_ACTION_STATE_MENU_RUNNING_FINISHED, NULL);
         break;
      case EVENT_CMD_RESTART_RETROARCH:
         {
#if defined(GEKKO) && defined(HW_RVL)
            char new_path[PATH_MAX_LENGTH];
            fill_pathname_join(new_path, g_defaults.dir.core, SALAMANDER_FILE, sizeof(new_path));
            rarch_main_ctl(RARCH_MAIN_CTL_SET_CONTENT_PATH, new_path);
#endif
            if (driver->frontend_ctx && driver->frontend_ctx->set_fork)
               driver->frontend_ctx->set_fork(true, false);
         }
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
         rarch_main_ctl(RARCH_MAIN_CTL_IS_PAUSED, &boolean);

         if (boolean)
         {
            RARCH_LOG("%s\n", msg_hash_to_str(MSG_PAUSED));
            event_command(EVENT_CMD_AUDIO_STOP);

            if (settings->video.black_frame_insertion)
               video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER, NULL);
         }
         else
         {
            RARCH_LOG("%s\n", msg_hash_to_str(MSG_UNPAUSED));
            event_command(EVENT_CMD_AUDIO_START);
         }
         break;
      case EVENT_CMD_PAUSE_TOGGLE:
         rarch_main_ctl(RARCH_MAIN_CTL_IS_PAUSED,  &boolean);
         boolean = !boolean;
         rarch_main_ctl(RARCH_MAIN_CTL_SET_PAUSED, &boolean);
         event_command(EVENT_CMD_PAUSE_CHECKS);
         break;
      case EVENT_CMD_UNPAUSE:
         boolean = false;

         rarch_main_ctl(RARCH_MAIN_CTL_SET_PAUSED, &boolean);
         event_command(EVENT_CMD_PAUSE_CHECKS);
         break;
      case EVENT_CMD_PAUSE:
         boolean = true;

         rarch_main_ctl(RARCH_MAIN_CTL_SET_PAUSED, &boolean);
         event_command(EVENT_CMD_PAUSE_CHECKS);
         break;
      case EVENT_CMD_MENU_PAUSE_LIBRETRO:
#ifdef HAVE_MENU
         if (menu_driver_alive())
         {
            if (settings->menu.pause_libretro)
               event_command(EVENT_CMD_AUDIO_STOP);
            else
               event_command(EVENT_CMD_AUDIO_START);
         }
         else
         {
            if (settings->menu.pause_libretro)
               event_command(EVENT_CMD_AUDIO_START);
         }
#endif
         break;
      case EVENT_CMD_SHADER_DIR_DEINIT:
         if (!global)
            break;

         dir_list_free(global->dir.shader_dir.list);
         global->dir.shader_dir.list = NULL;
         global->dir.shader_dir.ptr  = 0;
         break;
      case EVENT_CMD_SHADER_DIR_INIT:
         event_command(EVENT_CMD_SHADER_DIR_DEINIT);

         if (!*settings->video.shader_dir)
            return false;

         global->dir.shader_dir.list = dir_list_new_special(NULL, DIR_LIST_SHADERS, NULL);

         if (!global->dir.shader_dir.list || global->dir.shader_dir.list->size == 0)
         {
            event_command(EVENT_CMD_SHADER_DIR_DEINIT);
            return false;
         }

         global->dir.shader_dir.ptr  = 0;
         dir_list_sort(global->dir.shader_dir.list, false);

         for (i = 0; i < global->dir.shader_dir.list->size; i++)
            RARCH_LOG("%s \"%s\"\n",
                  msg_hash_to_str(MSG_FOUND_SHADER),
                  global->dir.shader_dir.list->elems[i].data);
         break;
      case EVENT_CMD_SAVEFILES:
         event_save_files();
         break;
      case EVENT_CMD_SAVEFILES_DEINIT:
         if (!global)
            break;

         if (global->savefiles)
            string_list_free(global->savefiles);
         global->savefiles = NULL;
         break;
      case EVENT_CMD_SAVEFILES_INIT:
         global->sram.use = global->sram.use && !global->sram.save_disable
#ifdef HAVE_NETPLAY
            && (!driver->netplay_data || !global->netplay.is_client)
#endif
            ;

         if (!global->sram.use)
            RARCH_LOG("%s\n",
                  msg_hash_to_str(MSG_SRAM_WILL_NOT_BE_SAVED));

         if (global->sram.use)
            event_command(EVENT_CMD_AUTOSAVE_INIT);
         break;
      case EVENT_CMD_MSG_QUEUE_DEINIT:
         rarch_main_msg_queue_free();
         break;
      case EVENT_CMD_MSG_QUEUE_INIT:
         event_command(EVENT_CMD_MSG_QUEUE_DEINIT);
         rarch_main_msg_queue_init();
         rarch_main_data_init_queues();
         break;
      case EVENT_CMD_BSV_MOVIE_DEINIT:
         if (!global)
            break;

         if (global->bsv.movie)
            bsv_movie_free(global->bsv.movie);
         global->bsv.movie = NULL;
         break;
      case EVENT_CMD_BSV_MOVIE_INIT:
         event_command(EVENT_CMD_BSV_MOVIE_DEINIT);
         event_init_movie();
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
         event_command(EVENT_CMD_NETPLAY_DEINIT);
#ifdef HAVE_NETPLAY
         if (!init_netplay())
            return false;
#endif
         break;
      case EVENT_CMD_NETPLAY_FLIP_PLAYERS:
#ifdef HAVE_NETPLAY
         {
            netplay_t *netplay = (netplay_t*)driver->netplay_data;
            if (!netplay)
               return false;
            netplay_flip_users(netplay);
         }
#endif
         break;
      case EVENT_CMD_FULLSCREEN_TOGGLE:
         if (!video_driver_ctl(RARCH_DISPLAY_CTL_HAS_WINDOWED, NULL))
            return false;

         /* If we go fullscreen we drop all drivers and
          * reinitialize to be safe. */
         settings->video.fullscreen = !settings->video.fullscreen;
         event_command(EVENT_CMD_REINIT);
         break;
      case EVENT_CMD_COMMAND_DEINIT:
#ifdef HAVE_COMMAND
         if (driver->command)
            rarch_cmd_free(driver->command);
         driver->command = NULL;
#endif
         break;
      case EVENT_CMD_COMMAND_INIT:
         event_command(EVENT_CMD_COMMAND_DEINIT);

#ifdef HAVE_COMMAND
         event_init_command();
#endif
         break;
      case EVENT_CMD_TEMPORARY_CONTENT_DEINIT:
         if (!global)
            break;

         if (global->temporary_content)
            event_free_temporary_content();
         global->temporary_content = NULL;
         break;
      case EVENT_CMD_SUBSYSTEM_FULLPATHS_DEINIT:
         if (!global)
            break;

         if (global->subsystem_fullpaths)
            string_list_free(global->subsystem_fullpaths);
         global->subsystem_fullpaths = NULL;
         break;
      case EVENT_CMD_LOG_FILE_DEINIT:
         if (!global)
            break;

         if (global->log_file && global->log_file != stderr)
            fclose(global->log_file);
         global->log_file = NULL;
         break;
      case EVENT_CMD_DISK_EJECT_TOGGLE:
         if (info && info->disk_control.get_num_images)
         {
            const struct retro_disk_control_callback *control =
               (const struct retro_disk_control_callback*)
               &info->disk_control;

            if (control)
               event_check_disk_eject(control);
         }
         else
            rarch_main_msg_queue_push_new(
                  MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS,
                  1, 120, true);
         break;
      case EVENT_CMD_DISK_NEXT:
         if (info && info->disk_control.get_num_images)
         {
            const struct retro_disk_control_callback *control =
               (const struct retro_disk_control_callback*)
               &info->disk_control;

            if (!control)
               return false;

            if (!control->get_eject_state())
               return false;

            event_check_disk_next(control);
         }
         else
            rarch_main_msg_queue_push_new(
                  MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS,
                  1, 120, true);
         break;
      case EVENT_CMD_DISK_PREV:
         if (info && info->disk_control.get_num_images)
         {
            const struct retro_disk_control_callback *control =
               (const struct retro_disk_control_callback*)
               &info->disk_control;

            if (!control)
               return false;

            if (!control->get_eject_state())
               return false;

            event_check_disk_prev(control);
         }
         else
            rarch_main_msg_queue_push_new(
                  MSG_CORE_DOES_NOT_SUPPORT_DISK_OPTIONS,
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
            static bool grab_mouse_state  = false;
            bool grab_mouse_state_tmp;

            grab_mouse_state = !grab_mouse_state;

            if (!driver->input || !input_driver_grab_mouse(grab_mouse_state))
               return false;

            RARCH_LOG("%s: %s.\n",
                  msg_hash_to_str(MSG_GRAB_MOUSE_STATE),
                  grab_mouse_state ? "yes" : "no");

            grab_mouse_state_tmp = !grab_mouse_state;
            video_driver_ctl(RARCH_DISPLAY_CTL_SHOW_MOUSE, &grab_mouse_state_tmp);
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
         rarch_main_ctl(RARCH_MAIN_CTL_SET_FRAME_LIMIT_LAST_TIME, NULL);
         break;
      case EVENT_CMD_NONE:
      default:
         return false;
   }

   return true;
}

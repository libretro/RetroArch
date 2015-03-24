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

#include "driver.h"
#include "general.h"
#include "retroarch.h"
#include "runloop.h"
#include "compat/posix_string.h"
#include "gfx/video_monitor.h"
#include "audio/audio_monitor.h"

#ifdef HAVE_MENU
#include "menu/menu.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

static driver_t *g_driver;

void driver_free(void)
{
   driver_t *driver   = driver_get_ptr();

   if (driver)
      free(driver);
}

static driver_t *driver_new(void)
{
   driver_t *driver = (driver_t*)calloc(1, sizeof(driver_t));

   if (!driver)
      return NULL;

   return driver;
}

void driver_clear_state(void)
{
   driver_free();
   g_driver  = driver_new();
}

driver_t *driver_get_ptr(void)
{
   return g_driver;
}

/**
 * find_driver_nonempty:
 * @label              : string of driver type to be found.
 * @i                  : index of driver.
 * @str                : identifier name of the found driver
 *                       gets written to this string.
 * @sizeof_str         : size of @str.
 *
 * Find driver based on @label.
 *
 * Returns: NULL if no driver based on @label found, otherwise
 * pointer to driver.
 **/
static const void *find_driver_nonempty(const char *label, int i,
      char *str, size_t sizeof_str)
{
   const void *drv = NULL;

   if (!strcmp(label, "camera_driver"))
   {
      drv = camera_driver_find_handle(i);
      if (drv)
         strlcpy(str, camera_driver_find_ident(i), sizeof_str);
   }
   else if (!strcmp(label, "location_driver"))
   {
      drv = location_driver_find_handle(i);
      if (drv)
         strlcpy(str, location_driver_find_ident(i), sizeof_str);
   }
#ifdef HAVE_MENU
   else if (!strcmp(label, "menu_driver"))
   {
      drv = menu_driver_find_handle(i);
      if (drv)
         strlcpy(str, menu_driver_find_ident(i), sizeof_str);
   }
#endif
   else if (!strcmp(label, "input_driver"))
   {
      drv = input_driver_find_handle(i);
      if (drv)
         strlcpy(str, input_driver_find_ident(i), sizeof_str);
   }
   else if (!strcmp(label, "input_joypad_driver"))
   {
      drv = joypad_driver_find_handle(i);
      if (drv)
         strlcpy(str, joypad_driver_find_ident(i), sizeof_str);
   }
   else if (!strcmp(label, "video_driver"))
   {
      drv = video_driver_find_handle(i);
      if (drv)
         strlcpy(str, video_driver_find_ident(i), sizeof_str);
   }
   else if (!strcmp(label, "audio_driver"))
   {
      drv = audio_driver_find_handle(i);
      if (drv)
         strlcpy(str, audio_driver_find_ident(i), sizeof_str);
   }
   else if (!strcmp(label, "audio_resampler_driver"))
   {
      drv = audio_resampler_driver_find_handle(i);
      if (drv)
         strlcpy(str, audio_resampler_driver_find_ident(i), sizeof_str);
   }

   return drv;
}

/**
 * find_driver_index:
 * @label              : string of driver type to be found.
 * @drv                : identifier of driver to be found.
 *
 * Find index of the driver, based on @label.
 *
 * Returns: -1 if no driver based on @label and @drv found, otherwise
 * index number of the driver found in the array.
 **/
int find_driver_index(const char * label, const char *drv)
{
   unsigned i;
   char str[PATH_MAX_LENGTH];
   const void *obj = NULL;

   for (i = 0; (obj = (const void*)
            find_driver_nonempty(label, i, str, sizeof(str))) != NULL; i++)
   {
      if (!obj)
         return -1;
      if (str[0] == '\0')
         break;
      if (!strcasecmp(drv, str))
         return i;
   }

   return -1;
}

bool find_first_driver(const char *label, char *str, size_t sizeof_str)
{
   find_driver_nonempty(label, 0, str, sizeof_str);
   return true;
}

/**
 * find_prev_driver:
 * @label              : string of driver type to be found.
 * @str                : identifier of driver to be found.
 * @sizeof_str         : size of @str.
 *
 * Find previous driver in driver array.
 **/
bool find_prev_driver(const char *label, char *str, size_t sizeof_str)
{
   int i = find_driver_index(label, str);
   if (i > 0)
      find_driver_nonempty(label, i - 1, str, sizeof_str);
   else
   {
      RARCH_WARN(
            "Couldn't find any previous driver (current one: \"%s\").\n", str);
      return false;
   }
   return true;
}

/**
 * find_next_driver:
 * @label              : string of driver type to be found.
 * @str                : identifier of driver to be found.
 * @sizeof_str         : size of @str.
 *
 * Find next driver in driver array.
 **/
bool find_next_driver(const char *label, char *str, size_t sizeof_str)
{
   int i = find_driver_index(label, str);
   if (i >= 0 && (strcmp(str, "null") != 0))
      find_driver_nonempty(label, i + 1, str, sizeof_str);
   else
   {
      RARCH_WARN("Couldn't find any next driver (current one: \"%s\").\n", str);
      return false;
   }
   return true;
}

/**
 * init_drivers_pre:
 *
 * Attempts to find a default driver for 
 * all driver types.
 *
 * Should be run before init_drivers().
 **/
void init_drivers_pre(void)
{
   find_audio_driver();
   find_video_driver();
   find_input_driver();
   find_camera_driver();
   find_location_driver();
#ifdef HAVE_MENU
   find_menu_driver();
#endif
}

static void driver_adjust_system_rates(void)
{
   global_t *global = global_get_ptr();
   driver_t *driver = driver_get_ptr();

   audio_monitor_adjust_system_rates();
   video_monitor_adjust_system_rates();

   if (!driver->video_data)
      return;

   if (global->system.force_nonblock)
      rarch_main_command(RARCH_CMD_VIDEO_SET_NONBLOCKING_STATE);
   else
      driver_set_nonblock_state(driver->nonblock_state);
}

/**
 * driver_set_refresh_rate:
 * @hz                 : New refresh rate for monitor.
 *
 * Sets monitor refresh rate to new value by calling
 * video_monitor_set_refresh_rate(). Subsequently
 * calls audio_monitor_set_refresh_rate().
 **/
void driver_set_refresh_rate(float hz)
{
   video_monitor_set_refresh_rate(hz);
   audio_monitor_set_refresh_rate();
   driver_adjust_system_rates();
}

/**
 * driver_set_nonblock_state:
 * @enable             : Enable nonblock state?
 *
 * Sets audio and video drivers to nonblock state.
 *
 * If @enable is false, sets blocking state for both
 * audio and video drivers instead.
 **/
void driver_set_nonblock_state(bool enable)
{
   settings_t *settings = config_get_ptr();
   global_t *global     = global_get_ptr();
   driver_t *driver     = driver_get_ptr();

   /* Only apply non-block-state for video if we're using vsync. */
   if (driver->video_active && driver->video_data)
   {
      bool video_nonblock = enable;

      if (!settings->video.vsync || global->system.force_nonblock)
         video_nonblock = true;
      video_driver_set_nonblock_state(video_nonblock);
   }

   if (driver->audio_active && driver->audio_data)
      audio_driver_set_nonblock_state(settings->audio.sync ? enable : true);

   global->audio_data.chunk_size = enable ?
      global->audio_data.nonblock_chunk_size : 
      global->audio_data.block_chunk_size;
}

/**
 * driver_update_system_av_info:
 * @info               : pointer to new A/V info
 *
 * Update the system Audio/Video information. 
 * Will reinitialize audio/video drivers.
 * Used by RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool driver_update_system_av_info(const struct retro_system_av_info *info)
{
   global_t *global = global_get_ptr();
   driver_t *driver = driver_get_ptr();

   global->system.av_info = *info;
   rarch_main_command(RARCH_CMD_REINIT);

   /* Cannot continue recording with different parameters.
    * Take the easiest route out and just restart the recording. */
   if (driver->recording_data)
   {
      static const char *msg = "Restarting recording due to driver reinit.";
      rarch_main_msg_queue_push(msg, 2, 180, false);
      RARCH_WARN("%s\n", msg);
      rarch_main_command(RARCH_CMD_RECORD_DEINIT);
      rarch_main_command(RARCH_CMD_RECORD_INIT);
   }

   return true;
}

/**
 * init_drivers:
 * @flags              : Bitmask of drivers to initialize.
 *
 * Initializes drivers.
 * @flags determines which drivers get initialized.
 **/
void init_drivers(int flags)
{
   driver_t *driver = driver_get_ptr();

   if (flags & DRIVER_VIDEO)
      driver->video_data_own = false;
   if (flags & DRIVER_AUDIO)
      driver->audio_data_own = false;
   if (flags & DRIVER_INPUT)
      driver->input_data_own = false;
   if (flags & DRIVER_CAMERA)
      driver->camera_data_own = false;
   if (flags & DRIVER_LOCATION)
      driver->location_data_own = false;

#ifdef HAVE_MENU
   /* By default, we want the menu to persist through driver reinits. */
   driver->menu_data_own = true;
#endif

   if (flags & (DRIVER_VIDEO | DRIVER_AUDIO))
      driver_adjust_system_rates();

   if (flags & DRIVER_VIDEO)
   {
      runloop_t *runloop = rarch_main_get_ptr();
      global_t *global   = global_get_ptr();

      runloop->frames.video.count = 0;

      init_video();

      if (!driver->video_cache_context_ack
            && global->system.hw_render_callback.context_reset)
         global->system.hw_render_callback.context_reset();
      driver->video_cache_context_ack = false;

      global->system.frame_time_last = 0;
   }

   if (flags & DRIVER_AUDIO)
      init_audio();

   /* Only initialize camera driver if we're ever going to use it. */
   if ((flags & DRIVER_CAMERA) && driver->camera_active)
      init_camera();

   /* Only initialize location driver if we're ever going to use it. */
   if ((flags & DRIVER_LOCATION) && driver->location_active)
      init_location();

#ifdef HAVE_MENU
   if (flags & DRIVER_MENU)
   {
      init_menu();
      menu_driver_context_reset();
   }
#endif

   if (flags & (DRIVER_VIDEO | DRIVER_AUDIO))
   {
      /* Keep non-throttled state as good as possible. */
      if (driver->nonblock_state)
         driver_set_nonblock_state(driver->nonblock_state);
   }
}


/**
 * uninit_drivers:
 * @flags              : Bitmask of drivers to deinitialize.
 *
 * Deinitializes drivers.
 * @flags determines which drivers get deinitialized.
 **/
void uninit_drivers(int flags)
{
   driver_t *driver = driver_get_ptr();

   if (flags & DRIVER_AUDIO)
      uninit_audio();

   if (flags & DRIVER_VIDEO)
   {
      global_t *global = global_get_ptr();

      if (global->system.hw_render_callback.context_destroy &&
               !driver->video_cache_context)
            global->system.hw_render_callback.context_destroy();
   }

#ifdef HAVE_MENU
   if (flags & DRIVER_MENU)
   {
      if (driver->menu_ctx && driver->menu_ctx->context_destroy)
            driver->menu_ctx->context_destroy();

         if (!driver->menu_data_own)
         {
            menu_free_list(driver->menu);
            menu_free(driver->menu);
            driver->menu = NULL;
         }
   }
#endif

   if (flags & DRIVERS_VIDEO_INPUT)
      uninit_video_input();

   if ((flags & DRIVER_VIDEO) && !driver->video_data_own)
      driver->video_data = NULL;

   if ((flags & DRIVER_CAMERA) && !driver->camera_data_own)
   {
      uninit_camera();
      driver->camera_data = NULL;
   }

   if ((flags & DRIVER_LOCATION) && !driver->location_data_own)
   {
      uninit_location();
      driver->location_data = NULL;
   }
   
   if ((flags & DRIVER_INPUT) && !driver->input_data_own)
      driver->input_data = NULL;

   if ((flags & DRIVER_AUDIO) && !driver->audio_data_own)
      driver->audio_data = NULL;
}


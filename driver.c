/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
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

#include <stdint.h>

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include "core_info.h"
#include "driver.h"
#include "retroarch.h"
#include "runloop.h"
#include "verbosity.h"

#ifdef HAVE_BLUETOOTH
#include "bluetooth/bluetooth_driver.h"
#endif
#ifdef HAVE_NETWORKING
#ifdef HAVE_WIFI
#include "network/wifi_driver.h"
#endif
#endif
#include "led/led_driver.h"
#include "midi_driver.h"
#include "gfx/video_driver.h"
#include "gfx/video_display_server.h"
#include "audio/audio_driver.h"
#include "camera/camera_driver.h"
#include "record/record_driver.h"
#include "location_driver.h"

#ifdef HAVE_GFX_WIDGETS
#include "gfx/gfx_widgets.h"
#endif

#ifdef HAVE_MENU
#include "menu/menu_driver.h"
#ifdef HAVE_CHEEVOS
#include "cheevos/cheevos_menu.h"
#endif
#endif

static void retro_frame_null(const void *data, unsigned width,
      unsigned height, size_t pitch) { }
void retro_input_poll_null(void) { }

/**
 * find_driver_nonempty:
 * @label              : string of driver type to be found.
 * @i                  : index of driver.
 * @str                : identifier name of the found driver
 *                       gets written to this string.
 * @len                : size of @str.
 *
 * Find driver based on @label.
 *
 * Returns: NULL if no driver based on @label found, otherwise
 * pointer to driver.
 **/
static const void *find_driver_nonempty(
      const char *label, int i,
      char *s, size_t len)
{
   if (string_is_equal(label, "camera_driver"))
   {
      if (camera_drivers[i])
      {
         const char *ident = camera_drivers[i]->ident;

         strlcpy(s, ident, len);
         return camera_drivers[i];
      }
   }
   else if (string_is_equal(label, "location_driver"))
   {
      if (location_drivers[i])
      {
         const char *ident = location_drivers[i]->ident;

         strlcpy(s, ident, len);
         return location_drivers[i];
      }
   }
#ifdef HAVE_MENU
   else if (string_is_equal(label, "menu_driver"))
   {
      if (menu_ctx_drivers[i])
      {
         const char *ident = menu_ctx_drivers[i]->ident;

         strlcpy(s, ident, len);
         return menu_ctx_drivers[i];
      }
   }
#endif
   else if (string_is_equal(label, "input_driver"))
   {
      if (input_drivers[i])
      {
         const char *ident = input_drivers[i]->ident;

         strlcpy(s, ident, len);
         return input_drivers[i];
      }
   }
   else if (string_is_equal(label, "input_joypad_driver"))
   {
      if (joypad_drivers[i])
      {
         const char *ident = joypad_drivers[i]->ident;

         strlcpy(s, ident, len);
         return joypad_drivers[i];
      }
   }
   else if (string_is_equal(label, "video_driver"))
   {
      if (video_drivers[i])
      {
         const char *ident = video_drivers[i]->ident;

         strlcpy(s, ident, len);
         return video_drivers[i];
      }
   }
   else if (string_is_equal(label, "audio_driver"))
   {
      if (audio_drivers[i])
      {
         const char *ident = audio_drivers[i]->ident;

         strlcpy(s, ident, len);
         return audio_drivers[i];
      }
   }
   else if (string_is_equal(label, "record_driver"))
   {
      if (record_drivers[i])
      {
         const char *ident = record_drivers[i]->ident;

         strlcpy(s, ident, len);
         return record_drivers[i];
      }
   }
   else if (string_is_equal(label, "midi_driver"))
   {
      if (midi_driver_find_handle(i))
      {
         const char *ident = midi_drivers[i]->ident;

         strlcpy(s, ident, len);
         return midi_drivers[i];
      }
   }
   else if (string_is_equal(label, "audio_resampler_driver"))
   {
      if (audio_resampler_driver_find_handle(i))
      {
         const char *ident = audio_resampler_driver_find_ident(i);

         strlcpy(s, ident, len);
         return audio_resampler_driver_find_handle(i);
      }
   }
#ifdef HAVE_BLUETOOTH
   else if (string_is_equal(label, "bluetooth_driver"))
   {
      if (bluetooth_drivers[i])
      {
         const char *ident = bluetooth_drivers[i]->ident;

         strlcpy(s, ident, len);
         return bluetooth_drivers[i];
      }
   }
#endif
#ifdef HAVE_WIFI
   else if (string_is_equal(label, "wifi_driver"))
   {
      if (wifi_drivers[i])
      {
         const char *ident = wifi_drivers[i]->ident;

         strlcpy(s, ident, len);
         return wifi_drivers[i];
      }
   }
#endif

   return NULL;
}



int driver_find_index(const char *label, const char *drv)
{
   unsigned i;
   char str[NAME_MAX_LENGTH];

   str[0] = '\0';

   for (i = 0;
         find_driver_nonempty(label, i, str, sizeof(str)) != NULL; i++)
   {
      if (string_is_empty(str))
         break;
      if (string_is_equal_noncase(drv, str))
         return i;
   }

   return -1;
}

/**
 * driver_find_last:
 * @label              : string of driver type to be found.
 * @s                  : identifier of driver to be found.
 * @len                : size of @s.
 *
 * Find last driver in driver array.
 **/
static void driver_find_last(const char *label, char *s, size_t len)
{
   unsigned i;

   for (i = 0;
         find_driver_nonempty(label, i, s, len) != NULL; i++) { }

   if (i)
      i = i - 1;
   else
      i = 0;

   find_driver_nonempty(label, i, s, len);
}

/**
 * driver_find_prev:
 * @label              : string of driver type to be found.
 * @s                  : identifier of driver to be found.
 * @len                : size of @s.
 *
 * Find previous driver in driver array.
 **/
static bool driver_find_prev(const char *label, char *s, size_t len)
{
   int i = driver_find_index(label, s);

   if (i > 0)
   {
      find_driver_nonempty(label, i - 1, s, len);
      return true;
   }

   RARCH_WARN(
         "Couldn't find any previous driver (current one: \"%s\").\n", s);
   return false;
}

/**
 * driver_find_next:
 * @label              : string of driver type to be found.
 * @s                  : identifier of driver to be found.
 * @len                : size of @s.
 *
 * Find next driver in driver array.
 **/
static bool driver_find_next(const char *label, char *s, size_t len)
{
   int i = driver_find_index(label, s);

   if (i >= 0 && string_is_not_equal(s, "null"))
   {
      find_driver_nonempty(label, i + 1, s, len);
      return true;
   }

   RARCH_WARN("%s (current one: \"%s\").\n",
         msg_hash_to_str(MSG_COULD_NOT_FIND_ANY_NEXT_DRIVER),
         s);
   return false;
}

static void driver_adjust_system_rates(
      bool vrr_runloop_enable,
      float video_refresh_rate,
      float audio_max_timing_skew,
      bool video_adaptive_vsync,
      unsigned video_swap_interval)
{
   runloop_state_t     *runloop_st        = runloop_state_get_ptr();
   video_driver_state_t *video_st         = video_state_get_ptr();
   struct retro_system_av_info *av_info   = &video_st->av_info;
   const struct retro_system_timing *info =
      (const struct retro_system_timing*)&av_info->timing;
   double input_sample_rate               = info->sample_rate;
   double input_fps                       = info->fps;

   /* Update video swap interval if automatic
    * switching is enabled */
   runloop_set_video_swap_interval(
         vrr_runloop_enable,
         video_st->crt_switching_active,
         video_swap_interval,
         audio_max_timing_skew,
         video_refresh_rate,
         input_fps);
   video_swap_interval = runloop_get_video_swap_interval(
         video_swap_interval);

   if (input_sample_rate > 0.0)
   {
      audio_driver_state_t *audio_st      = audio_state_get_ptr();
      if (vrr_runloop_enable)
         audio_st->input = input_sample_rate;
      else
         audio_st->input =
            audio_driver_monitor_adjust_system_rates(
                  input_sample_rate,
                  input_fps,
                  video_refresh_rate,
                  video_swap_interval,
                  audio_max_timing_skew);

      RARCH_LOG("[Audio]: Set audio input rate to: %.2f Hz.\n",
            audio_st->input);
   }

   runloop_st->force_nonblock       = false;

   if (input_fps > 0.0)
   {
      float timing_skew_hz          = video_refresh_rate;

      if (video_st->crt_switching_active)
         timing_skew_hz             = input_fps;
      video_st->core_hz             = input_fps;

      if (!video_driver_monitor_adjust_system_rates(
         timing_skew_hz,
         video_refresh_rate,
         vrr_runloop_enable,
         audio_max_timing_skew,
         video_swap_interval,
         input_fps))
      {
         /* We won't be able to do VSync reliably 
            when game FPS > monitor FPS. */
         runloop_st->force_nonblock = true;
         RARCH_LOG("[Video]: Game FPS > Monitor FPS. Cannot rely on VSync.\n");

         if (VIDEO_DRIVER_GET_PTR_INTERNAL(video_st))
         {
            if (video_st->current_video->set_nonblock_state)
               video_st->current_video->set_nonblock_state(
                     video_st->data, true,
                     video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC) &&
                     video_adaptive_vsync,
                     video_swap_interval
                     );
         }
         return;
      }
   }

   if (VIDEO_DRIVER_GET_PTR_INTERNAL(video_st))
      driver_set_nonblock_state();
}

/**
 * driver_set_nonblock_state:
 *
 * Sets audio and video drivers to nonblock state (if enabled).
 *
 * If nonblock state is false, sets
 * blocking state for both audio and video drivers instead.
 **/
void driver_set_nonblock_state(void)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   input_driver_state_t 
      *input_st                = input_state_get_ptr();
   audio_driver_state_t 
      *audio_st                = audio_state_get_ptr();
   video_driver_state_t 
      *video_st                = video_state_get_ptr();
   bool                 enable = input_st ?
      input_st->nonblocking_flag : false;
   settings_t       *settings  = config_get_ptr();
   bool audio_sync             = settings->bools.audio_sync;
   bool video_vsync            = settings->bools.video_vsync;
   bool adaptive_vsync         = settings->bools.video_adaptive_vsync;
   unsigned swap_interval      = runloop_get_video_swap_interval(
         settings->uints.video_swap_interval);
   bool video_driver_active    = video_st->active;
   bool audio_driver_active    = audio_st->active;
   bool runloop_force_nonblock = runloop_st->force_nonblock;

   /* Only apply non-block-state for video if we're using vsync. */
   if (video_driver_active && VIDEO_DRIVER_GET_PTR_INTERNAL(video_st))
   {
      if (video_st->current_video->set_nonblock_state)
      {
         bool video_nonblock        = enable;
         if (!video_vsync || runloop_force_nonblock)
            video_nonblock = true;
         video_st->current_video->set_nonblock_state(video_st->data,
               video_nonblock,
               video_driver_test_all_flags(GFX_CTX_FLAGS_ADAPTIVE_VSYNC) &&
               adaptive_vsync, swap_interval);
      }
   }

   if (audio_driver_active && audio_st->context_audio_data)
      audio_st->current_audio->set_nonblock_state(
            audio_st->context_audio_data,
            audio_sync ? enable : true);

   audio_st->chunk_size = enable
      ? audio_st->chunk_nonblock_size
      : audio_st->chunk_block_size;
}

void drivers_init(
      settings_t *settings,
      int flags,
      bool verbosity_enabled)
{
   runloop_state_t *runloop_st = runloop_state_get_ptr();
   audio_driver_state_t 
      *audio_st                = audio_state_get_ptr();
   input_driver_state_t 
      *input_st                = input_state_get_ptr();
   video_driver_state_t 
      *video_st                = video_state_get_ptr();
#ifdef HAVE_MENU
   struct menu_state *menu_st  = menu_state_get_ptr();
#endif
   camera_driver_state_t 
      *camera_st               = camera_state_get_ptr();
   location_driver_state_t 
      *location_st             = location_state_get_ptr();
   bool video_is_threaded      = VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st);
   gfx_display_t *p_disp       = disp_get_ptr();
#if defined(HAVE_GFX_WIDGETS)
   bool video_font_enable      = settings->bools.video_font_enable;
   bool menu_enable_widgets    = settings->bools.menu_enable_widgets;

   /* By default, we want display widgets to persist through driver reinits. */
   dispwidget_get_ptr()->persisting = true;
#endif

#ifdef HAVE_MENU
   /* By default, we want the menu to persist through driver reinits. */
   if (menu_st)
      menu_st->data_own = true;
#endif

   if (flags & (DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK))
      driver_adjust_system_rates(
                                 settings->bools.vrr_runloop_enable,
                                 settings->floats.video_refresh_rate,
                                 settings->floats.audio_max_timing_skew,
                                 settings->bools.video_adaptive_vsync,
                                 settings->uints.video_swap_interval
                                 );

   /* Initialize video driver */
   if (flags & DRIVER_VIDEO_MASK)
   {
      struct retro_hw_render_callback *hwr   =
         VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st);

      video_st->frame_time_count = 0;

      video_driver_lock_new();
#ifdef HAVE_VIDEO_FILTER
      video_driver_filter_free();
#endif
      video_driver_set_cached_frame_ptr(NULL);
      if (!video_driver_init_internal(&video_is_threaded,
               verbosity_enabled))
         retroarch_fail(1, "video_driver_init_internal()");

      if (!video_st->cache_context_ack
            && hwr->context_reset)
         hwr->context_reset();
      video_st->cache_context_ack = false;
      runloop_st->frame_time_last = 0;
   }

   /* Initialize audio driver */
   if (flags & DRIVER_AUDIO_MASK)
   {
      audio_driver_init_internal(
            settings,
            audio_st->callback.callback != NULL);
      if (  audio_st->current_audio &&
            audio_st->current_audio->device_list_new &&
            audio_st->context_audio_data)
         audio_st->devices_list = (struct string_list*)
            audio_st->current_audio->device_list_new(
                  audio_st->context_audio_data);
   }

   /* Regular display refresh rate startup autoswitch based on content av_info */
   if (flags & (DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK))
   {
      struct retro_system_av_info *av_info = &video_st->av_info;
      float refresh_rate                   = av_info->timing.fps;
      unsigned autoswitch_refresh_rate     = settings->uints.video_autoswitch_refresh_rate;
      bool exclusive_fullscreen            = settings->bools.video_fullscreen && !settings->bools.video_windowed_fullscreen;
      bool windowed_fullscreen             = settings->bools.video_fullscreen && settings->bools.video_windowed_fullscreen;
      bool all_fullscreen                  = settings->bools.video_fullscreen || settings->bools.video_windowed_fullscreen;
   
      if (  refresh_rate > 0.0 &&
            !settings->uints.crt_switch_resolution &&
            !settings->bools.vrr_runloop_enable &&
            video_display_server_has_resolution_list() &&
            (autoswitch_refresh_rate != AUTOSWITCH_REFRESH_RATE_OFF) &&
            fabs(settings->floats.video_refresh_rate - refresh_rate) > 1)
      {
         if (((autoswitch_refresh_rate == AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN) && exclusive_fullscreen) ||
             ((autoswitch_refresh_rate == AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN) && windowed_fullscreen)   ||
             ((autoswitch_refresh_rate == AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN) && all_fullscreen))
         {
            bool video_switch_refresh_rate = false;
   
            video_switch_refresh_rate_maybe(&refresh_rate, &video_switch_refresh_rate);
   
            if (video_switch_refresh_rate && video_display_server_set_refresh_rate(refresh_rate))
            {
               int reinit_flags = DRIVER_AUDIO_MASK;
               video_monitor_set_refresh_rate(refresh_rate);
               /* Audio must reinit after successful rate switch */
               command_event(CMD_EVENT_REINIT, &reinit_flags);
            }
         }
      }
   }

   if (flags & DRIVER_CAMERA_MASK)
   {
      /* Only initialize camera driver if we're ever going to use it. */
      if (camera_st->active)
      {
         /* Resource leaks will follow if camera is initialized twice. */
         if (!camera_st->data)
         {
            if (!camera_driver_find_driver("camera driver",
                     verbosity_enabled))
               retroarch_fail(1, "find_camera_driver()");

            if (camera_st->driver)
            {
               camera_st->data = camera_st->driver->init(
                     *settings->arrays.camera_device ?
                     settings->arrays.camera_device : NULL,
                     camera_st->cb.caps,
                     settings->uints.camera_width ?
                     settings->uints.camera_width  : camera_st->cb.width,
                     settings->uints.camera_height ?
                     settings->uints.camera_height : camera_st->cb.height);

               if (!camera_st->data)
               {
                  RARCH_ERR("Failed to initialize camera driver. Will continue without camera.\n");
                  camera_st->active = false;
               }

               if (camera_st->cb.initialized)
                  camera_st->cb.initialized();
            }
         }
      }
   }

#ifdef HAVE_BLUETOOTH
   if (flags & DRIVER_BLUETOOTH_MASK)
      bluetooth_driver_ctl(RARCH_BLUETOOTH_CTL_INIT, NULL);
#endif
#ifdef HAVE_WIFI
   if ((flags & DRIVER_WIFI_MASK))
      wifi_driver_ctl(RARCH_WIFI_CTL_INIT, NULL);
#endif

   if (flags & DRIVER_LOCATION_MASK)
   {
      /* Only initialize location driver if we're ever going to use it. */
      if (location_st->active)
         if (!init_location(&runloop_state_get_ptr()->system,
                  settings, verbosity_is_enabled()))
            location_st->active = false;
   }

   core_info_init_current_core();

#if defined(HAVE_GFX_WIDGETS)
   /* Note that we only enable widgets if 'video_font_enable'
    * is true. 'video_font_enable' corresponds to the generic
    * 'On-Screen Notifications' setting, which should serve as
    * a global notifications on/off toggle switch */
   if (video_font_enable &&
       menu_enable_widgets &&
       video_driver_has_widgets())
   {
      bool rarch_force_fullscreen = video_st->force_fullscreen;
      bool video_is_fullscreen    = settings->bools.video_fullscreen ||
            rarch_force_fullscreen;

      dispwidget_get_ptr()->active= gfx_widgets_init(
            p_disp,
            anim_get_ptr(),
            settings,
            (uintptr_t)&dispwidget_get_ptr()->active,
            video_is_threaded,
            video_st->width,
            video_st->height,
            video_is_fullscreen,
            settings->paths.directory_assets,
            settings->paths.path_font);
   }
   else
#endif
   {
      gfx_display_init_first_driver(p_disp, video_is_threaded);
   }

#ifdef HAVE_MENU
   if (flags & DRIVER_VIDEO_MASK)
   {
      /* Initialize menu driver */
      if (flags & DRIVER_MENU_MASK)
      {
         if (!menu_driver_init(video_is_threaded))
             RARCH_ERR("Unable to init menu driver.\n");

#ifdef HAVE_LIBRETRODB
         menu_explore_context_init();
#endif
         menu_contentless_cores_context_init();
      }
   }

   /* Initialising the menu driver will also initialise
    * core info - if we are not initialising the menu
    * driver, must initialise core info 'by hand' */
   if (!(flags & DRIVER_VIDEO_MASK) ||
       !(flags & DRIVER_MENU_MASK))
   {
      command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
      command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
   }

#else
   /* Qt uses core info, even if the menu is disabled */
   command_event(CMD_EVENT_CORE_INFO_INIT, NULL);
   command_event(CMD_EVENT_LOAD_CORE_PERSIST, NULL);
#endif

   /* Keep non-throttled state as good as possible. */
   if (flags & (DRIVER_VIDEO_MASK | DRIVER_AUDIO_MASK))
      if (input_st && input_st->nonblocking_flag)
         driver_set_nonblock_state();

   /* Initialize LED driver */
   if (flags & DRIVER_LED_MASK)
      led_driver_init(settings->arrays.led_driver);

   /* Initialize MIDI  driver */
   if (flags & DRIVER_MIDI_MASK)
      midi_driver_init(settings);

#ifndef HAVE_LAKKA_SWITCH
#ifdef HAVE_LAKKA
   cpu_scaling_driver_init();
#endif
#endif /* #ifndef HAVE_LAKKA_SWITCH */
}

void driver_uninit(int flags)
{
   runloop_state_t *runloop_st  = runloop_state_get_ptr();
   video_driver_state_t 
      *video_st                 = video_state_get_ptr();
   camera_driver_state_t 
      *camera_st                = camera_state_get_ptr();

   core_info_deinit_list();
   core_info_free_current_core();

#if defined(HAVE_GFX_WIDGETS)
   /* This absolutely has to be done before video_driver_free_internal()
    * is called/completes, otherwise certain menu drivers
    * (e.g. Vulkan) will segfault */
   if (dispwidget_get_ptr()->inited)
   {
      gfx_widgets_deinit(dispwidget_get_ptr()->persisting);
      dispwidget_get_ptr()->active = false;
   }
#endif

#ifdef HAVE_MENU
   if (flags & DRIVER_MENU_MASK)
   {
#ifdef HAVE_LIBRETRODB
      menu_explore_context_deinit();
#endif
      menu_contentless_cores_context_deinit();

#ifdef HAVE_CHEEVOS
      rcheevos_menu_reset_badges();
#endif

      menu_driver_ctl(RARCH_MENU_CTL_DEINIT, NULL);
   }
#endif

   if ((flags & DRIVER_LOCATION_MASK))
      uninit_location(&runloop_st->system);

   if ((flags & DRIVER_CAMERA_MASK))
   {
      if (camera_st->data && camera_st->driver)
      {
         if (camera_st->cb.deinitialized)
            camera_st->cb.deinitialized();

         if (camera_st->driver->free)
            camera_st->driver->free(camera_st->data);
      }

      camera_st->data = NULL;
   }

#ifdef HAVE_BLUETOOTH
   if ((flags & DRIVER_BLUETOOTH_MASK))
      bluetooth_driver_ctl(RARCH_BLUETOOTH_CTL_DEINIT, NULL);
#endif
#ifdef HAVE_WIFI
   if ((flags & DRIVER_WIFI_MASK))
      wifi_driver_ctl(RARCH_WIFI_CTL_DEINIT, NULL);
#endif

   if (flags & DRIVER_LED)
      led_driver_free();

   if (flags & DRIVERS_VIDEO_INPUT)
   {
      video_driver_free_internal();
      VIDEO_DRIVER_LOCK_FREE(video_st);
      video_st->data = NULL;
      video_driver_set_cached_frame_ptr(NULL);
   }

   if (flags & DRIVER_AUDIO_MASK)
      audio_driver_deinit();

   if ((flags & DRIVER_VIDEO_MASK))
      video_st->data = NULL;

   if ((flags & DRIVER_INPUT_MASK))
      input_state_get_ptr()->current_data = NULL;

   if ((flags & DRIVER_AUDIO_MASK))
      audio_state_get_ptr()->context_audio_data = NULL;

   if (flags & DRIVER_MIDI_MASK)
      midi_driver_free();

#ifndef HAVE_LAKKA_SWITCH
#ifdef HAVE_LAKKA
   cpu_scaling_driver_free();
#endif
#endif /* #ifndef HAVE_LAKKA_SWITCH */
}

void retroarch_deinit_drivers(struct retro_callbacks *cbs)
{
   input_driver_state_t *input_st  = input_state_get_ptr();
   video_driver_state_t *video_st  = video_state_get_ptr();
   camera_driver_state_t *camera_st= camera_state_get_ptr();
   location_driver_state_t 
      *location_st                 = location_state_get_ptr();
   runloop_state_t     *runloop_st = runloop_state_get_ptr();

#if defined(HAVE_GFX_WIDGETS)
   /* Tear down display widgets no matter what
    * in case the handle is lost in the threaded
    * video driver in the meantime
    * (breaking video_driver_has_widgets) */
   if (dispwidget_get_ptr()->inited)
   {
      gfx_widgets_deinit(
            dispwidget_get_ptr()->persisting);
      dispwidget_get_ptr()->active = false;
   }
#endif

#if defined(HAVE_CRTSWITCHRES)
   /* Switchres deinit */
   if (video_st->crt_switching_active)
   {
#if defined(DEBUG)
      RARCH_LOG("[CRT]: Getting video info\n");
      RARCH_LOG("[CRT]: About to destroy SR\n");
#endif
      crt_destroy_modes(&video_st->crt_switch_st);
   }
#endif

   /* Video */
   video_display_server_destroy();

   video_st->use_rgba                   = false;
   video_st->hdr_support                = false;
   video_st->active                     = false;
   video_st->cache_context              = false;
   video_st->cache_context_ack          = false;
   video_st->record_gpu_buffer          = NULL;
   video_st->current_video              = NULL;
   video_driver_set_cached_frame_ptr(NULL);

   /* Audio */
   audio_state_get_ptr()->active                    = false;
   audio_state_get_ptr()->current_audio             = NULL;

   if (input_st)
   {
      /* Input */
      input_st->keyboard_linefeed_enable = false;
      input_st->block_hotkey             = false;
      input_st->block_libretro_input     = false;
      input_st->nonblocking_flag         = false;

      memset(&input_st->turbo_btns, 0, sizeof(turbo_buttons_t));
      memset(&input_st->analog_requested, 0,
         sizeof(input_st->analog_requested));
      input_st->current_driver           = NULL;
   }

#ifdef HAVE_MENU
   menu_driver_destroy(
         menu_state_get_ptr());
#endif
   location_st->active                              = false;
   destroy_location();

   /* Camera */
   camera_st->active                                = false;
   camera_st->driver                                = NULL;
   camera_st->data                                  = NULL;

#ifdef HAVE_BLUETOOTH
   bluetooth_driver_ctl(RARCH_BLUETOOTH_CTL_DESTROY, NULL);
#endif
#ifdef HAVE_WIFI
   wifi_driver_ctl(RARCH_WIFI_CTL_DESTROY, NULL);
#endif

   cbs->frame_cb                                    = retro_frame_null;
   cbs->poll_cb                                     = retro_input_poll_null;
   cbs->sample_cb                                   = NULL;
   cbs->sample_batch_cb                             = NULL;
   cbs->state_cb                                    = NULL;

   runloop_st->current_core.inited                  = false;
}

bool driver_ctl(enum driver_ctl_state state, void *data)
{
   driver_ctx_info_t      *drv = (driver_ctx_info_t*)data;

   switch (state)
   {
      case RARCH_DRIVER_CTL_SET_REFRESH_RATE:
         {
            float *hz                    = (float*)data;
            audio_driver_state_t 
               *audio_st                 = audio_state_get_ptr();
            settings_t *settings         = config_get_ptr();
            unsigned 
               audio_output_sample_rate  = settings->uints.audio_output_sample_rate;
            bool vrr_runloop_enable      = settings->bools.vrr_runloop_enable;
            float video_refresh_rate     = settings->floats.video_refresh_rate;
            float audio_max_timing_skew  = settings->floats.audio_max_timing_skew;
            bool video_adaptive_vsync    = settings->bools.video_adaptive_vsync;
            unsigned video_swap_interval = settings->uints.video_swap_interval;

            video_monitor_set_refresh_rate(*hz);

            /* Sets audio monitor rate to new value. */
            audio_st->source_ratio_original   =
            audio_st->source_ratio_current    = 
            (double)audio_output_sample_rate / audio_st->input;

            driver_adjust_system_rates(
                                       vrr_runloop_enable,
                                       video_refresh_rate,
                                       audio_max_timing_skew,
                                       video_adaptive_vsync,
                                       video_swap_interval
                                       );
         }
         break;
      case RARCH_DRIVER_CTL_FIND_FIRST:
         if (!drv)
            return false;
         find_driver_nonempty(drv->label, 0, drv->s, drv->len);
         break;
      case RARCH_DRIVER_CTL_FIND_LAST:
         if (!drv)
            return false;
         driver_find_last(drv->label, drv->s, drv->len);
         break;
      case RARCH_DRIVER_CTL_FIND_PREV:
         if (!drv)
            return false;
         return driver_find_prev(drv->label, drv->s, drv->len);
      case RARCH_DRIVER_CTL_FIND_NEXT:
         if (!drv)
            return false;
         return driver_find_next(drv->label, drv->s, drv->len);
      case RARCH_DRIVER_CTL_NONE:
      default:
         break;
   }

   return true;
}

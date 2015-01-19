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
#include <stdint.h>
#include <string.h>
#include "compat/posix_string.h"
#include "gfx/video_thread_wrapper.h"
#include "gfx/video_monitor.h"
#include "gfx/video_viewport.h"
#include "gfx/video_pixel_converter.h"
#include "audio/audio_monitor.h"
#include "gfx/drivers_context/win32_dwm_common.h"

#ifdef HAVE_MENU
#include "menu/menu.h"
#endif

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

driver_t driver;

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
   else if (!strcmp(label, "osk_driver"))
   {
      drv = osk_driver_find_handle(i);
      if (drv)
         strlcpy(str, osk_driver_find_ident(i), sizeof_str);
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


/**
 * find_prev_driver:
 * @label              : string of driver type to be found.
 * @str                : identifier of driver to be found.
 * @sizeof_str         : size of @str.
 *
 * Find previous driver in driver array.
 **/
void find_prev_driver(const char *label, char *str, size_t sizeof_str)
{
   int i = find_driver_index(label, str);
   if (i > 0)
      find_driver_nonempty(label, i - 1, str, sizeof_str);
   else
      RARCH_WARN(
            "Couldn't find any previous driver (current one: \"%s\").\n", str);
}

/**
 * find_next_driver:
 * @label              : string of driver type to be found.
 * @str                : identifier of driver to be found.
 * @sizeof_str         : size of @str.
 *
 * Find next driver in driver array.
 **/
void find_next_driver(const char *label, char *str, size_t sizeof_str)
{
   int i = find_driver_index(label, str);
   if (i >= 0)
      find_driver_nonempty(label, i + 1, str, sizeof_str);
   else
      RARCH_WARN("Couldn't find any next driver (current one: \"%s\").\n", str);
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
   find_osk_driver();
#ifdef HAVE_MENU
   find_menu_driver();
#endif
}

static void driver_adjust_system_rates(void)
{
   audio_monitor_adjust_system_rates();
   video_monitor_adjust_system_rates();

   if (!driver.video_data)
      return;

   if (g_extern.system.force_nonblock)
      rarch_main_command(RARCH_CMD_VIDEO_SET_NONBLOCKING_STATE);
   else
      driver_set_nonblock_state(driver.nonblock_state);
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
   /* Only apply non-block-state for video if we're using vsync. */
   if (driver.video_active && driver.video_data)
   {
      bool video_nonblock = enable;

      if (!g_settings.video.vsync || g_extern.system.force_nonblock)
         video_nonblock = true;
      driver.video->set_nonblock_state(driver.video_data, video_nonblock);
   }

   if (driver.audio_active && driver.audio_data)
      driver.audio->set_nonblock_state(driver.audio_data,
            g_settings.audio.sync ? enable : true);

   g_extern.audio_data.chunk_size = enable ?
      g_extern.audio_data.nonblock_chunk_size : 
      g_extern.audio_data.block_chunk_size;
}

/**
 * driver_get_current_framebuffer:
 *
 * Gets pointer to current hardware renderer framebuffer object.
 * Used by RETRO_ENVIRONMENT_SET_HW_RENDER.
 *
 * Returns: pointer to hardware framebuffer object, otherwise 0.
 **/
uintptr_t driver_get_current_framebuffer(void)
{
#ifdef HAVE_FBO
   if (driver.video_poke && driver.video_poke->get_current_framebuffer)
      return driver.video_poke->get_current_framebuffer(driver.video_data);
#endif
   return 0;
}

retro_proc_address_t driver_get_proc_address(const char *sym)
{
#ifdef HAVE_FBO
   if (driver.video_poke && driver.video_poke->get_proc_address)
      return driver.video_poke->get_proc_address(driver.video_data, sym);
#endif
   return NULL;
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
   g_extern.system.av_info = *info;
   rarch_main_command(RARCH_CMD_REINIT);

   /* Cannot continue recording with different parameters.
    * Take the easiest route out and just restart the recording. */
   if (driver.recording_data)
   {
      static const char *msg = "Restarting recording due to driver reinit.";
      msg_queue_push(g_extern.msg_queue, msg, 2, 180);
      RARCH_WARN("%s\n", msg);
      rarch_main_command(RARCH_CMD_RECORD_DEINIT);
      rarch_main_command(RARCH_CMD_RECORD_INIT);
   }

   return true;
}

static void deinit_video_filter(void)
{
   rarch_softfilter_free(g_extern.filter.filter);
   free(g_extern.filter.buffer);
   memset(&g_extern.filter, 0, sizeof(g_extern.filter));
}

static void init_video_filter(enum retro_pixel_format colfmt)
{
   unsigned width, height, pow2_x, pow2_y, maxsize;
   struct retro_game_geometry *geom = NULL;

   deinit_video_filter();

   if (!*g_settings.video.softfilter_plugin)
      return;

   /* Deprecated format. Gets pre-converted. */
   if (colfmt == RETRO_PIXEL_FORMAT_0RGB1555)
      colfmt = RETRO_PIXEL_FORMAT_RGB565;

   if (g_extern.system.hw_render_callback.context_type)
   {
      RARCH_WARN("Cannot use CPU filters when hardware rendering is used.\n");
      return;
   }

   geom    = (struct retro_game_geometry*)&g_extern.system.av_info.geometry;
   width   = geom->max_width;
   height  = geom->max_height;

   g_extern.filter.filter = rarch_softfilter_new(
         g_settings.video.softfilter_plugin,
         RARCH_SOFTFILTER_THREADS_AUTO, colfmt, width, height);

   if (!g_extern.filter.filter)
   {
      RARCH_ERR("Failed to load filter.\n");
      return;
   }

   rarch_softfilter_get_max_output_size(g_extern.filter.filter,
         &width, &height);

   pow2_x                = next_pow2(width);
   pow2_y                = next_pow2(height);
   maxsize               = max(pow2_x, pow2_y); 
   g_extern.filter.scale = maxsize / RARCH_SCALE_BASE;

   g_extern.filter.out_rgb32 = rarch_softfilter_get_output_format(
         g_extern.filter.filter) == RETRO_PIXEL_FORMAT_XRGB8888;

   g_extern.filter.out_bpp = g_extern.filter.out_rgb32 ?
      sizeof(uint32_t) : sizeof(uint16_t);

   /* TODO: Aligned output. */
   g_extern.filter.buffer = malloc(width * height * g_extern.filter.out_bpp);
   if (!g_extern.filter.buffer)
      goto error;

   return;

error:
   RARCH_ERR("Softfilter initialization failed.\n");
   deinit_video_filter();
}

static void init_video_input(const input_driver_t *tmp)
{
   /* Video driver didn't provide an input driver,
    * so we use configured one. */
   RARCH_LOG("Graphics driver did not initialize an input driver. Attempting to pick a suitable driver.\n");

   if (tmp)
      driver.input = tmp;
   else
      find_input_driver();

   if (!driver.input)
   {
      /* This should never really happen as tmp (driver.input) is always 
       * found before this in find_driver_input(), or we have aborted 
       * in a similar fashion anyways. */
      rarch_fail(1, "init_video_input()");
   }

   driver.input_data = driver.input->init();

   if (driver.input_data)
      return;

   RARCH_ERR("Cannot initialize input driver. Exiting ...\n");
   rarch_fail(1, "init_video_input()");
}

static void init_video(void)
{
   unsigned max_dim, scale, width, height;
   rarch_viewport_t *custom_vp;
   const input_driver_t *tmp = NULL;
   const struct retro_game_geometry *geom = NULL;
   video_info_t video = {0};
   static uint16_t dummy_pixels[32] = {0};

   init_video_filter(g_extern.system.pix_fmt);
   rarch_main_command(RARCH_CMD_SHADER_DIR_INIT);

   geom = (const struct retro_game_geometry*)&g_extern.system.av_info.geometry;
   max_dim = max(geom->max_width, geom->max_height);
   scale = next_pow2(max_dim) / RARCH_SCALE_BASE;
   scale = max(scale, 1);

   if (g_extern.filter.filter)
      scale = g_extern.filter.scale;

   /* Update core-dependent aspect ratio values. */
   video_viewport_set_square_pixel(geom->base_width, geom->base_height);
   video_viewport_set_core();
   video_viewport_set_config();

   /* Update CUSTOM viewport. */
   custom_vp = (rarch_viewport_t*)&g_extern.console.screen.viewports.custom_vp;
   if (g_settings.video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      float default_aspect = aspectratio_lut[ASPECT_RATIO_CORE].value;
      aspectratio_lut[ASPECT_RATIO_CUSTOM].value = 
         (custom_vp->width && custom_vp->height) ?
         (float)custom_vp->width / custom_vp->height : default_aspect;
   }

   g_extern.system.aspect_ratio = 
      aspectratio_lut[g_settings.video.aspect_ratio_idx].value;

   if (g_settings.video.fullscreen)
   {
      width = g_settings.video.fullscreen_x;
      height = g_settings.video.fullscreen_y;
   }
   else
   {
      if (g_settings.video.force_aspect)
      {
         /* Do rounding here to simplify integer scale correctness. */
         unsigned base_width = roundf(geom->base_height *
               g_extern.system.aspect_ratio);
         width = roundf(base_width * g_settings.video.scale);
         height = roundf(geom->base_height * g_settings.video.scale);
      }
      else
      {
         width = roundf(geom->base_width * g_settings.video.scale);
         height = roundf(geom->base_height * g_settings.video.scale);
      }
   }

   if (width && height)
      RARCH_LOG("Video @ %ux%u\n", width, height);
   else
      RARCH_LOG("Video @ fullscreen\n");

   driver.display_type  = RARCH_DISPLAY_NONE;
   driver.video_display = 0;
   driver.video_window  = 0;

   if (!init_video_pixel_converter(RARCH_SCALE_BASE * scale))
   {
      RARCH_ERR("Failed to initialize pixel converter.\n");
      rarch_fail(1, "init_video()");
   }

   video.width = width;
   video.height = height;
   video.fullscreen = g_settings.video.fullscreen;
   video.vsync = g_settings.video.vsync && !g_extern.system.force_nonblock;
   video.force_aspect = g_settings.video.force_aspect;
#ifdef GEKKO
   video.viwidth = g_settings.video.viwidth;
   video.vfilter = g_settings.video.vfilter;
#endif
   video.smooth = g_settings.video.smooth;
   video.input_scale = scale;
   video.rgb32 = g_extern.filter.filter ? 
      g_extern.filter.out_rgb32 : 
      (g_extern.system.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);

   tmp = (const input_driver_t*)driver.input;
   /* Need to grab the "real" video driver interface on a reinit. */
   find_video_driver();

#ifdef HAVE_THREADS
   if (g_settings.video.threaded && !g_extern.system.hw_render_callback.context_type)
   {
      /* Can't do hardware rendering with threaded driver currently. */
      RARCH_LOG("Starting threaded video driver ...\n");

      if (!rarch_threaded_video_init(&driver.video, &driver.video_data,
               &driver.input, &driver.input_data,
               driver.video, &video))
      {
         RARCH_ERR("Cannot open threaded video driver ... Exiting ...\n");
         rarch_fail(1, "init_video()");
      }
   }
   else
#endif
      driver.video_data = driver.video->init(&video, &driver.input,
            &driver.input_data);

   if (!driver.video_data)
   {
      RARCH_ERR("Cannot open video driver ... Exiting ...\n");
      rarch_fail(1, "init_video()");
   }

   driver.video_poke = NULL;
   if (driver.video->poke_interface)
      driver.video->poke_interface(driver.video_data, &driver.video_poke);

   if (driver.video->viewport_info && (!custom_vp->width ||
            !custom_vp->height))
   {
      /* Force custom viewport to have sane parameters. */
      custom_vp->width = width;
      custom_vp->height = height;
      driver.video->viewport_info(driver.video_data, custom_vp);
   }

   if (driver.video->set_rotation)
      driver.video->set_rotation(driver.video_data,
            (g_settings.video.rotation + g_extern.system.rotation) % 4);

   if (driver.video->suppress_screensaver)
      driver.video->suppress_screensaver(driver.video_data,
            g_settings.ui.suspend_screensaver_enable);

   if (!driver.input)
      init_video_input(tmp);

   rarch_main_command(RARCH_CMD_OVERLAY_DEINIT);
   rarch_main_command(RARCH_CMD_OVERLAY_INIT);

   g_extern.measure_data.frame_time_samples_count = 0;

   g_extern.frame_cache.width = 4;
   g_extern.frame_cache.height = 4;
   g_extern.frame_cache.pitch = 8;
   g_extern.frame_cache.data = &dummy_pixels;

   if (driver.video_poke && driver.video_poke->set_texture_frame)
      driver.video_poke->set_texture_frame(driver.video_data,
               &dummy_pixels, false, 1, 1, 1.0f);
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
   if (flags & DRIVER_VIDEO)
      driver.video_data_own = false;
   if (flags & DRIVER_AUDIO)
      driver.audio_data_own = false;
   if (flags & DRIVER_INPUT)
      driver.input_data_own = false;
   if (flags & DRIVER_CAMERA)
      driver.camera_data_own = false;
   if (flags & DRIVER_LOCATION)
      driver.location_data_own = false;
   if (flags & DRIVER_OSK)
      driver.osk_data_own = false;

#ifdef HAVE_MENU
   /* By default, we want the menu to persist through driver reinits. */
   driver.menu_data_own = true;
#endif

   if (flags & (DRIVER_VIDEO | DRIVER_AUDIO))
      driver_adjust_system_rates();

   if (flags & DRIVER_VIDEO)
   {
      g_extern.frame_count = 0;

      init_video();

      if (!driver.video_cache_context_ack
            && g_extern.system.hw_render_callback.context_reset)
         g_extern.system.hw_render_callback.context_reset();
      driver.video_cache_context_ack = false;

      g_extern.system.frame_time_last = 0;
   }

   if (flags & DRIVER_AUDIO)
      init_audio();

   /* Only initialize camera driver if we're ever going to use it. */
   if ((flags & DRIVER_CAMERA) && driver.camera_active)
      init_camera();

   /* Only initialize location driver if we're ever going to use it. */
   if ((flags & DRIVER_LOCATION) && driver.location_active)
      init_location();

   if (flags & DRIVER_OSK)
      init_osk();

#ifdef HAVE_MENU
   if (flags & DRIVER_MENU)
   {
      init_menu();

      if (driver.menu && driver.menu_ctx && driver.menu_ctx->context_reset)
         driver.menu_ctx->context_reset(driver.menu);
   }
#endif

   if (flags & (DRIVER_VIDEO | DRIVER_AUDIO))
   {
      /* Keep non-throttled state as good as possible. */
      if (driver.nonblock_state)
         driver_set_nonblock_state(driver.nonblock_state);
   }
}

static void uninit_video_input(void)
{
   rarch_main_command(RARCH_CMD_OVERLAY_DEINIT);

   if (
         !driver.input_data_own &&
         (driver.input_data != driver.video_data) &&
         driver.input &&
         driver.input->free)
      driver.input->free(driver.input_data);

   if (
         !driver.video_data_own &&
         driver.video_data &&
         driver.video &&
         driver.video->free)
      driver.video->free(driver.video_data);

   deinit_pixel_converter();

   deinit_video_filter();

   rarch_main_command(RARCH_CMD_SHADER_DIR_DEINIT);
   video_monitor_compute_fps_statistics();
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
   if (flags & DRIVER_AUDIO)
      uninit_audio();

   if (flags & DRIVER_VIDEO)
   {
      if (g_extern.system.hw_render_callback.context_destroy &&
               !driver.video_cache_context)
            g_extern.system.hw_render_callback.context_destroy();
   }

#ifdef HAVE_MENU
   if (flags & DRIVER_MENU)
   {
      if (driver.menu && driver.menu_ctx && driver.menu_ctx->context_destroy)
            driver.menu_ctx->context_destroy(driver.menu);

         if (!driver.menu_data_own)
         {
            menu_free_list(driver.menu);
            menu_free(driver.menu);
            driver.menu = NULL;
         }
   }
#endif

   if (flags & DRIVERS_VIDEO_INPUT)
      uninit_video_input();

   if ((flags & DRIVER_VIDEO) && !driver.video_data_own)
      driver.video_data = NULL;

   if ((flags & DRIVER_CAMERA) && !driver.camera_data_own)
   {
      uninit_camera();
      driver.camera_data = NULL;
   }

   if ((flags & DRIVER_LOCATION) && !driver.location_data_own)
   {
      uninit_location();
      driver.location_data = NULL;
   }
   
   if ((flags & DRIVER_OSK) && !driver.osk_data_own)
   {
      uninit_osk();
      driver.osk_data = NULL;
   }

   if ((flags & DRIVER_INPUT) && !driver.input_data_own)
      driver.input_data = NULL;

   if ((flags & DRIVER_AUDIO) && !driver.audio_data_own)
      driver.audio_data = NULL;
}

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

#include <file/config_file.h>

#include "video_thread_wrapper.h"
#include "video_pixel_converter.h"
#include "video_context_driver.h"
#include "video_monitor.h"
#include "../config.def.h"
#include "../general.h"
#include "../performance.h"
#include "../string_list_special.h"

#ifndef MEASURE_FRAME_TIME_SAMPLES_COUNT
#define MEASURE_FRAME_TIME_SAMPLES_COUNT (2 * 1024)
#endif

typedef struct video_driver_state
{
   retro_time_t frame_time_samples[MEASURE_FRAME_TIME_SAMPLES_COUNT];
   struct retro_hw_render_callback hw_render_callback;
   uint64_t frame_time_samples_count;
   enum retro_pixel_format pix_fmt;

   unsigned video_width;
   unsigned video_height;
   float aspect_ratio;

   struct
   {
      const void *data;
      unsigned width;
      unsigned height;
      size_t pitch;
   } frame_cache;

   struct
   {
      rarch_softfilter_t *filter;

      void *buffer;
      unsigned scale;
      unsigned out_bpp;
      bool out_rgb32;
   } filter;
} video_driver_state_t;

static video_driver_state_t video_state;

static const video_driver_t *video_drivers[] = {
#ifdef HAVE_OPENGL
   &video_gl,
#endif
#ifdef XENON
   &video_xenon360,
#endif
#if defined(HAVE_D3D)
   &video_d3d,
#endif
#ifdef HAVE_VITA2D
   &video_vita2d,
#endif
#ifdef PSP
   &video_psp1,
#endif
#ifdef _3DS
   &video_ctr,
#endif
#ifdef HAVE_SDL
   &video_sdl,
#endif
#ifdef HAVE_SDL2
   &video_sdl2,
#endif
#ifdef HAVE_XVIDEO
   &video_xvideo,
#endif
#ifdef GEKKO
   &video_gx,
#endif
#ifdef HAVE_VG
   &video_vg,
#endif
#ifdef HAVE_OMAP
   &video_omap,
#endif
#ifdef HAVE_EXYNOS
   &video_exynos,
#endif
#ifdef HAVE_DISPMANX
   &video_dispmanx,
#endif
#ifdef HAVE_SUNXI
   &video_sunxi,
#endif
#ifdef HAVE_XSHM
   &video_xshm,
#endif
   &video_null,
   NULL,
};

/**
 * video_driver_find_handle:
 * @idx                : index of driver to get handle to.
 *
 * Returns: handle to video driver at index. Can be NULL
 * if nothing found.
 **/
const void *video_driver_find_handle(int idx)
{
   const void *drv = video_drivers[idx];
   if (!drv)
      return NULL;
   return drv;
}

/**
 * video_driver_find_ident:
 * @idx                : index of driver to get handle to.
 *
 * Returns: Human-readable identifier of video driver at index. Can be NULL
 * if nothing found.
 **/
const char *video_driver_find_ident(int idx)
{
   const video_driver_t *drv = video_drivers[idx];
   if (!drv)
      return NULL;
   return drv->ident;
}

/**
 * config_get_video_driver_options:
 *
 * Get an enumerated list of all video driver names, separated by '|'.
 *
 * Returns: string listing of all video driver names, separated by '|'.
 **/
const char* config_get_video_driver_options(void)
{
   return char_list_new_special(STRING_LIST_VIDEO_DRIVERS, NULL);
}

static bool find_video_driver(void)
{
   int i;
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();

#if defined(HAVE_OPENGL) && defined(HAVE_FBO)
   if (video_state.hw_render_callback.context_type)
   {
      RARCH_LOG("Using HW render, OpenGL driver forced.\n");
      driver->video = &video_gl;
      return true;
   }
#endif

   if (driver->frontend_ctx &&
       driver->frontend_ctx->get_video_driver)
   {
      driver->video = driver->frontend_ctx->get_video_driver();

      if (driver->video)
         return true;
      RARCH_WARN("Frontend supports get_video_driver() but did not specify one.\n");
   }

   i = find_driver_index("video_driver", settings->video.driver);
   if (i >= 0)
      driver->video = (const video_driver_t*)video_driver_find_handle(i);
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any video driver named \"%s\"\n",
            settings->video.driver);
      RARCH_LOG_OUTPUT("Available video drivers are:\n");
      for (d = 0; video_driver_find_handle(d); d++)
         RARCH_LOG_OUTPUT("\t%s\n", video_driver_find_ident(d));
      RARCH_WARN("Going to default to first video driver...\n");

      driver->video = (const video_driver_t*)video_driver_find_handle(0);

      if (!driver->video)
         retro_fail(1, "find_video_driver()");
   }

   return true;
}

/**
 * video_driver_get_ptr:
 *
 * Use this if you need the real video driver
 * and driver data pointers.
 *
 * Returns: video driver's userdata.
 **/
void *video_driver_get_ptr(void)
{
   driver_t *driver     = driver_get_ptr();

#ifdef HAVE_THREADS
   settings_t *settings = config_get_ptr();

   if (settings->video.threaded
         && !video_state.hw_render_callback.context_type)
      return rarch_threaded_video_get_ptr(NULL);
#endif

   return driver->video_data;
}

#define video_driver_get_poke_ptr(driver) (driver) ? driver->video_poke : NULL

#define video_driver_ctx_get_ptr(driver)  (driver) ? driver->video : NULL

const char *video_driver_get_ident(void)
{
   driver_t *driver     = driver_get_ptr();
   const video_driver_t *video = video_driver_ctx_get_ptr(driver);
   return (video) ? video->ident : NULL;
}

/**
 * video_driver_get_current_framebuffer:
 *
 * Gets pointer to current hardware renderer framebuffer object.
 * Used by RETRO_ENVIRONMENT_SET_HW_RENDER.
 *
 * Returns: pointer to hardware framebuffer object, otherwise 0.
 **/
uintptr_t video_driver_get_current_framebuffer(void)
{
   driver_t                   *driver = driver_get_ptr();
   const video_poke_interface_t *poke = video_driver_get_poke_ptr(driver);

   if (poke && poke->get_current_framebuffer)
      return poke->get_current_framebuffer(driver->video_data);
   return 0;
}

static uint64_t video_frame_count;

retro_proc_address_t video_driver_get_proc_address(const char *sym)
{
   driver_t                   *driver = driver_get_ptr();
   const video_poke_interface_t *poke = video_driver_get_poke_ptr(driver);

   if (poke && poke->get_proc_address)
      return poke->get_proc_address(driver->video_data, sym);
   return NULL;
}

bool video_driver_set_shader(enum rarch_shader_type type,
      const char *path)
{
   driver_t            *driver = driver_get_ptr();
   const video_driver_t *video = video_driver_ctx_get_ptr(driver);

   if (video && video->set_shader)
      return video->set_shader(driver->video_data, type, path);
   return false;
}

static void deinit_video_filter(void)
{
   rarch_softfilter_free(video_state.filter.filter);
#ifdef _3DS
   linearFree(video_state.filter.buffer);
#else
   free(video_state.filter.buffer);
#endif
   memset(&video_state.filter, 0, sizeof(video_state.filter));
}

static void init_video_filter(enum retro_pixel_format colfmt)
{
   unsigned width, height, pow2_x, pow2_y, maxsize;
   struct retro_game_geometry *geom = NULL;
   settings_t *settings             = config_get_ptr();
   struct retro_system_av_info *av_info =
      video_viewport_get_system_av_info();

   deinit_video_filter();

   if (!*settings->video.softfilter_plugin)
      return;

   /* Deprecated format. Gets pre-converted. */
   if (colfmt == RETRO_PIXEL_FORMAT_0RGB1555)
      colfmt = RETRO_PIXEL_FORMAT_RGB565;

   if (video_state.hw_render_callback.context_type)
   {
      RARCH_WARN("Cannot use CPU filters when hardware rendering is used.\n");
      return;
   }

   geom    = av_info ? (struct retro_game_geometry*)&av_info->geometry : NULL;

   if (!geom)
      return;

   width   = geom->max_width;
   height  = geom->max_height;

   video_state.filter.filter = rarch_softfilter_new(
         settings->video.softfilter_plugin,
         RARCH_SOFTFILTER_THREADS_AUTO, colfmt, width, height);

   if (!video_state.filter.filter)
   {
      RARCH_ERR("Failed to load filter.\n");
      return;
   }

   rarch_softfilter_get_max_output_size(video_state.filter.filter,
         &width, &height);

   pow2_x                    = next_pow2(width);
   pow2_y                    = next_pow2(height);
   maxsize                   = max(pow2_x, pow2_y);
   video_state.filter.scale  = maxsize / RARCH_SCALE_BASE;
   video_state.filter.out_rgb32 = rarch_softfilter_get_output_format(
         video_state.filter.filter) == RETRO_PIXEL_FORMAT_XRGB8888;

   video_state.filter.out_bpp = video_state.filter.out_rgb32 ?
      sizeof(uint32_t) : sizeof(uint16_t);

   /* TODO: Aligned output. */
#ifdef _3DS
   video_state.filter.buffer = linearMemAlign(width * height * video_state.filter.out_bpp, 0x80);
#else
   video_state.filter.buffer = malloc(width * height * video_state.filter.out_bpp);
#endif
   if (!video_state.filter.buffer)
      goto error;

   return;

error:
   RARCH_ERR("Softfilter initialization failed.\n");
   deinit_video_filter();
}

static void init_video_input(const input_driver_t *tmp)
{
   driver_t *driver = driver_get_ptr();

   /* Reset video frame count */
   video_frame_count = 0;

   /* Video driver didn't provide an input driver,
    * so we use configured one. */
   RARCH_LOG("Graphics driver did not initialize an input driver. Attempting to pick a suitable driver.\n");

   if (tmp)
      driver->input = tmp;
   else
      find_input_driver();

   /* This should never really happen as tmp (driver.input) is always
    * found before this in find_driver_input(), or we have aborted
    * in a similar fashion anyways. */
   if (!driver->input)
      goto error;

   driver->input_data = input_driver_init();

   if (driver->input_data)
      return;

error:
   RARCH_ERR("Cannot initialize input driver. Exiting ...\n");
   retro_fail(1, "init_video_input()");
}

static void video_driver_unset_callback(void)
{
   struct retro_hw_render_callback *hw_render =
      video_driver_callback();

   if (hw_render)
      hw_render = NULL;
}

/**
 * video_monitor_compute_fps_statistics:
 *
 * Computes monitor FPS statistics.
 **/
static void video_monitor_compute_fps_statistics(void)
{
   double avg_fps = 0.0, stddev = 0.0;
   unsigned samples = 0;
   settings_t *settings = config_get_ptr();

   if (settings->video.threaded)
   {
      RARCH_LOG("Monitor FPS estimation is disabled for threaded video.\n");
      return;
   }

   if (video_state.frame_time_samples_count < 2 * MEASURE_FRAME_TIME_SAMPLES_COUNT)
   {
      RARCH_LOG(
            "Does not have enough samples for monitor refresh rate estimation. Requires to run for at least %u frames.\n",
            2 * MEASURE_FRAME_TIME_SAMPLES_COUNT);
      return;
   }

   if (video_monitor_fps_statistics(&avg_fps, &stddev, &samples))
   {
      RARCH_LOG("Average monitor Hz: %.6f Hz. (%.3f %% frame time deviation, based on %u last samples).\n",
            avg_fps, 100.0 * stddev, samples);
   }
}

static bool uninit_video_input(void)
{
   driver_t *driver = driver_get_ptr();

   event_command(EVENT_CMD_OVERLAY_DEINIT);

   if (
         !driver->input_data_own &&
         (driver->input_data != driver->video_data)
      )
      input_driver_free();

   if (
         !driver->video_data_own &&
         driver->video_data &&
         driver->video &&
         driver->video->free)
      driver->video->free(driver->video_data);

   deinit_pixel_converter();

   deinit_video_filter();

   video_driver_unset_callback();
   event_command(EVENT_CMD_SHADER_DIR_DEINIT);
   video_monitor_compute_fps_statistics();

   return true;
}

static bool init_video(void)
{
   unsigned max_dim, scale, width, height;
   video_viewport_t *custom_vp      = NULL;
   const input_driver_t *tmp        = NULL;
   const struct retro_game_geometry *geom = NULL;
   video_info_t video               = {0};
   static uint16_t dummy_pixels[32] = {0};
   driver_t *driver                 = driver_get_ptr();
   settings_t *settings             = config_get_ptr();
   rarch_system_info_t *system      = rarch_system_info_get_ptr();
   struct retro_system_av_info *av_info =
      video_viewport_get_system_av_info();

   init_video_filter(video_state.pix_fmt);
   event_command(EVENT_CMD_SHADER_DIR_INIT);

   if (av_info)
      geom      = (const struct retro_game_geometry*)&av_info->geometry;

   if (!geom)
   {
      RARCH_ERR("AV geometry not initialized, cannot initialize video driver.\n");
      goto error;
   }

   max_dim   = max(geom->max_width, geom->max_height);
   scale     = next_pow2(max_dim) / RARCH_SCALE_BASE;
   scale     = max(scale, 1);

   if (video_state.filter.filter)
      scale = video_state.filter.scale;

   /* Update core-dependent aspect ratio values. */
   video_viewport_set_square_pixel(geom->base_width, geom->base_height);
   video_viewport_set_core();
   video_viewport_set_config();

   /* Update CUSTOM viewport. */
   custom_vp = video_viewport_get_custom();

   if (settings->video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      float default_aspect = aspectratio_lut[ASPECT_RATIO_CORE].value;
      aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
         (custom_vp->width && custom_vp->height) ?
         (float)custom_vp->width / custom_vp->height : default_aspect;
   }

   video_driver_set_aspect_ratio_value(
      aspectratio_lut[settings->video.aspect_ratio_idx].value);

   if (settings->video.fullscreen)
   {
      width  = settings->video.fullscreen_x;
      height = settings->video.fullscreen_y;
   }
   else
   {
      if (settings->video.force_aspect)
      {
         /* Do rounding here to simplify integer scale correctness. */
         unsigned base_width =
            roundf(geom->base_height * video_driver_get_aspect_ratio());
         width  = roundf(base_width * settings->video.scale);
      }
      else
         width  = roundf(geom->base_width   * settings->video.scale);
      height = roundf(geom->base_height * settings->video.scale);
   }

   if (width && height)
      RARCH_LOG("Video @ %ux%u\n", width, height);
   else
      RARCH_LOG("Video @ fullscreen\n");

   driver->display_type  = RARCH_DISPLAY_NONE;
   driver->video_display = 0;
   driver->video_window  = 0;

   if (!init_video_pixel_converter(RARCH_SCALE_BASE * scale))
   {
      RARCH_ERR("Failed to initialize pixel converter.\n");
      goto error;
   }

   video.width        = width;
   video.height       = height;
   video.fullscreen   = settings->video.fullscreen;
   video.vsync        = settings->video.vsync && !system->force_nonblock;
   video.force_aspect = settings->video.force_aspect;
#ifdef GEKKO
   video.viwidth      = settings->video.viwidth;
   video.vfilter      = settings->video.vfilter;
#endif
   video.smooth       = settings->video.smooth;
   video.input_scale  = scale;
   video.rgb32        = video_state.filter.filter ?
      video_state.filter.out_rgb32 :
      (video_state.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);

   tmp = (const input_driver_t*)driver->input;
   /* Need to grab the "real" video driver interface on a reinit. */
   video_driver_ctl(RARCH_DISPLAY_CTL_FIND_DRIVER, NULL);

#ifdef HAVE_THREADS
   if (settings->video.threaded && !video_state.hw_render_callback.context_type)
   {
      /* Can't do hardware rendering with threaded driver currently. */
      RARCH_LOG("Starting threaded video driver ...\n");

      if (!rarch_threaded_video_init(&driver->video, &driver->video_data,
               &driver->input, &driver->input_data,
               driver->video, &video))
      {
         RARCH_ERR("Cannot open threaded video driver ... Exiting ...\n");
         goto error;
      }
   }
   else
#endif
      driver->video_data = driver->video->init(&video, &driver->input,
            &driver->input_data);

   if (!driver->video_data)
   {
      RARCH_ERR("Cannot open video driver ... Exiting ...\n");
      goto error;
   }

   driver->video_poke = NULL;
   if (driver->video->poke_interface)
      driver->video->poke_interface(driver->video_data, &driver->video_poke);

   if (driver->video->viewport_info && (!custom_vp->width ||
            !custom_vp->height))
   {
      /* Force custom viewport to have sane parameters. */
      custom_vp->width = width;
      custom_vp->height = height;
      video_driver_viewport_info(custom_vp);
   }

   video_driver_set_rotation(
            (settings->video.rotation + system->rotation) % 4);

   video_driver_suppress_screensaver(settings->ui.suspend_screensaver_enable);

   if (!driver->input)
      init_video_input(tmp);

   event_command(EVENT_CMD_OVERLAY_DEINIT);
   event_command(EVENT_CMD_OVERLAY_INIT);

   video_driver_cached_frame_set(&dummy_pixels, 4, 4, 8);

#if defined(PSP)
   video_driver_set_texture_frame(&dummy_pixels, false, 1, 1, 1.0f);
#endif

   return true;

error:
   retro_fail(1, "init_video()");
   return false;
}

bool video_driver_suppress_screensaver(bool enable)
{
   driver_t            *driver = driver_get_ptr();
   const video_driver_t *video = video_driver_ctx_get_ptr(driver);

   return video->suppress_screensaver(driver->video_data, enable);
}


bool video_driver_set_viewport(unsigned width, unsigned height,
      bool force_fullscreen, bool allow_rotate)
{
   driver_t                   *driver = driver_get_ptr();
   const video_driver_t        *video = video_driver_ctx_get_ptr(driver);

   if (video && video->set_viewport)
   {
      video->set_viewport(driver->video_data, width, height,
            force_fullscreen, allow_rotate);
      return true;
   }
   return false;
}

bool video_driver_set_rotation(unsigned rotation)
{
   driver_t                   *driver = driver_get_ptr();
   const video_driver_t        *video = video_driver_ctx_get_ptr(driver);

   if (video && video->set_rotation)
   {
      video->set_rotation(driver->video_data, rotation);
      return true;
   }
   return false;
}


bool video_driver_set_video_mode(unsigned width,
      unsigned height, bool fullscreen)
{
   driver_t                   *driver = driver_get_ptr();
   const video_poke_interface_t *poke = video_driver_get_poke_ptr(driver);

   if (poke && poke->set_video_mode)
   {
      poke->set_video_mode(driver->video_data,
            width, height, fullscreen);
      return true;
   }

   return gfx_ctx_set_video_mode(driver->video_context_data, width, height, fullscreen);
}

bool video_driver_get_video_output_size(unsigned *width, unsigned *height)
{
   driver_t                   *driver = driver_get_ptr();
   const video_poke_interface_t *poke = video_driver_get_poke_ptr(driver);

   if (poke && poke->get_video_output_size)
   {
      poke->get_video_output_size(driver->video_data, width, height);
      return true;
   }
   return false;
}



void video_driver_set_osd_msg(const char *msg,
      const struct font_params *params, void *font)
{
   driver_t                   *driver = driver_get_ptr();
   const video_poke_interface_t *poke = video_driver_get_poke_ptr(driver);

   if (poke && poke->set_osd_msg)
      poke->set_osd_msg(driver->video_data, msg, params, font);
}

void video_driver_set_texture_enable(bool enable, bool fullscreen)
{
#ifdef HAVE_MENU
   driver_t                   *driver = driver_get_ptr();
   const video_poke_interface_t *poke = video_driver_get_poke_ptr(driver);

   if (poke && poke->set_texture_enable)
      poke->set_texture_enable(driver->video_data,
            enable, fullscreen);
#endif
}

void video_driver_set_texture_frame(const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
#ifdef HAVE_MENU
   driver_t                   *driver = driver_get_ptr();
   const video_poke_interface_t *poke = video_driver_get_poke_ptr(driver);

   if (poke &&  poke->set_texture_frame)
      poke->set_texture_frame(driver->video_data,
            frame, rgb32, width, height, alpha);
#endif
}

bool video_driver_viewport_info(struct video_viewport *vp)
{
   driver_t            *driver = driver_get_ptr();
   const video_driver_t *video = video_driver_ctx_get_ptr(driver);

   if (video && video->viewport_info)
   {
      video->viewport_info(driver->video_data, vp);
      return true;
   }
   return false;
}


#ifdef HAVE_OVERLAY
bool video_driver_overlay_interface(const video_overlay_interface_t **iface)
{
   driver_t            *driver = driver_get_ptr();
   const video_driver_t *video = video_driver_ctx_get_ptr(driver);

   if (video && video->overlay_interface)
   {
      video->overlay_interface(driver->video_data, iface);
      return true;
   }
   return false;
}
#endif

void *video_driver_read_frame_raw(unsigned *width,
   unsigned *height, size_t *pitch)
{
   driver_t            *driver = driver_get_ptr();
   const video_driver_t *video = video_driver_ctx_get_ptr(driver);

   if (video && video->read_frame_raw)
      return video->read_frame_raw(driver->video_data, width,
            height, pitch);
   return NULL;
}

void video_driver_set_filtering(unsigned index, bool smooth)
{
   driver_t                   *driver = driver_get_ptr();
   const video_poke_interface_t *poke = video_driver_get_poke_ptr(driver);

   if (poke && poke->set_filtering)
      poke->set_filtering(driver->video_data, index, smooth);
}

void video_driver_cached_frame_set_ptr(const void *data)
{
   if (!data)
      return;

   video_state.frame_cache.data   = data;
}

void video_driver_cached_frame_set(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   video_state.frame_cache.data   = data;
   video_state.frame_cache.width  = width;
   video_state.frame_cache.height = height;
   video_state.frame_cache.pitch  = pitch;
}

void video_driver_cached_frame_get(const void **data, unsigned *width,
      unsigned *height, size_t *pitch)
{
   if (data)
      *data    = video_state.frame_cache.data;
   if (width)
      *width  = video_state.frame_cache.width;
   if (height)
      *height = video_state.frame_cache.height;
   if (pitch)
      *pitch  = video_state.frame_cache.pitch;
}

void video_driver_get_size(unsigned *width, unsigned *height)
{
   if (width)
      *width  = video_state.video_width;
   if (height)
      *height = video_state.video_height;
}

void video_driver_set_size_width(unsigned width)
{
   video_state.video_width = width;
}

void video_driver_set_size_height(unsigned height)
{
   video_state.video_height = height;
}


/**
 * video_monitor_set_refresh_rate:
 * @hz                 : New refresh rate for monitor.
 *
 * Sets monitor refresh rate to new value.
 **/
void video_monitor_set_refresh_rate(float hz)
{
   char msg[PATH_MAX_LENGTH];
   settings_t *settings = config_get_ptr();

   snprintf(msg, sizeof(msg), "Setting refresh rate to: %.3f Hz.", hz);
   rarch_main_msg_queue_push(msg, 1, 180, false);
   RARCH_LOG("%s\n", msg);

   settings->video.refresh_rate = hz;
}


/**
 * video_monitor_fps_statistics
 * @refresh_rate       : Monitor refresh rate.
 * @deviation          : Deviation from measured refresh rate.
 * @sample_points      : Amount of sampled points.
 *
 * Gets the monitor FPS statistics based on the current
 * runtime.
 *
 * Returns: true (1) on success.
 * false (0) if:
 * a) threaded video mode is enabled
 * b) less than 2 frame time samples.
 * c) FPS monitor enable is off.
 **/
bool video_monitor_fps_statistics(double *refresh_rate,
      double *deviation, unsigned *sample_points)
{
   unsigned i;
   retro_time_t accum   = 0, avg, accum_var = 0;
   unsigned samples     = 0;
   settings_t *settings = config_get_ptr();

   samples = min(MEASURE_FRAME_TIME_SAMPLES_COUNT,
         video_state.frame_time_samples_count);

   if (settings->video.threaded || (samples < 2))
      return false;

   /* Measure statistics on frame time (microsecs), *not* FPS. */
   for (i = 0; i < samples; i++)
      accum += video_state.frame_time_samples[i];

#if 0
   for (i = 0; i < samples; i++)
      RARCH_LOG("Interval #%u: %d usec / frame.\n",
            i, (int)video_state.frame_time_samples[i]);
#endif

   avg = accum / samples;

   /* Drop first measurement. It is likely to be bad. */
   for (i = 0; i < samples; i++)
   {
      retro_time_t diff = video_state.frame_time_samples[i] - avg;
      accum_var += diff * diff;
   }

   *deviation     = sqrt((double)accum_var / (samples - 1)) / avg;
   *refresh_rate  = 1000000.0 / avg;
   *sample_points = samples;

   return true;
}

#ifndef TIME_TO_FPS
#define TIME_TO_FPS(last_time, new_time, frames) ((1000000.0f * (frames)) / ((new_time) - (last_time)))
#endif

#define FPS_UPDATE_INTERVAL 256

/**
 * video_monitor_get_fps:
 * @buf           : string suitable for Window title
 * @size          : size of buffer.
 * @buf_fps       : string of raw FPS only (optional).
 * @size_fps      : size of raw FPS buffer.
 *
 * Get the amount of frames per seconds.
 *
 * Returns: true if framerate per seconds could be obtained,
 * otherwise false.
 *
 **/

#ifdef _WIN32
#define U64_SIGN "%I64u"
#else
#define U64_SIGN "%llu"
#endif

bool video_monitor_get_fps(char *buf, size_t size,
      char *buf_fps, size_t size_fps)
{
   retro_time_t        new_time;
   static retro_time_t curr_time;
   static retro_time_t fps_time;
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   *buf = '\0';

   new_time = retro_get_time_usec();

   if (video_frame_count)
   {
      static float last_fps;
      bool ret = false;
      unsigned write_index = video_state.frame_time_samples_count++ &
         (MEASURE_FRAME_TIME_SAMPLES_COUNT - 1);

      video_state.frame_time_samples[write_index] = new_time - fps_time;
      fps_time = new_time;

      if ((video_frame_count % FPS_UPDATE_INTERVAL) == 0)
      {
         last_fps = TIME_TO_FPS(curr_time, new_time, FPS_UPDATE_INTERVAL);
         curr_time = new_time;

         snprintf(buf, size, "%s || FPS: %6.1f || Frames: " U64_SIGN,
               system->title_buf, last_fps, (unsigned long long)video_frame_count);
         ret = true;
      }

      if (buf_fps)
         snprintf(buf_fps, size_fps, "FPS: %6.1f || Frames: " U64_SIGN,
               last_fps, (unsigned long long)video_frame_count);

      return ret;
   }

   curr_time = fps_time = new_time;
   strlcpy(buf, system->title_buf, size);
   if (buf_fps)
      strlcpy(buf_fps, "N/A", size_fps);

   return true;
}

float video_driver_get_aspect_ratio(void)
{
   return video_state.aspect_ratio;
}

void video_driver_set_aspect_ratio_value(float value)
{
   video_state.aspect_ratio = value;
}

struct retro_hw_render_callback *video_driver_callback(void)
{
   return &video_state.hw_render_callback;
}


bool video_driver_frame_filter(const void *data,
      unsigned width, unsigned height,
      size_t pitch,
      unsigned *output_width, unsigned *output_height,
      unsigned *output_pitch)
{
   static struct retro_perf_counter softfilter_process = {0};
   settings_t *settings = config_get_ptr();

   rarch_perf_init(&softfilter_process, "softfilter_process");

   if (!video_state.filter.filter || !data)
      return false;

   rarch_softfilter_get_output_size(video_state.filter.filter,
         output_width, output_height, width, height);

   *output_pitch = (*output_width) * video_state.filter.out_bpp;

   retro_perf_start(&softfilter_process);
   rarch_softfilter_process(video_state.filter.filter,
         video_state.filter.buffer, *output_pitch,
         data, width, height, pitch);
   retro_perf_stop(&softfilter_process);

   if (settings->video.post_filter_record)
      recording_dump_frame(video_state.filter.buffer,
            *output_width, *output_height, *output_pitch);

   return true;
}

rarch_softfilter_t *video_driver_frame_filter_get_ptr(void)
{
   return video_state.filter.filter;
}

void *video_driver_frame_filter_get_buf_ptr(void)
{
   return video_state.filter.buffer;
}

enum retro_pixel_format video_driver_get_pixel_format(void)
{
   return video_state.pix_fmt;
}

void video_driver_set_pixel_format(enum retro_pixel_format fmt)
{
   video_state.pix_fmt = fmt;
}

/**
 * video_driver_cached_frame:
 *
 * Renders the current video frame.
 **/
static bool video_driver_cached_frame(driver_t *driver)
{
   bool is_idle;
   void *recording    = driver ? driver->recording_data : NULL;

   rarch_main_ctl(RARCH_MAIN_CTL_IS_IDLE, &is_idle);

   if (is_idle)
      return true; /* Maybe return false here for indication of idleness? */

   /* Cannot allow recording when pushing duped frames. */
   driver->recording_data = NULL;

   /* Not 100% safe, since the library might have
    * freed the memory, but no known implementations do this.
    * It would be really stupid at any rate ...
    */
   if (driver->retro_ctx.frame_cb)
      driver->retro_ctx.frame_cb(
            (video_state.frame_cache.data == RETRO_HW_FRAME_BUFFER_VALID)
            ? NULL : video_state.frame_cache.data,
            video_state.frame_cache.width,
            video_state.frame_cache.height,
            video_state.frame_cache.pitch);

   driver->recording_data = recording;

   return true;
}

static void video_monitor_adjust_system_rates(void)
{
   float timing_skew;
   const struct retro_system_timing *info = NULL;
   struct retro_system_av_info *av_info =
      video_viewport_get_system_av_info();
   settings_t *settings = config_get_ptr();
   rarch_system_info_t *system = rarch_system_info_get_ptr();

   if (!system)
      return;

   system->force_nonblock = false;

   if  (av_info)
      info = (const struct retro_system_timing*)&av_info->timing;

   if (!info || info->fps <= 0.0)
      return;

   timing_skew = fabs(1.0f - info->fps / settings->video.refresh_rate);

   /* We don't want to adjust pitch too much. If we have extreme cases,
    * just don't readjust at all. */
   if (timing_skew <= settings->audio.max_timing_skew)
      return;

   RARCH_LOG("Timings deviate too much. Will not adjust. (Display = %.2f Hz, Game = %.2f Hz)\n",
         settings->video.refresh_rate,
         (float)info->fps);

   if (info->fps <= settings->video.refresh_rate)
      return;

   /* We won't be able to do VSync reliably when game FPS > monitor FPS. */
   system->force_nonblock = true;
   RARCH_LOG("Game FPS > Monitor FPS. Cannot rely on VSync.\n");
}

bool video_driver_ctl(enum rarch_display_ctl_state state, void *data)
{
   driver_t                 *driver   = driver_get_ptr();
   const video_driver_t        *video = video_driver_ctx_get_ptr(driver);
   const video_poke_interface_t *poke = video_driver_get_poke_ptr(driver);
   settings_t               *settings = config_get_ptr();

   switch (state)
   {
      case RARCH_DISPLAY_CTL_GET_NEXT_VIDEO_OUT:
         if (poke && poke->get_video_output_next)
         {
            poke->get_video_output_next(driver->video_data);
            return true;
         }
         return gfx_ctx_get_video_output_next(driver->video_context_data);
      case RARCH_DISPLAY_CTL_GET_PREV_VIDEO_OUT:
         if (poke && poke->get_video_output_prev)
         {
            poke->get_video_output_prev(driver->video_data);
            return true;
         }
         return gfx_ctx_get_video_output_next(driver->video_context_data);
      case RARCH_DISPLAY_CTL_INIT:
         return init_video();
      case RARCH_DISPLAY_CTL_DEINIT:
         return uninit_video_input();
      case RARCH_DISPLAY_CTL_MONITOR_RESET:
         video_state.frame_time_samples_count = 0;
         return true;
      case RARCH_DISPLAY_CTL_MONITOR_ADJUST_SYSTEM_RATES:
         video_monitor_adjust_system_rates();
         return true;
      case RARCH_DISPLAY_CTL_SET_ASPECT_RATIO:
         if (!poke || !poke->set_aspect_ratio)
            return false;
         poke->set_aspect_ratio(driver->video_data, settings->video.aspect_ratio_idx);
         return true;
      case RARCH_DISPLAY_CTL_SHOW_MOUSE:
         {
            bool *toggle                  = (bool*)data;

            if (poke && poke->show_mouse)
               poke->show_mouse(driver->video_data, *toggle);
         }
         return true;
      case RARCH_DISPLAY_CTL_SET_NONBLOCK_STATE:
         {
            bool *toggle                  = (bool*)data;

            if (!toggle || !video || !driver)
               return false;

            if (video && video->set_nonblock_state)
               video->set_nonblock_state(driver->video_data, *toggle);
         }
         return true;
      case RARCH_DISPLAY_CTL_FIND_DRIVER:
         return find_video_driver();
      case RARCH_DISPLAY_CTL_APPLY_STATE_CHANGES:
         if (poke && poke->apply_state_changes)
            poke->apply_state_changes(driver->video_data);
         return true;
      case RARCH_DISPLAY_CTL_READ_VIEWPORT:
         if (video && video->read_viewport)
            return video->read_viewport(driver->video_data,
                  (uint8_t*)data);
         return false;
      case RARCH_DISPLAY_CTL_CACHED_FRAME_HAS_VALID_FB:
         if (!video_state.frame_cache.data)
            return false;
         return (video_state.frame_cache.data == RETRO_HW_FRAME_BUFFER_VALID);
      case RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER:
         return video_driver_cached_frame(driver);
      case RARCH_DISPLAY_CTL_IS_FOCUSED:
         return video->focus(driver->video_data);
      case RARCH_DISPLAY_CTL_HAS_WINDOWED:
         return video->has_windowed(driver->video_data);
      case RARCH_DISPLAY_CTL_GET_FRAME_COUNT:
         {
            uint64_t **ptr = (uint64_t**)data;
            if (!ptr)
               return false;
            *ptr = &video_frame_count;
         }
         return true;
      case RARCH_DISPLAY_CTL_FRAME_FILTER_ALIVE:
         if (video_state.filter.filter)
            return true;
         return false;
      case RARCH_DISPLAY_CTL_FRAME_FILTER_IS_32BIT:
         return video_state.filter.out_rgb32;
      case RARCH_DISPLAY_CTL_DEFAULT_SETTINGS:
         {
            global_t *global    = global_get_ptr();

            if (!global)
               return false;

            global->console.screen.gamma_correction       = DEFAULT_GAMMA;
            global->console.flickerfilter_enable          = false;
            global->console.softfilter_enable             = false;

            global->console.screen.resolutions.current.id = 0;
         }
         return true;
      case RARCH_DISPLAY_CTL_LOAD_SETTINGS:
         {
            global_t *global    = global_get_ptr();
            config_file_t *conf = (config_file_t*)data;

            if (!conf)
               return false;

            CONFIG_GET_BOOL_BASE(conf, global, console.screen.gamma_correction, "gamma_correction");
            config_get_bool(conf, "flicker_filter_enable",
                  &global->console.flickerfilter_enable);
            config_get_bool(conf, "soft_filter_enable",
                  &global->console.softfilter_enable);

            CONFIG_GET_INT_BASE(conf, global, console.screen.resolutions.width,
                  "console_resolution_width");
            CONFIG_GET_INT_BASE(conf, global, console.screen.resolutions.height,
                  "console_resolution_height");
            CONFIG_GET_INT_BASE(conf, global, console.screen.soft_filter_index,
                  "soft_filter_index");
            CONFIG_GET_INT_BASE(conf, global, console.screen.resolutions.current.id,
                  "current_resolution_id");
            CONFIG_GET_INT_BASE(conf, global, console.screen.flicker_filter_index,
                  "flicker_filter_index");
         }
         return true;
      case RARCH_DISPLAY_CTL_SAVE_SETTINGS:
         {
            global_t *global    = global_get_ptr();
            config_file_t *conf = (config_file_t*)data;

            if (!conf)
               return false;

            config_set_bool(conf, "gamma_correction",
                  global->console.screen.gamma_correction);
            config_set_bool(conf, "flicker_filter_enable",
                  global->console.flickerfilter_enable);
            config_set_bool(conf, "soft_filter_enable",
                  global->console.softfilter_enable);

            config_set_int(conf, "console_resolution_width",
                  global->console.screen.resolutions.width);
            config_set_int(conf, "console_resolution_height",
                  global->console.screen.resolutions.height);
            config_set_int(conf, "soft_filter_index",
                  global->console.screen.soft_filter_index);
            config_set_int(conf, "current_resolution_id",
                  global->console.screen.resolutions.current.id);
            config_set_int(conf, "flicker_filter_index",
                  global->console.screen.flicker_filter_index);
         }
         return true;
      case RARCH_DISPLAY_CTL_NONE:
      default:
         break;
   }

   return false;
}

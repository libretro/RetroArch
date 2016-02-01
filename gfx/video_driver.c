/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include <stddef.h>
#include <string.h>

#include <gfx/scaler/pixconv.h>
#include <gfx/scaler/scaler.h>

#include <file/config_file.h>

#include "video_thread_wrapper.h"
#include "../frontend/frontend_driver.h"
#include "video_context_driver.h"
#include "../record/record_driver.h"
#include "../config.def.h"
#include "../general.h"
#include "../performance.h"
#include "../string_list_special.h"
#include "../libretro_version_1.h"
#include "../system.h"
#include "../command_event.h"

#ifdef HAVE_MENU
#include "../menu/menu_hash.h"
#include "../menu/menu_setting.h"
#endif

#include "../verbosity.h"

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

typedef struct video_pixel_scaler
{
   struct scaler_ctx *scaler;
   void *scaler_out;
} video_pixel_scaler_t;

/* Opaque handles to currently running window.
 * Used by e.g. input drivers which bind to a window.
 * Drivers are responsible for setting these if an input driver
 * could potentially make use of this. */
static uintptr_t video_driver_display;
static uintptr_t video_driver_window;
static enum rarch_display_type video_driver_display_type;

static uint64_t video_driver_frame_count;

static void *video_driver_data;
static video_driver_t *current_video;

/* Interface for "poking". */
static const video_poke_interface_t *video_driver_poke;

static video_driver_state_t video_driver_state;

/* Used for 16-bit -> 16-bit conversions that take place before
 * being passed to video driver. */
static video_pixel_scaler_t *video_driver_scaler_ptr;

char rotation_lut[4][32] =
{
   "Normal",
   "90 deg",
   "180 deg",
   "270 deg"
};

struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END] = {
   { "4:3",           1.3333f },
   { "16:9",          1.7778f },
   { "16:10",         1.6f },
   { "16:15",         16.0f / 15.0f },
   { "1:1",           1.0f },
   { "2:1",           2.0f },
   { "3:2",           1.5f },
   { "3:4",           0.75f },
   { "4:1",           4.0f },
   { "4:4",           1.0f },
   { "5:4",           1.25f },
   { "6:5",           1.2f },
   { "7:9",           0.7777f },
   { "8:3",           2.6666f },
   { "8:7",           1.1428f },
   { "19:12",         1.5833f },
   { "19:14",         1.3571f },
   { "30:17",         1.7647f },
   { "32:9",          3.5555f },
   { "Config",        0.0f },
   { "Square pixel",  1.0f },
   { "Core provided", 1.0f },
   { "Custom",        0.0f }
};

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
   driver_ctx_info_t drv;
   settings_t *settings = config_get_ptr();

#if defined(HAVE_OPENGL) && defined(HAVE_FBO)
   if (video_driver_state.hw_render_callback.context_type)
   {
      RARCH_LOG("Using HW render, OpenGL driver forced.\n");
      current_video = &video_gl;
      return true;
   }
#endif

   if (frontend_driver_has_get_video_driver_func())
   {
      current_video = (video_driver_t*)frontend_driver_get_video_driver();

      if (current_video)
         return true;
      RARCH_WARN("Frontend supports get_video_driver() but did not specify one.\n");
   }

   drv.label = "video_driver";
   drv.s     = settings->video.driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i = drv.len;

   if (i >= 0)
      current_video = (video_driver_t*)video_driver_find_handle(i);
   else
   {
      unsigned d;
      RARCH_ERR("Couldn't find any video driver named \"%s\"\n",
            settings->video.driver);
      RARCH_LOG_OUTPUT("Available video drivers are:\n");
      for (d = 0; video_driver_find_handle(d); d++)
         RARCH_LOG_OUTPUT("\t%s\n", video_driver_find_ident(d));
      RARCH_WARN("Going to default to first video driver...\n");

      current_video = (video_driver_t*)video_driver_find_handle(0);

      if (!current_video)
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
void *video_driver_get_ptr(bool force_nonthreaded_data)
{
#ifdef HAVE_THREADS
   settings_t *settings = config_get_ptr();

   if (settings->video.threaded
         && !video_driver_state.hw_render_callback.context_type && !force_nonthreaded_data)
      return rarch_threaded_video_get_ptr(NULL);
#endif

   return video_driver_data;
}

const char *video_driver_get_ident(void)
{
   return (current_video) ? current_video->ident : NULL;
}

const video_poke_interface_t *video_driver_get_poke(void)
{
   return video_driver_poke;
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
   if (video_driver_poke && video_driver_poke->get_current_framebuffer)
      return video_driver_poke->get_current_framebuffer(video_driver_data);
   return 0;
}

bool video_driver_get_current_software_framebuffer(struct retro_framebuffer *framebuffer)
{
   if (video_driver_poke && video_driver_poke->get_current_software_framebuffer)
      return video_driver_poke->get_current_software_framebuffer(video_driver_data, framebuffer);
   return false;
}

retro_proc_address_t video_driver_get_proc_address(const char *sym)
{
   if (video_driver_poke && video_driver_poke->get_proc_address)
      return video_driver_poke->get_proc_address(video_driver_data, sym);
   return NULL;
}

bool video_driver_set_shader(enum rarch_shader_type type,
      const char *path)
{
   if (current_video->set_shader)
      return current_video->set_shader(video_driver_data, type, path);
   return false;
}

static void deinit_video_filter(void)
{
   rarch_softfilter_free(video_driver_state.filter.filter);
#ifdef _3DS
   linearFree(video_driver_state.filter.buffer);
#else
   free(video_driver_state.filter.buffer);
#endif
   memset(&video_driver_state.filter, 0, sizeof(video_driver_state.filter));
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

   if (video_driver_state.hw_render_callback.context_type)
   {
      RARCH_WARN("Cannot use CPU filters when hardware rendering is used.\n");
      return;
   }

   geom    = av_info ? (struct retro_game_geometry*)&av_info->geometry : NULL;

   if (!geom)
      return;

   width   = geom->max_width;
   height  = geom->max_height;

   video_driver_state.filter.filter = rarch_softfilter_new(
         settings->video.softfilter_plugin,
         RARCH_SOFTFILTER_THREADS_AUTO, colfmt, width, height);

   if (!video_driver_state.filter.filter)
   {
      RARCH_ERR("Failed to load filter.\n");
      return;
   }

   rarch_softfilter_get_max_output_size(video_driver_state.filter.filter,
         &width, &height);

   pow2_x                    = next_pow2(width);
   pow2_y                    = next_pow2(height);
   maxsize                   = max(pow2_x, pow2_y);
   video_driver_state.filter.scale  = maxsize / RARCH_SCALE_BASE;
   video_driver_state.filter.out_rgb32 = rarch_softfilter_get_output_format(
         video_driver_state.filter.filter) == RETRO_PIXEL_FORMAT_XRGB8888;

   video_driver_state.filter.out_bpp = video_driver_state.filter.out_rgb32 ?
      sizeof(uint32_t) : sizeof(uint16_t);

   /* TODO: Aligned output. */
#ifdef _3DS
   video_driver_state.filter.buffer = linearMemAlign(width * height * video_driver_state.filter.out_bpp, 0x80);
#else
   video_driver_state.filter.buffer = malloc(width * height * video_driver_state.filter.out_bpp);
#endif
   if (!video_driver_state.filter.buffer)
      goto error;

   return;

error:
   RARCH_ERR("Softfilter initialization failed.\n");
   deinit_video_filter();
}

static void init_video_input(const input_driver_t *tmp)
{
   const input_driver_t **input = input_get_double_ptr();
   if (*input)
      return;

   /* Reset video frame count */
   video_driver_frame_count = 0;

   /* Video driver didn't provide an input driver,
    * so we use configured one. */
   RARCH_LOG("Graphics driver did not initialize an input driver. Attempting to pick a suitable driver.\n");

   if (tmp)
      *input = tmp;
   else
      input_driver_ctl(RARCH_INPUT_CTL_FIND_DRIVER, NULL);

   /* This should never really happen as tmp (driver.input) is always
    * found before this in find_driver_input(), or we have aborted
    * in a similar fashion anyways. */
   if (!input_get_ptr())
      goto error;

   if (input_driver_ctl(RARCH_INPUT_CTL_INIT, NULL))
      return;

error:
   RARCH_ERR("Cannot initialize input driver. Exiting ...\n");
   retro_fail(1, "init_video_input()");
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

   if (video_driver_state.frame_time_samples_count < 2 * MEASURE_FRAME_TIME_SAMPLES_COUNT)
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

static void deinit_pixel_converter(void)
{
   if (!video_driver_scaler_ptr)
      return;

   scaler_ctx_gen_reset(video_driver_scaler_ptr->scaler);

   if (video_driver_scaler_ptr->scaler)
      free(video_driver_scaler_ptr->scaler);
   video_driver_scaler_ptr->scaler     = NULL;

   if (video_driver_scaler_ptr->scaler_out)
      free(video_driver_scaler_ptr->scaler_out);
   video_driver_scaler_ptr->scaler_out = NULL;

   if (video_driver_scaler_ptr)
      free(video_driver_scaler_ptr);
   video_driver_scaler_ptr             = NULL;
}

void video_driver_callback_destroy_context(void)
{
   const struct retro_hw_render_callback *hw_render =
      (const struct retro_hw_render_callback*)video_driver_callback();
   if (hw_render->context_destroy)
      hw_render->context_destroy();
}

static bool uninit_video_input(void)
{
   event_cmd_ctl(EVENT_CMD_OVERLAY_DEINIT, NULL);

   if (!video_driver_ctl(RARCH_DISPLAY_CTL_IS_VIDEO_CACHE_CONTEXT, NULL))
   {
      video_driver_callback_destroy_context();
      video_driver_unset_callback();
   }

   if (
         !input_driver_ctl(RARCH_INPUT_CTL_OWNS_DRIVER, NULL) &&
         !input_driver_data_ptr_is_same(video_driver_data)
      )
      input_driver_ctl(RARCH_INPUT_CTL_DEINIT, NULL);

   if (
         !video_driver_ctl(RARCH_DISPLAY_CTL_OWNS_DRIVER, NULL)
         && video_driver_data 
         && current_video->free
      )
      current_video->free(video_driver_data);

   deinit_pixel_converter();

   deinit_video_filter();

   event_cmd_ctl(EVENT_CMD_SHADER_DIR_DEINIT, NULL);
   video_monitor_compute_fps_statistics();

   return true;
}

static bool init_video_pixel_converter(unsigned size)
{
   /* This function can be called multiple times
    * without deiniting first on consoles. */
   deinit_pixel_converter();

   /* If pixel format is not 0RGB1555, we don't need to do
    * any internal pixel conversion. */
   if (video_driver_get_pixel_format() != RETRO_PIXEL_FORMAT_0RGB1555)
      return true;

   RARCH_WARN("0RGB1555 pixel format is deprecated, and will be slower. For 15/16-bit, RGB565 format is preferred.\n");

   video_driver_scaler_ptr = (video_pixel_scaler_t*)calloc(1, sizeof(*video_driver_scaler_ptr));

   if (!video_driver_scaler_ptr)
      goto error;

   video_driver_scaler_ptr->scaler = (struct scaler_ctx*)calloc(1, sizeof(*video_driver_scaler_ptr->scaler));

   if (!video_driver_scaler_ptr->scaler)
      goto error;

   video_driver_scaler_ptr->scaler->scaler_type = SCALER_TYPE_POINT;
   video_driver_scaler_ptr->scaler->in_fmt      = SCALER_FMT_0RGB1555;

   /* TODO: Pick either ARGB8888 or RGB565 depending on driver. */
   video_driver_scaler_ptr->scaler->out_fmt     = SCALER_FMT_RGB565;

   if (!scaler_ctx_gen_filter(video_driver_scaler_ptr->scaler))
      goto error;

   video_driver_scaler_ptr->scaler_out = calloc(sizeof(uint16_t), size * size);

   if (!video_driver_scaler_ptr->scaler_out)
      goto error;

   return true;

error:
   if (video_driver_scaler_ptr)
   {
      if (video_driver_scaler_ptr->scaler_out)
         free(video_driver_scaler_ptr->scaler_out);
      if (video_driver_scaler_ptr->scaler)
         free(video_driver_scaler_ptr->scaler);

      free(video_driver_scaler_ptr);
      video_driver_scaler_ptr = NULL;
   }

   return false;
}

static bool init_video(void)
{
   unsigned max_dim, scale, width, height;
   video_viewport_t *custom_vp            = NULL;
   const input_driver_t *tmp              = NULL;
   const struct retro_game_geometry *geom = NULL;
   rarch_system_info_t *system            = NULL;
   video_info_t video                     = {0};
   static uint16_t dummy_pixels[32]       = {0};
   settings_t *settings                   = config_get_ptr();
   struct retro_system_av_info *av_info   =
      video_viewport_get_system_av_info();

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

   init_video_filter(video_driver_state.pix_fmt);
   event_cmd_ctl(EVENT_CMD_SHADER_DIR_INIT, NULL);

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

   if (video_driver_state.filter.filter)
      scale = video_driver_state.filter.scale;

   /* Update core-dependent aspect ratio values. */
   video_driver_ctl(RARCH_DISPLAY_CTL_SET_VIEWPORT_SQUARE_PIXEL, NULL);
   video_driver_ctl(RARCH_DISPLAY_CTL_SET_VIEWPORT_CORE, NULL);
   video_driver_ctl(RARCH_DISPLAY_CTL_SET_VIEWPORT_CONFIG, NULL);

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

   video_driver_display_type_set(RARCH_DISPLAY_NONE);
   video_driver_display_set(0);
   video_driver_window_set(0);

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
   video.rgb32        = video_driver_state.filter.filter ?
      video_driver_state.filter.out_rgb32 :
      (video_driver_state.pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);

   tmp = input_get_ptr();
   /* Need to grab the "real" video driver interface on a reinit. */
   video_driver_ctl(RARCH_DISPLAY_CTL_FIND_DRIVER, NULL);

#ifdef HAVE_THREADS
   if (settings->video.threaded && !video_driver_state.hw_render_callback.context_type)
   {
      /* Can't do hardware rendering with threaded driver currently. */
      RARCH_LOG("Starting threaded video driver ...\n");

      if (!rarch_threaded_video_init((const video_driver_t**)&current_video, &video_driver_data,
               input_get_double_ptr(), input_driver_get_data_ptr(),
               current_video, &video))
      {
         RARCH_ERR("Cannot open threaded video driver ... Exiting ...\n");
         goto error;
      }
   }
   else
#endif
      video_driver_data = current_video->init(&video, input_get_double_ptr(),
            input_driver_get_data_ptr());

   if (!video_driver_data)
   {
      RARCH_ERR("Cannot open video driver ... Exiting ...\n");
      goto error;
   }

   video_driver_poke = NULL;
   if (current_video->poke_interface)
      current_video->poke_interface(video_driver_data, &video_driver_poke);

   if (current_video->viewport_info && (!custom_vp->width ||
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

   init_video_input(tmp);

   event_cmd_ctl(EVENT_CMD_OVERLAY_DEINIT, NULL);
   event_cmd_ctl(EVENT_CMD_OVERLAY_INIT, NULL);

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
   return current_video->suppress_screensaver(video_driver_data, enable);
}


bool video_driver_set_viewport(unsigned width, unsigned height,
      bool force_fullscreen, bool allow_rotate)
{
   if (current_video->set_viewport)
   {
      current_video->set_viewport(video_driver_data, width, height,
            force_fullscreen, allow_rotate);
      return true;
   }
   return false;
}

bool video_driver_set_rotation(unsigned rotation)
{
   if (current_video->set_rotation)
   {
      current_video->set_rotation(video_driver_data, rotation);
      return true;
   }
   return false;
}


bool video_driver_set_video_mode(unsigned width,
      unsigned height, bool fullscreen)
{
   if (video_driver_poke && video_driver_poke->set_video_mode)
   {
      video_driver_poke->set_video_mode(video_driver_data, width, height, fullscreen);
      return true;
   }

   return gfx_ctx_set_video_mode(width, height, fullscreen);
}

bool video_driver_get_video_output_size(unsigned *width, unsigned *height)
{
   if (video_driver_poke && video_driver_poke->get_video_output_size)
   {
      video_driver_poke->get_video_output_size(video_driver_data, width, height);
      return true;
   }
   return false;
}

void video_driver_set_osd_msg(const char *msg,
      const struct font_params *params, void *font)
{
   if (video_driver_poke && video_driver_poke->set_osd_msg)
      video_driver_poke->set_osd_msg(video_driver_data, msg, params, font);
}

void video_driver_set_texture_enable(bool enable, bool fullscreen)
{
#ifdef HAVE_MENU
   if (video_driver_poke && video_driver_poke->set_texture_enable)
      video_driver_poke->set_texture_enable(video_driver_data, enable, fullscreen);
#endif
}

void video_driver_set_texture_frame(const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
#ifdef HAVE_MENU
   if (video_driver_poke && video_driver_poke->set_texture_frame)
      video_driver_poke->set_texture_frame(video_driver_data, frame, rgb32, width, height, alpha);
#endif
}

bool video_driver_viewport_info(struct video_viewport *vp)
{
   if (current_video->viewport_info)
   {
      current_video->viewport_info(video_driver_data, vp);
      return true;
   }
   return false;
}


#ifdef HAVE_OVERLAY
bool video_driver_overlay_interface(const video_overlay_interface_t **iface)
{
   if (current_video->overlay_interface)
   {
      current_video->overlay_interface(video_driver_data, iface);
      return true;
   }
   return false;
}
#endif

void *video_driver_read_frame_raw(unsigned *width,
   unsigned *height, size_t *pitch)
{
   if (current_video->read_frame_raw)
      return current_video->read_frame_raw(video_driver_data, width,
            height, pitch);
   return NULL;
}

void video_driver_set_filtering(unsigned index, bool smooth)
{
   if (video_driver_poke && video_driver_poke->set_filtering)
      video_driver_poke->set_filtering(video_driver_data, index, smooth);
}

void video_driver_cached_frame_set(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   video_driver_ctl(RARCH_DISPLAY_CTL_CACHED_FRAME_SET_PTR, (void*)data);
   video_driver_state.frame_cache.width  = width;
   video_driver_state.frame_cache.height = height;
   video_driver_state.frame_cache.pitch  = pitch;
}

void video_driver_cached_frame_get(const void **data, unsigned *width,
      unsigned *height, size_t *pitch)
{
   if (data)
      *data    = video_driver_state.frame_cache.data;
   if (width)
      *width  = video_driver_state.frame_cache.width;
   if (height)
      *height = video_driver_state.frame_cache.height;
   if (pitch)
      *pitch  = video_driver_state.frame_cache.pitch;
}

void video_driver_get_size(unsigned *width, unsigned *height)
{
   if (width)
      *width  = video_driver_state.video_width;
   if (height)
      *height = video_driver_state.video_height;
}

void video_driver_set_size(unsigned *width, unsigned *height)
{
   if (width)
      video_driver_state.video_width  = *width;
   if (height)
      video_driver_state.video_height = *height;
}

/**
 * video_monitor_set_refresh_rate:
 * @hz                 : New refresh rate for monitor.
 *
 * Sets monitor refresh rate to new value.
 **/
void video_monitor_set_refresh_rate(float hz)
{
   char msg[128];
   settings_t *settings = config_get_ptr();

   snprintf(msg, sizeof(msg), "Setting refresh rate to: %.3f Hz.", hz);
   runloop_msg_queue_push(msg, 1, 180, false);
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
         video_driver_state.frame_time_samples_count);

   if (settings->video.threaded || (samples < 2))
      return false;

   /* Measure statistics on frame time (microsecs), *not* FPS. */
   for (i = 0; i < samples; i++)
      accum += video_driver_state.frame_time_samples[i];

#if 0
   for (i = 0; i < samples; i++)
      RARCH_LOG("Interval #%u: %d usec / frame.\n",
            i, (int)video_driver_state.frame_time_samples[i]);
#endif

   avg = accum / samples;

   /* Drop first measurement. It is likely to be bad. */
   for (i = 0; i < samples; i++)
   {
      retro_time_t diff = video_driver_state.frame_time_samples[i] - avg;
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
   static retro_time_t curr_time;
   static retro_time_t fps_time;
   retro_time_t        new_time  = retro_get_time_usec();
   rarch_system_info_t *system   = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

   *buf = '\0';

   if (video_driver_frame_count)
   {
      static float last_fps;
      bool ret = false;
      unsigned write_index = video_driver_state.frame_time_samples_count++ &
         (MEASURE_FRAME_TIME_SAMPLES_COUNT - 1);

      video_driver_state.frame_time_samples[write_index] = new_time - fps_time;
      fps_time = new_time;

      if ((video_driver_frame_count % FPS_UPDATE_INTERVAL) == 0)
      {
         last_fps = TIME_TO_FPS(curr_time, new_time, FPS_UPDATE_INTERVAL);
         curr_time = new_time;

         snprintf(buf, size, "%s || FPS: %6.1f || Frames: " U64_SIGN,
               system->title_buf, last_fps, (unsigned long long)video_driver_frame_count);
         ret = true;
      }

      if (buf_fps)
         snprintf(buf_fps, size_fps, "FPS: %6.1f || Frames: " U64_SIGN,
               last_fps, (unsigned long long)video_driver_frame_count);

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
   return video_driver_state.aspect_ratio;
}

void video_driver_set_aspect_ratio_value(float value)
{
   video_driver_state.aspect_ratio = value;
}

struct retro_hw_render_callback *video_driver_callback(void)
{
   return &video_driver_state.hw_render_callback;
}

void video_driver_unset_callback(void)
{
   memset(&video_driver_state.hw_render_callback, 0,
         sizeof(video_driver_state.hw_render_callback));
}

static bool video_driver_frame_filter(const void *data,
      unsigned width, unsigned height,
      size_t pitch,
      unsigned *output_width, unsigned *output_height,
      unsigned *output_pitch)
{
   static struct retro_perf_counter softfilter_process = {0};
   settings_t *settings = config_get_ptr();

   rarch_perf_init(&softfilter_process, "softfilter_process");

   if (!video_driver_state.filter.filter || !data)
      return false;

   rarch_softfilter_get_output_size(video_driver_state.filter.filter,
         output_width, output_height, width, height);

   *output_pitch = (*output_width) * video_driver_state.filter.out_bpp;

   retro_perf_start(&softfilter_process);
   rarch_softfilter_process(video_driver_state.filter.filter,
         video_driver_state.filter.buffer, *output_pitch,
         data, width, height, pitch);
   retro_perf_stop(&softfilter_process);

   if (settings->video.post_filter_record)
      recording_dump_frame(video_driver_state.filter.buffer,
            *output_width, *output_height, *output_pitch);

   return true;
}

rarch_softfilter_t *video_driver_frame_filter_get_ptr(void)
{
   return video_driver_state.filter.filter;
}

enum retro_pixel_format video_driver_get_pixel_format(void)
{
   return video_driver_state.pix_fmt;
}

void video_driver_set_pixel_format(enum retro_pixel_format fmt)
{
   video_driver_state.pix_fmt = fmt;
}

/**
 * video_driver_cached_frame:
 *
 * Renders the current video frame.
 **/
static bool video_driver_cached_frame(void)
{
   retro_ctx_frame_info_t info;
   void *recording  = recording_driver_get_data_ptr();

   if (runloop_ctl(RUNLOOP_CTL_IS_IDLE, NULL))
      return true; /* Maybe return false here for indication of idleness? */

   /* Cannot allow recording when pushing duped frames. */
   recording_driver_clear_data_ptr();

   /* Not 100% safe, since the library might have
    * freed the memory, but no known implementations do this.
    * It would be really stupid at any rate ...
    */
   info.data        = NULL;
   info.width       = video_driver_state.frame_cache.width;
   info.height      = video_driver_state.frame_cache.height;
   info.pitch       = video_driver_state.frame_cache.pitch;

   if (video_driver_state.frame_cache.data != RETRO_HW_FRAME_BUFFER_VALID)
      info.data = video_driver_state.frame_cache.data;

   core_ctl(CORE_CTL_RETRO_CTX_FRAME_CB, &info);

   recording_driver_set_data_ptr(recording);

   return true;
}

static void video_monitor_adjust_system_rates(void)
{
   float timing_skew;
   const struct retro_system_timing *info = NULL;
   struct retro_system_av_info *av_info   =
      video_viewport_get_system_av_info();
   settings_t *settings                   = config_get_ptr();
   rarch_system_info_t *system            = NULL;

   runloop_ctl(RUNLOOP_CTL_SYSTEM_INFO_GET, &system);

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

void video_driver_menu_settings(void **list_data, void *list_info_data,
      void *group_data, void *subgroup_data, const char *parent_group)
{
#ifdef HAVE_MENU
   rarch_setting_t **list                    = (rarch_setting_t**)list_data;
   rarch_setting_info_t *list_info           = (rarch_setting_info_t*)list_info_data;
   rarch_setting_group_info_t *group_info    = (rarch_setting_group_info_t*)group_data;
   rarch_setting_group_info_t *subgroup_info = (rarch_setting_group_info_t*)subgroup_data;
   global_t                        *global   = global_get_ptr();

   (void)list;
   (void)list_info;
   (void)group_info;
   (void)subgroup_info;
   (void)global;

#if defined(GEKKO) || defined(__CELLOS_LV2__)
   CONFIG_ACTION(
         list, list_info,
         menu_hash_to_str(MENU_LABEL_SCREEN_RESOLUTION),
         menu_hash_to_str(MENU_LABEL_VALUE_SCREEN_RESOLUTION),
         group_info,
         subgroup_info,
         parent_group);
#endif
#if defined(__CELLOS_LV2__)
   CONFIG_BOOL(
         list, list_info,
         &global->console.screen.pal60_enable,
         menu_hash_to_str(MENU_LABEL_PAL60_ENABLE),
         menu_hash_to_str(MENU_LABEL_VALUE_PAL60_ENABLE),
         false,
         menu_hash_to_str(MENU_VALUE_OFF),
         menu_hash_to_str(MENU_VALUE_ON),
         group_info,
         subgroup_info,
         parent_group,
         general_write_handler,
         general_read_handler);
#endif
#if defined(HW_RVL) || defined(_XBOX360)
   CONFIG_UINT(
         list, list_info,
         &global->console.screen.gamma_correction,
         menu_hash_to_str(MENU_LABEL_VIDEO_GAMMA),
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_GAMMA),
         0,
         group_info,
         subgroup_info,
         parent_group,
         general_write_handler,
         general_read_handler);
   menu_settings_list_current_add_cmd(
         list,
         list_info,
         EVENT_CMD_VIDEO_APPLY_STATE_CHANGES);
   menu_settings_list_current_add_range(
         list,
         list_info,
         0,
         MAX_GAMMA_SETTING,
         1,
         true,
         true);
   settings_data_list_current_add_flags(list, list_info, SD_FLAG_CMD_APPLY_AUTO|SD_FLAG_ADVANCED);
#endif
#if defined(_XBOX1) || defined(HW_RVL)
   CONFIG_BOOL(
         list, list_info,
         &global->console.softfilter_enable,
         menu_hash_to_str(MENU_LABEL_VIDEO_SOFT_FILTER),
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_SOFT_FILTER),
         false,
         menu_hash_to_str(MENU_VALUE_OFF),
         menu_hash_to_str(MENU_VALUE_ON),
         group_info,
         subgroup_info,
         parent_group,
         general_write_handler,
         general_read_handler);
   menu_settings_list_current_add_cmd(
         list,
         list_info,
         EVENT_CMD_VIDEO_APPLY_STATE_CHANGES);
#endif
#ifdef _XBOX1
   CONFIG_UINT(
         list, list_info,
         &settings->video.swap_interval,
         menu_hash_to_str(MENU_LABEL_VIDEO_FILTER_FLICKER),
         menu_hash_to_str(MENU_LABEL_VALUE_VIDEO_FILTER_FLICKER),
         0,
         group_info,
         subgroup_info,
         parent_group,
         general_write_handler,
         general_read_handler);
   menu_settings_list_current_add_range(list, list_info, 0, 5, 1, true, true);
#endif
#endif
}

/**
 * video_viewport_set_square_pixel:
 * @width         : Width.
 * @height        : Height.
 *
 * Sets viewport to square pixel aspect ratio based on @width and @height. 
 **/
static void video_viewport_set_square_pixel(unsigned width, unsigned height)
{
   unsigned len, highest, i, aspect_x, aspect_y;
   if (width == 0 || height == 0)
      return;

   len      = min(width, height);
   highest  = 1;

   for (i = 1; i < len; i++)
   {
      if ((width % i) == 0 && (height % i) == 0)
         highest = i;
   }

   aspect_x = width / highest;
   aspect_y = height / highest;

   snprintf(aspectratio_lut[ASPECT_RATIO_SQUARE].name,
         sizeof(aspectratio_lut[ASPECT_RATIO_SQUARE].name),
         "%u:%u (1:1 PAR)", aspect_x, aspect_y);

   aspectratio_lut[ASPECT_RATIO_SQUARE].value = (float)aspect_x / aspect_y;
}

/**
 * video_viewport_set_config:
 *
 * Sets viewport to config aspect ratio.
 **/
static bool video_viewport_set_config(void)
{
   settings_t *settings = config_get_ptr();
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();

   if (settings->video.aspect_ratio < 0.0f)
   {
      struct retro_game_geometry *geom = &av_info->geometry;

      if (!geom)
         return false;

      if (geom->aspect_ratio > 0.0f && settings->video.aspect_ratio_auto)
         aspectratio_lut[ASPECT_RATIO_CONFIG].value = geom->aspect_ratio;
      else
      {
         unsigned base_width  = geom->base_width;
         unsigned base_height = geom->base_height;

         /* Get around division by zero errors */
         if (base_width == 0)
            base_width = 1;
         if (base_height == 0)
            base_height = 1;
         aspectratio_lut[ASPECT_RATIO_CONFIG].value = 
            (float)base_width / base_height; /* 1:1 PAR. */
      }
   }
   else
      aspectratio_lut[ASPECT_RATIO_CONFIG].value = 
         settings->video.aspect_ratio;

   return true;
}

bool video_driver_ctl(enum rarch_display_ctl_state state, void *data)
{
   /* Graphics driver requires RGBA byte order data (ABGR on little-endian)
    * for 32-bit.
    * This takes effect for overlay and shader cores that wants to load
    * data into graphics driver. Kinda hackish to place it here, it is only
    * used for GLES.
    * TODO: Refactor this better. */
   static bool video_driver_use_rgba                = false;
   static bool video_driver_data_own                = false;
   static bool video_driver_active                  = false;
   static video_driver_frame_t frame_bak            = NULL;
   /* If set during context deinit, the driver should keep
    * graphics context alive to avoid having to reset all 
    * context state. */
   static bool video_driver_cache_context           = false;
   /* Set to true by driver if context caching succeeded. */
   static bool video_driver_cache_context_ack       = false;
   settings_t *settings                             = config_get_ptr();
   const struct retro_hw_render_callback *hw_render = 
      (const struct retro_hw_render_callback*)video_driver_callback();
   static uint8_t *video_driver_record_gpu_buffer   = NULL;

   switch (state)
   {
      case RARCH_DISPLAY_CTL_DESTROY:
         video_driver_use_rgba          = false;
         video_driver_data_own          = false;
         video_driver_active            = false;
         video_driver_cache_context     = false;
         video_driver_cache_context_ack = false;
         video_driver_record_gpu_buffer = NULL;
         current_video                  = NULL;
         break;
      case RARCH_DISPLAY_CTL_CACHED_FRAME_SET_PTR:
         if (!data)
            return false;
         video_driver_state.frame_cache.data   = (const void*)data;
         return true;
      case RARCH_DISPLAY_CTL_SET_STUB_FRAME:
         frame_bak                      = current_video->frame;
         current_video->frame           = video_null.frame;
         break;
      case RARCH_DISPLAY_CTL_UNSET_STUB_FRAME:
         if (frame_bak != NULL)
            current_video->frame        = frame_bak;
         frame_bak                      = NULL;
         break;
      case RARCH_DISPLAY_CTL_SUPPORTS_RECORDING:
         return settings->video.gpu_record && current_video->read_viewport;
      case RARCH_DISPLAY_CTL_SUPPORTS_VIEWPORT_READ:
         return (settings->video.gpu_screenshot ||
         ((hw_render->context_type
         != RETRO_HW_CONTEXT_NONE) && !current_video->read_frame_raw))
         && current_video->read_viewport && current_video->viewport_info;
      case RARCH_DISPLAY_CTL_SUPPORTS_READ_FRAME_RAW:
         return current_video->read_frame_raw;
      case RARCH_DISPLAY_CTL_SET_VIEWPORT_CONFIG:
         return video_viewport_set_config();
      case RARCH_DISPLAY_CTL_SET_VIEWPORT_SQUARE_PIXEL:
         {
            struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
            struct retro_game_geometry *geom = av_info ? &av_info->geometry : NULL;

            if (!geom)
               return false;

            video_viewport_set_square_pixel(geom->base_width, geom->base_height);
         }
         return true;
      case RARCH_DISPLAY_CTL_SET_VIEWPORT_CORE:
         {
            struct retro_system_av_info *av_info = 
               video_viewport_get_system_av_info();
            struct retro_game_geometry *geom = &av_info->geometry;

            if (!geom || geom->base_width <= 0.0f || geom->base_height <= 0.0f)
               return false;

            /* Fallback to 1:1 pixel ratio if none provided */
            if (geom->aspect_ratio > 0.0f)
               aspectratio_lut[ASPECT_RATIO_CORE].value = geom->aspect_ratio;
            else
               aspectratio_lut[ASPECT_RATIO_CORE].value = 
                  (float)geom->base_width / geom->base_height;
         }
         return true;
      case RARCH_DISPLAY_CTL_RESET_CUSTOM_VIEWPORT:
         {
            struct video_viewport *custom_vp = video_viewport_get_custom();
            if (!custom_vp)
               return false;

            custom_vp->width  = 0;
            custom_vp->height = 0;
            custom_vp->x      = 0;
            custom_vp->y      = 0;
         }
         return true;
      case RARCH_DISPLAY_CTL_SET_RGBA:
         video_driver_use_rgba = true;
         break;
      case RARCH_DISPLAY_CTL_UNSET_RGBA:
         video_driver_use_rgba = false;
         break;
      case RARCH_DISPLAY_CTL_SUPPORTS_RGBA:
         return video_driver_use_rgba;
      case RARCH_DISPLAY_CTL_GET_NEXT_VIDEO_OUT:
         if (!video_driver_poke)
            return false;

         if (video_driver_poke->get_video_output_next)
         {
            video_driver_poke->get_video_output_next(video_driver_data);
            return true;
         }
         return gfx_ctx_get_video_output_next();
      case RARCH_DISPLAY_CTL_GET_PREV_VIDEO_OUT:
         if (!video_driver_poke)
            return false;

         if (video_driver_poke->get_video_output_prev)
         {
            video_driver_poke->get_video_output_prev(video_driver_data);
            return true;
         }
         return gfx_ctx_get_video_output_next();
      case RARCH_DISPLAY_CTL_INIT:
         return init_video();
      case RARCH_DISPLAY_CTL_DESTROY_DATA:
         video_driver_data = NULL;
         break;
      case RARCH_DISPLAY_CTL_DEINIT:
         uninit_video_input();
         video_driver_data = NULL;
         return true;
      case RARCH_DISPLAY_CTL_MONITOR_RESET:
         video_driver_state.frame_time_samples_count = 0;
         return true;
      case RARCH_DISPLAY_CTL_MONITOR_ADJUST_SYSTEM_RATES:
         video_monitor_adjust_system_rates();
         return true;
      case RARCH_DISPLAY_CTL_SET_ASPECT_RATIO:
         if (!video_driver_poke || !video_driver_poke->set_aspect_ratio)
            return false;
         video_driver_poke->set_aspect_ratio(video_driver_data, settings->video.aspect_ratio_idx);
         return true;
      case RARCH_DISPLAY_CTL_SHOW_MOUSE:
         if (!video_driver_poke)
            return false;

         {
            bool *toggle                  = (bool*)data;

            if (video_driver_poke->show_mouse)
               video_driver_poke->show_mouse(video_driver_data, *toggle);
         }
         return true;
      case RARCH_DISPLAY_CTL_SET_NONBLOCK_STATE:
         {
            bool *toggle                  = (bool*)data;

            if (!toggle)
               return false;

            if (current_video->set_nonblock_state)
               current_video->set_nonblock_state(video_driver_data, *toggle);
         }
         return true;
      case RARCH_DISPLAY_CTL_FIND_DRIVER:
         return find_video_driver();
      case RARCH_DISPLAY_CTL_APPLY_STATE_CHANGES:
         if (!video_driver_poke)
            return false;
         if (video_driver_poke->apply_state_changes)
            video_driver_poke->apply_state_changes(video_driver_data);
         return true;
      case RARCH_DISPLAY_CTL_READ_VIEWPORT:
         if (current_video->read_viewport)
            return current_video->read_viewport(video_driver_data,
                  (uint8_t*)data);
         return false;
      case RARCH_DISPLAY_CTL_CACHED_FRAME_HAS_VALID_FB:
         if (!video_driver_state.frame_cache.data)
            return false;
         return (video_driver_state.frame_cache.data == RETRO_HW_FRAME_BUFFER_VALID);
      case RARCH_DISPLAY_CTL_CACHED_FRAME_RENDER:
         if (!current_video)
            return false;
         return video_driver_cached_frame();
      case RARCH_DISPLAY_CTL_IS_ALIVE:
         if (current_video)
            return current_video->alive(video_driver_data);
         break;
      case RARCH_DISPLAY_CTL_IS_FOCUSED:
         return current_video->focus(video_driver_data);
      case RARCH_DISPLAY_CTL_HAS_WINDOWED:
         return current_video->has_windowed(video_driver_data);
      case RARCH_DISPLAY_CTL_GET_FRAME_COUNT:
         {
            uint64_t **ptr = (uint64_t**)data;
            if (!ptr)
               return false;
            *ptr = &video_driver_frame_count;
         }
         return true;
      case RARCH_DISPLAY_CTL_FRAME_FILTER_ALIVE:
         if (video_driver_state.filter.filter)
            return true;
         return false;
      case RARCH_DISPLAY_CTL_FRAME_FILTER_IS_32BIT:
         return video_driver_state.filter.out_rgb32;
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
      case RARCH_DISPLAY_CTL_SET_OWN_DRIVER:
         video_driver_data_own = true;
         break;
      case RARCH_DISPLAY_CTL_UNSET_OWN_DRIVER:
         video_driver_data_own = false;
         break;
      case RARCH_DISPLAY_CTL_OWNS_DRIVER:
         return video_driver_data_own;
      case RARCH_DISPLAY_CTL_SET_VIDEO_CACHE_CONTEXT:
         video_driver_cache_context = true;
         break;
      case RARCH_DISPLAY_CTL_UNSET_VIDEO_CACHE_CONTEXT:
         video_driver_cache_context = false;
         break;
      case RARCH_DISPLAY_CTL_IS_VIDEO_CACHE_CONTEXT:
         return video_driver_cache_context;
      case RARCH_DISPLAY_CTL_SET_VIDEO_CACHE_CONTEXT_ACK:
         video_driver_cache_context_ack = true;
         break;
      case RARCH_DISPLAY_CTL_UNSET_VIDEO_CACHE_CONTEXT_ACK:
         video_driver_cache_context_ack = false;
         break;
      case RARCH_DISPLAY_CTL_IS_VIDEO_CACHE_CONTEXT_ACK:
         return video_driver_cache_context_ack;
      case RARCH_DISPLAY_CTL_SET_ACTIVE:
         video_driver_active = true;
         break;
      case RARCH_DISPLAY_CTL_UNSET_ACTIVE:
         video_driver_active = false;
         break;
      case RARCH_DISPLAY_CTL_IS_ACTIVE:
         return video_driver_active;
      case RARCH_DISPLAY_CTL_HAS_GPU_RECORD:
         return (video_driver_record_gpu_buffer != NULL);
      case RARCH_DISPLAY_CTL_GPU_RECORD_GET:
         {
            uint8_t **new_data  = (uint8_t**)data;

            if (!new_data)
               return false;
            *new_data = video_driver_record_gpu_buffer;
         }
         return true;
      case RARCH_DISPLAY_CTL_GPU_RECORD_INIT:
         {
            unsigned *new_size  = (unsigned*)data;
            if (!new_size)
               return false;
            video_driver_record_gpu_buffer = (uint8_t*)malloc(*new_size);
            if (!video_driver_record_gpu_buffer)
               return false;
            return true;
         }
      case RARCH_DISPLAY_CTL_GPU_RECORD_DEINIT:
         if (video_driver_record_gpu_buffer)
            free(video_driver_record_gpu_buffer);
         video_driver_record_gpu_buffer = NULL;
         break;
      case RARCH_DISPLAY_CTL_NONE:
      default:
         break;
   }

   return false;
}


/**
 * video_viewport_get_scaled_integer:
 * @vp            : Viewport handle
 * @width         : Width.
 * @height        : Height.
 * @aspect_ratio  : Aspect ratio (in float).
 * @keep_aspect   : Preserve aspect ratio?
 *
 * Gets viewport scaling dimensions based on 
 * scaled integer aspect ratio.
 **/
void video_viewport_get_scaled_integer(struct video_viewport *vp,
      unsigned width, unsigned height,
      float aspect_ratio, bool keep_aspect)
{
   int padding_x = 0, padding_y = 0;
   settings_t *settings = config_get_ptr();

   if (!vp)
      return;

   if (settings->video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      struct video_viewport *custom = video_viewport_get_custom();

      if (custom)
      {
         padding_x = width - custom->width;
         padding_y = height - custom->height;
         width = custom->width;
         height = custom->height;
      }
   }
   else
   {
      unsigned base_width;
      /* Use system reported sizes as these define the 
       * geometry for the "normal" case. */
      struct retro_system_av_info *av_info = video_viewport_get_system_av_info();
      unsigned base_height = av_info ? av_info->geometry.base_height : 0;

      if (base_height == 0)
         base_height = 1;

      /* Account for non-square pixels.
       * This is sort of contradictory with the goal of integer scale,
       * but it is desirable in some cases.
       *
       * If square pixels are used, base_height will be equal to 
       * system->av_info.base_height. */
      base_width = (unsigned)roundf(base_height * aspect_ratio);

      /* Make sure that we don't get 0x scale ... */
      if (width >= base_width && height >= base_height)
      {
         if (keep_aspect)
         {
            /* X/Y scale must be same. */
            unsigned max_scale = min(width / base_width, height / base_height);
            padding_x = width - base_width * max_scale;
            padding_y = height - base_height * max_scale;
         }
         else
         {
            /* X/Y can be independent, each scaled as much as possible. */
            padding_x = width % base_width;
            padding_y = height % base_height;
         }
      }

      width     -= padding_x;
      height    -= padding_y;
   }

   vp->width  = width;
   vp->height = height;
   vp->x      = padding_x / 2;
   vp->y      = padding_y / 2;
}

struct retro_system_av_info *video_viewport_get_system_av_info(void)
{
   static struct retro_system_av_info av_info;

   return &av_info;
}

struct video_viewport *video_viewport_get_custom(void)
{
   settings_t *settings = config_get_ptr();
   return &settings->video_viewport_custom;
}

unsigned video_pixel_get_alignment(unsigned pitch)
{
   if (pitch & 1)
      return 1;
   if (pitch & 2)
      return 2;
   if (pitch & 4)
      return 4;
   return 8;
}

static bool video_pixel_frame_scale(const void *data,
      unsigned width, unsigned height,
      size_t pitch)
{
   static struct retro_perf_counter video_frame_conv = {0};

   rarch_perf_init(&video_frame_conv, "video_frame_conv");

   if (!data || video_driver_get_pixel_format() != RETRO_PIXEL_FORMAT_0RGB1555)
      return false;
   if (data == RETRO_HW_FRAME_BUFFER_VALID)
      return false;

   retro_perf_start(&video_frame_conv);

   video_driver_scaler_ptr->scaler->in_width      = width;
   video_driver_scaler_ptr->scaler->in_height     = height;
   video_driver_scaler_ptr->scaler->out_width     = width;
   video_driver_scaler_ptr->scaler->out_height    = height;
   video_driver_scaler_ptr->scaler->in_stride     = pitch;
   video_driver_scaler_ptr->scaler->out_stride    = width * sizeof(uint16_t);

   scaler_ctx_scale(video_driver_scaler_ptr->scaler,
         video_driver_scaler_ptr->scaler_out, data);

   retro_perf_stop(&video_frame_conv);

   return true;
}

/**
 * video_driver_frame:
 * @data                 : pointer to data of the video frame.
 * @width                : width of the video frame.
 * @height               : height of the video frame.
 * @pitch                : pitch of the video frame.
 *
 * Video frame render callback function.
 **/
void video_driver_frame(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   static char video_driver_msg[256];
   unsigned output_width  = 0;
   unsigned output_height = 0;
   unsigned  output_pitch = 0;
   const char *msg        = runloop_msg_queue_pull();
   settings_t *settings   = config_get_ptr();

   if (!video_driver_ctl(RARCH_DISPLAY_CTL_IS_ACTIVE, NULL))
      return;

   if (video_driver_scaler_ptr &&
         video_pixel_frame_scale(data, width, height, pitch))
   {
      data                = video_driver_scaler_ptr->scaler_out;
      pitch               = video_driver_scaler_ptr->scaler->out_stride;
   }

   video_driver_cached_frame_set(data, width, height, pitch);

   /* Slightly messy code,
    * but we really need to do processing before blocking on VSync
    * for best possible scheduling.
    */
   if (
         (
             !video_driver_state.filter.filter
          || !settings->video.post_filter_record 
          || !data
          || video_driver_ctl(RARCH_DISPLAY_CTL_HAS_GPU_RECORD, NULL)
         )
      )
      recording_dump_frame(data, width, height, pitch);

   if (video_driver_frame_filter(data, width, height, pitch,
            &output_width, &output_height, &output_pitch))
   {
      data   = video_driver_state.filter.buffer;
      width  = output_width;
      height = output_height;
      pitch  = output_pitch;
   }

   video_driver_msg[0] = '\0';
   if (msg)
      strlcpy(video_driver_msg, msg, sizeof(video_driver_msg));

   if (!current_video || !current_video->frame(
            video_driver_data, data, width, height, video_driver_frame_count,
            pitch, video_driver_msg))
      video_driver_ctl(RARCH_DISPLAY_CTL_UNSET_ACTIVE, NULL);

   video_driver_frame_count++;
}

void video_driver_display_type_set(enum rarch_display_type type)
{
   video_driver_display_type = type;
}

uintptr_t video_driver_display_get(void)
{
   return video_driver_display;
}

void video_driver_display_set(uintptr_t idx)
{
   video_driver_display = idx;
}

enum rarch_display_type video_driver_display_type_get(void)
{
   return video_driver_display_type;
}

void video_driver_window_set(uintptr_t idx)
{
   video_driver_window = idx;
}

uintptr_t video_driver_window_get(void)
{
   return video_driver_window;
}

bool video_driver_texture_load(void *data,
      enum texture_filter_type  filter_type,
      unsigned *id)
{
#ifdef HAVE_THREADS
   settings_t *settings = config_get_ptr();
   const struct retro_hw_render_callback *hw_render =
      (const struct retro_hw_render_callback*)video_driver_callback();
#endif

   if (!id || !video_driver_poke || !video_driver_poke->load_texture)
      return false;

   *id = video_driver_poke->load_texture(video_driver_data, data,
#ifdef HAVE_THREADS
         settings->video.threaded && !hw_render->context_type,
#else
         false,
#endif
         filter_type);

   return true;
}

bool video_driver_texture_unload(uintptr_t *id)
{
   if (!video_driver_poke || !video_driver_poke->unload_texture)
      return false;

   video_driver_poke->unload_texture(video_driver_data, id);

   return true;
}

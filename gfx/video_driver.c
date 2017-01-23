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

#include <stddef.h>
#include <string.h>

#include <compat/strl.h>
#include <retro_common_api.h>
#include <file/config_file.h>
#include <features/features_cpu.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include <retro_assert.h>
#include <gfx/scaler/pixconv.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <formats/image.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/menu_setting.h"
#endif

#include "video_thread_wrapper.h"
#include "video_context_driver.h"

#include "../frontend/frontend_driver.h"
#include "../record/record_driver.h"
#include "../config.def.h"
#include "../configuration.h"
#include "../driver.h"
#include "../retroarch.h"
#include "../runloop.h"
#include "../performance_counters.h"
#include "../list_special.h"
#include "../core.h"
#include "../command.h"
#include "../msg_hash.h"
#include "../verbosity.h"

#define MEASURE_FRAME_TIME_SAMPLES_COUNT (2 * 1024)

#define TIME_TO_FPS(last_time, new_time, frames) ((1000000.0f * (frames)) / ((new_time) - (last_time)))

#define FPS_UPDATE_INTERVAL 256

#ifdef HAVE_THREADS
#define video_driver_lock() \
   if (display_lock) \
      slock_lock(display_lock)

#define video_driver_unlock() \
   if (display_lock) \
      slock_unlock(display_lock)

#define video_driver_context_lock() \
   if (context_lock) \
      slock_lock(context_lock)

#define video_driver_context_unlock() \
   if (context_lock) \
      slock_unlock(context_lock)

#define video_driver_lock_free() \
   slock_free(display_lock); \
   slock_free(context_lock); \
   display_lock = NULL; \
   context_lock = NULL

#define video_driver_threaded_lock() \
   if (video_driver_is_threaded()) \
      video_driver_lock()

#define video_driver_threaded_unlock() \
   if (video_driver_is_threaded()) \
      video_driver_unlock()
#else
#define video_driver_lock()            ((void)0)
#define video_driver_unlock()          ((void)0)
#define video_driver_lock_free()       ((void)0)
#define video_driver_threaded_lock()   ((void)0)
#define video_driver_threaded_unlock() ((void)0)
#define video_driver_context_lock()    ((void)0)
#define video_driver_context_unlock()  ((void)0)
#endif

typedef struct video_pixel_scaler
{
   struct scaler_ctx *scaler;
   void *scaler_out;
} video_pixel_scaler_t;

/* Opaque handles to currently running window.
 * Used by e.g. input drivers which bind to a window.
 * Drivers are responsible for setting these if an input driver
 * could potentially make use of this. */
static uintptr_t video_driver_display                    = 0;
static uintptr_t video_driver_window                     = 0;

static rarch_softfilter_t *video_driver_state_filter     = NULL;
static void               *video_driver_state_buffer     = NULL;
static unsigned            video_driver_state_scale      = 0;
static unsigned            video_driver_state_out_bpp    = 0;
static bool                video_driver_state_out_rgb32  = false;

static enum retro_pixel_format video_driver_pix_fmt      = RETRO_PIXEL_FORMAT_0RGB1555;

const void *frame_cache_data                             = NULL;
static unsigned frame_cache_width                        = 0;
static unsigned frame_cache_height                       = 0;
static size_t frame_cache_pitch                          = 0;

static float video_driver_aspect_ratio;
static unsigned video_driver_width                       = 0;
static unsigned video_driver_height                      = 0;

static enum rarch_display_type video_driver_display_type = RARCH_DISPLAY_NONE;
static char video_driver_title_buf[64]                   = {0};
static char video_driver_window_title[128]               = {0};
static bool video_driver_window_title_update             = true;

static retro_time_t video_driver_frame_time_samples[MEASURE_FRAME_TIME_SAMPLES_COUNT];
static uint64_t video_driver_frame_time_count            = 0;
static uint64_t video_driver_frame_count                 = 0;

void *video_driver_data                                  = NULL;
video_driver_t *current_video                            = NULL;

/* Interface for "poking". */
static const video_poke_interface_t *video_driver_poke   = NULL;

/* Used for 15-bit -> 16-bit conversions that take place before
 * being passed to video driver. */
static video_pixel_scaler_t *video_driver_scaler_ptr     = NULL;

static struct retro_hw_render_callback hw_render;

static const struct 
retro_hw_render_context_negotiation_interface *
hw_render_context_negotiation                            = NULL;

/* Graphics driver requires RGBA byte order data (ABGR on little-endian)
 * for 32-bit.
 * This takes effect for overlay and shader cores that wants to load
 * data into graphics driver. Kinda hackish to place it here, it is only
 * used for GLES.
 * TODO: Refactor this better. */
static bool video_driver_use_rgba                        = false;
static bool video_driver_data_own                        = false;
static bool video_driver_active                          = false;

static video_driver_frame_t frame_bak                    = NULL;

/* If set during context deinit, the driver should keep
 * graphics context alive to avoid having to reset all 
 * context state. */
static bool video_driver_cache_context                   = false;

/* Set to true by driver if context caching succeeded. */
static bool video_driver_cache_context_ack               = false;
static uint8_t *video_driver_record_gpu_buffer           = NULL;

#ifdef HAVE_THREADS
static slock_t *display_lock                             = NULL;
static slock_t *context_lock                             = NULL;
#endif

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
   { "9:16",          0.5625f },
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
#ifdef HAVE_VULKAN
   &video_vulkan,
#endif
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
#ifdef WIIU
   &video_wiiu,
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
#ifdef HAVE_PLAIN_DRM
   &video_drm,
#endif
#ifdef HAVE_XSHM
   &video_xshm,
#endif
#if defined(_WIN32) && !defined(_XBOX)
   &video_gdi,
#endif
#ifdef HAVE_CACA
   &video_caca,
#endif
#ifdef DJGPP
   &video_vga,
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

static bool hw_render_context_is_vulkan(enum retro_hw_context_type type)
{
   return type == RETRO_HW_CONTEXT_VULKAN;
}

static bool hw_render_context_is_gl(enum retro_hw_context_type type)
{
   switch (type)
   {
      case RETRO_HW_CONTEXT_OPENGL:
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGL_CORE:
      case RETRO_HW_CONTEXT_OPENGLES3:
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
         return true;
      default:
         break;
   }

   return false;
}

bool video_driver_is_threaded(void)
{
#ifdef HAVE_THREADS
   settings_t *settings = config_get_ptr();
   if (!video_driver_is_hw_context()
         && settings->video.threaded)
      return true;
#endif
   return false;
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
   if (video_driver_is_threaded() && !force_nonthreaded_data)
      return video_thread_get_ptr(NULL);
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
   rarch_softfilter_free(video_driver_state_filter);
#ifdef _3DS
   linearFree(video_driver_state_buffer);
#else
   free(video_driver_state_buffer);
#endif
   video_driver_state_filter    = NULL;
   video_driver_state_buffer    = NULL;
   video_driver_state_scale     = 0;
   video_driver_state_out_bpp   = 0;
   video_driver_state_out_rgb32 = false;
}

static void init_video_filter(enum retro_pixel_format colfmt)
{
   unsigned width, height, pow2_x, pow2_y, maxsize;
   void *buf                            = NULL;
   struct retro_game_geometry *geom     = NULL;
   settings_t *settings                 = config_get_ptr();
   struct retro_system_av_info *av_info =
      video_viewport_get_system_av_info();

   /* Deprecated format. Gets pre-converted. */
   if (colfmt == RETRO_PIXEL_FORMAT_0RGB1555)
      colfmt = RETRO_PIXEL_FORMAT_RGB565;

   if (video_driver_is_hw_context())
   {
      RARCH_WARN("Cannot use CPU filters when hardware rendering is used.\n");
      return;
   }

   if (av_info)
      geom = (struct retro_game_geometry*)&av_info->geometry;

   if (!geom)
      return;

   width                     = geom->max_width;
   height                    = geom->max_height;

   video_driver_state_filter = rarch_softfilter_new(
         settings->path.softfilter_plugin,
         RARCH_SOFTFILTER_THREADS_AUTO, colfmt, width, height);

   if (!video_driver_state_filter)
   {
      RARCH_ERR("Failed to load filter.\n");
      return;
   }

   rarch_softfilter_get_max_output_size(video_driver_state_filter,
         &width, &height);

   pow2_x                              = next_pow2(width);
   pow2_y                              = next_pow2(height);
   maxsize                             = MAX(pow2_x, pow2_y);
   video_driver_state_scale            = maxsize / RARCH_SCALE_BASE;
   video_driver_state_out_rgb32        = rarch_softfilter_get_output_format(
                                         video_driver_state_filter) == 
                                         RETRO_PIXEL_FORMAT_XRGB8888;

   video_driver_state_out_bpp          = video_driver_state_out_rgb32 ?
                                         sizeof(uint32_t)             : 
                                         sizeof(uint16_t);

   /* TODO: Aligned output. */
#ifdef _3DS
   buf = linearMemAlign(width * height * video_driver_state_out_bpp, 0x80);
#else
   buf = malloc(width * height * video_driver_state_out_bpp);
#endif
   if (!buf)
      goto error;

   video_driver_state_buffer    = buf;

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

   /* Video driver didn't provide an input driver,
    * so we use configured one. */
   RARCH_LOG("Graphics driver did not initialize an input driver. Attempting to pick a suitable driver.\n");

   if (tmp)
      *input = tmp;
   else
      input_driver_find_driver();

   /* This should never really happen as tmp (driver.input) is always
    * found before this in find_driver_input(), or we have aborted
    * in a similar fashion anyways. */
   if (!input_get_ptr())
      goto error;

   if (input_driver_init())
      return;

error:
   RARCH_ERR("Cannot initialize input driver. Exiting ...\n");
   retroarch_fail(1, "init_video_input()");
}

/**
 * video_monitor_compute_fps_statistics:
 *
 * Computes monitor FPS statistics.
 **/
static void video_monitor_compute_fps_statistics(void)
{
   double avg_fps       = 0.0;
   double stddev        = 0.0;
   unsigned samples     = 0;

   if (video_driver_is_threaded())
   {
      RARCH_LOG("Monitor FPS estimation is disabled for threaded video.\n");
      return;
   }

   if (video_driver_frame_time_count < 
         (2 * MEASURE_FRAME_TIME_SAMPLES_COUNT))
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

static bool uninit_video_input(void)
{
   command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);

   if (!video_driver_is_video_cache_context())
      video_driver_deinit_hw_context();

   if (
         !input_driver_owns_driver() &&
         !input_driver_is_data_ptr_same(video_driver_data)
      )
      input_driver_deinit();

   if (
         !video_driver_owns_driver()
         && video_driver_data 
         && current_video && current_video->free
      )
      current_video->free(video_driver_data);

   deinit_pixel_converter();
   deinit_video_filter();

   command_event(CMD_EVENT_SHADER_DIR_DEINIT, NULL);
   video_monitor_compute_fps_statistics();

   return true;
}

static bool init_video_pixel_converter(unsigned size)
{
   struct retro_hw_render_callback *hwr =
      video_driver_get_hw_context();
   void *scalr_out                      = NULL;
   video_pixel_scaler_t          *scalr = NULL;
   struct scaler_ctx        *scalr_ctx  = NULL;

   /* If pixel format is not 0RGB1555, we don't need to do
    * any internal pixel conversion. */
   if (video_driver_pix_fmt != RETRO_PIXEL_FORMAT_0RGB1555)
      return true;

   /* No need to perform pixel conversion for HW rendering contexts. */
   if (hwr && hwr->context_type != RETRO_HW_CONTEXT_NONE)
      return true;

   RARCH_WARN("0RGB1555 pixel format is deprecated, and will be slower. For 15/16-bit, RGB565 format is preferred.\n");

   scalr = (video_pixel_scaler_t*)calloc(1, sizeof(*scalr));

   if (!scalr)
      goto error;

   video_driver_scaler_ptr         = scalr;

   scalr_ctx = (struct scaler_ctx*)calloc(1, sizeof(*scalr_ctx));

   if (!scalr_ctx)
      goto error;

   video_driver_scaler_ptr->scaler              = scalr_ctx;
   video_driver_scaler_ptr->scaler->scaler_type = SCALER_TYPE_POINT;
   video_driver_scaler_ptr->scaler->in_fmt      = SCALER_FMT_0RGB1555;

   /* TODO: Pick either ARGB8888 or RGB565 depending on driver. */
   video_driver_scaler_ptr->scaler->out_fmt     = SCALER_FMT_RGB565;

   if (!scaler_ctx_gen_filter(scalr_ctx))
      goto error;

   scalr_out = calloc(sizeof(uint16_t), size * size);

   if (!scalr_out)
      goto error;

   video_driver_scaler_ptr->scaler_out          = scalr_out;

   return true;

error:
   deinit_pixel_converter();
   deinit_video_filter();

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

   deinit_video_filter();

   if (!string_is_empty(settings->path.softfilter_plugin))
      init_video_filter(video_driver_pix_fmt);

   command_event(CMD_EVENT_SHADER_DIR_INIT, NULL);

   if (av_info)
      geom      = (const struct retro_game_geometry*)&av_info->geometry;

   if (!geom)
   {
      RARCH_ERR("AV geometry not initialized, cannot initialize video driver.\n");
      goto error;
   }

   max_dim   = MAX(geom->max_width, geom->max_height);
   scale     = next_pow2(max_dim) / RARCH_SCALE_BASE;
   scale     = MAX(scale, 1);

   if (video_driver_state_filter)
      scale = video_driver_state_scale;

   /* Update core-dependent aspect ratio values. */
   video_driver_set_viewport_square_pixel();
   video_driver_set_viewport_core();
   video_driver_set_viewport_config();

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
      if(settings->video.window_x || settings->video.window_y)
      {
         width  = settings->video.window_x;
         height = settings->video.window_y;
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

   video.width         = width;
   video.height        = height;
   video.fullscreen    = settings->video.fullscreen;
   video.vsync         = settings->video.vsync && !runloop_ctl(RUNLOOP_CTL_IS_NONBLOCK_FORCED, NULL);
   video.force_aspect  = settings->video.force_aspect;
#ifdef GEKKO
   video.viwidth       = settings->video.viwidth;
   video.vfilter       = settings->video.vfilter;
#endif
   video.smooth        = settings->video.smooth;
   video.input_scale   = scale;
   video.rgb32         = video_driver_state_filter ?
      video_driver_state_out_rgb32 :
      (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);
   video.swap_interval = settings->video.swap_interval;
   video.font_enable   = settings->video.font_enable;

   /* Reset video frame count */
   video_driver_frame_count = 0;

   tmp = input_get_ptr();
   /* Need to grab the "real" video driver interface on a reinit. */
   video_driver_find_driver();

#ifdef HAVE_THREADS
   if (video_driver_is_threaded())
   {
      /* Can't do hardware rendering with threaded driver currently. */
      RARCH_LOG("Starting threaded video driver ...\n");

      if (!video_init_thread((const video_driver_t**)&current_video,
               &video_driver_data,
               input_get_double_ptr(), input_driver_get_data_ptr(),
               current_video, video))
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

      video_driver_get_viewport_info(custom_vp);
   }

   video_driver_set_rotation(
            (settings->video.rotation + system->rotation) % 4);

   current_video->suppress_screensaver(video_driver_data,
         settings->ui.suspend_screensaver_enable);

   init_video_input(tmp);

   command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
   command_event(CMD_EVENT_OVERLAY_INIT, NULL);

   video_driver_cached_frame_set(&dummy_pixels, 4, 4, 8);

#if defined(PSP)
   video_driver_set_texture_frame(&dummy_pixels, false, 1, 1, 1.0f);
#endif

   return true;

error:
   retroarch_fail(1, "init_video()");
   return false;
}

bool video_driver_set_viewport(unsigned width, unsigned height,
      bool force_fullscreen, bool allow_rotate)
{
   if (!current_video || !current_video->set_viewport)
      return false;
   current_video->set_viewport(video_driver_data, width, height,
         force_fullscreen, allow_rotate);
   return true;
}

bool video_driver_set_rotation(unsigned rotation)
{
   if (!current_video || !current_video->set_rotation)
      return false;
   current_video->set_rotation(video_driver_data, rotation);
   return true;
}

bool video_driver_set_video_mode(unsigned width,
      unsigned height, bool fullscreen)
{
   gfx_ctx_mode_t mode;

   if (video_driver_poke && video_driver_poke->set_video_mode)
   {
      video_driver_poke->set_video_mode(video_driver_data,
            width, height, fullscreen);
      return true;
   }

   mode.width      = width;
   mode.height     = height;
   mode.fullscreen = fullscreen;

   return video_context_driver_set_video_mode(&mode);
}

bool video_driver_get_video_output_size(unsigned *width, unsigned *height)
{
   if (!video_driver_poke || !video_driver_poke->get_video_output_size)
      return false;
   video_driver_poke->get_video_output_size(video_driver_data,
         width, height);
   return true;
}

void video_driver_set_osd_msg(const char *msg, const void *data, void *font)
{
   if (video_driver_poke && video_driver_poke->set_osd_msg)
      video_driver_poke->set_osd_msg(video_driver_data, msg, data, font);
}

void video_driver_set_texture_enable(bool enable, bool fullscreen)
{
   if (video_driver_poke && video_driver_poke->set_texture_enable)
      video_driver_poke->set_texture_enable(video_driver_data,
            enable, fullscreen);
}

void video_driver_set_texture_frame(const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
#ifdef HAVE_MENU
   if (video_driver_poke && video_driver_poke->set_texture_frame)
      video_driver_poke->set_texture_frame(video_driver_data,
            frame, rgb32, width, height, alpha);
#endif
}

#ifdef HAVE_OVERLAY
bool video_driver_overlay_interface(const video_overlay_interface_t **iface)
{
   if (!current_video || !current_video->overlay_interface)
      return false;
   current_video->overlay_interface(video_driver_data, iface);
   return true;
}
#endif

void *video_driver_read_frame_raw(unsigned *width,
   unsigned *height, size_t *pitch)
{
   if (!current_video || !current_video->read_frame_raw)
      return NULL;
   return current_video->read_frame_raw(video_driver_data, width,
         height, pitch);
}

void video_driver_set_filtering(unsigned index, bool smooth)
{
   if (video_driver_poke && video_driver_poke->set_filtering)
      video_driver_poke->set_filtering(video_driver_data, index, smooth);
}

void video_driver_cached_frame_set(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   video_driver_set_cached_frame_ptr(data);
   frame_cache_width  = width;
   frame_cache_height = height;
   frame_cache_pitch  = pitch;
}

void video_driver_cached_frame_get(const void **data, unsigned *width,
      unsigned *height, size_t *pitch)
{
   if (data)
      *data    = frame_cache_data;
   if (width)
      *width   = frame_cache_width;
   if (height)
      *height  = frame_cache_height;
   if (pitch)
      *pitch   = frame_cache_pitch;
}

void video_driver_get_size(unsigned *width, unsigned *height)
{
   video_driver_threaded_lock();
   if (width)
      *width  = video_driver_width;
   if (height)
      *height = video_driver_height;
   video_driver_threaded_unlock();
}

void video_driver_set_size(unsigned *width, unsigned *height)
{
   video_driver_threaded_lock();
   if (width)
      video_driver_width  = *width;
   if (height)
      video_driver_height = *height;
   video_driver_threaded_unlock();
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

   snprintf(msg, sizeof(msg),
         "Setting refresh rate to: %.3f Hz.", hz);
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
   unsigned samples      = MIN(MEASURE_FRAME_TIME_SAMPLES_COUNT,
         video_driver_frame_time_count);

   if (video_driver_is_threaded() || (samples < 2))
      return false;

   /* Measure statistics on frame time (microsecs), *not* FPS. */
   for (i = 0; i < samples; i++)
      accum += video_driver_frame_time_samples[i];

#if 0
   for (i = 0; i < samples; i++)
      RARCH_LOG("Interval #%u: %d usec / frame.\n",
            i, (int)frame_time_samples[i]);
#endif

   avg = accum / samples;

   /* Drop first measurement. It is likely to be bad. */
   for (i = 0; i < samples; i++)
   {
      retro_time_t diff = video_driver_frame_time_samples[i] - avg;
      accum_var += diff * diff;
   }

   *deviation     = sqrt((double)accum_var / (samples - 1)) / avg;
   *refresh_rate  = 1000000.0 / avg;
   *sample_points = samples;

   return true;
}



float video_driver_get_aspect_ratio(void)
{
   return video_driver_aspect_ratio;
}

void video_driver_set_aspect_ratio_value(float value)
{
   video_driver_aspect_ratio = value;
}

static bool video_driver_frame_filter(
      const void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      size_t pitch,
      unsigned *output_width, unsigned *output_height,
      unsigned *output_pitch)
{
   static struct retro_perf_counter softfilter_process = {0};
   
   performance_counter_init(&softfilter_process, "softfilter_process");

   rarch_softfilter_get_output_size(video_driver_state_filter,
         output_width, output_height, width, height);

   *output_pitch = (*output_width) * video_driver_state_out_bpp;

   performance_counter_start(&softfilter_process);
   rarch_softfilter_process(video_driver_state_filter,
         video_driver_state_buffer, *output_pitch,
         data, width, height, pitch);
   performance_counter_stop(&softfilter_process);

   if (video_info->post_filter_record && recording_data)
      recording_dump_frame(video_driver_state_buffer,
            *output_width, *output_height, *output_pitch,
            video_info->runloop_is_idle);

   return true;
}

rarch_softfilter_t *video_driver_frame_filter_get_ptr(void)
{
   return video_driver_state_filter;
}

enum retro_pixel_format video_driver_get_pixel_format(void)
{
   return video_driver_pix_fmt;
}

void video_driver_set_pixel_format(enum retro_pixel_format fmt)
{
   video_driver_pix_fmt = fmt;
}

/**
 * video_driver_cached_frame:
 *
 * Renders the current video frame.
 **/
bool video_driver_cached_frame(void)
{
   retro_ctx_frame_info_t info;
   void *recording  = recording_driver_get_data_ptr();

   /* Cannot allow recording when pushing duped frames. */
   recording_data   = NULL;

   /* Not 100% safe, since the library might have
    * freed the memory, but no known implementations do this.
    * It would be really stupid at any rate ...
    */
   info.data        = (frame_cache_data != RETRO_HW_FRAME_BUFFER_VALID) 
      ? frame_cache_data : NULL;
   info.width       = frame_cache_width;
   info.height      = frame_cache_height;
   info.pitch       = frame_cache_pitch;

   core_frame(&info);

   recording_data   = recording;

   return true;
}

void video_driver_monitor_adjust_system_rates(void)
{
   float timing_skew;
   const struct retro_system_timing *info = NULL;
   struct retro_system_av_info *av_info   =
      video_viewport_get_system_av_info();
   settings_t *settings                   = config_get_ptr();

   runloop_ctl(RUNLOOP_CTL_UNSET_NONBLOCK_FORCED, NULL);

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
   runloop_ctl(RUNLOOP_CTL_SET_NONBLOCK_FORCED, NULL);
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
         MENU_ENUM_LABEL_SCREEN_RESOLUTION,
         MENU_ENUM_LABEL_VALUE_SCREEN_RESOLUTION,
         group_info,
         subgroup_info,
         parent_group);
#endif
#if defined(__CELLOS_LV2__)
   CONFIG_BOOL(
         list, list_info,
         &global->console.screen.pal60_enable,
         MENU_ENUM_LABEL_PAL60_ENABLE,
         MENU_ENUM_LABEL_VALUE_PAL60_ENABLE,
         false,
         MENU_ENUM_LABEL_VALUE_OFF,
         MENU_ENUM_LABEL_VALUE_ON,
         group_info,
         subgroup_info,
         parent_group,
         general_write_handler,
         general_read_handler,
         SD_FLAG_NONE);
#endif
#if defined(GEKKO) || defined(_XBOX360)
   CONFIG_UINT(
         list, list_info,
         &global->console.screen.gamma_correction,
         MENU_ENUM_LABEL_VIDEO_GAMMA,
         MENU_ENUM_LABEL_VALUE_VIDEO_GAMMA,
         0,
         group_info,
         subgroup_info,
         parent_group,
         general_write_handler,
         general_read_handler);
   menu_settings_list_current_add_cmd(
         list,
         list_info,
         CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
   menu_settings_list_current_add_range(
         list,
         list_info,
         0,
         MAX_GAMMA_SETTING,
         1,
         true,
         true);
   settings_data_list_current_add_flags(list, list_info,
         SD_FLAG_CMD_APPLY_AUTO|SD_FLAG_ADVANCED);
#endif
#if defined(_XBOX1) || defined(HW_RVL)
   CONFIG_BOOL(
         list, list_info,
         &global->console.softfilter_enable,
         MENU_ENUM_LABEL_VIDEO_SOFT_FILTER,
         MENU_ENUM_LABEL_VALUE_VIDEO_SOFT_FILTER,
         false,
         MENU_ENUM_LABEL_VALUE_OFF,
         MENU_ENUM_LABEL_VALUE_ON,
         group_info,
         subgroup_info,
         parent_group,
         general_write_handler,
         general_read_handler,
         SD_FLAG_NONE);
   menu_settings_list_current_add_cmd(
         list,
         list_info,
         CMD_EVENT_VIDEO_APPLY_STATE_CHANGES);
#endif
#ifdef _XBOX1
   CONFIG_UINT(
         list, list_info,
         &global->console.screen.flicker_filter_index,
         MENU_ENUM_LABEL_VIDEO_FILTER_FLICKER,
         MENU_ENUM_LABEL_VALUE_VIDEO_FILTER_FLICKER,
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

static void video_driver_lock_new(void)
{
   video_driver_lock_free();
#ifdef HAVE_THREADS
   if (!display_lock)
      display_lock = slock_new();
   retro_assert(display_lock);

   if (!context_lock)
      context_lock = slock_new();
   retro_assert(context_lock);
#endif
}

void video_driver_destroy(void)
{
   video_driver_use_rgba          = false;
   video_driver_data_own          = false;
   video_driver_active            = false;
   video_driver_cache_context     = false;
   video_driver_cache_context_ack = false;
   video_driver_record_gpu_buffer = NULL;
   current_video                  = NULL;
}

void video_driver_set_cached_frame_ptr(const void *data)
{
   if (data)
      frame_cache_data = data;
}

void video_driver_set_stub_frame(void)
{
   frame_bak            = current_video->frame;
   current_video->frame = video_null.frame;
}

void video_driver_unset_stub_frame(void)
{
   if (frame_bak != NULL)
      current_video->frame = frame_bak;

   frame_bak = NULL;
}

bool video_driver_supports_recording(void)
{
   settings_t *settings = config_get_ptr();
   return settings->video.gpu_record && current_video->read_viewport;
}

bool video_driver_supports_viewport_read(void)
{
   settings_t *settings = config_get_ptr();
   return (settings->video.gpu_screenshot ||
         (video_driver_is_hw_context() && !current_video->read_frame_raw))
      && current_video->read_viewport && current_video->viewport_info;
}

bool video_driver_supports_read_frame_raw(void)
{
   return current_video->read_frame_raw;
}

void video_driver_set_viewport_config(void)
{
   settings_t *settings = config_get_ptr();
   struct retro_system_av_info *av_info = video_viewport_get_system_av_info();

   if (settings->video.aspect_ratio < 0.0f)
   {
      struct retro_game_geometry *geom = &av_info->geometry;

      if (!geom)
         return;

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
   {
      aspectratio_lut[ASPECT_RATIO_CONFIG].value = 
         settings->video.aspect_ratio;
   }
}

void video_driver_set_viewport_square_pixel(void)
{
   unsigned len, highest, i, aspect_x, aspect_y;
   unsigned width, height;
   struct retro_game_geometry *geom     = NULL;
   struct retro_system_av_info *av_info = 
      video_viewport_get_system_av_info();

   if (av_info)
      geom = &av_info->geometry;

   if (!geom)
      return;

   width  = geom->base_width;
   height = geom->base_height;

   if (width == 0 || height == 0)
      return;

   len      = MIN(width, height);
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
         "1:1 PAR (%u:%u DAR)", aspect_x, aspect_y);

   aspectratio_lut[ASPECT_RATIO_SQUARE].value = (float)aspect_x / aspect_y;
}

void video_driver_set_viewport_core(void)
{
   struct retro_system_av_info *av_info = 
      video_viewport_get_system_av_info();
   struct retro_game_geometry *geom = &av_info->geometry;

   if (!geom || geom->base_width <= 0.0f || geom->base_height <= 0.0f)
      return;

   /* Fallback to 1:1 pixel ratio if none provided */
   if (geom->aspect_ratio > 0.0f)
   {
      aspectratio_lut[ASPECT_RATIO_CORE].value = geom->aspect_ratio;
   }
   else
   {
      aspectratio_lut[ASPECT_RATIO_CORE].value = 
         (float)geom->base_width / geom->base_height;
   }
}

void video_driver_reset_custom_viewport(void)
{
   struct video_viewport *custom_vp = video_viewport_get_custom();
   if (!custom_vp)
      return;

   custom_vp->width  = 0;
   custom_vp->height = 0;
   custom_vp->x      = 0;
   custom_vp->y      = 0;
}

void video_driver_set_rgba(void)
{
   video_driver_lock();
   video_driver_use_rgba = true;
   video_driver_unlock();
}

void video_driver_unset_rgba(void)
{
   video_driver_lock();
   video_driver_use_rgba = false;
   video_driver_unlock();
}

bool video_driver_supports_rgba(void)
{
   bool tmp;
   video_driver_lock();
   tmp = video_driver_use_rgba;
   video_driver_unlock();
   return tmp;
}

bool video_driver_get_next_video_out(void)
{
   if (!video_driver_poke)
      return false;

   if (!video_driver_poke->get_video_output_next)
      return video_context_driver_get_video_output_next();
   video_driver_poke->get_video_output_next(video_driver_data);
   return true;
}

bool video_driver_get_prev_video_out(void)
{
   if (!video_driver_poke)
      return false;

   if (!video_driver_poke->get_video_output_prev)
      return video_context_driver_get_video_output_prev();
   video_driver_poke->get_video_output_prev(video_driver_data);
   return true;
}

bool video_driver_init(void)
{
   video_driver_lock_new();
   return init_video();
}

void video_driver_destroy_data(void)
{
   video_driver_data = NULL;
}

void video_driver_deinit(void)
{
   uninit_video_input();
   video_driver_lock_free();
   video_driver_data = NULL;
}

void video_driver_monitor_reset(void)
{
   video_driver_frame_time_count = 0;
}

void video_driver_set_aspect_ratio(void)
{
   settings_t *settings = config_get_ptr();
   if (!video_driver_poke || !video_driver_poke->set_aspect_ratio)
      return;
   video_driver_poke->set_aspect_ratio(
         video_driver_data, settings->video.aspect_ratio_idx);
}

void video_driver_show_mouse(void)
{
   if (video_driver_poke && video_driver_poke->show_mouse)
      video_driver_poke->show_mouse(video_driver_data, true);
}

void video_driver_hide_mouse(void)
{
   if (video_driver_poke && video_driver_poke->show_mouse)
      video_driver_poke->show_mouse(video_driver_data, false);
}

void video_driver_set_nonblock_state(bool toggle)
{
   if (current_video->set_nonblock_state)
      current_video->set_nonblock_state(video_driver_data, toggle);
}

bool video_driver_find_driver(void)
{
   int i;
   driver_ctx_info_t drv;
   settings_t *settings = config_get_ptr();

   if (video_driver_is_hw_context())
   {
      struct retro_hw_render_callback *hwr = video_driver_get_hw_context();

      current_video                        = NULL;

      if (hwr && hw_render_context_is_vulkan(hwr->context_type))
      {
#if defined(HAVE_VULKAN)
         RARCH_LOG("Using HW render, Vulkan driver forced.\n");
         current_video = &video_vulkan;
#endif
      }

      if (hwr && hw_render_context_is_gl(hwr->context_type))
      {
#if defined(HAVE_OPENGL) && defined(HAVE_FBO)
         RARCH_LOG("Using HW render, OpenGL driver forced.\n");
         current_video = &video_gl;
#endif
      }

      if (current_video)
         return true;
   }

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
         retroarch_fail(1, "find_video_driver()");
   }
   return true;
}

void video_driver_apply_state_changes(void)
{
   if (!video_driver_poke)
      return;
   if (video_driver_poke->apply_state_changes)
      video_driver_poke->apply_state_changes(video_driver_data);
}

bool video_driver_read_viewport(uint8_t *buffer, bool is_idle)
{
   if (     current_video->read_viewport
         && current_video->read_viewport(video_driver_data, buffer, is_idle))
      return true;

   return false;
}

uint64_t video_driver_get_frame_count(void)
{
   uint64_t frame_count;
   video_driver_threaded_lock();
   frame_count = video_driver_frame_count;
   video_driver_threaded_unlock();
   return frame_count;
}

bool video_driver_frame_filter_alive(void)
{
   return !!video_driver_state_filter;
}

bool video_driver_frame_filter_is_32bit(void)
{
   return video_driver_state_out_rgb32;
}

void video_driver_default_settings(void)
{
   global_t *global    = global_get_ptr();

   if (!global)
      return;

   global->console.screen.gamma_correction       = DEFAULT_GAMMA;
   global->console.flickerfilter_enable          = false;
   global->console.softfilter_enable             = false;

   global->console.screen.resolutions.current.id = 0;
}

void video_driver_load_settings(config_file_t *conf)
{
   bool tmp_bool    = false;
   global_t *global = global_get_ptr();

   if (!conf)
      return;

   CONFIG_GET_BOOL_BASE(conf, global,
         console.screen.gamma_correction, "gamma_correction");

   if (config_get_bool(conf, "flicker_filter_enable",
         &tmp_bool))
      global->console.flickerfilter_enable = tmp_bool;

   if (config_get_bool(conf, "soft_filter_enable",
         &tmp_bool))
      global->console.softfilter_enable = tmp_bool;

   CONFIG_GET_INT_BASE(conf, global,
         console.screen.soft_filter_index,
         "soft_filter_index");
   CONFIG_GET_INT_BASE(conf, global, 
         console.screen.resolutions.current.id,
         "current_resolution_id");
   CONFIG_GET_INT_BASE(conf, global, 
         console.screen.flicker_filter_index,
         "flicker_filter_index");
}

void video_driver_save_settings(config_file_t *conf)
{
   global_t *global = global_get_ptr();
   if (!conf)
      return;

   config_set_bool(conf, "gamma_correction",
         global->console.screen.gamma_correction);
   config_set_bool(conf, "flicker_filter_enable",
         global->console.flickerfilter_enable);
   config_set_bool(conf, "soft_filter_enable",
         global->console.softfilter_enable);

   config_set_int(conf, "soft_filter_index",
         global->console.screen.soft_filter_index);
   config_set_int(conf, "current_resolution_id",
         global->console.screen.resolutions.current.id);
   config_set_int(conf, "flicker_filter_index",
         global->console.screen.flicker_filter_index);
}

void video_driver_reinit(void)
{
   struct retro_hw_render_callback *hwr =
      video_driver_get_hw_context();

   if (hwr->cache_context)
      video_driver_cache_context    = true;
   else
      video_driver_cache_context = false;

   video_driver_cache_context_ack = false;
   command_event(CMD_EVENT_RESET_CONTEXT, NULL);
   video_driver_cache_context = false;
}

void video_driver_set_own_driver(void)
{
   video_driver_data_own = true;
}

void video_driver_unset_own_driver(void)
{
   video_driver_data_own = false;
}

bool video_driver_owns_driver(void)
{
   return video_driver_data_own;
}

bool video_driver_is_hw_context(void)
{
   bool is_hw_context = false;

   video_driver_context_lock();
   is_hw_context = (hw_render.context_type != RETRO_HW_CONTEXT_NONE);
   video_driver_context_unlock();

   return is_hw_context;
}

void video_driver_deinit_hw_context(void)
{
   video_driver_context_lock();

   if (hw_render.context_destroy)
      hw_render.context_destroy();

   memset(&hw_render, 0, sizeof(hw_render));

   video_driver_context_unlock();

   hw_render_context_negotiation = NULL;
}

struct retro_hw_render_callback *video_driver_get_hw_context(void)
{
   return &hw_render;
}

const struct retro_hw_render_context_negotiation_interface *
   video_driver_get_context_negotiation_interface(void)
{
   return hw_render_context_negotiation;
}

void video_driver_set_context_negotiation_interface(
      const struct retro_hw_render_context_negotiation_interface *iface)
{
   hw_render_context_negotiation = iface;
}

bool video_driver_is_video_cache_context(void)
{
   return video_driver_cache_context;
}

void video_driver_set_video_cache_context_ack(void)
{
   video_driver_cache_context_ack = true;
}

void video_driver_unset_video_cache_context_ack(void)
{
   video_driver_cache_context_ack = false;
}

bool video_driver_is_video_cache_context_ack(void)
{
   return video_driver_cache_context_ack;
}

void video_driver_set_active(void)
{
   video_driver_active = true;
}

bool video_driver_is_active(void)
{
   return video_driver_active;
}

bool video_driver_has_gpu_record(void)
{
   return video_driver_record_gpu_buffer != NULL;
}

uint8_t *video_driver_get_gpu_record(void)
{
   return video_driver_record_gpu_buffer;
}

bool video_driver_gpu_record_init(unsigned size)
{
   video_driver_record_gpu_buffer = (uint8_t*)malloc(size);
   if (!video_driver_record_gpu_buffer)
      return false;
   return true;
}

void video_driver_gpu_record_deinit(void)
{
   free(video_driver_record_gpu_buffer);
   video_driver_record_gpu_buffer = NULL;
}

bool video_driver_get_current_software_framebuffer(struct retro_framebuffer *fb)
{
   if (
            video_driver_poke 
         && video_driver_poke->get_current_software_framebuffer
         && video_driver_poke->get_current_software_framebuffer(
            video_driver_data, fb))
      return true;

   return false;
}

bool video_driver_get_hw_render_interface(
      const struct retro_hw_render_interface **iface)
{
   if (
            video_driver_poke 
         && video_driver_poke->get_hw_render_interface
         && video_driver_poke->get_hw_render_interface(
            video_driver_data, iface))
      return true;

   return false;
}

bool video_driver_get_viewport_info(struct video_viewport *viewport)
{
   if (!current_video || !current_video->viewport_info)
      return false;
   current_video->viewport_info(video_driver_data, viewport);
   return true;
}

void video_driver_set_title_buf(void)
{
   struct retro_system_info info;
   core_get_system_info(&info);

   fill_pathname_noext(video_driver_title_buf, 
         msg_hash_to_str(MSG_PROGRAM),
         " ",
         sizeof(video_driver_title_buf));
   strlcat(video_driver_title_buf, 
         info.library_name,
         sizeof(video_driver_title_buf));
   strlcat(video_driver_title_buf,
         " ", sizeof(video_driver_title_buf));
   strlcat(video_driver_title_buf,
         info.library_version,
         sizeof(video_driver_title_buf));
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
   int padding_x        = 0;
   int padding_y        = 0;
   settings_t *settings = config_get_ptr();

   if (settings->video.aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      struct video_viewport *custom = video_viewport_get_custom();

      if (custom)
      {
         padding_x = width - custom->width;
         padding_y = height - custom->height;
         width     = custom->width;
         height    = custom->height;
      }
   }
   else
   {
      unsigned base_width;
      /* Use system reported sizes as these define the 
       * geometry for the "normal" case. */
      struct retro_system_av_info *av_info = 
         video_viewport_get_system_av_info();
      unsigned base_height = 0;
      
      if (av_info)
         base_height = av_info->geometry.base_height;

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
            unsigned max_scale = MIN(width / base_width,
                  height / base_height);
            padding_x          = width - base_width * max_scale;
            padding_y          = height - base_height * max_scale;
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

/**
 * video_monitor_get_fps:
 *
 * Get the amount of frames per seconds.
 **/
static void video_monitor_get_fps(video_frame_info_t *video_info)
{
   static retro_time_t curr_time;
   static retro_time_t fps_time;
   static float last_fps;
   unsigned write_index          = 0;
   retro_time_t        new_time  = cpu_features_get_time_usec();

   if (!video_info->frame_count)
   {
      curr_time = fps_time = new_time;
      strlcpy(video_driver_window_title,
            video_driver_title_buf,
            sizeof(video_driver_window_title));

      if (video_info->fps_show)
         strlcpy(video_info->fps_text,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               sizeof(video_info->fps_text));

      video_driver_window_title_update = true;
      return;
   }

   write_index                                  = 
      video_driver_frame_time_count++ & 
      (MEASURE_FRAME_TIME_SAMPLES_COUNT - 1);
   video_driver_frame_time_samples[write_index] = new_time - fps_time;
   fps_time                                     = new_time;

   if ((video_info->frame_count % FPS_UPDATE_INTERVAL) == 0)
   {
      char frames_text[64];

      fill_pathname_noext(video_driver_window_title,
            video_driver_title_buf,
            " || ",
            sizeof(video_driver_window_title));

      if (video_info->fps_show)
      {
         last_fps = TIME_TO_FPS(curr_time, new_time, FPS_UPDATE_INTERVAL);
         snprintf(video_info->fps_text,
               sizeof(video_info->fps_text),
               " FPS: %6.1f || ", last_fps);
         strlcat(video_driver_window_title,
               video_info->fps_text,
               sizeof(video_driver_window_title));
      }

      curr_time = new_time;

      strlcat(video_driver_window_title,
            "Frames: ",
            sizeof(video_driver_window_title));

      snprintf(frames_text,
            sizeof(frames_text),
            STRING_REP_UINT64,
            (unsigned long long)video_info->frame_count);

      strlcat(video_driver_window_title,
            frames_text,
            sizeof(video_driver_window_title));

      video_driver_window_title_update = true;
   }

   if (video_info->fps_show)
      snprintf(
            video_info->fps_text,
            sizeof(video_info->fps_text),
            "FPS: %6.1f || %s: " STRING_REP_UINT64,
            last_fps,
            msg_hash_to_str(MSG_FRAMES),
            (unsigned long long)video_info->frame_count);
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
   video_frame_info_t video_info;
   static struct retro_perf_counter video_frame_conv = {0};
   unsigned output_width                             = 0;
   unsigned output_height                            = 0;
   unsigned output_pitch                             = 0;
   const char *msg                                   = NULL;

   if (!video_driver_active)
      return;

   performance_counter_init(&video_frame_conv, "video_frame_conv");
   performance_counter_start(&video_frame_conv);

   if (video_driver_scaler_ptr && data &&
         (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_0RGB1555) &&
         (data != RETRO_HW_FRAME_BUFFER_VALID) &&
         video_pixel_frame_scale(
            video_driver_scaler_ptr->scaler,
            video_driver_scaler_ptr->scaler_out,
            data, width, height, pitch))
   {
      data                = video_driver_scaler_ptr->scaler_out;
      pitch               = video_driver_scaler_ptr->scaler->out_stride;
   }

   performance_counter_stop(&video_frame_conv);

   if (data)
      frame_cache_data = data;
   frame_cache_width   = width;
   frame_cache_height  = height;
   frame_cache_pitch   = pitch;

   video_driver_build_info(&video_info);

   video_driver_threaded_lock();
   video_info.frame_count = video_driver_frame_count;
   video_driver_frame_count++;
   video_driver_threaded_unlock();
   
   video_monitor_get_fps(&video_info); 

   /* Slightly messy code,
    * but we really need to do processing before blocking on VSync
    * for best possible scheduling.
    */
   if (
         (
             !video_driver_state_filter
          || !video_info.post_filter_record 
          || !data
          || video_driver_record_gpu_buffer
         ) && recording_data
      )
      recording_dump_frame(data, width, height, pitch, video_info.runloop_is_idle);

   if (data && video_driver_state_filter &&
         video_driver_frame_filter(data, &video_info, width, height, pitch,
            &output_width, &output_height, &output_pitch))
   {
      data   = video_driver_state_buffer;
      width  = output_width;
      height = output_height;
      pitch  = output_pitch;
   }

   video_driver_msg[0] = '\0';

   if (     video_info.font_enable
         && runloop_msg_queue_pull((const char**)&msg) 
         && msg)
      strlcpy(video_driver_msg, msg, sizeof(video_driver_msg));

   if (!current_video || !current_video->frame(
            video_driver_data, data, width, height,
            video_info.frame_count,
            pitch, video_driver_msg, &video_info))
      video_driver_active = false;

   if (video_info.fps_show)
      runloop_msg_queue_push(video_info.fps_text, 1, 1, false);
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
      uintptr_t *id)
{
   if (!id || !video_driver_poke || !video_driver_poke->load_texture)
      return false;

   *id = video_driver_poke->load_texture(video_driver_data, data,
         video_driver_is_threaded(),
         filter_type);

   return true;
}

bool video_driver_texture_unload(uintptr_t *id)
{
   if (!video_driver_poke || !video_driver_poke->unload_texture)
      return false;

   video_driver_poke->unload_texture(video_driver_data, *id);
   *id = 0;
   return true;
}

void video_driver_build_info(video_frame_info_t *video_info)
{
   bool is_paused                    = false;
   bool is_idle                      = false;
   bool is_slowmotion                = false;
   settings_t *settings              = NULL;
   video_driver_threaded_lock();
   settings                          = config_get_ptr();
   video_info->refresh_rate          = settings->video.refresh_rate;
   video_info->black_frame_insertion = 
      settings->video.black_frame_insertion;
   video_info->hard_sync             = settings->video.hard_sync;
   video_info->hard_sync_frames      = settings->video.hard_sync_frames;
   video_info->fps_show              = settings->fps_show;
   video_info->scale_integer         = settings->video.scale_integer;
   video_info->aspect_ratio_idx      = settings->video.aspect_ratio_idx;
   video_info->post_filter_record    = settings->video.post_filter_record;
   video_info->max_swapchain_images  = settings->video.max_swapchain_images;
   video_info->windowed_fullscreen   = settings->video.windowed_fullscreen;
   video_info->fullscreen            = settings->video.fullscreen;
   video_info->monitor_index         = settings->video.monitor_index;
   video_info->shared_context        = settings->video.shared_context;
   video_info->font_enable           = settings->video.font_enable;
   video_info->font_msg_pos_x        = settings->video.msg_pos_x;
   video_info->font_msg_pos_y        = settings->video.msg_pos_y;
   video_info->font_msg_color_r      = settings->video.msg_color_r;
   video_info->font_msg_color_g      = settings->video.msg_color_g;
   video_info->font_msg_color_b      = settings->video.msg_color_b;

   video_info->frame_count           = 0;
   video_info->fps_text[0]           = '\0';

   video_info->width                 = video_driver_width;
   video_info->height                = video_driver_height;

   video_info->use_rgba              = video_driver_use_rgba;

   video_info->libretro_running       = false;
#ifdef HAVE_MENU
   video_info->menu_is_alive          = menu_driver_is_alive();
   video_info->menu_footer_opacity    = settings->menu.footer.opacity;
   video_info->menu_header_opacity    = settings->menu.header.opacity;
   video_info->materialui_color_theme = settings->menu.materialui.menu_color_theme;
   video_info->menu_shader_pipeline   = settings->menu.xmb.shader_pipeline;
   video_info->xmb_theme              = settings->menu.xmb.theme;
   video_info->xmb_color_theme        = settings->menu.xmb.menu_color_theme;
   video_info->timedate_enable        = settings->menu.timedate_enable;
   video_info->battery_level_enable   = settings->menu.battery_level_enable;
   video_info->xmb_shadows_enable     = settings->menu.xmb.shadows_enable;
   video_info->xmb_alpha_factor       = settings->menu.xmb.alpha_factor;
   video_info->menu_wallpaper_opacity = settings->menu.wallpaper.opacity;

   if (!settings->menu.pause_libretro)
      video_info->libretro_running    = (rarch_ctl(RARCH_CTL_IS_INITED, NULL)
            && !rarch_ctl(RARCH_CTL_IS_DUMMY_CORE, NULL));
#else
   video_info->menu_is_alive          = false;
   video_info->menu_footer_opacity    = 0.0f;
   video_info->menu_header_opacity    = 0.0f;
   video_info->materialui_color_theme = 0;
   video_info->menu_shader_pipeline   = 0;
   video_info->xmb_color_theme        = 0;
   video_info->xmb_theme              = 0;
   video_info->timedate_enable        = false;
   video_info->battery_level_enable   = false;
   video_info->xmb_shadows_enable     = false;
   video_info->xmb_alpha_factor       = 0.0f;
   video_info->menu_wallpaper_opacity = 0.0f;
#endif

   runloop_get_status(&is_paused, &is_idle, &is_slowmotion);

   video_info->runloop_is_paused      = is_paused;
   video_info->runloop_is_idle        = is_idle;
   video_info->runloop_is_slowmotion  = is_slowmotion;
   video_driver_threaded_unlock();
}

/**
 * video_driver_translate_coord_viewport:
 * @mouse_x                        : Pointer X coordinate.
 * @mouse_y                        : Pointer Y coordinate.
 * @res_x                          : Scaled  X coordinate.
 * @res_y                          : Scaled  Y coordinate.
 * @res_screen_x                   : Scaled screen X coordinate.
 * @res_screen_y                   : Scaled screen Y coordinate.
 *
 * Translates pointer [X,Y] coordinates into scaled screen
 * coordinates based on viewport info.
 *
 * Returns: true (1) if successful, false if video driver doesn't support
 * viewport info.
 **/
bool video_driver_translate_coord_viewport(
      void *data,
      int mouse_x,           int mouse_y,
      int16_t *res_x,        int16_t *res_y,
      int16_t *res_screen_x, int16_t *res_screen_y)
{
   int scaled_screen_x, scaled_screen_y, scaled_x, scaled_y;
   struct video_viewport *vp = (struct video_viewport*)data;
   int norm_full_vp_width    = (int)vp->full_width;
   int norm_full_vp_height   = (int)vp->full_height;

   if (norm_full_vp_width <= 0 || norm_full_vp_height <= 0)
      return false;

   scaled_screen_x     = (2 * mouse_x * 0x7fff) / norm_full_vp_width  - 0x7fff;
   scaled_screen_y     = (2 * mouse_y * 0x7fff) / norm_full_vp_height - 0x7fff;
   if (scaled_screen_x < -0x7fff || scaled_screen_x > 0x7fff)
      scaled_screen_x  = -0x8000; /* OOB */
   if (scaled_screen_y < -0x7fff || scaled_screen_y > 0x7fff)
      scaled_screen_y  = -0x8000; /* OOB */

   mouse_x           -= vp->x;
   mouse_y           -= vp->y;

   scaled_x           = (2 * mouse_x * 0x7fff) / norm_full_vp_width  - 0x7fff;
   scaled_y           = (2 * mouse_y * 0x7fff) / norm_full_vp_height - 0x7fff;
   if (scaled_x < -0x7fff || scaled_x > 0x7fff)
      scaled_x        = -0x8000; /* OOB */
   if (scaled_y < -0x7fff || scaled_y > 0x7fff)
      scaled_y        = -0x8000; /* OOB */

   *res_x             = scaled_x;
   *res_y             = scaled_y;
   *res_screen_x      = scaled_screen_x;
   *res_screen_y      = scaled_screen_y;

   return true;
}

void video_driver_get_window_title(char *buf, unsigned len)
{
   if (buf && video_driver_window_title_update)
   {
      strlcpy(buf, video_driver_window_title, len);
      video_driver_window_title_update = false;
   }
}

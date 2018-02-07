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
#include <retro_math.h>

#include <retro_assert.h>
#include <gfx/scaler/pixconv.h>
#include <gfx/scaler/scaler.h>
#include <gfx/video_frame.h>
#include <formats/image.h>

#include "menu/menu_shader.h"

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include "../dynamic.h"

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#include "../menu/menu_setting.h"
#endif

#include "video_thread_wrapper.h"
#include "video_driver.h"
#include "video_display_server.h"

#include "../frontend/frontend_driver.h"
#include "../record/record_driver.h"
#include "../config.def.h"
#include "../configuration.h"
#include "../driver.h"
#include "../retroarch.h"
#include "../input/input_driver.h"
#include "../list_special.h"
#include "../core.h"
#include "../command.h"
#include "../msg_hash.h"
#include "../verbosity.h"

#define MEASURE_FRAME_TIME_SAMPLES_COUNT (2 * 1024)

#define TIME_TO_FPS(last_time, new_time, frames) ((1000000.0f * (frames)) / ((new_time) - (last_time)))

#define FPS_UPDATE_INTERVAL 256

#ifdef HAVE_THREADS
#define video_driver_is_threaded() ((!video_driver_is_hw_context() && video_driver_threaded) ? true : false)
#else
#define video_driver_is_threaded() (false)
#endif

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

#define video_driver_threaded_lock(is_threaded) \
   if (is_threaded) \
      video_driver_lock()

#define video_driver_threaded_unlock(is_threaded) \
   if (is_threaded) \
      video_driver_unlock()
#else
#define video_driver_lock()            ((void)0)
#define video_driver_unlock()          ((void)0)
#define video_driver_lock_free()       ((void)0)
#define video_driver_threaded_lock(is_threaded)   ((void)0)
#define video_driver_threaded_unlock(is_threaded) ((void)0)
#define video_driver_context_lock()    ((void)0)
#define video_driver_context_unlock()  ((void)0)
#endif

typedef struct video_pixel_scaler
{
   struct scaler_ctx *scaler;
   void *scaler_out;
} video_pixel_scaler_t;

static void (*video_driver_cb_shader_use)(void *data,
      void *shader_data, unsigned index, bool set_active);
static bool (*video_driver_cb_shader_set_mvp)(void *data,
      void *shader_data, const void *mat_data);
bool (*video_driver_cb_has_focus)(void);

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

static struct retro_system_av_info video_driver_av_info;

static enum retro_pixel_format video_driver_pix_fmt      = RETRO_PIXEL_FORMAT_0RGB1555;

static const void *frame_cache_data                      = NULL;
static unsigned frame_cache_width                        = 0;
static unsigned frame_cache_height                       = 0;
static size_t frame_cache_pitch                          = 0;
static bool   video_driver_threaded                      = false;

static float video_driver_aspect_ratio                   = 0.0f;
static unsigned video_driver_width                       = 0;
static unsigned video_driver_height                      = 0;

static enum rarch_display_type video_driver_display_type = RARCH_DISPLAY_NONE;
static char video_driver_title_buf[64]                   = {0};
static char video_driver_window_title[128]               = {0};
static bool video_driver_window_title_update             = true;

static retro_time_t video_driver_frame_time_samples[MEASURE_FRAME_TIME_SAMPLES_COUNT];
static uint64_t video_driver_frame_time_count            = 0;
static uint64_t video_driver_frame_count                 = 0;

static void *video_driver_data                           = NULL;
static video_driver_t *current_video                     = NULL;

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

static gfx_ctx_driver_t current_video_context;

static void *video_context_data                          = NULL;

/**
 * dynamic.c:dynamic_request_hw_context will try to set flag data when the context
 * is in the middle of being rebuilt; in these cases we will save flag
 * data and set this to true.
 * When the context is reinit, it checks this, reads from
 * deferred_flag_data and cleans it.
 *
 * TODO - Dirty hack, fix it better
 */
static bool deferred_video_context_driver_set_flags      = false;
static gfx_ctx_flags_t deferred_flag_data                = {0};

static enum gfx_ctx_api current_video_context_api        = GFX_CTX_NONE;

shader_backend_t *current_shader                         = NULL;
void *shader_data                                        = NULL;

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
#if defined(HAVE_D3D10)
   &video_d3d10,
#endif
#if defined(HAVE_D3D11)
   &video_d3d11,
#endif
#if defined(HAVE_D3D12)
   &video_d3d12,
#endif
#if defined(HAVE_D3D9)
   &video_d3d9,
#endif
#if defined(HAVE_D3D8)
   &video_d3d8,
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
#ifdef SWITCH
   &video_switch,
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

static const gfx_ctx_driver_t *gfx_ctx_drivers[] = {
#if defined(__CELLOS_LV2__)
   &gfx_ctx_ps3,
#endif
#if defined(HAVE_D3D)
   &gfx_ctx_d3d,
#endif
#if defined(HAVE_VIDEOCORE)
   &gfx_ctx_videocore,
#endif
#if defined(HAVE_MALI_FBDEV)
   &gfx_ctx_mali_fbdev,
#endif
#if defined(HAVE_VIVANTE_FBDEV)
   &gfx_ctx_vivante_fbdev,
#endif
#if defined(HAVE_OPENDINGUX_FBDEV)
   &gfx_ctx_opendingux_fbdev,
#endif
#if defined(_WIN32) && (defined(HAVE_OPENGL) || defined(HAVE_VULKAN))
   &gfx_ctx_wgl,
#endif
#if defined(HAVE_WAYLAND)
   &gfx_ctx_wayland,
#endif
#if defined(HAVE_X11) && !defined(HAVE_OPENGLES)
#if defined(HAVE_OPENGL) || defined(HAVE_VULKAN)
   &gfx_ctx_x,
#endif
#endif
#if defined(HAVE_X11) && defined(HAVE_OPENGL) && defined(HAVE_EGL)
   &gfx_ctx_x_egl,
#endif
#if defined(HAVE_KMS)
   &gfx_ctx_drm,
#endif
#if defined(ANDROID)
   &gfx_ctx_android,
#endif
#if defined(__QNX__)
   &gfx_ctx_qnx,
#endif
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH)
   &gfx_ctx_cocoagl,
#endif
#if defined(__APPLE__) && !defined(TARGET_IPHONE_SIMULATOR) && !defined(TARGET_OS_IPHONE)
   &gfx_ctx_cgl,
#endif
#if (defined(HAVE_SDL) || defined(HAVE_SDL2)) && defined(HAVE_OPENGL)
   &gfx_ctx_sdl_gl,
#endif
#ifdef HAVE_OSMESA
   &gfx_ctx_osmesa,
#endif
#ifdef EMSCRIPTEN
   &gfx_ctx_emscripten,
#endif
#if defined(HAVE_VULKAN) && defined(HAVE_VULKAN_DISPLAY)
   &gfx_ctx_khr_display,
#endif
#if defined(_WIN32) && !defined(_XBOX)
   &gfx_ctx_gdi,
#endif
   &gfx_ctx_null,
   NULL
};

static const shader_backend_t *shader_ctx_drivers[] = {
#ifdef HAVE_GLSL
   &gl_glsl_backend,
#endif
#ifdef HAVE_CG
   &gl_cg_backend,
#endif
#ifdef HAVE_HLSL
   &hlsl_backend,
#endif
   &shader_null_backend,
   NULL
};


static const gl_renderchain_driver_t *renderchain_gl_drivers[] = {
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   &gl2_renderchain,
#endif
   NULL
};

/* Stub functions */

static void update_window_title_null(void *data, void *data2)
{
}

static void swap_buffers_null(void *data, void *data2)
{
}

static bool get_metrics_null(void *data, enum display_metric_types type,
      float *value)
{
   return false;
}

static bool set_resize_null(void *a, unsigned b, unsigned c)
{
   return false;
}

void video_driver_set_resize(unsigned width, unsigned height)
{
   if (current_video_context.set_resize)
      current_video_context.set_resize(video_context_data, width, height);
}

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

#ifdef HAVE_VULKAN
static bool hw_render_context_is_vulkan(enum retro_hw_context_type type)
{
   return type == RETRO_HW_CONTEXT_VULKAN;
}
#endif

#if defined(HAVE_OPENGL)
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
#endif

bool *video_driver_get_threaded(void)
{
   return &video_driver_threaded;
}

void video_driver_set_threaded(bool val)
{
   video_driver_threaded = val;
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

static bool video_context_has_focus(void)
{
   return current_video_context.has_focus(video_context_data);
}

static bool video_driver_has_focus(void)
{
   return current_video->focus(video_driver_data);
}

static bool null_driver_has_focus(void)
{
   return true;
}

static void video_context_driver_reset(void)
{
   if (!current_video_context.get_metrics)
      current_video_context.get_metrics         = get_metrics_null;

   if (!current_video_context.update_window_title)
      current_video_context.update_window_title = update_window_title_null;

   if (!current_video_context.set_resize)
      current_video_context.set_resize          = set_resize_null;

   if (!current_video_context.swap_buffers)
      current_video_context.swap_buffers        = swap_buffers_null;

   if (current_video_context.has_focus)
      video_driver_cb_has_focus                 = video_context_has_focus;


   if(current_video_context_api == GFX_CTX_NONE)
   {
      const char *video_driver = video_driver_get_ident();

      if(string_is_equal(video_driver, "d3d11"))
         current_video_context_api = GFX_CTX_DIRECT3D11_API;
      else if(string_is_equal(video_driver, "gx2"))
         current_video_context_api = GFX_CTX_GX2_API;
   }
}

bool video_context_driver_set(const gfx_ctx_driver_t *data)
{
   if (!data)
      return false;
   current_video_context = *data;
   video_context_driver_reset();
   return true;
}

void video_context_driver_destroy(void)
{
   current_video_context.init                       = NULL;
   current_video_context.bind_api                   = NULL;
   current_video_context.swap_interval              = NULL;
   current_video_context.set_video_mode             = NULL;
   current_video_context.get_video_size             = NULL;
   current_video_context.get_video_output_size      = NULL;
   current_video_context.get_video_output_prev      = NULL;
   current_video_context.get_video_output_next      = NULL;
   current_video_context.get_metrics                = get_metrics_null;
   current_video_context.translate_aspect           = NULL;
   current_video_context.update_window_title        = update_window_title_null;
   current_video_context.check_window               = NULL;
   current_video_context.set_resize                 = set_resize_null;
   current_video_context.has_focus                  = NULL;
   current_video_context.suppress_screensaver       = NULL;
   current_video_context.has_windowed               = NULL;
   current_video_context.swap_buffers               = swap_buffers_null;
   current_video_context.input_driver               = NULL;
   current_video_context.get_proc_address           = NULL;
   current_video_context.image_buffer_init          = NULL;
   current_video_context.image_buffer_write         = NULL;
   current_video_context.show_mouse                 = NULL;
   current_video_context.ident                      = NULL;
   current_video_context.get_flags                  = NULL;
   current_video_context.set_flags                  = NULL;
   current_video_context.bind_hw_render             = NULL;
   current_video_context.get_context_data           = NULL;
   current_video_context.make_current               = NULL;

   current_video_context_api = GFX_CTX_NONE;
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
   bool ret = false;
   if (current_video->set_shader)
      ret = current_video->set_shader(video_driver_data, type, path);

   return ret;
}

static void video_driver_filter_free(void)
{
   if (video_driver_state_filter)
      rarch_softfilter_free(video_driver_state_filter);
   video_driver_state_filter    = NULL;

   if (video_driver_state_buffer)
   {
#ifdef _3DS
      linearFree(video_driver_state_buffer);
#else
      free(video_driver_state_buffer);
#endif
   }
   video_driver_state_buffer    = NULL;

   video_driver_state_scale     = 0;
   video_driver_state_out_bpp   = 0;
   video_driver_state_out_rgb32 = false;
}

static void video_driver_init_filter(enum retro_pixel_format colfmt_int)
{
   unsigned pow2_x, pow2_y, maxsize;
   void *buf                            = NULL;
   settings_t *settings                 = config_get_ptr();
   struct retro_game_geometry *geom     = &video_driver_av_info.geometry;
   unsigned width                       = geom->max_width;
   unsigned height                      = geom->max_height;
   /* Deprecated format. Gets pre-converted. */
   enum retro_pixel_format colfmt       =
      (colfmt_int == RETRO_PIXEL_FORMAT_0RGB1555) ?
      RETRO_PIXEL_FORMAT_RGB565 : colfmt_int;

   if (video_driver_is_hw_context())
   {
      RARCH_WARN("Cannot use CPU filters when hardware rendering is used.\n");
      return;
   }

   video_driver_state_filter            = rarch_softfilter_new(
         settings->paths.path_softfilter_plugin,
         RARCH_SOFTFILTER_THREADS_AUTO, colfmt, width, height);

   if (!video_driver_state_filter)
   {
      RARCH_ERR("[Video]: Failed to load filter.\n");
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
   buf = linearMemAlign(
         width * height * video_driver_state_out_bpp, 0x80);
#else
   buf = malloc(
         width * height * video_driver_state_out_bpp);
#endif
   if (!buf)
   {
      RARCH_ERR("[Video]: Softfilter initialization failed.\n");
      video_driver_filter_free();
      return;
   }

   video_driver_state_buffer    = buf;
}

static void video_driver_init_input(const input_driver_t *tmp)
{
   const input_driver_t **input = input_get_double_ptr();
   if (*input)
      return;

   /* Video driver didn't provide an input driver,
    * so we use configured one. */
   RARCH_LOG("[Video]: Graphics driver did not initialize an input driver. Attempting to pick a suitable driver.\n");

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
   RARCH_ERR("[Video]: Cannot initialize input driver. Exiting ...\n");
   retroarch_fail(1, "video_driver_init_input()");
}

/**
 * video_driver_monitor_compute_fps_statistics:
 *
 * Computes monitor FPS statistics.
 **/
static void video_driver_monitor_compute_fps_statistics(void)
{
   double avg_fps       = 0.0;
   double stddev        = 0.0;
   unsigned samples     = 0;

   if (video_driver_frame_time_count <
         (2 * MEASURE_FRAME_TIME_SAMPLES_COUNT))
   {
      RARCH_LOG(
            "[Video]: Does not have enough samples for monitor refresh rate estimation. Requires to run for at least %u frames.\n",
            2 * MEASURE_FRAME_TIME_SAMPLES_COUNT);
      return;
   }

   if (video_monitor_fps_statistics(&avg_fps, &stddev, &samples))
   {
      RARCH_LOG("[Video]: Average monitor Hz: %.6f Hz. (%.3f %% frame time deviation, based on %u last samples).\n",
            avg_fps, 100.0 * stddev, samples);
   }
}

static void video_driver_pixel_converter_free(void)
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

static void video_driver_free_internal(void)
{
#ifdef HAVE_THREADS
   bool is_threaded     = video_driver_is_threaded();
#endif

   command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);

   if (!video_driver_is_video_cache_context())
      video_driver_free_hw_context();

   if (
         !input_driver_owns_driver() &&
         !input_driver_is_data_ptr_same(video_driver_data)
      )
      input_driver_deinit();

   if (
         !video_driver_data_own
         && video_driver_data
         && current_video && current_video->free
      )
      current_video->free(video_driver_data);

   video_driver_pixel_converter_free();
   video_driver_filter_free();

   command_event(CMD_EVENT_SHADER_DIR_DEINIT, NULL);

#ifdef HAVE_THREADS
   if (is_threaded)
      return;
#endif

   video_driver_monitor_compute_fps_statistics();
}

static bool video_driver_pixel_converter_init(unsigned size)
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
   video_driver_pixel_converter_free();
   video_driver_filter_free();

   return false;
}

static bool video_driver_init_internal(bool *video_is_threaded)
{
   video_info_t video;
   unsigned max_dim, scale, width, height;
   video_viewport_t *custom_vp            = NULL;
   const input_driver_t *tmp              = NULL;
   rarch_system_info_t *system            = NULL;
   static uint16_t dummy_pixels[32]       = {0};
   settings_t *settings                   = config_get_ptr();
   struct retro_game_geometry *geom       = &video_driver_av_info.geometry;

   if (!string_is_empty(settings->paths.path_softfilter_plugin))
      video_driver_init_filter(video_driver_pix_fmt);

   command_event(CMD_EVENT_SHADER_DIR_INIT, NULL);

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

   if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      float default_aspect = aspectratio_lut[ASPECT_RATIO_CORE].value;
      aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
         (custom_vp->width && custom_vp->height) ?
         (float)custom_vp->width / custom_vp->height : default_aspect;
   }

   video_driver_set_aspect_ratio_value(
      aspectratio_lut[settings->uints.video_aspect_ratio_idx].value);

   if (settings->bools.video_fullscreen|| retroarch_is_forced_fullscreen())
   {
      width  = settings->uints.video_fullscreen_x;
      height = settings->uints.video_fullscreen_y;
   }
   else
   {
      if(settings->uints.video_window_x || settings->uints.video_window_y)
      {
         width  = settings->uints.video_window_x;
         height = settings->uints.video_window_y;
      }
      else
      {
         if (settings->bools.video_force_aspect)
         {
            /* Do rounding here to simplify integer scale correctness. */
            unsigned base_width =
               roundf(geom->base_height * video_driver_get_aspect_ratio());
            width  = roundf(base_width * settings->floats.video_scale);
         }
         else
            width  = roundf(geom->base_width   * settings->floats.video_scale);
         height = roundf(geom->base_height * settings->floats.video_scale);
      }
   }

   if (width && height)
      RARCH_LOG("[Video]: Video @ %ux%u\n", width, height);
   else
      RARCH_LOG("[Video]: Video @ fullscreen\n");

   video_driver_display_type_set(RARCH_DISPLAY_NONE);
   video_driver_display_set(0);
   video_driver_window_set(0);

   if (!video_driver_pixel_converter_init(RARCH_SCALE_BASE * scale))
   {
      RARCH_ERR("[Video]: Failed to initialize pixel converter.\n");
      goto error;
   }

   video.width         = width;
   video.height        = height;
   video.fullscreen    = settings->bools.video_fullscreen || retroarch_is_forced_fullscreen();
   video.vsync         = settings->bools.video_vsync && !rarch_ctl(RARCH_CTL_IS_NONBLOCK_FORCED, NULL);
   video.force_aspect  = settings->bools.video_force_aspect;
   video.font_enable   = settings->bools.video_font_enable;
   video.swap_interval = settings->uints.video_swap_interval;
#ifdef GEKKO
   video.viwidth       = settings->uints.video_viwidth;
   video.vfilter       = settings->bools.video_vfilter;
#endif
   video.smooth        = settings->bools.video_smooth;
   video.input_scale   = scale;
   video.rgb32         = video_driver_state_filter ?
      video_driver_state_out_rgb32 :
      (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);

   /* Reset video frame count */
   video_driver_frame_count = 0;

   tmp = input_get_ptr();
   /* Need to grab the "real" video driver interface on a reinit. */
   video_driver_find_driver();

#ifdef HAVE_THREADS
   video.is_threaded   = video_driver_is_threaded();
   *video_is_threaded  = video.is_threaded;

   if (video.is_threaded)
   {
      /* Can't do hardware rendering with threaded driver currently. */
      RARCH_LOG("[Video]: Starting threaded video driver ...\n");

      if (!video_init_thread((const video_driver_t**)&current_video,
               &video_driver_data,
               input_get_double_ptr(), input_driver_get_data_ptr(),
               current_video, video))
      {
         RARCH_ERR("[Video]: Cannot open threaded video driver ... Exiting ...\n");
         goto error;
      }
   }
   else
#endif
      video_driver_data = current_video->init(&video, input_get_double_ptr(),
            input_driver_get_data_ptr());

   if (!video_driver_data)
   {
      RARCH_ERR("[Video]: Cannot open video driver ... Exiting ...\n");
      goto error;
   }

   if (current_video->focus)
      video_driver_cb_has_focus = video_driver_has_focus;

   video_driver_poke = NULL;
   if (current_video->poke_interface)
      current_video->poke_interface(video_driver_data, &video_driver_poke);

   if (current_video->viewport_info &&
         (!custom_vp->width  ||
          !custom_vp->height))
   {
      /* Force custom viewport to have sane parameters. */
      custom_vp->width = width;
      custom_vp->height = height;

      video_driver_get_viewport_info(custom_vp);
   }

   system              = runloop_get_system_info();

   video_driver_set_rotation(
            (settings->uints.video_rotation + system->rotation) % 4);

   current_video->suppress_screensaver(video_driver_data,
         settings->bools.ui_suspend_screensaver_enable);

   video_driver_init_input(tmp);

   command_event(CMD_EVENT_OVERLAY_DEINIT, NULL);
   command_event(CMD_EVENT_OVERLAY_INIT, NULL);

   if (!core_is_game_loaded())
      video_driver_cached_frame_set(&dummy_pixels, 4, 4, 8);

#if defined(PSP)
   video_driver_set_texture_frame(&dummy_pixels, false, 1, 1, 1.0f);
#endif

   video_context_driver_reset();

   video_display_server_init();

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
   video_frame_info_t video_info;
   video_driver_build_info(&video_info);
   if (video_driver_poke && video_driver_poke->set_osd_msg)
      video_driver_poke->set_osd_msg(video_driver_data, &video_info, msg, data, font);
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
   if (video_driver_poke && video_driver_poke->set_texture_frame)
      video_driver_poke->set_texture_frame(video_driver_data,
            frame, rgb32, width, height, alpha);
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
   if (data)
      frame_cache_data = data;
   frame_cache_width   = width;
   frame_cache_height  = height;
   frame_cache_pitch   = pitch;
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
#ifdef HAVE_THREADS
   bool is_threaded = video_driver_is_threaded();
   video_driver_threaded_lock(is_threaded);
#endif
   if (width)
      *width  = video_driver_width;
   if (height)
      *height = video_driver_height;
#ifdef HAVE_THREADS
   video_driver_threaded_unlock(is_threaded);
#endif
}

void video_driver_set_size(unsigned *width, unsigned *height)
{
#ifdef HAVE_THREADS
   bool is_threaded = video_driver_is_threaded();
   video_driver_threaded_lock(is_threaded);
#endif
   if (width)
      video_driver_width  = *width;
   if (height)
      video_driver_height = *height;
#ifdef HAVE_THREADS
   video_driver_threaded_unlock(is_threaded);
#endif
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

   configuration_set_float(settings,
         settings->floats.video_refresh_rate,
         hz);
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
   retro_time_t accum     = 0;
   retro_time_t avg       = 0;
   retro_time_t accum_var = 0;
   unsigned samples       = 0;

#ifdef HAVE_THREADS
   if (video_driver_is_threaded())
      return false;
#endif

   samples = MIN(MEASURE_FRAME_TIME_SAMPLES_COUNT,
         (unsigned)video_driver_frame_time_count);

   if (samples < 2)
      return false;

   /* Measure statistics on frame time (microsecs), *not* FPS. */
   for (i = 0; i < samples; i++)
   {
      accum += video_driver_frame_time_samples[i];
#if 0
      RARCH_LOG("[Video]: Interval #%u: %d usec / frame.\n",
            i, (int)frame_time_samples[i]);
#endif
   }

   avg = accum / samples;

   /* Drop first measurement. It is likely to be bad. */
   for (i = 0; i < samples; i++)
   {
      retro_time_t diff = video_driver_frame_time_samples[i] - avg;
      accum_var         += diff * diff;
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
   rarch_softfilter_get_output_size(video_driver_state_filter,
         output_width, output_height, width, height);

   *output_pitch = (*output_width) * video_driver_state_out_bpp;

   rarch_softfilter_process(video_driver_state_filter,
         video_driver_state_buffer, *output_pitch,
         data, width, height, pitch);

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
   void *recording  = recording_driver_get_data_ptr();

   /* Cannot allow recording when pushing duped frames. */
   recording_data   = NULL;

   retro_ctx.frame_cb(
         (frame_cache_data != RETRO_HW_FRAME_BUFFER_VALID)
         ? frame_cache_data : NULL,
         frame_cache_width,
         frame_cache_height, frame_cache_pitch);

   recording_data   = recording;

   return true;
}

void video_driver_monitor_adjust_system_rates(void)
{
   float timing_skew;
   settings_t *settings                   = config_get_ptr();
   float video_refresh_rate               = settings->floats.video_refresh_rate;
   const struct retro_system_timing *info = (const struct retro_system_timing*)&video_driver_av_info.timing;

   rarch_ctl(RARCH_CTL_UNSET_NONBLOCK_FORCED, NULL);


   if (!info || info->fps <= 0.0)
      return;

   timing_skew = fabs(1.0f - info->fps / video_refresh_rate);

   /* We don't want to adjust pitch too much. If we have extreme cases,
    * just don't readjust at all. */
   if (timing_skew <= settings->floats.audio_max_timing_skew)
      return;

   RARCH_LOG("[Video]: Timings deviate too much. Will not adjust. (Display = %.2f Hz, Game = %.2f Hz)\n",
         video_refresh_rate,
         (float)info->fps);

   if (info->fps <= video_refresh_rate)
      return;

   /* We won't be able to do VSync reliably when game FPS > monitor FPS. */
   rarch_ctl(RARCH_CTL_SET_NONBLOCK_FORCED, NULL);
   RARCH_LOG("[Video]: Game FPS > Monitor FPS. Cannot rely on VSync.\n");
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
   video_display_server_destroy();
   video_driver_cb_has_focus      = null_driver_has_focus;
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
   return settings->bools.video_gpu_record && current_video->read_viewport;
}

bool video_driver_supports_viewport_read(void)
{
   settings_t *settings = config_get_ptr();
   return (settings->bools.video_gpu_screenshot ||
         (video_driver_is_hw_context() && !current_video->read_frame_raw))
      && current_video->read_viewport && current_video->viewport_info;
}

bool video_driver_supports_read_frame_raw(void)
{
   if (current_video->read_frame_raw)
	   return true;
   return false;
}

void video_driver_set_viewport_config(void)
{
   settings_t *settings                   = config_get_ptr();

   if (settings->floats.video_aspect_ratio < 0.0f)
   {
      struct retro_game_geometry *geom = &video_driver_av_info.geometry;

      if (geom->aspect_ratio > 0.0f && settings->bools.video_aspect_ratio_auto)
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
         settings->floats.video_aspect_ratio;
   }
}

void video_driver_set_viewport_square_pixel(void)
{
   unsigned len, highest, i, aspect_x, aspect_y;
   struct retro_game_geometry *geom  = &video_driver_av_info.geometry;
   unsigned width                    = geom->base_width;
   unsigned height                   = geom->base_height;

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
   struct retro_game_geometry *geom     = &video_driver_av_info.geometry;

   if (!geom || geom->base_width <= 0.0f || geom->base_height <= 0.0f)
      return;

   /* Fallback to 1:1 pixel ratio if none provided */
   if (geom->aspect_ratio > 0.0f)
      aspectratio_lut[ASPECT_RATIO_CORE].value = geom->aspect_ratio;
   else
      aspectratio_lut[ASPECT_RATIO_CORE].value =
         (float)geom->base_width / geom->base_height;
}

void video_driver_reset_custom_viewport(void)
{
   struct video_viewport *custom_vp = video_viewport_get_custom();

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

bool video_driver_init(bool *video_is_threaded)
{
   video_driver_lock_new();
   video_driver_filter_free();
   return video_driver_init_internal(video_is_threaded);
}

void video_driver_destroy_data(void)
{
   video_driver_data = NULL;
}

void video_driver_free(void)
{
   video_driver_free_internal();
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

   switch (settings->uints.video_aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         video_driver_set_viewport_square_pixel();
         break;

      case ASPECT_RATIO_CORE:
         video_driver_set_viewport_core();
         break;

      case ASPECT_RATIO_CONFIG:
         video_driver_set_viewport_config();
         break;

      default:
         break;
   }

   video_driver_set_aspect_ratio_value(
            aspectratio_lut[settings->uints.video_aspect_ratio_idx].value);

   if (!video_driver_poke || !video_driver_poke->set_aspect_ratio)
      return;
   video_driver_poke->set_aspect_ratio(
         video_driver_data, settings->uints.video_aspect_ratio_idx);
}

void video_driver_update_viewport(struct video_viewport* vp, bool force_full, bool keep_aspect)
{
   gfx_ctx_aspect_t aspect_data;
   float            device_aspect = (float)vp->full_width / vp->full_height;
   settings_t*      settings      = config_get_ptr();

   aspect_data.aspect = &device_aspect;
   aspect_data.width  = vp->full_width;
   aspect_data.height = vp->full_height;

   video_context_driver_translate_aspect(&aspect_data);

   vp->x      = 0;
   vp->y      = 0;
   vp->width  = vp->full_width;
   vp->height = vp->full_height;

   if (settings->bools.video_scale_integer && !force_full)
   {
      video_viewport_get_scaled_integer(
            vp, vp->full_width, vp->full_height, video_driver_get_aspect_ratio(), keep_aspect);
   }
   else if (keep_aspect && !force_full)
   {
      float desired_aspect = video_driver_get_aspect_ratio();

#if defined(HAVE_MENU)
      if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const struct video_viewport* custom = video_viewport_get_custom();

         vp->x      = custom->x;
         vp->y      = custom->y;
         vp->width  = custom->width;
         vp->height = custom->height;
      }
      else
#endif
      {
         float delta;

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta      = (desired_aspect / device_aspect - 1.0f) / 2.0f + 0.5f;
            vp->x      = (int)roundf(vp->full_width * (0.5f - delta));
            vp->width  = (unsigned)roundf(2.0f * vp->full_width * delta);
            vp->y      = 0;
            vp->height = vp->full_height;
         }
         else
         {
            vp->x      = 0;
            vp->width  = vp->full_width;
            delta      = (device_aspect / desired_aspect - 1.0f) / 2.0f + 0.5f;
            vp->y      = (int)roundf(vp->full_height * (0.5f - delta));
            vp->height = (unsigned)roundf(2.0f * vp->full_height * delta);
         }
      }
   }

#if defined(RARCH_MOBILE)
   /* In portrait mode, we want viewport to gravitate to top of screen. */
   if (device_aspect < 1.0f)
      vp->y = 0;
#endif
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

      (void)hwr;

#if defined(HAVE_VULKAN)
      if (hwr && hw_render_context_is_vulkan(hwr->context_type))
      {
         RARCH_LOG("[Video]: Using HW render, Vulkan driver forced.\n");
         current_video = &video_vulkan;
      }
#endif

#if defined(HAVE_OPENGL)
      if (hwr && hw_render_context_is_gl(hwr->context_type))
      {
         RARCH_LOG("[Video]: Using HW render, OpenGL driver forced.\n");
         current_video = &video_gl;
      }
#endif

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
   drv.s     = settings->arrays.video_driver;

   driver_ctl(RARCH_DRIVER_CTL_FIND_INDEX, &drv);

   i = (int)drv.len;

   if (i >= 0)
      current_video = (video_driver_t*)video_driver_find_handle(i);
   else
   {
      if (verbosity_is_enabled())
      {
         unsigned d;
         RARCH_ERR("Couldn't find any video driver named \"%s\"\n",
               settings->arrays.video_driver);
         RARCH_LOG_OUTPUT("Available video drivers are:\n");
         for (d = 0; video_driver_find_handle(d); d++)
            RARCH_LOG_OUTPUT("\t%s\n", video_driver_find_ident(d));
         RARCH_WARN("Going to default to first video driver...\n");
      }

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

void video_driver_free_hw_context(void)
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

void video_driver_get_record_status(
      bool *has_gpu_record,
      uint8_t **gpu_buf)
{
   *gpu_buf        = video_driver_record_gpu_buffer;
   *has_gpu_record = video_driver_record_gpu_buffer != NULL;
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

   fill_pathname_join_concat_noext(
         video_driver_title_buf,
         msg_hash_to_str(MSG_PROGRAM),
         " ",
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

   if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
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
      unsigned base_height                 = video_driver_av_info.geometry.base_height;

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
   return &video_driver_av_info;
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
   static retro_time_t curr_time;
   static retro_time_t fps_time;
   static float last_fps;
   unsigned output_width                             = 0;
   unsigned output_height                            = 0;
   unsigned output_pitch                             = 0;
   const char *msg                                   = NULL;
   retro_time_t        new_time                      =
      cpu_features_get_time_usec();

   if (!video_driver_active)
      return;

   if (video_driver_scaler_ptr && data &&
         (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_0RGB1555) &&
         (data != RETRO_HW_FRAME_BUFFER_VALID))
   {
      if (video_pixel_frame_scale(
               video_driver_scaler_ptr->scaler,
               video_driver_scaler_ptr->scaler_out,
               data, width, height, pitch))
      {
         data                = video_driver_scaler_ptr->scaler_out;
         pitch               = video_driver_scaler_ptr->scaler->out_stride;
      }
   }


   if (data)
      frame_cache_data = data;
   frame_cache_width   = width;
   frame_cache_height  = height;
   frame_cache_pitch   = pitch;

   video_driver_build_info(&video_info);

   /* Get the amount of frames per seconds. */
   if (video_driver_frame_count)
   {
      unsigned write_index                         =
         video_driver_frame_time_count++ &
         (MEASURE_FRAME_TIME_SAMPLES_COUNT - 1);
      video_driver_frame_time_samples[write_index] = new_time - fps_time;
      fps_time                                     = new_time;

      if ((video_driver_frame_count % FPS_UPDATE_INTERVAL) == 0)
      {
         char frames_text[64];

         fill_pathname_noext(video_driver_window_title,
               video_driver_title_buf,
               " || ",
               sizeof(video_driver_window_title));

         if (video_info.fps_show)
         {
            last_fps = TIME_TO_FPS(curr_time, new_time, FPS_UPDATE_INTERVAL);
            snprintf(video_info.fps_text,
                  sizeof(video_info.fps_text),
                  " FPS: %6.1f", last_fps);
            strlcat(video_driver_window_title,
                  video_info.fps_text,
                  sizeof(video_driver_window_title));
         }

         curr_time = new_time;

         if (video_info.framecount_show)
         {
            strlcat(video_driver_window_title,
                  " || Frames: ",
                  sizeof(video_driver_window_title));

            snprintf(frames_text,
                  sizeof(frames_text),
                  "%" PRIu64,
                  (uint64_t)video_driver_frame_count);

            strlcat(video_driver_window_title,
                  frames_text,
                  sizeof(video_driver_window_title));
         }

         video_driver_window_title_update = true;
      }

      if (video_info.fps_show)
      {
         if (video_info.framecount_show)
         {
            snprintf(
                  video_info.fps_text,
                  sizeof(video_info.fps_text),
                  "FPS: %6.1f || %s: %" PRIu64,
                  last_fps,
                  msg_hash_to_str(MSG_FRAMES),
                  (uint64_t)video_driver_frame_count);
         }
         else
         {
            snprintf(
                  video_info.fps_text,
                  sizeof(video_info.fps_text),
                  "FPS: %6.1f",
                  last_fps);
         }
      }
   }
   else
   {

      curr_time = fps_time = new_time;

      strlcpy(video_driver_window_title,
            video_driver_title_buf,
            sizeof(video_driver_window_title));

      if (video_info.fps_show)
         strlcpy(video_info.fps_text,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               sizeof(video_info.fps_text));

      video_driver_window_title_update = true;
   }

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
   {
#ifdef HAVE_THREADS
      /* the msg pointer may point to data modified by another thread */
      runloop_msg_queue_lock();
#endif
      strlcpy(video_driver_msg, msg, sizeof(video_driver_msg));
#ifdef HAVE_THREADS
      runloop_msg_queue_unlock();
#endif
   }

   video_driver_active = current_video->frame(
         video_driver_data, data, width, height,
         video_driver_frame_count,
         (unsigned)pitch, video_driver_msg, &video_info);

   video_driver_frame_count++;

   /* Display the FPS, with a higher priority. */
   if (video_info.fps_show)
      runloop_msg_queue_push(video_info.fps_text, 2, 1, true);
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

static void video_shader_driver_use_null(void *data,
      void *shader_data, unsigned idx, bool set_active)
{
   (void)data;
   (void)idx;
   (void)set_active;
}

static bool video_driver_cb_set_coords(void *handle_data,
      void *shader_data, const struct video_coords *coords)
{
   video_shader_ctx_coords_t ctx_coords;
   ctx_coords.handle_data = handle_data;
   ctx_coords.data        = coords;

   video_driver_set_coords(&ctx_coords);
   return true;
}

void video_driver_build_info(video_frame_info_t *video_info)
{
   bool is_perfcnt_enable            = false;
   bool is_paused                    = false;
   bool is_idle                      = false;
   bool is_slowmotion                = false;
   settings_t *settings              = NULL;
   video_viewport_t *custom_vp       = NULL;
   struct retro_hw_render_callback *hwr =
      video_driver_get_hw_context();
#ifdef HAVE_THREADS
   bool is_threaded                  = video_driver_is_threaded();
   video_driver_threaded_lock(is_threaded);
#endif
   settings                          = config_get_ptr();
   custom_vp                         = &settings->video_viewport_custom;
   video_info->refresh_rate          = settings->floats.video_refresh_rate;
   video_info->black_frame_insertion =
      settings->bools.video_black_frame_insertion;
   video_info->hard_sync             = settings->bools.video_hard_sync;
   video_info->hard_sync_frames      = settings->uints.video_hard_sync_frames;
   video_info->fps_show              = settings->bools.video_fps_show;
   video_info->framecount_show       = settings->bools.video_framecount_show;
   video_info->scale_integer         = settings->bools.video_scale_integer;
   video_info->aspect_ratio_idx      = settings->uints.video_aspect_ratio_idx;
   video_info->post_filter_record    = settings->bools.video_post_filter_record;
   video_info->max_swapchain_images  = settings->uints.video_max_swapchain_images;
   video_info->windowed_fullscreen   = settings->bools.video_windowed_fullscreen;
   video_info->fullscreen            = settings->bools.video_fullscreen || retroarch_is_forced_fullscreen();
   video_info->monitor_index         = settings->uints.video_monitor_index;
   video_info->shared_context        = settings->bools.video_shared_context;

   if (libretro_get_shared_context() && hwr && hwr->context_type != RETRO_HW_CONTEXT_NONE)
      video_info->shared_context     = true;

   video_info->font_enable           = settings->bools.video_font_enable;
   video_info->font_msg_pos_x        = settings->floats.video_msg_pos_x;
   video_info->font_msg_pos_y        = settings->floats.video_msg_pos_y;
   video_info->font_msg_color_r      = settings->floats.video_msg_color_r;
   video_info->font_msg_color_g      = settings->floats.video_msg_color_g;
   video_info->font_msg_color_b      = settings->floats.video_msg_color_b;
   video_info->custom_vp_x           = custom_vp->x;
   video_info->custom_vp_y           = custom_vp->y;
   video_info->custom_vp_width       = custom_vp->width;
   video_info->custom_vp_height      = custom_vp->height;
   video_info->custom_vp_full_width  = custom_vp->full_width;
   video_info->custom_vp_full_height = custom_vp->full_height;

   video_info->fps_text[0]           = '\0';

   video_info->width                 = video_driver_width;
   video_info->height                = video_driver_height;

   video_info->use_rgba              = video_driver_use_rgba;

   video_info->libretro_running       = false;
#ifdef HAVE_MENU
   video_info->menu_is_alive          = menu_driver_is_alive();
   video_info->menu_footer_opacity    = settings->floats.menu_footer_opacity;
   video_info->menu_header_opacity    = settings->floats.menu_header_opacity;
   video_info->materialui_color_theme = settings->uints.menu_materialui_color_theme;
   video_info->menu_shader_pipeline   = settings->uints.menu_xmb_shader_pipeline;
   video_info->xmb_theme              = settings->uints.menu_xmb_theme;
   video_info->xmb_color_theme        = settings->uints.menu_xmb_color_theme;
   video_info->timedate_enable        = settings->bools.menu_timedate_enable;
   video_info->battery_level_enable   = settings->bools.menu_battery_level_enable;
   video_info->xmb_shadows_enable     = settings->bools.menu_xmb_shadows_enable;
   video_info->xmb_alpha_factor       = settings->uints.menu_xmb_alpha_factor;
   video_info->menu_wallpaper_opacity   = settings->floats.menu_wallpaper_opacity;
   video_info->menu_framebuffer_opacity = settings->floats.menu_framebuffer_opacity;

   video_info->libretro_running       = core_is_game_loaded();
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
   video_info->menu_framebuffer_opacity = 0.0f;
   video_info->menu_wallpaper_opacity = 0.0f;
#endif

   runloop_get_status(&is_paused, &is_idle, &is_slowmotion, &is_perfcnt_enable);

   video_info->is_perfcnt_enable      = is_perfcnt_enable;
   video_info->runloop_is_paused      = is_paused;
   video_info->runloop_is_idle        = is_idle;
   video_info->runloop_is_slowmotion  = is_slowmotion;

   video_info->input_driver_nonblock_state = input_driver_is_nonblock_state();

   video_info->context_data           = video_context_data;
   video_info->shader_data            = shader_data;

   video_info->cb_update_window_title = current_video_context.update_window_title;
   video_info->cb_swap_buffers        = current_video_context.swap_buffers;
   video_info->cb_get_metrics         = current_video_context.get_metrics;
   video_info->cb_set_resize          = current_video_context.set_resize;

   video_info->cb_shader_use          = video_driver_cb_shader_use;
   video_info->cb_set_mvp             = video_driver_cb_shader_set_mvp;

#if 0
   video_info->cb_set_coords          = video_driver_cb_set_coords;
#endif

#ifdef HAVE_THREADS
   video_driver_threaded_unlock(is_threaded);
#endif
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
   int norm_vp_width         = (int)vp->width;
   int norm_vp_height        = (int)vp->height;
   int norm_full_vp_width    = (int)vp->full_width;
   int norm_full_vp_height   = (int)vp->full_height;

   if (norm_full_vp_width <= 0 || norm_full_vp_height <= 0)
      return false;

   if (mouse_x >= 0 && mouse_x <= norm_full_vp_width)
      scaled_screen_x = ((2 * mouse_x * 0x7fff) / norm_full_vp_width)  - 0x7fff;
   else
      scaled_screen_x = -0x8000; /* OOB */

   if (mouse_y >= 0 && mouse_y <= norm_full_vp_height)
      scaled_screen_y = ((2 * mouse_y * 0x7fff) / norm_full_vp_height) - 0x7fff;
   else
      scaled_screen_y = -0x8000; /* OOB */

   mouse_x           -= vp->x;
   mouse_y           -= vp->y;

   if (mouse_x >= 0 && mouse_x <= norm_vp_width)
      scaled_x        = ((2 * mouse_x * 0x7fff) / norm_vp_width) - 0x7fff;
   else
      scaled_x        = -0x8000; /* OOB */

   if (mouse_y >= 0 && mouse_y <= norm_vp_height)
      scaled_y        = ((2 * mouse_y * 0x7fff) / norm_vp_height) - 0x7fff;
   else
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

void video_driver_get_status(uint64_t *frame_count, bool * is_alive,
      bool *is_focused)
{
   *frame_count = video_driver_frame_count;
   *is_alive    = current_video ? current_video->alive(video_driver_data) : true;
   *is_focused  = video_driver_cb_has_focus();
}

/**
 * find_video_context_driver_driver_index:
 * @ident                      : Identifier of resampler driver to find.
 *
 * Finds graphics context driver index by @ident name.
 *
 * Returns: graphics context driver index if driver was found, otherwise
 * -1.
 **/
static int find_video_context_driver_index(const char *ident)
{
   unsigned i;
   for (i = 0; gfx_ctx_drivers[i]; i++)
      if (string_is_equal_noncase(ident, gfx_ctx_drivers[i]->ident))
         return i;
   return -1;
}

/**
 * find_prev_context_driver:
 *
 * Finds previous driver in graphics context driver array.
 **/
bool video_context_driver_find_prev_driver(void)
{
   settings_t *settings = config_get_ptr();
   int                i = find_video_context_driver_index(
         settings->arrays.video_context_driver);

   if (i > 0)
   {
      strlcpy(settings->arrays.video_context_driver,
            gfx_ctx_drivers[i - 1]->ident,
            sizeof(settings->arrays.video_context_driver));
      return true;
   }

   RARCH_WARN("Couldn't find any previous video context driver.\n");
   return false;
}

/**
 * find_next_context_driver:
 *
 * Finds next driver in graphics context driver array.
 **/
bool video_context_driver_find_next_driver(void)
{
   settings_t *settings = config_get_ptr();
   int i = find_video_context_driver_index(settings->arrays.video_context_driver);

   if (i >= 0 && gfx_ctx_drivers[i + 1])
   {
      strlcpy(settings->arrays.video_context_driver,
            gfx_ctx_drivers[i + 1]->ident,
            sizeof(settings->arrays.video_context_driver));
      return true;
   }

   RARCH_WARN("Couldn't find any next video context driver.\n");
   return false;
}

/**
 * video_context_driver_init:
 * @data                    : Input data.
 * @ctx                     : Graphics context driver to initialize.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Initialize graphics context driver.
 *
 * Returns: graphics context driver if successfully initialized, otherwise NULL.
 **/
static const gfx_ctx_driver_t *video_context_driver_init(
      void *data,
      const gfx_ctx_driver_t *ctx,
      const char *ident,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx)
{
   if (ctx->bind_api(data, api, major, minor))
   {
      video_frame_info_t video_info;
      void       *ctx_data = NULL;

      video_driver_build_info(&video_info);

      ctx_data = ctx->init(&video_info, data);

      if (!ctx_data)
         return NULL;

      if (ctx->bind_hw_render)
         ctx->bind_hw_render(ctx_data,
               video_info.shared_context && hw_render_ctx);

      video_context_driver_set_data(ctx_data);

      current_video_context_api = api;

      return ctx;
   }

#ifndef _WIN32
   RARCH_WARN("Failed to bind API (#%u, version %u.%u) on context driver \"%s\".\n",
         (unsigned)api, major, minor, ctx->ident);
#endif

   return NULL;
}

/**
 * video_context_driver_find_driver:
 * @data                    : Input data.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Finds graphics context driver and initializes.
 *
 * Returns: graphics context driver if found, otherwise NULL.
 **/
static const gfx_ctx_driver_t *video_context_driver_find_driver(void *data,
      const char *ident,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx)
{
   int i = find_video_context_driver_index(ident);

   if (i >= 0)
      return video_context_driver_init(data, gfx_ctx_drivers[i], ident,
            api, major, minor, hw_render_ctx);

   for (i = 0; gfx_ctx_drivers[i]; i++)
   {
      const gfx_ctx_driver_t *ctx =
         video_context_driver_init(data, gfx_ctx_drivers[i], ident,
            api, major, minor, hw_render_ctx);

      if (ctx)
         return ctx;
   }

   return NULL;
}

/**
 * video_context_driver_init_first:
 * @data                    : Input data.
 * @ident                   : Identifier of graphics context driver to find.
 * @api                     : API of higher-level graphics API.
 * @major                   : Major version number of higher-level graphics API.
 * @minor                   : Minor version number of higher-level graphics API.
 * @hw_render_ctx           : Request a graphics context driver capable of
 *                            hardware rendering?
 *
 * Finds first suitable graphics context driver and initializes.
 *
 * Returns: graphics context driver if found, otherwise NULL.
 **/
const gfx_ctx_driver_t *video_context_driver_init_first(void *data,
      const char *ident, enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx)
{
   return video_context_driver_find_driver(data, ident, api,
         major, minor, hw_render_ctx);
}

bool video_context_driver_check_window(gfx_ctx_size_t *size_data)
{
   if (     video_context_data
         && current_video_context.check_window)
   {
      bool is_shutdown = rarch_ctl(RARCH_CTL_IS_SHUTDOWN, NULL);
      current_video_context.check_window(video_context_data,
            size_data->quit,
            size_data->resize,
            size_data->width,
            size_data->height,
            is_shutdown);
      return true;
   }

   return false;
}

bool video_context_driver_init_image_buffer(const video_info_t *data)
{
   if (
            current_video_context.image_buffer_init
         && current_video_context.image_buffer_init(video_context_data, data))
      return true;
   return false;
}

bool video_context_driver_write_to_image_buffer(gfx_ctx_image_t *img)
{
   if (
            current_video_context.image_buffer_write
         && current_video_context.image_buffer_write(video_context_data,
            img->frame, img->width, img->height, img->pitch,
            img->rgb32, img->index, img->handle))
      return true;
   return false;
}

bool video_context_driver_get_video_output_prev(void)
{
   if (!current_video_context.get_video_output_prev)
      return false;
   current_video_context.get_video_output_prev(video_context_data);
   return true;
}

bool video_context_driver_get_video_output_next(void)
{
   if (!current_video_context.get_video_output_next)
      return false;
   current_video_context.get_video_output_next(video_context_data);
   return true;
}

bool video_context_driver_bind_hw_render(bool *enable)
{
   if (!current_video_context.bind_hw_render)
      return false;
   current_video_context.bind_hw_render(video_context_data, *enable);
   return true;
}

void video_context_driver_make_current(bool release)
{
   if (current_video_context.make_current)
      current_video_context.make_current(release);
}


bool video_context_driver_translate_aspect(gfx_ctx_aspect_t *aspect)
{
   if (!video_context_data || !aspect)
      return false;
   if (!current_video_context.translate_aspect)
      return false;
   *aspect->aspect = current_video_context.translate_aspect(
         video_context_data, aspect->width, aspect->height);
   return true;
}

void video_context_driver_free(void)
{
   if (current_video_context.destroy)
      current_video_context.destroy(video_context_data);
   video_context_driver_destroy();
   video_context_data    = NULL;
}

bool video_context_driver_get_video_output_size(gfx_ctx_size_t *size_data)
{
   if (!size_data)
      return false;
   if (!current_video_context.get_video_output_size)
      return false;
   current_video_context.get_video_output_size(video_context_data,
         size_data->width, size_data->height);
   return true;
}

bool video_context_driver_swap_interval(unsigned *interval)
{
   if (!current_video_context.swap_interval)
      return false;
   current_video_context.swap_interval(video_context_data, *interval);
   return true;
}

bool video_context_driver_get_proc_address(gfx_ctx_proc_address_t *proc)
{
   if (!current_video_context.get_proc_address)
      return false;

   proc->addr = current_video_context.get_proc_address(proc->sym);

   return true;
}

bool video_context_driver_get_metrics(gfx_ctx_metrics_t *metrics)
{
   if (
         current_video_context.get_metrics(video_context_data,
            metrics->type,
            metrics->value))
      return true;
   return false;
}

bool video_context_driver_input_driver(gfx_ctx_input_t *inp)
{
   settings_t *settings    = config_get_ptr();
   const char *joypad_name = settings ? settings->arrays.input_joypad_driver : NULL;

   if (!current_video_context.input_driver)
      return false;
   current_video_context.input_driver(
         video_context_data, joypad_name,
         inp->input, inp->input_data);
   return true;
}

bool video_context_driver_suppress_screensaver(bool *bool_data)
{
   if (     video_context_data
         && current_video_context.suppress_screensaver(
            video_context_data, *bool_data))
      return true;
   return false;
}

bool video_context_driver_get_ident(gfx_ctx_ident_t *ident)
{
   if (!ident)
      return false;
   ident->ident = current_video_context.ident;
   return true;
}

bool video_context_driver_set_video_mode(gfx_ctx_mode_t *mode_info)
{
   video_frame_info_t video_info;

   if (!current_video_context.set_video_mode)
      return false;

   video_driver_build_info(&video_info);

   if (!current_video_context.set_video_mode(
            video_context_data, &video_info, mode_info->width,
            mode_info->height, mode_info->fullscreen))
      return false;
   return true;
}

bool video_context_driver_get_video_size(gfx_ctx_mode_t *mode_info)
{
   if (!current_video_context.get_video_size)
      return false;
   current_video_context.get_video_size(video_context_data,
         &mode_info->width, &mode_info->height);
   return true;
}

bool video_context_driver_get_context_data(void *data)
{
   if (!current_video_context.get_context_data)
      return false;
   *(void**)data = current_video_context.get_context_data(video_context_data);
   return true;
}

bool video_context_driver_show_mouse(bool *bool_data)
{
   if (!current_video_context.show_mouse)
      return false;
   current_video_context.show_mouse(video_context_data, *bool_data);
   return true;
}

void video_context_driver_set_data(void *data)
{
   video_context_data = data;
}

bool video_context_driver_get_flags(gfx_ctx_flags_t *flags)
{
   if (!flags)
      return false;
   if (!current_video_context.get_flags)
      return false;

   if (deferred_video_context_driver_set_flags)
   {
      flags->flags = deferred_flag_data.flags;
      deferred_video_context_driver_set_flags = false;
      return true;
   }

   flags->flags = current_video_context.get_flags(video_context_data);
   return true;
}

bool video_context_driver_set_flags(gfx_ctx_flags_t *flags)
{
   if (!flags)
      return false;
   if (!current_video_context.set_flags)
   {
      deferred_flag_data.flags = flags->flags;
      deferred_video_context_driver_set_flags = true;
      return false;
   }

   current_video_context.set_flags(video_context_data, flags->flags);
   return true;
}

enum gfx_ctx_api video_context_driver_get_api(void)
{
   return current_video_context_api;
}

bool video_driver_has_windowed(void)
{
#if defined(RARCH_CONSOLE) || defined(RARCH_MOBILE)
   return false;
#else
   if (video_driver_data && current_video->has_windowed)
      return current_video->has_windowed(video_driver_data);
   else if (video_context_data && current_video_context.has_windowed)
      return current_video_context.has_windowed(video_context_data);
   return false;
#endif
}

bool video_driver_cached_frame_has_valid_framebuffer(void)
{
   if (frame_cache_data)
      return (frame_cache_data == RETRO_HW_FRAME_BUFFER_VALID);
   return false;
}

static const shader_backend_t *video_shader_set_backend(enum rarch_shader_type type)
{
   switch (type)
   {
      case RARCH_SHADER_CG:
         {
#ifdef HAVE_CG
            gfx_ctx_flags_t flags;
            flags.flags = 0;
            video_context_driver_get_flags(&flags);

            if (BIT32_GET(flags.flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT))
            {
               RARCH_ERR("[Shader driver]: Cg cannot be used with core GL context. Trying to fall back to GLSL...\n");
               return video_shader_set_backend(RARCH_SHADER_GLSL);
            }

            RARCH_LOG("[Shader driver]: Using Cg shader backend.\n");
            return &gl_cg_backend;
#else
            break;
#endif
         }
      case RARCH_SHADER_GLSL:
#ifdef HAVE_GLSL
         RARCH_LOG("[Shader driver]: Using GLSL shader backend.\n");
         return &gl_glsl_backend;
#else
         break;
#endif
      case RARCH_SHADER_NONE:
      default:
         break;
   }

   return NULL;
}

bool video_shader_driver_get_prev_textures(video_shader_ctx_texture_t *texture)
{
   if (!texture)
      return false;
   texture->id = current_shader->get_prev_textures(shader_data);

   return true;
}

bool video_shader_driver_get_ident(video_shader_ctx_ident_t *ident)
{
   if (!ident)
      return false;
   ident->ident = current_shader->ident;
   return true;
}

bool video_shader_driver_get_current_shader(video_shader_ctx_t *shader)
{
   void *video_driver                       = video_driver_get_ptr(true);
   const video_poke_interface_t *video_poke = video_driver_get_poke();

   shader->data = NULL;
   if (!video_poke || !video_driver || !video_poke->get_current_shader)
      return false;
   shader->data = video_poke->get_current_shader(video_driver);
   return true;
}

bool video_shader_driver_direct_get_current_shader(video_shader_ctx_t *shader)
{
   shader->data = current_shader->get_current_shader(shader_data);

   return true;
}

bool video_shader_driver_deinit(void)
{
   if (!current_shader)
      return false;

   if (current_shader->deinit)
      current_shader->deinit(shader_data);

   shader_data    = NULL;
   current_shader = NULL;
   return true;
}

static enum gfx_wrap_type video_shader_driver_wrap_type_null(
      void *data, unsigned index)
{
   return RARCH_WRAP_BORDER;
}

static bool video_driver_cb_set_mvp(void *data,
      void *shader_data, const void *mat_data)
{
   video_shader_ctx_mvp_t mvp;
   mvp.data   = data;
   mvp.matrix = mat_data;

   video_driver_set_mvp(&mvp);
   return true;
}

static struct video_shader *video_shader_driver_get_current_shader_null(void *data)
{
   return NULL;
}

static void video_shader_driver_set_params_null(void *data, void *shader_data,
      unsigned width, unsigned height,
      unsigned tex_width, unsigned tex_height,
      unsigned out_width, unsigned out_height,
      unsigned frame_count,
      const void *info,
      const void *prev_info,
      const void *feedback_info,
      const void *fbo_info, unsigned fbo_info_cnt)
{
}

static void video_shader_driver_scale_null(void *data,
      unsigned idx, struct gfx_fbo_scale *scale)
{
   (void)idx;
   (void)scale;
}

static bool video_shader_driver_mipmap_input_null(void *data, unsigned idx)
{
   (void)idx;
   return false;
}

static bool video_shader_driver_filter_type_null(void *data, unsigned idx, bool *smooth)
{
   (void)idx;
   (void)smooth;
   return false;
}

static unsigned video_shader_driver_num_null(void *data)
{
   return 0;
}

static bool video_shader_driver_get_feedback_pass_null(void *data, unsigned *idx)
{
   (void)idx;
   return false;
}

static void video_shader_driver_reset_to_defaults(void)
{
   if (!current_shader)
      return;

   if (!current_shader->wrap_type)
      current_shader->wrap_type         = video_shader_driver_wrap_type_null;
   if (current_shader->set_mvp)
      video_driver_cb_shader_set_mvp    = current_shader->set_mvp;
   else
   {
      current_shader->set_mvp           = video_driver_cb_set_mvp;
      video_driver_cb_shader_set_mvp    = video_driver_cb_set_mvp;
   }
   if (!current_shader->set_coords)
      current_shader->set_coords        = video_driver_cb_set_coords;

   if (current_shader->use)
      video_driver_cb_shader_use        = current_shader->use;
   else
   {
      current_shader->use               = video_shader_driver_use_null;
      video_driver_cb_shader_use        = video_shader_driver_use_null;
   }
   if (!current_shader->set_params)
      current_shader->set_params        = video_shader_driver_set_params_null;
   if (!current_shader->shader_scale)
      current_shader->shader_scale      = video_shader_driver_scale_null;
   if (!current_shader->mipmap_input)
      current_shader->mipmap_input      = video_shader_driver_mipmap_input_null;
   if (!current_shader->filter_type)
      current_shader->filter_type       = video_shader_driver_filter_type_null;
   if (!current_shader->num_shaders)
      current_shader->num_shaders       = video_shader_driver_num_null;
   if (!current_shader->get_current_shader)
      current_shader->get_current_shader= video_shader_driver_get_current_shader_null;
   if (!current_shader->get_feedback_pass)
      current_shader->get_feedback_pass = video_shader_driver_get_feedback_pass_null;
}

/* Finds first suitable shader context driver. */
bool video_shader_driver_init_first(void)
{
   current_shader = (shader_backend_t*)shader_ctx_drivers[0];
   video_shader_driver_reset_to_defaults();
   return true;
}

bool video_shader_driver_init(video_shader_ctx_init_t *init)
{
   void *tmp = NULL;

   if (!init->shader || !init->shader->init)
   {
      init->shader = video_shader_set_backend(init->shader_type);

      if (!init->shader)
         return false;
   }

   tmp = init->shader->init(init->data, init->path);

   if (!tmp)
      return false;

   shader_data    = tmp;
   current_shader = (shader_backend_t*)init->shader;
   video_shader_driver_reset_to_defaults();

   return true;
}

bool video_shader_driver_get_feedback_pass(unsigned *data)
{
   return current_shader->get_feedback_pass(shader_data, data);
}

bool video_shader_driver_mipmap_input(unsigned *index)
{
   return current_shader->mipmap_input(shader_data, *index);
}

bool video_shader_driver_scale(video_shader_ctx_scale_t *scaler)
{
   if (!scaler || !scaler->scale)
      return false;

   scaler->scale->valid = false;

   current_shader->shader_scale(shader_data, scaler->idx, scaler->scale);
   return true;
}

bool video_shader_driver_info(video_shader_ctx_info_t *shader_info)
{
   if (!shader_info)
      return false;

   shader_info->num = current_shader->num_shaders(shader_data);

   return true;
}

bool video_shader_driver_filter_type(video_shader_ctx_filter_t *filter)
{
   if (filter)
      return current_shader->filter_type(shader_data,
            filter->index, filter->smooth);
   return false;
}

bool video_shader_driver_compile_program(
      struct shader_program_info *program_info)
{
   if (program_info)
      return current_shader->compile_program(program_info->data,
            program_info->idx, NULL, program_info);
   return false;
}

bool video_shader_driver_wrap_type(video_shader_ctx_wrap_t *wrap)
{
   wrap->type = current_shader->wrap_type(shader_data, wrap->idx);
   return true;
}

void video_driver_set_coords(video_shader_ctx_coords_t *coords)
{
   if (current_shader && current_shader->set_coords)
      current_shader->set_coords(coords->handle_data, shader_data, (const struct video_coords*)coords->data);
   else
   {
      if (video_driver_poke && video_driver_poke->set_coords)
         video_driver_poke->set_coords(coords->handle_data, shader_data, (const struct video_coords*)coords->data);
   }
}

void video_driver_set_mvp(video_shader_ctx_mvp_t *mvp)
{
   if (!mvp || !mvp->matrix)
      return;

   if (current_shader && current_shader->set_mvp)
      current_shader->set_mvp(mvp->data, shader_data, mvp->matrix);
   else
   {
      if (video_driver_poke && video_driver_poke->set_mvp)
         video_driver_poke->set_mvp(mvp->data, shader_data, mvp->matrix);
   }
}

bool renderchain_d3d_init_first(
      enum gfx_ctx_api api,
      const d3d_renderchain_driver_t **renderchain_driver,
      void **renderchain_handle)
{
   switch (api)
   {
      case GFX_CTX_DIRECT3D9_API:
#ifdef HAVE_D3D9
         {
            static const d3d_renderchain_driver_t *renderchain_d3d_drivers[] = {
#if defined(_WIN32) && defined(HAVE_CG)
               &cg_d3d9_renderchain,
#endif
#if defined(_WIN32) && defined(HAVE_HLSL)
               &hlsl_d3d9_renderchain,
#endif
               &null_d3d_renderchain,
               NULL
            };
            unsigned i;

            for (i = 0; renderchain_d3d_drivers[i]; i++)
            {
               void *data = renderchain_d3d_drivers[i]->chain_new();

               if (!data)
                  continue;

               *renderchain_driver = renderchain_d3d_drivers[i];
               *renderchain_handle = data;
               return true;
            }
         }
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

bool renderchain_gl_init_first(
      const gl_renderchain_driver_t **renderchain_driver,
      void **renderchain_handle)
{
   unsigned i;

   for (i = 0; renderchain_gl_drivers[i]; i++)
   {
      void *data = renderchain_gl_drivers[i]->chain_new();

      if (!data)
         continue;

      *renderchain_driver = renderchain_gl_drivers[i];
      *renderchain_handle = data;
      return true;
   }

   return false;
}

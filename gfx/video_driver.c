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

#include <stdlib.h>
#include <math.h>

#include <retro_inline.h>
#include <string/stdstring.h>
#include <retro_math.h>
#include <retro_timers.h>

#ifdef HAVE_CONFIG_H
#include "../config.h"
#endif

#include <gfx/video_frame.h>

#include "../config.def.h"

#include "video_driver.h"
#include "video_filter.h"
#include "video_display_server.h"

#include "gfx_animation.h"
#ifdef HAVE_GFX_WIDGETS
#include "gfx_widgets.h"
#endif

#ifdef HAVE_THREADS
#include "video_thread_wrapper.h"
#endif

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

#ifdef _WIN32
#include "common/win32_common.h"
#endif

#ifdef __WINRT__
#include "../uwp/uwp_func.h"
#endif

#include "../audio/audio_driver.h"
#include "../frontend/frontend_driver.h"
#include "../record/record_driver.h"
#include "../ui/ui_companion_driver.h"
#include "../driver.h"
#include "../file_path_special.h"
#include "../list_special.h"
#include "../retroarch.h"
#include "../verbosity.h"

#define TIME_TO_FPS(last_time, new_time, frames) ((1000000.0f * (frames)) / ((new_time) - (last_time)))

#define FRAME_DELAY_AUTO_DEBUG 0
#define FRAME_REST_DEBUG 0

typedef struct
{
   struct string_list *list;
   enum gfx_ctx_api api;
} gfx_api_gpu_map;

static gfx_api_gpu_map gpu_map[] = {
   { NULL,                   GFX_CTX_VULKAN_API     },
   { NULL,                   GFX_CTX_DIRECT3D10_API },
   { NULL,                   GFX_CTX_DIRECT3D11_API },
   { NULL,                   GFX_CTX_DIRECT3D12_API }
};

static const video_display_server_t dispserv_null = {
   NULL, /* init */
   NULL, /* destroy */
   NULL, /* set_window_opacity */
   NULL, /* set_window_progress */
   NULL, /* set_window_decorations */
   NULL, /* set_resolution */
   NULL, /* get_resolution_list */
   NULL, /* get_output_options */
   NULL, /* set_screen_orientation */
   NULL, /* get_screen_orientation */
   NULL, /* get_flags */
   "null"
};

static const gfx_ctx_driver_t *gfx_ctx_gl_drivers[] = {
#if defined(ORBIS)
   &orbis_ctx,
#endif
#if defined(HAVE_VITAGL) | defined(HAVE_VITAGLES)
   &vita_ctx,
#endif
#if defined(__PS3__)
   &gfx_ctx_ps3,
#endif
#if defined(HAVE_LIBNX) && defined(HAVE_OPENGL)
   &switch_ctx,
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
#if defined(_WIN32) && !defined(__WINRT__) && (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE))
   &gfx_ctx_wgl,
#endif
#if defined(__WINRT__) && defined(HAVE_OPENGLES)
   &gfx_ctx_uwp,
#endif
#if defined(HAVE_WAYLAND)
   &gfx_ctx_wayland,
#endif
#if defined(HAVE_X11) && !defined(HAVE_OPENGLES)
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE)
   &gfx_ctx_x,
#endif
#endif
#if defined(HAVE_X11) && defined(HAVE_OPENGL) && defined(HAVE_EGL)
   &gfx_ctx_x_egl,
#endif
#if defined(HAVE_KMS)
#if defined(HAVE_ODROIDGO2)
   &gfx_ctx_go2_drm,
#endif
   &gfx_ctx_drm,
#endif
#if defined(ANDROID)
   &gfx_ctx_android,
#endif
#if defined(__QNX__)
   &gfx_ctx_qnx,
#endif
#if defined(HAVE_COCOA) || defined(HAVE_COCOATOUCH) || defined(HAVE_COCOA_METAL)
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   &gfx_ctx_cocoagl,
#endif
#endif
#if (defined(HAVE_SDL) || defined(HAVE_SDL2)) && (defined(HAVE_OPENGL) || defined(HAVE_OPENGL1) || defined(HAVE_OPENGL_CORE))
   &gfx_ctx_sdl_gl,
#endif
#ifdef HAVE_OSMESA
   &gfx_ctx_osmesa,
#endif
#ifdef EMSCRIPTEN
   &gfx_ctx_emscripten,
#endif
   &gfx_ctx_null,
   NULL
};

/* Beware when changing this - it must match with enum aspect_ratio order */
struct aspect_ratio_elem aspectratio_lut[ASPECT_RATIO_END] = {
   { 4.0f / 3.0f  , "4:3"           },
   { 16.0f / 9.0f , "16:9"          },
   { 16.0f / 10.0f, "16:10"         },
   { 16.0f / 15.0f, "16:15"         },
   { 21.0f / 9.0f , "21:9"          },
   { 1.0f / 1.0f  , "1:1"           },
   { 2.0f / 1.0f  , "2:1"           },
   { 3.0f / 2.0f  , "3:2"           },
   { 3.0f / 4.0f  , "3:4"           },
   { 4.0f / 1.0f  , "4:1"           },
   { 9.0f / 16.0f , "9:16"          },
   { 5.0f / 4.0f  , "5:4"           },
   { 6.0f / 5.0f  , "6:5"           },
   { 7.0f / 9.0f  , "7:9"           },
   { 8.0f / 3.0f  , "8:3"           },
   { 8.0f / 7.0f  , "8:7"           },
   { 19.0f / 12.0f, "19:12"         },
   { 19.0f / 14.0f, "19:14"         },
   { 30.0f / 17.0f, "30:17"         },
   { 32.0f / 9.0f , "32:9"          },
   { 0.0f         , ""              }, /* config -        initialized in video_driver_init_internal */
   { 1.0f         , ""              }, /* square pixel -  initialized in video_driver_set_viewport_square_pixel */
   { 1.0f         , ""              }, /* core provided - initialized in video_driver_init_internal */
   { 0.0f         , ""              }, /* custom -        initialized in video_driver_init_internal */
   { 4.0f / 3.0f  , ""              }  /* full -          initialized in video_driver_init_internal */
};

static INLINE bool realloc_checked(void **ptr, size_t size)
{
   void *nptr = NULL;

   if (*ptr)
      nptr = realloc(*ptr, size);
   else
      nptr = malloc(size);

   if (nptr)
      *ptr = nptr;

   return *ptr == nptr;
}

static bool video_coord_array_resize(video_coord_array_t *ca,
   unsigned cap)
{
   size_t base_size    = sizeof(float) * cap;

   if (!realloc_checked((void**)&ca->coords.vertex,
            2 * base_size))
      return false;
   if (!realloc_checked((void**)&ca->coords.color,
            4 * base_size))
      return false;
   if (!realloc_checked((void**)&ca->coords.tex_coord,
            2 * base_size))
      return false;
   if (!realloc_checked((void**)&ca->coords.lut_tex_coord,
            2 * base_size))
      return false;

   ca->allocated = cap;

   return true;
}

bool video_coord_array_append(video_coord_array_t *ca,
      const video_coords_t *coords, unsigned count)
{
   size_t base_size, offset;
   count          = MIN(count, coords->vertices);

   if (ca->coords.vertices + count >= ca->allocated)
   {
      unsigned cap = next_pow2(ca->coords.vertices + count);
      if (!video_coord_array_resize(ca, cap))
         return false;
   }

   base_size = count * sizeof(float);
   offset    = ca->coords.vertices;

   /* XXX: I wish we used interlaced arrays so
    * we could call memcpy only once. */
   memcpy(ca->coords.vertex        + offset * 2,
         coords->vertex, base_size * 2);

   memcpy(ca->coords.color         + offset * 4,
         coords->color, base_size * 4);

   memcpy(ca->coords.tex_coord     + offset * 2,
         coords->tex_coord, base_size * 2);

   memcpy(ca->coords.lut_tex_coord + offset * 2,
         coords->lut_tex_coord, base_size * 2);

   ca->coords.vertices += count;

   return true;
}

void video_coord_array_free(video_coord_array_t *ca)
{
   if (!ca->allocated)
      return;

   if (ca->coords.vertex)
      free(ca->coords.vertex);
   ca->coords.vertex        = NULL;

   if (ca->coords.color)
      free(ca->coords.color);
   ca->coords.color         = NULL;

   if (ca->coords.tex_coord)
      free(ca->coords.tex_coord);
   ca->coords.tex_coord     = NULL;

   if (ca->coords.lut_tex_coord)
      free(ca->coords.lut_tex_coord);
   ca->coords.lut_tex_coord = NULL;

   ca->coords.vertices      = 0;
   ca->allocated            = 0;
}

static void *video_null_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   *input      = NULL;
   *input_data = NULL;

   frontend_driver_install_signal_handler();

   return (void*)-1;
}

static bool video_null_frame(void *a, const void *b, unsigned c, unsigned d,
uint64_t e,
unsigned f, const char *g, video_frame_info_t *h) { return true; }
static void video_null_free(void *a) { }
static void video_null_set_nonblock_state(void *a, bool b, bool c, unsigned d) { }
static bool video_null_alive(void *a) { return frontend_driver_get_signal_handler_state() != 1; }
static bool video_null_focus(void *a) { return true; }
static bool video_null_has_windowed(void *a) { return true; }
static bool video_null_suppress_screensaver(void *a, bool b) { return false; }
static bool video_null_set_shader(void *a, enum rarch_shader_type b, const char
*c) { return false; }

video_driver_t video_null = {
   video_null_init,
   video_null_frame,
   video_null_set_nonblock_state,
   video_null_alive,
   video_null_focus,
   video_null_suppress_screensaver,
   video_null_has_windowed,
   video_null_set_shader,
   video_null_free,
   "null",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   NULL, /* viewport_info */
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */

#ifdef HAVE_OVERLAY
  NULL, /* overlay_interface */
#endif
  NULL, /* get_poke_interface */
};

const video_driver_t *video_drivers[] = {
#ifdef HAVE_GCM
   &video_gcm,
#endif
#ifdef HAVE_VITA2D
   &video_vita2d,
#endif
#ifdef HAVE_VULKAN
   &video_vulkan,
#endif
#if defined(HAVE_OPENGL_CORE)
   &video_gl3,
#endif
#ifdef HAVE_OPENGL
   &video_gl2,
#endif
#ifdef HAVE_OPENGL1
   &video_gl1,
#endif
#ifdef HAVE_METAL
   &video_metal,
#endif
#ifdef XENON
   &video_xenon360,
#endif
#if defined(HAVE_D3D12)
   &video_d3d12,
#endif
#if defined(HAVE_D3D11)
   &video_d3d11,
#endif
#if defined(HAVE_D3D10)
   &video_d3d10,
#endif
#if defined(HAVE_D3D9)
#if defined(HAVE_HLSL)
   &video_d3d9_hlsl,
#endif
#if defined(HAVE_CG)
   &video_d3d9_cg,
#endif
#endif
#if defined(HAVE_D3D8)
   &video_d3d8,
#endif
#ifdef PSP
   &video_psp1,
#endif
#ifdef PS2
   &video_ps2,
#endif
#ifdef _3DS
   &video_ctr,
#endif
#ifdef SWITCH
   &video_switch,
#endif
#ifdef HAVE_ODROIDGO2
   &video_oga,
#endif
#if defined(HAVE_SDL) && !defined(HAVE_SDL_DINGUX)
   &video_sdl,
#endif
#ifdef HAVE_SDL2
   &video_sdl2,
#endif
#ifdef HAVE_SDL_DINGUX
#if defined(RS90) || defined(MIYOO)
   &video_sdl_rs90,
#else
   &video_sdl_dingux,
#endif
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
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
#ifdef HAVE_GDI
   &video_gdi,
#endif
#endif
#ifdef DJGPP
   &video_vga,
#endif
#ifdef HAVE_FPGA
   &video_fpga,
#endif
#ifdef HAVE_SIXEL
   &video_sixel,
#endif
#ifdef HAVE_CACA
   &video_caca,
#endif
#ifdef HAVE_NETWORK_VIDEO
   &video_network,
#endif
   &video_null,
   NULL,
};

static video_driver_state_t video_driver_st = { 0 };
static const video_display_server_t *current_display_server =
&dispserv_null;

struct retro_hw_render_callback *video_driver_get_hw_context(void)
{
   video_driver_state_t *video_st         = &video_driver_st;
   return VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st);
}

video_driver_state_t *video_state_get_ptr(void)
{
   return &video_driver_st;
}

#ifdef HAVE_THREADS
void *video_thread_get_ptr(video_driver_state_t *video_st)
{
   const thread_video_t *thr   = (const thread_video_t*)video_st->data;
   if (thr)
      return thr->driver_data;
   return NULL;
}
#endif

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
   video_driver_state_t *video_st         = &video_driver_st;
   return VIDEO_DRIVER_GET_PTR_INTERNAL(video_st);
}


video_driver_t *hw_render_context_driver(
      enum retro_hw_context_type type, int major, int minor)
{
   switch (type)
   {
      case RETRO_HW_CONTEXT_OPENGL_CORE:
#ifdef HAVE_OPENGL_CORE
         return &video_gl3;
#else
         break;
#endif
      case RETRO_HW_CONTEXT_OPENGL:
#ifdef HAVE_OPENGL
         return &video_gl2;
#else
         break;
#endif
      case RETRO_HW_CONTEXT_D3D10:
#if defined(HAVE_D3D10)
         return &video_d3d10;
#else
         break;
#endif
      case RETRO_HW_CONTEXT_D3D11:
#if defined(HAVE_D3D11)
         return &video_d3d11;
#else
         break;
#endif
      case RETRO_HW_CONTEXT_D3D12:
#if defined(HAVE_D3D12)
         return &video_d3d12;
#else
         break;
#endif
      case RETRO_HW_CONTEXT_D3D9:
#if defined(HAVE_D3D9) && defined(HAVE_HLSL)
         return &video_d3d9_hlsl;
#else
         break;
#endif
      case RETRO_HW_CONTEXT_VULKAN:
#if defined(HAVE_VULKAN)
         return &video_vulkan;
#else
         break;
#endif
      default:
      case RETRO_HW_CONTEXT_NONE:
         break;
   }

   return NULL;
}

const char *hw_render_context_name(
      enum retro_hw_context_type type, int major, int minor)
{
#ifdef HAVE_OPENGL_CORE
   if (type == RETRO_HW_CONTEXT_OPENGL_CORE)
      return "glcore";
#endif
#ifdef HAVE_OPENGL
   switch (type)
   {
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
      case RETRO_HW_CONTEXT_OPENGL:
#ifndef HAVE_OPENGL_CORE
      case RETRO_HW_CONTEXT_OPENGL_CORE:
#endif
         return "gl";
      default:
         break;
   }
#endif
#ifdef HAVE_VULKAN
   if (type == RETRO_HW_CONTEXT_VULKAN)
      return "vulkan";
#endif
#if defined(HAVE_D3D9) && defined(HAVE_HLSL)
   if (type == RETRO_HW_CONTEXT_D3D9)
      return "d3d9_hlsl";
#endif
#ifdef HAVE_D3D10
   if (type == RETRO_HW_CONTEXT_D3D10)
      return "d3d10";
#endif
#ifdef HAVE_D3D11
   if (type == RETRO_HW_CONTEXT_D3D11)
      return "d3d11";
#endif
#ifdef HAVE_D3D12
   if (type == RETRO_HW_CONTEXT_D3D12)
      return "d3d12";
#endif
   return "N/A";
}

static enum retro_hw_context_type hw_render_context_type(const char *s)
{
#ifdef HAVE_OPENGL_CORE
   if (string_is_equal(s, "glcore"))
      return RETRO_HW_CONTEXT_OPENGL_CORE;
#endif
#ifdef HAVE_OPENGL
   if (string_is_equal(s, "gl"))
      return RETRO_HW_CONTEXT_OPENGL;
#endif
#ifdef HAVE_VULKAN
   if (string_is_equal(s, "vulkan"))
      return RETRO_HW_CONTEXT_VULKAN;
#endif
#if defined(HAVE_D3D9) && defined(HAVE_HLSL)
   if (string_is_equal(s, "d3d9_hlsl"))
      return RETRO_HW_CONTEXT_D3D9;
#endif
#ifdef HAVE_D3D10
   if (string_is_equal(s, "d3d10"))
      return RETRO_HW_CONTEXT_D3D10;
#endif
#ifdef HAVE_D3D11
   if (string_is_equal(s, "d3d11"))
      return RETRO_HW_CONTEXT_D3D11;
#endif
#ifdef HAVE_D3D12
   if (string_is_equal(s, "d3d12"))
      return RETRO_HW_CONTEXT_D3D12;
#endif
   return RETRO_HW_CONTEXT_NONE;
}

/* string list stays owned by the caller and must be available at
 * all times after the video driver is inited */
void video_driver_set_gpu_api_devices(
      enum gfx_ctx_api api, struct string_list *list)
{
   int i;

   for (i = 0; i < (int)ARRAY_SIZE(gpu_map); i++)
   {
      if (api == gpu_map[i].api)
      {
         gpu_map[i].list = list;
         break;
      }
   }
}

struct string_list* video_driver_get_gpu_api_devices(enum gfx_ctx_api api)
{
   int i;

   for (i = 0; i < (int)ARRAY_SIZE(gpu_map); i++)
   {
      if (api == gpu_map[i].api)
         return gpu_map[i].list;
   }

   return NULL;
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
      struct video_viewport *vp,
      int mouse_x,           int mouse_y,
      int16_t *res_x,        int16_t *res_y,
      int16_t *res_screen_x, int16_t *res_screen_y)
{
   int norm_vp_width         = (int)vp->width;
   int norm_vp_height        = (int)vp->height;
   int norm_full_vp_width    = (int)vp->full_width;
   int norm_full_vp_height   = (int)vp->full_height;
   int scaled_screen_x       = -0x8000; /* OOB */
   int scaled_screen_y       = -0x8000; /* OOB */
   int scaled_x              = -0x8000; /* OOB */
   int scaled_y              = -0x8000; /* OOB */
   if (   (norm_vp_width       <= 0)
       || (norm_vp_height      <= 0)
       || (norm_full_vp_width  <= 0)
       || (norm_full_vp_height <= 0))
      return false;

   if (mouse_x >= 0 && mouse_x <= norm_full_vp_width)
      scaled_screen_x = ((2 * mouse_x * 0x7fff)
            / norm_full_vp_width)  - 0x7fff;

   if (mouse_y >= 0 && mouse_y <= norm_full_vp_height)
      scaled_screen_y = ((2 * mouse_y * 0x7fff)
            / norm_full_vp_height) - 0x7fff;

   mouse_x           -= vp->x;
   mouse_y           -= vp->y;

   if (mouse_x >= 0 && mouse_x <= norm_vp_width)
      scaled_x        = ((2 * mouse_x * 0x7fff)
            / norm_vp_width) - 0x7fff;
   else
      scaled_x        = -0x8000; /* OOB */

   if (mouse_y >= 0 && mouse_y <= norm_vp_height)
      scaled_y        = ((2 * mouse_y * 0x7fff)
            / norm_vp_height) - 0x7fff;

   *res_x             = scaled_x;
   *res_y             = scaled_y;
   *res_screen_x      = scaled_screen_x;
   *res_screen_y      = scaled_screen_y;
   return true;
}

void video_monitor_compute_fps_statistics(uint64_t
      frame_time_count)
{
   double        avg_fps       = 0.0;
   double        stddev        = 0.0;
   unsigned        samples     = 0;

   if (frame_time_count <
         (2 * MEASURE_FRAME_TIME_SAMPLES_COUNT))
   {
      RARCH_LOG(
            "[Video]: Does not have enough samples for monitor refresh rate"
            " estimation. Requires to run for at least %u frames.\n",
            2 * MEASURE_FRAME_TIME_SAMPLES_COUNT);
      return;
   }

   if (video_monitor_fps_statistics(&avg_fps, &stddev, &samples))
   {
      RARCH_LOG("[Video]: Average monitor Hz: %.6f Hz. (%.3f %% frame time"
            " deviation, based on %u last samples).\n",
            avg_fps, 100.0f * stddev, samples);
   }
}

void video_monitor_set_refresh_rate(float hz)
{
   char msg[256];
   char rate[8];
   settings_t        *settings = config_get_ptr();

   /* Avoid message spamming if there is no change. */
   if (settings->floats.video_refresh_rate == hz)
      return;

   snprintf(rate, sizeof(rate), "%.3f", hz);
   snprintf(msg, sizeof(msg),
      msg_hash_to_str(MSG_VIDEO_REFRESH_RATE_CHANGED), rate);

   /* Message is visible for twice the usual duration */
   /* as modeswitch will cause monitors to go blank for a while */
   if (settings->bools.notification_show_refresh_rate)
      runloop_msg_queue_push(msg, 1, 360, false, NULL,
            MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   RARCH_LOG("[Video]: %s\n", msg);

   configuration_set_float(settings,
         settings->floats.video_refresh_rate,
         hz);
}

void video_driver_force_fallback(const char *driver)
{
   settings_t *settings        = config_get_ptr();
   ui_msg_window_t *msg_window = NULL;

   configuration_set_string(settings,
         settings->arrays.video_driver,
         driver);

   command_event(CMD_EVENT_MENU_SAVE_CURRENT_CONFIG, NULL);

#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__) && !defined(WINAPI_FAMILY)
   /* UI companion driver is not inited yet, just call into it directly */
   msg_window = &ui_msg_window_win32;
#endif

   if (msg_window)
   {
      char text[128];
      ui_msg_window_state window_state;
      char *title          = strdup(msg_hash_to_str(MSG_ERROR));

      text[0]              = '\0';

      snprintf(text, sizeof(text),
            msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_DRIVER_FALLBACK),
            driver);

      window_state.buttons = UI_MSG_WINDOW_OK;
      window_state.text    = strdup(text);
      window_state.title   = title;
      window_state.window  = NULL;

      msg_window->error(&window_state);

      free(title);
   }
   exit(1);
}

static bool get_metrics_null(void *data, enum display_metric_types type,
      float *value) { return false; }

static bool video_context_driver_get_metrics_null(
      void *data, enum display_metric_types type,
      float *value) { return false; }

void video_context_driver_destroy(gfx_ctx_driver_t *ctx_driver)
{
   if (!ctx_driver)
      return;

   ctx_driver->init                       = NULL;
   ctx_driver->bind_api                   = NULL;
   ctx_driver->swap_interval              = NULL;
   ctx_driver->set_video_mode             = NULL;
   ctx_driver->get_video_size             = NULL;
   ctx_driver->get_video_output_size      = NULL;
   ctx_driver->get_video_output_prev      = NULL;
   ctx_driver->get_video_output_next      = NULL;
   ctx_driver->get_metrics                =
      video_context_driver_get_metrics_null;
   ctx_driver->translate_aspect           = NULL;
   ctx_driver->update_window_title        = NULL;
   ctx_driver->check_window               = NULL;
   ctx_driver->set_resize                 = NULL;
   ctx_driver->suppress_screensaver       = NULL;
   ctx_driver->swap_buffers               = NULL;
   ctx_driver->input_driver               = NULL;
   ctx_driver->get_proc_address           = NULL;
   ctx_driver->image_buffer_init          = NULL;
   ctx_driver->image_buffer_write         = NULL;
   ctx_driver->show_mouse                 = NULL;
   ctx_driver->ident                      = NULL;
   ctx_driver->get_flags                  = NULL;
   ctx_driver->set_flags                  = NULL;
   ctx_driver->bind_hw_render             = NULL;
   ctx_driver->get_context_data           = NULL;
   ctx_driver->make_current               = NULL;
}

const gfx_ctx_driver_t *video_context_driver_init(
      bool core_set_shared_context,
      settings_t *settings,
      void *data,
      const gfx_ctx_driver_t *ctx,
      const char *ident,
      enum gfx_ctx_api api, unsigned major,
      unsigned minor, bool hw_render_ctx,
      void **ctx_data)
{
   if (!ctx->bind_api(data, api, major, minor))
   {
      RARCH_WARN("Failed to bind API (#%u, version %u.%u)"
            " on context driver \"%s\".\n",
            (unsigned)api, major, minor, ctx->ident);

      return NULL;
   }

   if (!(*ctx_data = ctx->init(data)))
      return NULL;

   if (ctx->bind_hw_render)
   {
      bool  video_shared_context  =
         settings->bools.video_shared_context || core_set_shared_context;

      ctx->bind_hw_render(*ctx_data,
            video_shared_context && hw_render_ctx);
   }

   return ctx;
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

void video_driver_pixel_converter_free(video_pixel_scaler_t *scalr)
{
   if (!scalr)
      return;

   if (scalr->scaler)
   {
      scaler_ctx_gen_reset(scalr->scaler);
      free(scalr->scaler);
   }
   if (scalr->scaler_out)
      free(scalr->scaler_out);

   scalr->scaler     = NULL;
   scalr->scaler_out = NULL;

   free(scalr);
}

video_pixel_scaler_t *video_driver_pixel_converter_init(
      const enum retro_pixel_format video_driver_pix_fmt,
      struct retro_hw_render_callback *hwr,
      unsigned size)
{
   void *scalr_out                      = NULL;
   video_pixel_scaler_t          *scalr = NULL;
   struct scaler_ctx        *scalr_ctx  = NULL;

   /* If pixel format is not 0RGB1555, we don't need to do
    * any internal pixel conversion. */
   if (video_driver_pix_fmt != RETRO_PIXEL_FORMAT_0RGB1555)
      return NULL;

   /* No need to perform pixel conversion for HW rendering contexts. */
   if (hwr && hwr->context_type != RETRO_HW_CONTEXT_NONE)
      return NULL;

   RARCH_WARN("[Video]: 0RGB1555 pixel format is deprecated,"
         " and will be slower. For 15/16-bit, RGB565"
         " format is preferred.\n");

   if (!(scalr = (video_pixel_scaler_t*)malloc(sizeof(*scalr))))
      goto error;

   scalr->scaler                            = NULL;
   scalr->scaler_out                        = NULL;

   if (!(scalr_ctx = (struct scaler_ctx*)calloc(1, sizeof(*scalr_ctx))))
      goto error;

   scalr->scaler                            = scalr_ctx;
   scalr->scaler->scaler_type               = SCALER_TYPE_POINT;
   scalr->scaler->in_fmt                    = SCALER_FMT_0RGB1555;
   /* TODO/FIXME: Pick either ARGB8888 or RGB565 depending on driver. */
   scalr->scaler->out_fmt                   = SCALER_FMT_RGB565;

   if (!scaler_ctx_gen_filter(scalr_ctx))
      goto error;

   if (!(scalr_out = calloc(sizeof(uint16_t), size * size)))
      goto error;

   scalr->scaler_out                        = scalr_out;

   return scalr;

error:
   video_driver_pixel_converter_free(scalr);
#ifdef HAVE_VIDEO_FILTER
   video_driver_filter_free();
#endif
   return NULL;
}

void video_driver_gpu_record_deinit(void)
{
   video_driver_state_t *video_st = &video_driver_st;
   if (video_st->record_gpu_buffer)
      free(video_st->record_gpu_buffer);
   video_st->record_gpu_buffer = NULL;
}

void recording_dump_frame(
      const void *data, unsigned width,
      unsigned height, size_t pitch, bool is_idle)
{
   struct record_video_data ffemu_data;
   video_driver_state_t *video_st   = &video_driver_st;
   recording_state_t *record_st     = recording_state_get_ptr();

   ffemu_data.data     = data;
   ffemu_data.width    = width;
   ffemu_data.height   = height;
   ffemu_data.pitch    = (int)pitch;
   ffemu_data.is_dupe  = false;

   if (video_st->record_gpu_buffer)
   {
      struct video_viewport vp;

      vp.x                        = 0;
      vp.y                        = 0;
      vp.width                    = 0;
      vp.height                   = 0;
      vp.full_width               = 0;
      vp.full_height              = 0;

      if (video_st->current_video && video_st->current_video->viewport_info)
         video_st->current_video->viewport_info(video_st->data, &vp);

      if (!vp.width || !vp.height)
      {
         RARCH_WARN("[Recording]: %s\n",
               msg_hash_to_str(MSG_VIEWPORT_SIZE_CALCULATION_FAILED));
         video_driver_gpu_record_deinit();
         recording_dump_frame(
               data, width, height, pitch, is_idle);
         return;
      }

      /* User has resized. We kinda have a problem now. */
      if (     (vp.width  != record_st->gpu_width)
            || (vp.height != record_st->gpu_height))
      {
         const char *recording_failed_str =
            msg_hash_to_str(MSG_RECORDING_TERMINATED_DUE_TO_RESIZE);
         RARCH_WARN("[Recording]: %s\n", recording_failed_str);

         runloop_msg_queue_push(recording_failed_str,
               1, 180, true,
               NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
         command_event(CMD_EVENT_RECORD_DEINIT, NULL);
         return;
      }

      /* Big bottleneck.
       * Since we might need to do read-backs asynchronously,
       * it might take 3-4 times before this returns true. */
      if (!(      video_st->current_video->read_viewport
               && video_st->current_video->read_viewport(
                  video_st->data, video_st->record_gpu_buffer, is_idle)))
         return;

      ffemu_data.pitch  = (int)(record_st->gpu_width * 3);
      ffemu_data.width  = (unsigned)record_st->gpu_width;
      ffemu_data.height = (unsigned)record_st->gpu_height;
      ffemu_data.data   = video_st->record_gpu_buffer + (ffemu_data.height - 1) * ffemu_data.pitch;

      ffemu_data.pitch  = -ffemu_data.pitch;
   }
   else
      ffemu_data.is_dupe = !data;

   record_st->driver->push_video(record_st->data, &ffemu_data);
}

const char *video_display_server_get_ident(void)
{
   if (current_display_server)
      return current_display_server->ident;
   return FILE_PATH_UNKNOWN;
}

void* video_display_server_init(enum rarch_display_type type)
{
   video_driver_state_t *video_st = &video_driver_st;
   video_display_server_destroy();

   switch (type)
   {
      case RARCH_DISPLAY_WIN32:
#if defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)
         current_display_server = &dispserv_win32;
#endif
         break;
      case RARCH_DISPLAY_X11:
#if defined(HAVE_X11)
         current_display_server = &dispserv_x11;
#endif
         break;
      case RARCH_DISPLAY_KMS:
#if defined(HAVE_KMS)
         current_display_server = &dispserv_kms;
#endif
         break;
      default:
#if defined(ANDROID)
         current_display_server = &dispserv_android;
#else
         current_display_server = &dispserv_null;
#endif
         break;
   }

   if (current_display_server)
   {
      if (current_display_server->init)
         video_st->current_display_server_data = current_display_server->init();

      if (!string_is_empty(current_display_server->ident))
      {
         RARCH_LOG("[Video]: Found display server: \"%s\".\n",
               current_display_server->ident);
      }
   }

   video_st->initial_screen_orientation =
      video_display_server_get_screen_orientation();
   video_st->current_screen_orientation =
      video_st->initial_screen_orientation;

   return video_st->current_display_server_data;
}

void video_display_server_destroy(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   const enum rotation initial_screen_orientation = video_st->initial_screen_orientation;
   const enum rotation current_screen_orientation = video_st->current_screen_orientation;

   if (initial_screen_orientation != current_screen_orientation)
      video_display_server_set_screen_orientation(initial_screen_orientation);

   if (current_display_server)
      if (video_st->current_display_server_data)
         current_display_server->destroy(video_st->current_display_server_data);
}

bool video_display_server_set_window_opacity(unsigned opacity)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (current_display_server && current_display_server->set_window_opacity)
      return current_display_server->set_window_opacity(
            video_st->current_display_server_data, opacity);
   return false;
}

bool video_display_server_set_window_progress(int progress, bool finished)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (current_display_server && current_display_server->set_window_progress)
      return current_display_server->set_window_progress(
            video_st->current_display_server_data, progress, finished);
   return false;
}

bool video_display_server_set_window_decorations(bool on)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (current_display_server && current_display_server->set_window_decorations)
      return current_display_server->set_window_decorations(
            video_st->current_display_server_data, on);
   return false;
}

bool video_display_server_set_resolution(unsigned width, unsigned height,
      int int_hz, float hz, int center, int monitor_index, int xoffset, int padjust)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   RARCH_DBG("[Video]: Display server set resolution, hz: %f\n",hz);
   if (current_display_server && current_display_server->set_resolution)
      return current_display_server->set_resolution(
            video_st->current_display_server_data, width, height, int_hz,
            hz, center, monitor_index, xoffset, padjust);
   return false;
}

bool video_display_server_has_resolution_list(void)
{
   return (current_display_server
         && current_display_server->get_resolution_list);
}

void *video_display_server_get_resolution_list(unsigned *size)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (video_display_server_has_resolution_list())
      return current_display_server->get_resolution_list(
            video_st->current_display_server_data, size);
   return NULL;
}

bool video_display_server_has_refresh_rate(float hz)
{
   unsigned i, size            = 0;
   bool rate_exists            = false;

   struct video_display_config *video_list = (struct video_display_config*)
         video_display_server_get_resolution_list(&size);

   if (video_list)
   {
      video_driver_state_t *video_st = &video_driver_st;
      unsigned video_driver_width    = video_st->width;
      unsigned video_driver_height   = video_st->height;

      for (i = 0; i < size && !rate_exists; i++)
      {
         if (   (video_list[i].width       == video_driver_width)
             && (video_list[i].height      == video_driver_height)
             && (video_list[i].refreshrate == floor(hz)))
            rate_exists = true;
      }

      free(video_list);
   }

   return rate_exists;
}

void video_switch_refresh_rate_maybe(
      float *refresh_rate_suggest,
      bool *video_switch_refresh_rate)
{
   settings_t *settings               = config_get_ptr();
   video_driver_state_t *video_st     = &video_driver_st;

   float refresh_rate                 = *refresh_rate_suggest;
   float video_refresh_rate           = settings->floats.video_refresh_rate;
   float pal_threshold                = settings->floats.video_autoswitch_pal_threshold;
   unsigned crt_switch_resolution     = settings->uints.crt_switch_resolution;
   unsigned autoswitch_refresh_rate   = settings->uints.video_autoswitch_refresh_rate;
   unsigned video_swap_interval       = runloop_get_video_swap_interval(
         settings->uints.video_swap_interval);
   unsigned video_bfi                 = settings->uints.video_black_frame_insertion;
   bool vrr_runloop_enable            = settings->bools.vrr_runloop_enable;
   bool exclusive_fullscreen          = settings->bools.video_fullscreen && !settings->bools.video_windowed_fullscreen;
   bool windowed_fullscreen           = settings->bools.video_fullscreen && settings->bools.video_windowed_fullscreen;
   bool all_fullscreen                = settings->bools.video_fullscreen || settings->bools.video_windowed_fullscreen;

   /* Roundings to PAL & NTSC standards */
   if      (refresh_rate > 49.00 && refresh_rate <= pal_threshold)
      refresh_rate       = 50.00f;
   else if (refresh_rate > 54.00 && refresh_rate < 60.00)
      refresh_rate       = 59.94f;
   else if (refresh_rate > 60.00 && refresh_rate < 61.00)
      refresh_rate       = 60.00f;

   /* Black frame insertion + swap interval multiplier */
   refresh_rate          = (refresh_rate * (video_bfi + 1.0f) * video_swap_interval);

   /* Fallback when target refresh rate is not exposed or when below standards */
   if (!video_display_server_has_refresh_rate(refresh_rate) || refresh_rate < 50)
      refresh_rate       = video_refresh_rate;

   /* Replace target rate */
   *refresh_rate_suggest = refresh_rate;

   /* Try to switch display rate for the desired screen mode(s) when:
    * - Not already at correct rate
    * - 'CRT SwitchRes' OFF & 'Sync to Exact Content Framerate' OFF
    * - Automatic refresh rate switching not OFF
    */
    if (   (refresh_rate != video_refresh_rate)
        && !crt_switch_resolution
        && !vrr_runloop_enable
        && (autoswitch_refresh_rate != AUTOSWITCH_REFRESH_RATE_OFF))
    {
      *video_switch_refresh_rate = (
             ((autoswitch_refresh_rate == AUTOSWITCH_REFRESH_RATE_EXCLUSIVE_FULLSCREEN) && exclusive_fullscreen)
          || ((autoswitch_refresh_rate == AUTOSWITCH_REFRESH_RATE_WINDOWED_FULLSCREEN)  && windowed_fullscreen)
          || ((autoswitch_refresh_rate == AUTOSWITCH_REFRESH_RATE_ALL_FULLSCREEN)       && all_fullscreen));

      /* Store original refresh rate on automatic change, and
       * restore it in deinit_core and main_quit, because not all
       * cores announce refresh rate via SET_SYSTEM_AV_INFO */
      if (     *video_switch_refresh_rate
            && !video_st->video_refresh_rate_original)
         video_st->video_refresh_rate_original = video_refresh_rate;
    }
}

bool video_display_server_set_refresh_rate(float hz)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   RARCH_DBG("[Video]: Display server set refresh rate %f\n", hz);
   if (current_display_server && current_display_server->set_resolution)
      return current_display_server->set_resolution(
            video_st->current_display_server_data, 0, 0, (int)hz,
            hz, 0, 0, 0, 0);
   return false;
}

void video_display_server_restore_refresh_rate(void)
{
   video_driver_state_t *video_st = &video_driver_st;
   settings_t *settings           = config_get_ptr();
   float refresh_rate_original    = video_st->video_refresh_rate_original;
   float refresh_rate_current     = settings->floats.video_refresh_rate;

   if (!refresh_rate_original || refresh_rate_current == refresh_rate_original)
      return;
   RARCH_DBG("[Video]: Display server restore refresh rate %f\n", refresh_rate_original);
   video_monitor_set_refresh_rate(refresh_rate_original);
   video_display_server_set_refresh_rate(refresh_rate_original);
}

const char *video_display_server_get_output_options(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (current_display_server && current_display_server->get_output_options)
      return current_display_server->get_output_options(
            video_st->current_display_server_data);
   return NULL;
}

void video_display_server_set_screen_orientation(enum rotation rotation)
{
   video_driver_state_t *video_st             = &video_driver_st;
   if (current_display_server && current_display_server->set_screen_orientation)
   {
      RARCH_LOG("[Video]: Setting screen orientation to %d.\n", rotation);
      video_st->current_screen_orientation    = rotation;
      current_display_server->set_screen_orientation(video_st->current_display_server_data, rotation);
   }
}

bool video_display_server_can_set_screen_orientation(void)
{
   return (current_display_server && current_display_server->set_screen_orientation);
}

enum rotation video_display_server_get_screen_orientation(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (current_display_server && current_display_server->get_screen_orientation)
      return current_display_server->get_screen_orientation(
            video_st->current_display_server_data);
   return ORIENTATION_NORMAL;
}

bool video_display_server_get_flags(gfx_ctx_flags_t *flags)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (!flags || !current_display_server || !current_display_server->get_flags)
      return false;
   flags->flags = current_display_server->get_flags(
         video_st->current_display_server_data);
   return true;
}

#ifdef HAVE_THREADS
bool video_driver_is_threaded(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   return VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st);
}
#endif

bool *video_driver_get_threaded(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
#if defined(__MACH__) && defined(__APPLE__)
   /* TODO/FIXME - force threaded video to disabled on Apple for now
    * until NSWindow/UIWindow concurrency issues are taken care of */
   video_st->threaded = false;
#endif
   return &video_st->threaded;
}

void video_driver_set_threaded(bool val)
{
   video_driver_state_t *video_st                 = &video_driver_st;
#if defined(__MACH__) && defined(__APPLE__)
   /* TODO/FIXME - force threaded video to disabled on Apple for now
    * until NSWindow/UIWindow concurrency issues are taken care of */
   video_st->threaded = false;
#else
   video_st->threaded = val;
#endif
}

const char *video_driver_get_ident(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (!video_st->current_video)
      return NULL;
#ifdef HAVE_THREADS
   if (VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st))
   {
      const thread_video_t *thr   = (const thread_video_t*)video_st->data;
      if (!thr || !thr->driver)
         return NULL;
      return thr->driver->ident;
   }
#endif

   return video_st->current_video->ident;
}

void video_context_driver_reset(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (!video_st->current_video_context.get_metrics)
      video_st->current_video_context.get_metrics = get_metrics_null;
}

bool video_context_driver_set(const gfx_ctx_driver_t *data)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (!data)
      return false;
   video_st->current_video_context = *data;
   video_context_driver_reset();
   return true;
}

uintptr_t video_driver_get_current_framebuffer(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (     video_st->poke
         && video_st->poke->get_current_framebuffer)
      return video_st->poke->get_current_framebuffer(video_st->data);
   return 0;
}

retro_proc_address_t video_driver_get_proc_address(const char *sym)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (     video_st->poke
         && video_st->poke->get_proc_address)
      return video_st->poke->get_proc_address(video_st->data, sym);
   return NULL;
}

#ifdef HAVE_VIDEO_FILTER
void video_driver_filter_free(void)
{
   video_driver_state_t *video_st                 = &video_driver_st;
   if (video_st->state_filter)
      rarch_softfilter_free(video_st->state_filter);
   video_st->state_filter    = NULL;

   if (video_st->state_buffer)
   {
#ifdef _3DS
      linearFree(video_st->state_buffer);
#else
      free(video_st->state_buffer);
#endif
   }
   video_st->state_buffer    = NULL;

   video_st->state_scale     = 0;
   video_st->state_out_bpp   = 0;
   video_st->flags          &= ~(VIDEO_FLAG_STATE_OUT_RGB32);
}

void video_driver_init_filter(enum retro_pixel_format colfmt_int,
      settings_t *settings)
{
   unsigned pow2_x, pow2_y, maxsize;
   void *buf                            = NULL;
   video_driver_state_t *video_st       = &video_driver_st;
   struct retro_game_geometry *geom     = &video_st->av_info.geometry;
   unsigned width                       = geom->max_width;
   unsigned height                      = geom->max_height;
   /* Deprecated format. Gets pre-converted. */
   enum retro_pixel_format colfmt       =
      (colfmt_int == RETRO_PIXEL_FORMAT_0RGB1555)
      ? RETRO_PIXEL_FORMAT_RGB565 : colfmt_int;

   if (video_driver_is_hw_context())
   {
      RARCH_WARN("[Video]: Cannot use CPU filters when hardware rendering is used.\n");
      return;
   }

   if (!(video_st->state_filter = rarch_softfilter_new(
         settings->paths.path_softfilter_plugin,
         RARCH_SOFTFILTER_THREADS_AUTO, colfmt, width, height)))
   {
      RARCH_ERR("[Video]: Failed to load filter.\n");
      return;
   }

   rarch_softfilter_get_max_output_size(
         video_st->state_filter,
         &width, &height);

   pow2_x                              = next_pow2(width);
   pow2_y                              = next_pow2(height);
   maxsize                             = MAX(pow2_x, pow2_y);

#ifdef _3DS
   /* On 3DS, video is disabled if the output resolution
    * exceeds 2048x2048. To avoid the user being presented
    * with a black screen, we therefore have to check that
    * the filter upscaling buffer fits within this limit. */
   if (maxsize >= 2048)
   {
      RARCH_ERR("[Video]: Softfilter initialization failed."
            " Upscaling buffer exceeds hardware limitations.\n");
      video_driver_filter_free();
      return;
   }
#endif

   video_st->state_scale     = maxsize / RARCH_SCALE_BASE;
   if (rarch_softfilter_get_output_format(
         video_st->state_filter) == RETRO_PIXEL_FORMAT_XRGB8888)
      video_st->flags       |=  VIDEO_FLAG_STATE_OUT_RGB32;
   else
      video_st->flags       &= ~VIDEO_FLAG_STATE_OUT_RGB32;

   video_st->state_out_bpp   = (video_st->flags & VIDEO_FLAG_STATE_OUT_RGB32)
      ? sizeof(uint32_t) : sizeof(uint16_t);

   /* TODO: Aligned output. */
#ifdef _3DS
   buf = linearMemAlign(
         width * height * video_st->state_out_bpp, 0x80);
#else
   buf = malloc(
         width * height * video_st->state_out_bpp);
#endif
   if (!buf)
   {
      RARCH_ERR("[Video]: Softfilter initialization failed.\n");
      video_driver_filter_free();
      return;
   }

   video_st->state_buffer    = buf;
}
#endif

void video_driver_free_hw_context(void)
{
   video_driver_state_t *video_st       = &video_driver_st;
   VIDEO_DRIVER_CONTEXT_LOCK(video_st);

   if (video_st->hw_render.context_destroy)
      video_st->hw_render.context_destroy();

   memset(&video_st->hw_render, 0, sizeof(video_st->hw_render));

   VIDEO_DRIVER_CONTEXT_UNLOCK(video_st);

   video_st->hw_render_context_negotiation = NULL;
}

void video_driver_free_internal(void)
{
   input_driver_state_t *input_st    = input_state_get_ptr();
   video_driver_state_t *video_st    = &video_driver_st;
#ifdef HAVE_THREADS
   bool        is_threaded           = VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st);
#endif

   command_event(CMD_EVENT_OVERLAY_UNLOAD, NULL);

   if (!(video_st->flags & VIDEO_FLAG_CACHE_CONTEXT))
      video_driver_free_hw_context();

   if (!(input_st->current_data == video_st->data))
   {
      if (input_st->current_driver)
         if (input_st->current_driver->free)
            input_st->current_driver->free(input_st->current_data);
      if (input_st->primary_joypad)
      {
         const input_device_driver_t *tmp   = input_st->primary_joypad;
         input_st->primary_joypad    = NULL;
         tmp->destroy();
      }
#ifdef HAVE_MFI
      if (input_st->secondary_joypad)
      {
         const input_device_driver_t *tmp   = input_st->secondary_joypad;
         input_st->secondary_joypad         = NULL;
         tmp->destroy();
      }
#endif
      input_st->flags &= ~INP_FLAG_KB_MAPPING_BLOCKED;
      input_st->current_data                = NULL;
   }

   if (     video_st->data
         && video_st->current_video
         && video_st->current_video->free)
      video_st->current_video->free(video_st->data);

   if (video_st->scaler_ptr)
      video_driver_pixel_converter_free(video_st->scaler_ptr);
   video_st->scaler_ptr = NULL;
#ifdef HAVE_VIDEO_FILTER
   video_driver_filter_free();
#endif
#if defined(HAVE_CG) || defined(HAVE_GLSL) || defined(HAVE_SLANG) || defined(HAVE_HLSL)
   video_shader_dir_free_shader(
         (struct rarch_dir_shader_list*)&video_st->dir_shader_list,
         config_get_ptr()->bools.video_shader_remember_last_dir);
#endif
#ifdef HAVE_THREADS
   if (is_threaded)
      return;
#endif

   if (video_st->data)
      video_monitor_compute_fps_statistics(video_st->frame_time_count);
}

void video_driver_set_viewport_config(
      struct retro_game_geometry *geom,
      float video_aspect_ratio,
      bool video_aspect_ratio_auto)
{
   if (video_aspect_ratio < 0.0f)
   {
      if (geom->aspect_ratio > 0.0f && video_aspect_ratio_auto)
         aspectratio_lut[ASPECT_RATIO_CONFIG].value = geom->aspect_ratio;
      else
      {
         unsigned base_width  = geom->base_width;
         unsigned base_height = geom->base_height;

         /* Get around division by zero errors */
         if (base_width == 0)
            base_width        = 1;
         if (base_height == 0)
            base_height       = 1;
         aspectratio_lut[ASPECT_RATIO_CONFIG].value =
            (float)base_width / base_height; /* 1:1 PAR. */
      }
   }
   else
      aspectratio_lut[ASPECT_RATIO_CONFIG].value = video_aspect_ratio;
}

void video_driver_set_viewport_square_pixel(struct retro_game_geometry *geom)
{
   unsigned len, i, aspect_x, aspect_y;
   unsigned int rotation             = 0;
   unsigned highest                  = 1;
   unsigned width                    = geom->base_width;
   unsigned height                   = geom->base_height;

   if (width == 0 || height == 0)
      return;

   rotation                          = retroarch_get_rotation();
   len                               = MIN(width, height);

   for (i = 1; i < len; i++)
   {
      if ((width % i) == 0 && (height % i) == 0)
         highest = i;
   }

   if (rotation % 2)
   {
      aspect_x = height / highest;
      aspect_y = width  / highest;
   }
   else
   {
      aspect_x = width  / highest;
      aspect_y = height / highest;
   }

   snprintf(aspectratio_lut[ASPECT_RATIO_SQUARE].name,
         sizeof(aspectratio_lut[ASPECT_RATIO_SQUARE].name),
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_SQUARE_PIXEL),
         aspect_x, aspect_y);

   aspectratio_lut[ASPECT_RATIO_SQUARE].value = (float)aspect_x / aspect_y;
}

bool video_driver_set_rotation(unsigned rotation)
{
   video_driver_state_t *video_st         = &video_driver_st;
   if (!video_st->current_video || !video_st->current_video->set_rotation)
      return false;
   video_st->current_video->set_rotation(video_st->data, rotation);
   return true;
}

bool video_driver_set_video_mode(unsigned width,
      unsigned height, bool fullscreen)
{
   video_driver_state_t *video_st         = &video_driver_st;
   if (     video_st->poke
         && video_st->poke->set_video_mode)
   {
      video_st->poke->set_video_mode(video_st->data,
            width, height, fullscreen);
      return true;
   }
   return false;
}

bool video_driver_get_video_output_size(unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   video_driver_state_t *video_st         = &video_driver_st;
   if (!video_st->poke || !video_st->poke->get_video_output_size)
      return false;
   video_st->poke->get_video_output_size(video_st->data,
         width, height, desc, desc_len);
   return true;
}

void *video_driver_read_frame_raw(unsigned *width,
   unsigned *height, size_t *pitch)
{
   video_driver_state_t *video_st = &video_driver_st;
   if (      video_st->current_video
         &&  video_st->current_video->read_frame_raw)
      return video_st->current_video->read_frame_raw(
            video_st->data, width,
            height, pitch);
   return NULL;
}

void video_driver_set_filtering(unsigned index,
      bool smooth, bool ctx_scaling)
{
   video_driver_state_t *video_st = &video_driver_st;
   if (     video_st->poke
         && video_st->poke->set_filtering)
      video_st->poke->set_filtering(
            video_st->data,
            index, smooth, ctx_scaling);
}

void video_driver_get_size(unsigned *width, unsigned *height)
{
   video_driver_state_t *video_st = &video_driver_st;
#ifdef HAVE_THREADS
   bool is_threaded = VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st);

   VIDEO_DRIVER_THREADED_LOCK(video_st, is_threaded);
#endif
   if (width)
      *width        = video_st->width;
   if (height)
      *height       = video_st->height;
#ifdef HAVE_THREADS
   VIDEO_DRIVER_THREADED_UNLOCK(video_st, is_threaded);
#endif
}

void video_driver_set_size(unsigned width, unsigned height)
{
   video_driver_state_t *video_st = &video_driver_st;
#ifdef HAVE_THREADS
   bool            is_threaded    = VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st);
   VIDEO_DRIVER_THREADED_LOCK(video_st, is_threaded);
   video_st->width                = width;
   video_st->height               = height;
   VIDEO_DRIVER_THREADED_UNLOCK(video_st, is_threaded);
#else
   video_st->width                = width;
   video_st->height               = height;
#endif
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
   retro_time_t accum             = 0;
   retro_time_t avg               = 0;
   retro_time_t accum_var         = 0;
   unsigned samples               = 0;
   video_driver_state_t *video_st = &video_driver_st;

#ifdef HAVE_THREADS
   if (VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st))
      return false;
#endif

   if ((samples = MIN(MEASURE_FRAME_TIME_SAMPLES_COUNT,
         (unsigned)video_st->frame_time_count)) < 2)
      return false;

   /* Measure statistics on frame time (microsecs), *not* FPS. */
   for (i = 0; i < samples; i++)
   {
      accum += video_st->frame_time_samples[i];
#if 0
      RARCH_LOG("[Video]: Interval #%u: %d usec / frame.\n",
            i, (int)frame_time_samples[i]);
#endif
   }

   avg = accum / samples;

   /* Drop first measurement. It is likely to be bad. */
   for (i = 0; i < samples; i++)
   {
      retro_time_t diff  = video_st->frame_time_samples[i] - avg;
      accum_var         += diff * diff;
   }

   *deviation            = sqrt((double)accum_var / (samples - 1)) / avg;

   if (refresh_rate)
      *refresh_rate      = 1000000.0 / avg;

   if (sample_points)
      *sample_points     = samples;

   return true;
}

float video_driver_get_aspect_ratio(void)
{
   video_driver_state_t *video_st = &video_driver_st;
   return video_st->aspect_ratio;
}

void video_driver_lock_new(void)
{
#ifdef HAVE_THREADS
   video_driver_state_t *video_st = &video_driver_st;
   VIDEO_DRIVER_LOCK_FREE(video_st);
   if (!video_st->display_lock)
      video_st->display_lock = slock_new();

   if (!video_st->context_lock)
      video_st->context_lock = slock_new();
#endif
}

void video_driver_set_stub_frame(void)
{
   video_driver_state_t *video_st = &video_driver_st;
   video_st->frame_bak            = video_st->current_video->frame;
   video_st->current_video->frame = video_null.frame;
}

void video_driver_unset_stub_frame(void)
{
   video_driver_state_t *video_st    = &video_driver_st;
   if (video_st->frame_bak)
      video_st->current_video->frame = video_st->frame_bak;

   video_st->frame_bak               = NULL;
}

/* Get aspect ratio (DAR) requested by the core */
float video_driver_get_core_aspect(void)
{
   video_driver_state_t *video_st   = &video_driver_st;
   struct retro_game_geometry *geom = &video_st->av_info.geometry;
   float out_aspect                 = 0;

   if (!geom || geom->base_width <= 0.0f || geom->base_height <= 0.0f)
      return 0.0f;

   /* Fallback to 1:1 pixel ratio if none provided */
   if (geom->aspect_ratio > 0.0f)
      out_aspect = geom->aspect_ratio;
   else
      out_aspect = (float)geom->base_width / geom->base_height;

   /* Flip rotated aspect */
   if ((retroarch_get_rotation() + retroarch_get_core_requested_rotation()) % 2)
      return (1.0f / out_aspect);

   return out_aspect;
}

void video_driver_set_viewport_core(void)
{
   float core_aspect = video_driver_get_core_aspect();
   if (core_aspect != 0)
      aspectratio_lut[ASPECT_RATIO_CORE].value = core_aspect;
}

void video_driver_set_rgba(void)
{
   video_driver_state_t *video_st       = &video_driver_st;
   VIDEO_DRIVER_LOCK(video_st);
   video_st->flags |= VIDEO_FLAG_USE_RGBA;
   VIDEO_DRIVER_UNLOCK(video_st);
}

void video_driver_unset_rgba(void)
{
   video_driver_state_t *video_st       = &video_driver_st;
   VIDEO_DRIVER_LOCK(video_st);
   video_st->flags &= ~VIDEO_FLAG_USE_RGBA;
   VIDEO_DRIVER_UNLOCK(video_st);
}

bool video_driver_supports_rgba(void)
{
   bool tmp;
   video_driver_state_t *video_st       = &video_driver_st;
   VIDEO_DRIVER_LOCK(video_st);
   tmp = (video_st->flags & VIDEO_FLAG_USE_RGBA) ? true : false;
   VIDEO_DRIVER_UNLOCK(video_st);
   return tmp;
}

void video_driver_set_hdr_support(void)
{
   video_driver_state_t *video_st  = &video_driver_st;
   VIDEO_DRIVER_LOCK(video_st);
   video_st->flags                |= VIDEO_FLAG_HDR_SUPPORT;
   VIDEO_DRIVER_UNLOCK(video_st);
}

void video_driver_unset_hdr_support(void)
{
   video_driver_state_t *video_st  = &video_driver_st;
   VIDEO_DRIVER_LOCK(video_st);
   video_st->flags                &= ~VIDEO_FLAG_HDR_SUPPORT;
   VIDEO_DRIVER_UNLOCK(video_st);
}

bool video_driver_supports_hdr(void)
{
   bool tmp;
   video_driver_state_t *video_st       = &video_driver_st;
   VIDEO_DRIVER_LOCK(video_st);
   tmp = (video_st->flags & VIDEO_FLAG_HDR_SUPPORT) ? true : false;
   VIDEO_DRIVER_UNLOCK(video_st);
   return tmp;
}

bool video_driver_get_next_video_out(void)
{
   video_driver_state_t *video_st       = &video_driver_st;
   if (     !video_st->poke
         || !video_st->poke->get_video_output_next
      )
      return false;
   video_st->poke->get_video_output_next(video_st->data);
   return true;
}

bool video_driver_get_prev_video_out(void)
{
   video_driver_state_t *video_st       = &video_driver_st;
   if (
            !video_st->poke
         || !video_st->poke->get_video_output_prev
      )
      return false;

   video_st->poke->get_video_output_prev(video_st->data);
   return true;
}

void video_driver_monitor_reset(void)
{
   video_driver_state_t *video_st       = &video_driver_st;
   video_st->frame_time_count = 0;
}

void video_driver_set_aspect_ratio(void)
{
   settings_t  *settings          = config_get_ptr();
   video_driver_state_t *video_st = &video_driver_st;
   unsigned  aspect_ratio_idx     = settings->uints.video_aspect_ratio_idx;

   switch (aspect_ratio_idx)
   {
      case ASPECT_RATIO_SQUARE:
         video_driver_set_viewport_square_pixel(&video_st->av_info.geometry);
         break;

      case ASPECT_RATIO_CORE:
         video_driver_set_viewport_core();
         break;

      case ASPECT_RATIO_CONFIG:
         video_driver_set_viewport_config(
               &video_st->av_info.geometry,
               settings->floats.video_aspect_ratio,
               settings->bools.video_aspect_ratio_auto);
         break;

      case ASPECT_RATIO_FULL:
         {
            unsigned width  = video_st->width;
            unsigned height = video_st->height;

            if (width != 0 && height != 0)
               aspectratio_lut[ASPECT_RATIO_FULL].value = (float)width / (float)height;
         }
         break;

      default:
         break;
   }

   video_st->aspect_ratio = aspectratio_lut[aspect_ratio_idx].value;

   if (     video_st->poke
         && video_st->poke->set_aspect_ratio)
      video_st->poke->set_aspect_ratio(video_st->data, aspect_ratio_idx);
}

void video_driver_update_viewport(
      struct video_viewport* vp, bool force_full, bool keep_aspect)
{
   float            device_aspect  = (float)vp->full_width / vp->full_height;
   settings_t *settings            = config_get_ptr();
   bool video_scale_integer        = settings->bools.video_scale_integer;
   unsigned video_aspect_ratio_idx = settings->uints.video_aspect_ratio_idx;
   video_driver_state_t *video_st  = &video_driver_st;
   float video_driver_aspect_ratio = video_st->aspect_ratio;

   vp->x                           = 0;
   vp->y                           = 0;
   vp->width                       = vp->full_width;
   vp->height                      = vp->full_height;

   if (video_scale_integer && !force_full)
      video_viewport_get_scaled_integer(
            vp,
            vp->full_width,
            vp->full_height,
            video_driver_aspect_ratio, keep_aspect);
   else if (keep_aspect && !force_full)
   {
      float desired_aspect = video_driver_aspect_ratio;

#if defined(HAVE_MENU)
      if (video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         const struct video_viewport *custom_vp = &settings->video_viewport_custom;
         vp->x      = custom_vp->x;
         vp->y      = custom_vp->y;
         vp->width  = custom_vp->width;
         vp->height = custom_vp->height;
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
            delta      = (desired_aspect / device_aspect - 1.0f)
               / 2.0f + 0.5f;
            vp->x      = (int)roundf(vp->full_width * (0.5f - delta));
            vp->width  = (unsigned)roundf(2.0f * vp->full_width * delta);
            vp->y      = 0;
            vp->height = vp->full_height;
         }
         else
         {
            vp->x      = 0;
            vp->width  = vp->full_width;
            delta      = (device_aspect / desired_aspect - 1.0f)
               / 2.0f + 0.5f;
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

void video_driver_restore_cached(void *settings_data)
{
   settings_t *settings           = (settings_t*)settings_data;
   video_driver_state_t *video_st = &video_driver_st;
   if (video_st->cached_driver_id[0])
   {
      configuration_set_string(settings,
            settings->arrays.video_driver, video_st->cached_driver_id);

      video_st->cached_driver_id[0] = 0;
      RARCH_LOG("[Video]: Restored video driver to \"%s\".\n",
            settings->arrays.video_driver);
   }
}

bool video_driver_find_driver(
      void *settings_data,
      const char *prefix, bool verbosity_enabled)
{
   int i;
   settings_t *settings                    = (settings_t*)settings_data;
   video_driver_state_t *video_st          = &video_driver_st;

   if (video_driver_is_hw_context())
   {
      struct retro_hw_render_callback *hwr =
         VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st);
      int rdr_major                        = hwr->version_major;
      int rdr_minor                        = hwr->version_minor;
      const char *rdr_context_name         = hw_render_context_name(hwr->context_type, rdr_major, rdr_minor);
      enum retro_hw_context_type rdr_type  = hw_render_context_type(rdr_context_name);

      video_st->current_video              = NULL;

      if (hwr)
      {
         switch (rdr_type)
         {
            case RETRO_HW_CONTEXT_OPENGL_CORE:
            case RETRO_HW_CONTEXT_VULKAN:
            case RETRO_HW_CONTEXT_D3D9:
            case RETRO_HW_CONTEXT_D3D10:
            case RETRO_HW_CONTEXT_D3D11:
            case RETRO_HW_CONTEXT_D3D12:
#if defined(HAVE_VULKAN) || defined(HAVE_D3D9) || defined(HAVE_D3D10) || defined(HAVE_D3D11) || defined(HAVE_D3D12) || defined(HAVE_OPENGL_CORE)
               RARCH_LOG("[Video]: Using HW render, %s driver forced.\n",
                     rdr_context_name);

               if (!string_is_equal(settings->arrays.video_driver,
                        rdr_context_name))
               {
                  strlcpy(video_st->cached_driver_id,
                        settings->arrays.video_driver,
                        sizeof(video_st->cached_driver_id));
                  configuration_set_string(settings,
                        settings->arrays.video_driver,
                        rdr_context_name);
                  RARCH_LOG("[Video]: \"%s\" saved as cached driver.\n",
                        settings->arrays.video_driver);
               }

               video_st->current_video = hw_render_context_driver(rdr_type, rdr_major, rdr_minor);
               return true;
#else
               break;
#endif
            case RETRO_HW_CONTEXT_OPENGL:
#if defined(HAVE_OPENGL)
               RARCH_LOG("[Video]: Using HW render, OpenGL driver forced.\n");

               /* If we have configured one of the HW render
                * capable GL drivers, go with that. */
#if defined(HAVE_OPENGL_CORE)
               if (     !string_is_equal(settings->arrays.video_driver, "gl")
                     && !string_is_equal(settings->arrays.video_driver, "glcore"))
               {
                  strlcpy(video_st->cached_driver_id,
                        settings->arrays.video_driver,
                        sizeof(video_st->cached_driver_id));
                  configuration_set_string(settings,
                        settings->arrays.video_driver,
                        "glcore");
                  RARCH_LOG("[Video]: \"%s\" saved as cached driver.\n",
                        settings->arrays.video_driver);
                  video_st->current_video = &video_gl3;
                  return true;
               }
#else
               if (  !string_is_equal(settings->arrays.video_driver, "gl"))
               {
                  strlcpy(video_st->cached_driver_id,
                        settings->arrays.video_driver,
                        sizeof(video_st->cached_driver_id));
                  configuration_set_string(settings,
                        settings->arrays.video_driver,
                        "gl");
                  RARCH_LOG("[Video]: \"%s\" saved as cached driver.\n",
                        settings->arrays.video_driver);
                  video_st->current_video = &video_gl2;
                  return true;
               }
#endif

               RARCH_LOG("[Video]: Using configured \"%s\""
                     " driver for GL HW render.\n",
                     settings->arrays.video_driver);
               break;
#endif
            default:
            case RETRO_HW_CONTEXT_NONE:
               break;
         }
      }
   }

   if (frontend_driver_has_get_video_driver_func())
   {
      if ((video_st->current_video = (video_driver_t*)
               frontend_driver_get_video_driver()))
         return true;

      RARCH_WARN("[Video]: Frontend supports get_video_driver() but did not specify one.\n");
   }

   i                   = (int)driver_find_index(
         "video_driver",
         settings->arrays.video_driver);

   if (i >= 0)
      video_st->current_video = (video_driver_t*)video_drivers[i];
   else
   {
      if (verbosity_enabled)
      {
         unsigned d;
         RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
               settings->arrays.video_driver);
         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; video_drivers[d]; d++)
            RARCH_LOG_OUTPUT("\t%s\n", video_drivers[d]->ident);
         RARCH_WARN("Going to default to first %s..\n", prefix);
      }

      if (!(video_st->current_video = (video_driver_t*)video_drivers[0]))
         return false;
   }
   return true;
}

void video_driver_apply_state_changes(void)
{
   video_driver_state_t *video_st          = &video_driver_st;
   if (     video_st->poke
         && video_st->poke->apply_state_changes)
      video_st->poke->apply_state_changes(video_st->data);
}

bool video_driver_is_hw_context(void)
{
   bool            is_hw_context  = false;
   video_driver_state_t *video_st = &video_driver_st;

   VIDEO_DRIVER_CONTEXT_LOCK(video_st);
   is_hw_context                 = (video_st->hw_render.context_type
         != RETRO_HW_CONTEXT_NONE);
   VIDEO_DRIVER_CONTEXT_UNLOCK(video_st);

   return is_hw_context;
}

bool video_driver_get_viewport_info(struct video_viewport *viewport)
{
   video_driver_state_t *video_st  = &video_driver_st;
   if (!video_st->current_video || !video_st->current_video->viewport_info)
      return false;
   video_st->current_video->viewport_info(video_st->data, viewport);
   return true;
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
   int padding_x                   = 0;
   int padding_y                   = 0;
   settings_t *settings            = config_get_ptr();
   video_driver_state_t *video_st  = &video_driver_st;
   unsigned video_aspect_ratio_idx = settings->uints.video_aspect_ratio_idx;
   bool overscale                  = settings->bools.video_scale_integer_overscale;

   if (video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      struct video_viewport *custom_vp = &settings->video_viewport_custom;

      if (custom_vp)
      {
         padding_x = width  - custom_vp->width;
         padding_y = height - custom_vp->height;
         width     = custom_vp->width;
         height    = custom_vp->height;
      }
   }
   else
   {
      unsigned base_width;
      /* Use system reported sizes as these define the
       * geometry for the "normal" case. */
      unsigned base_height  =
         video_st->av_info.geometry.base_height;
      unsigned int rotation = retroarch_get_rotation();

      if (rotation % 2)
         base_height        = video_st->av_info.geometry.base_width;

      if (base_height == 0)
         base_height        = 1;

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
            unsigned max_scale = 1;

            if (overscale)
               max_scale = MIN((width / base_width) + !!(width % base_width),
                     (height / base_height) + !!(height % base_height));
            else
               max_scale = MIN(width / base_width,
                     height / base_height);

            padding_x          = width  - base_width  * max_scale;
            padding_y          = height - base_height * max_scale;
         }
         else
         {
            /* X/Y can be independent, each scaled as much as possible. */
            padding_x = width  % base_width;
            padding_y = height % base_height;
         }
      }

      width  -= padding_x;
      height -= padding_y;
   }

   vp->width  = width;
   vp->height = height;
   vp->x      = padding_x / 2;
   vp->y      = padding_y / 2;
}

void video_driver_display_type_set(enum rarch_display_type type)
{
   video_driver_state_t *video_st = &video_driver_st;
   video_st->display_type         = type;
}

uintptr_t video_driver_display_get(void)
{
   video_driver_state_t *video_st = &video_driver_st;
   return video_st->display;
}

uintptr_t video_driver_display_userdata_get(void)
{
   video_driver_state_t *video_st = &video_driver_st;
   return video_st->display_userdata;
}

void video_driver_display_userdata_set(uintptr_t idx)
{
   video_driver_state_t *video_st = &video_driver_st;
   video_st->display_userdata     = idx;
}

void video_driver_display_set(uintptr_t idx)
{
   video_driver_state_t *video_st = &video_driver_st;
   video_st->display              = idx;
}

enum rarch_display_type video_driver_display_type_get(void)
{
   video_driver_state_t *video_st = &video_driver_st;
   return video_st->display_type;
}

void video_driver_window_set(uintptr_t idx)
{
   video_driver_state_t *video_st = &video_driver_st;
   video_st->window               = idx;
}

uintptr_t video_driver_window_get(void)
{
   video_driver_state_t *video_st = &video_driver_st;
   return video_st->window;
}

bool video_driver_texture_load(void *data,
      enum texture_filter_type  filter_type,
      uintptr_t *id)
{
   video_driver_state_t *video_st = &video_driver_st;
   if (     !id
         || !video_st->poke
         || !video_st->poke->load_texture)
      return false;
   *id = video_st->poke->load_texture(
         video_st->data, data,
         VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st),
         filter_type);
   return true;
}

bool video_driver_texture_unload(uintptr_t *id)
{
   video_driver_state_t *video_st = &video_driver_st;
   if (     !video_st->poke
         || !video_st->poke->unload_texture)
      return false;
   video_st->poke->unload_texture(
         video_st->data,
         VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st),
         *id);
   *id = 0;
   return true;
}

/**
 * video_driver_cached_frame:
 *
 * Renders the current video frame.
 **/
void video_driver_cached_frame(void)
{
   runloop_state_t *runloop_st    = runloop_state_get_ptr();
   recording_state_t *recording_st= recording_state_get_ptr();
   video_driver_state_t *video_st = &video_driver_st;
   void             *recording    = recording_st->data;
   struct retro_callbacks *cbs    = &runloop_st->retro_ctx;

   /* Cannot allow recording when pushing duped frames. */
   recording_st->data             = NULL;

   if (runloop_st->current_core.flags & RETRO_CORE_FLAG_INITED)
      cbs->frame_cb(
            (video_st->frame_cache_data != RETRO_HW_FRAME_BUFFER_VALID)
            ? video_st->frame_cache_data
            : NULL,
            video_st->frame_cache_width,
            video_st->frame_cache_height,
            video_st->frame_cache_pitch);

   recording_st->data             = recording;
}

bool video_driver_has_focus(void)
{
   video_driver_state_t *video_st = &video_driver_st;
   return VIDEO_HAS_FOCUS(video_st);
}

size_t video_driver_get_window_title(char *buf, unsigned len)
{
   video_driver_state_t *video_st = &video_driver_st;
   if (buf && (video_st->flags & VIDEO_FLAG_WINDOW_TITLE_UPDATE))
   {
      strlcpy(buf, video_st->window_title, len);
      video_st->flags &= ~VIDEO_FLAG_WINDOW_TITLE_UPDATE;
   }
   return video_st->window_title_len;
}

void video_driver_update_title(void *data)
{
#ifndef _XBOX
   const ui_window_t *window      = ui_companion_driver_get_window_ptr();
   video_driver_state_t *video_st = &video_driver_st;
   if (     video_st->flags & VIDEO_FLAG_WINDOW_TITLE_UPDATE
         && window)
   {
      if (     video_st->window_title[0]
            && !string_is_equal(video_st->window_title, video_st->window_title_prev))
      {
         window->set_title((void*)video_st->display_userdata, video_st->window_title);
         strlcpy(video_st->window_title_prev, video_st->window_title, sizeof(video_st->window_title_prev));
      }
      video_st->flags &= ~VIDEO_FLAG_WINDOW_TITLE_UPDATE;
   }
#endif
}

void video_driver_build_info(video_frame_info_t *video_info)
{
   video_viewport_t *custom_vp             = NULL;
   runloop_state_t *runloop_st             = runloop_state_get_ptr();
   settings_t *settings                    = config_get_ptr();
   video_driver_state_t *video_st          = &video_driver_st;
   input_driver_state_t *input_st          = input_state_get_ptr();
#ifdef HAVE_MENU
   struct menu_state *menu_st              = menu_state_get_ptr();
#endif
#ifdef HAVE_GFX_WIDGETS
   dispgfx_widget_t *p_dispwidget          = dispwidget_get_ptr();
#endif
#ifdef HAVE_THREADS
   bool is_threaded                        =
         VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st);

   VIDEO_DRIVER_THREADED_LOCK(video_st, is_threaded);
#endif
   custom_vp                               = &settings->video_viewport_custom;
#ifdef HAVE_GFX_WIDGETS
   video_info->widgets_active              = p_dispwidget->active;
#else
   video_info->widgets_active              = false;
#endif
#ifdef HAVE_MENU
   video_info->notifications_hidden        = settings->bools.notification_show_when_menu_is_alive
         && !(menu_st->flags & MENU_ST_FLAG_ALIVE);
#endif
   video_info->refresh_rate                = settings->floats.video_refresh_rate;
   video_info->crt_switch_resolution       = settings->uints.crt_switch_resolution;
   video_info->crt_switch_resolution_super = settings->uints.crt_switch_resolution_super;
   video_info->crt_switch_center_adjust    = settings->ints.crt_switch_center_adjust;
   video_info->crt_switch_porch_adjust     = settings->ints.crt_switch_porch_adjust;
   video_info->crt_switch_hires_menu       = settings->bools.crt_switch_hires_menu;
   video_info->black_frame_insertion       = settings->uints.video_black_frame_insertion;
   video_info->hard_sync                   = settings->bools.video_hard_sync;
   video_info->hard_sync_frames            = settings->uints.video_hard_sync_frames;
   video_info->runahead                    = settings->bools.run_ahead_enabled;
   video_info->runahead_second_instance    = settings->bools.run_ahead_secondary_instance;
   video_info->preemptive_frames           = settings->bools.preemptive_frames_enable;
   video_info->runahead_frames             = settings->uints.run_ahead_frames;
   video_info->fps_show                    = settings->bools.video_fps_show;
   video_info->memory_show                 = settings->bools.video_memory_show;
   video_info->statistics_show             = settings->bools.video_statistics_show;
   video_info->framecount_show             = settings->bools.video_framecount_show;
   video_info->core_status_msg_show        = runloop_st->core_status_msg.set;
   video_info->aspect_ratio_idx            = settings->uints.video_aspect_ratio_idx;
   video_info->post_filter_record          = settings->bools.video_post_filter_record;
   video_info->input_menu_swap_ok_cancel_buttons
                                           = settings->bools.input_menu_swap_ok_cancel_buttons;
   video_info->max_swapchain_images        = settings->uints.video_max_swapchain_images;
   video_info->windowed_fullscreen         = settings->bools.video_windowed_fullscreen;
   video_info->fullscreen                  = settings->bools.video_fullscreen
         || (video_st->flags & VIDEO_FLAG_FORCE_FULLSCREEN);
   video_info->menu_mouse_enable           = settings->bools.menu_mouse_enable;
   video_info->monitor_index               = settings->uints.video_monitor_index;

   video_info->font_enable                 = settings->bools.video_font_enable;
   video_info->font_size                   = settings->floats.video_font_size;
   video_info->font_msg_pos_x              = settings->floats.video_msg_pos_x;
   video_info->font_msg_pos_y              = settings->floats.video_msg_pos_y;
   video_info->font_msg_color_r            = settings->floats.video_msg_color_r;
   video_info->font_msg_color_g            = settings->floats.video_msg_color_g;
   video_info->font_msg_color_b            = settings->floats.video_msg_color_b;
   video_info->custom_vp_x                 = custom_vp->x;
   video_info->custom_vp_y                 = custom_vp->y;
   video_info->custom_vp_width             = custom_vp->width;
   video_info->custom_vp_height            = custom_vp->height;
   video_info->custom_vp_full_width        = custom_vp->full_width;
   video_info->custom_vp_full_height       = custom_vp->full_height;

   video_info->video_st_flags              = video_st->flags;
#if defined(HAVE_GFX_WIDGETS)
   video_info->widgets_userdata            = p_dispwidget;
#else
   video_info->widgets_userdata            = NULL;
#endif

   video_info->width                       = video_st->width;
   video_info->height                      = video_st->height;

   video_info->hdr_enable                  = settings->bools.video_hdr_enable;

   video_info->libretro_running            = false;
   video_info->msg_bgcolor_enable          = settings->bools.video_msg_bgcolor_enable;

   video_info->fps_update_interval         = settings->uints.fps_update_interval;
   video_info->memory_update_interval      = settings->uints.memory_update_interval;

#ifdef HAVE_MENU
   video_info->menu_st_flags               = menu_st->flags;
   video_info->menu_footer_opacity         = settings->floats.menu_footer_opacity;
   video_info->menu_header_opacity         = settings->floats.menu_header_opacity;
   video_info->materialui_color_theme      = settings->uints.menu_materialui_color_theme;
   video_info->ozone_color_theme           = settings->uints.menu_ozone_color_theme;
   video_info->menu_shader_pipeline        = settings->uints.menu_xmb_shader_pipeline;
   video_info->xmb_theme                   = settings->uints.menu_xmb_theme;
   video_info->xmb_color_theme             = settings->uints.menu_xmb_color_theme;
   video_info->timedate_enable             = settings->bools.menu_timedate_enable;
   video_info->battery_level_enable        = settings->bools.menu_battery_level_enable;
   video_info->xmb_shadows_enable          = settings->bools.menu_xmb_shadows_enable;
   video_info->xmb_alpha_factor            = settings->uints.menu_xmb_alpha_factor;
   video_info->menu_wallpaper_opacity      = settings->floats.menu_wallpaper_opacity;
   video_info->menu_framebuffer_opacity    = settings->floats.menu_framebuffer_opacity;
   video_info->overlay_behind_menu         = settings->bools.input_overlay_behind_menu;
   video_info->libretro_running            = (runloop_st->current_core.flags & RETRO_CORE_FLAG_GAME_LOADED) ? true : false;
#else
   video_info->menu_st_flags               = 0;
   video_info->menu_footer_opacity         = 0.0f;
   video_info->menu_header_opacity         = 0.0f;
   video_info->materialui_color_theme      = 0;
   video_info->menu_shader_pipeline        = 0;
   video_info->xmb_color_theme             = 0;
   video_info->xmb_theme                   = 0;
   video_info->timedate_enable             = false;
   video_info->battery_level_enable        = false;
   video_info->xmb_shadows_enable          = false;
   video_info->xmb_alpha_factor            = 0.0f;
   video_info->menu_framebuffer_opacity    = 0.0f;
   video_info->menu_wallpaper_opacity      = 0.0f;
   video_info->overlay_behind_menu         = false;
#endif

   video_info->msg_queue_delay             = runloop_st->msg_queue_delay;
   video_info->runloop_is_paused           = (runloop_st->flags & RUNLOOP_FLAG_PAUSED) ? true : false;
   video_info->runloop_is_slowmotion       = (runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION) ? true : false;
   video_info->fastforward_frameskip       = settings->bools.fastforward_frameskip;
   video_info->frame_rest                  = settings->bools.video_frame_rest;

#ifdef _WIN32
#ifdef HAVE_VULKAN
   /* Vulkan in Windows does mailbox emulation
    * in fullscreen with vsync, effectively
    * discarding frames that can't be shown,
    * therefore do not do it twice. */
   if (     video_info->fullscreen
         && settings->bools.video_vsync
         && string_is_equal(video_driver_get_ident(), "vulkan"))
      video_info->fastforward_frameskip    = false;
#endif
#endif

   video_info->input_driver_nonblock_state   = (input_st->flags & INP_FLAG_NONBLOCKING)      ? true : false;
   video_info->input_driver_grab_mouse_state = (input_st->flags & INP_FLAG_GRAB_MOUSE_STATE) ? true : false;
   video_info->disp_userdata                 = disp_get_ptr();

   video_info->userdata                      = VIDEO_DRIVER_GET_PTR_INTERNAL(video_st);

#ifdef HAVE_THREADS
   VIDEO_DRIVER_THREADED_UNLOCK(video_st, is_threaded);
#endif
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
      unsigned minor, bool hw_render_ctx, void **ctx_data)
{
   unsigned j;
   int i = -1;
   uint32_t runloop_flags         = runloop_get_flags();
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = &video_driver_st;

   for (j = 0; gfx_ctx_gl_drivers[j]; j++)
   {
      if (string_is_equal_noncase(ident, gfx_ctx_gl_drivers[j]->ident))
      {
         i = j;
         break;
      }
   }

   if (i >= 0)
   {
      const gfx_ctx_driver_t *ctx = video_context_driver_init(
            (runloop_flags & RUNLOOP_FLAG_CORE_SET_SHARED_CONTEXT) ? true : false,
            settings,
            data,
            gfx_ctx_gl_drivers[i], ident,
            api, major, minor, hw_render_ctx, ctx_data);
      if (ctx)
      {
         video_st->context_data = *ctx_data;
         return ctx;
      }
   }

   for (i = 0; gfx_ctx_gl_drivers[i]; i++)
   {
      const gfx_ctx_driver_t *ctx =
         video_context_driver_init(
               (runloop_flags & RUNLOOP_FLAG_CORE_SET_SHARED_CONTEXT) ? true : false,
               settings,
               data,
               gfx_ctx_gl_drivers[i], ident,
               api, major, minor, hw_render_ctx, ctx_data);

      if (ctx)
      {
         video_st->context_data = *ctx_data;
         return ctx;
      }
   }

   return NULL;
}

void video_context_driver_free(void)
{
   video_driver_state_t *video_st  = &video_driver_st;
   video_context_driver_destroy(&video_st->current_video_context);
   video_st->context_data    = NULL;
}

bool video_context_driver_get_metrics(gfx_ctx_metrics_t *metrics)
{
   video_driver_state_t *video_st  = &video_driver_st;
   if (video_st->current_video_context.get_metrics)
      return video_st->current_video_context.get_metrics(
            video_st->context_data,
            metrics->type,
            metrics->value);
   return false;
}

bool video_context_driver_get_refresh_rate(float *refresh_rate)
{
   video_driver_state_t *video_st  = &video_driver_st;
   if (!video_st->current_video_context.get_refresh_rate || !refresh_rate)
      return false;
   if (!video_st->context_data)
      return false;

   if (video_st->flags & VIDEO_FLAG_CRT_SWITCHING_ACTIVE)
   {
      float refresh_holder      = 0;
      if (refresh_rate)
         refresh_holder         =
             video_st->current_video_context.get_refresh_rate(
                   video_st->context_data);

      /* Fix for incorrect interlacing detection --
       * HARD SET VSYNC TO REQUIRED REFRESH FOR CRT*/
      if (refresh_holder != video_st->core_hz)
         *refresh_rate          = video_st->core_hz;
   }
   else
   {
      if (refresh_rate)
         *refresh_rate =
             video_st->current_video_context.get_refresh_rate(
                   video_st->context_data);
   }

   return true;
}

bool video_context_driver_get_ident(gfx_ctx_ident_t *ident)
{
   video_driver_state_t *video_st  = &video_driver_st;
   if (!ident)
      return false;
   ident->ident = video_st->current_video_context.ident;
   return true;
}

bool video_context_driver_get_flags(gfx_ctx_flags_t *flags)
{
   video_driver_state_t *video_st  = &video_driver_st;
   if (!video_st->current_video_context.get_flags)
      return false;

   if (video_st->flags & VIDEO_FLAG_DEFERRED_VIDEO_CTX_DRIVER_SET_FLAGS)
   {
      flags->flags     = video_st->deferred_flag_data.flags;
      video_st->flags &= ~VIDEO_FLAG_DEFERRED_VIDEO_CTX_DRIVER_SET_FLAGS;
   }
   else
      flags->flags     = video_st->current_video_context.get_flags(
            video_st->context_data);
   return true;
}

static bool video_driver_get_flags(gfx_ctx_flags_t *flags)
{
   video_driver_state_t *video_st  = &video_driver_st;
   if (!video_st->poke || !video_st->poke->get_flags)
      return false;
   flags->flags = video_st->poke->get_flags(video_st->data);
   return true;
}

gfx_ctx_flags_t video_driver_get_flags_wrapper(void)
{
   gfx_ctx_flags_t flags;
   flags.flags                 = 0;

   if (!video_driver_get_flags(&flags))
      video_context_driver_get_flags(&flags);

   return flags;
}

/**
 * video_driver_test_all_flags:
 * @testflag          : flag to test
 *
 * Poll both the video and context driver's flags and test
 * whether @testflag is set or not.
 **/
bool video_driver_test_all_flags(enum display_flags testflag)
{
   gfx_ctx_flags_t flags;

   if (video_driver_get_flags(&flags))
      if (BIT32_GET(flags.flags, testflag))
         return true;

   if (video_context_driver_get_flags(&flags))
      if (BIT32_GET(flags.flags, testflag))
         return true;

   return false;
}

bool video_context_driver_set_flags(gfx_ctx_flags_t *flags)
{
   video_driver_state_t *video_st = &video_driver_st;
   if (!flags)
      return false;

   if (!video_st->current_video_context.set_flags)
   {
      video_st->deferred_flag_data.flags  = flags->flags;
      video_st->flags |= VIDEO_FLAG_DEFERRED_VIDEO_CTX_DRIVER_SET_FLAGS;
      return false;
   }

   video_st->current_video_context.set_flags(
         video_st->context_data, flags->flags);
   return true;

}

enum gfx_ctx_api video_context_driver_get_api(void)
{
   video_driver_state_t *video_st   = &video_driver_st;
   enum gfx_ctx_api         ctx_api = video_st->context_data
      ? video_st->current_video_context.get_api(video_st->context_data)
      : GFX_CTX_NONE;

   if (ctx_api == GFX_CTX_NONE)
   {
      const char *video_ident  = (video_st->current_video)
         ? video_st->current_video->ident
         : NULL;
      if (string_starts_with_size(video_ident, "d3d", STRLEN_CONST("d3d")))
      {
         if (string_is_equal(video_ident, "d3d9_hlsl"))
            return GFX_CTX_DIRECT3D9_API;
         else if (string_is_equal(video_ident, "d3d10"))
            return GFX_CTX_DIRECT3D10_API;
         else if (string_is_equal(video_ident, "d3d11"))
            return GFX_CTX_DIRECT3D11_API;
         else if (string_is_equal(video_ident, "d3d12"))
            return GFX_CTX_DIRECT3D12_API;
      }
      if (string_starts_with_size(video_ident, "gl", STRLEN_CONST("gl")))
      {
         if (string_is_equal(video_ident, "gl"))
            return GFX_CTX_OPENGL_API;
         else if (string_is_equal(video_ident, "gl1"))
            return GFX_CTX_OPENGL_API;
         else if (string_is_equal(video_ident, "glcore"))
            return GFX_CTX_OPENGL_API;
      }
      else if (string_is_equal(video_ident, "vulkan"))
         return GFX_CTX_VULKAN_API;
      else if (string_is_equal(video_ident, "metal"))
         return GFX_CTX_METAL_API;
      else if (string_is_equal(video_ident, "rsx"))
         return GFX_CTX_RSX_API;

      return GFX_CTX_NONE;
   }

   return ctx_api;
}

#if !(defined(RARCH_CONSOLE) || defined(RARCH_MOBILE))
bool video_driver_has_windowed(void)
{
   video_driver_state_t *video_st   = &video_driver_st;
   if (video_st->data && video_st->current_video->has_windowed)
      return video_st->current_video->has_windowed(video_st->data);
   return false;
}
#endif

bool video_shader_driver_get_current_shader(video_shader_ctx_t *shader)
{
   video_driver_state_t *video_st           = &video_driver_st;
   void *video_driver                       = video_st->data;
   const video_poke_interface_t *video_poke = video_st->poke;

   shader->data = NULL;
   if (!video_poke || !video_driver || !video_poke->get_current_shader)
      return false;
   shader->data = video_poke->get_current_shader(video_driver);
   return true;
}

float video_driver_get_refresh_rate(void)
{
   video_driver_state_t *video_st           = &video_driver_st;
   if (video_st->poke && video_st->poke->get_refresh_rate)
      return video_st->poke->get_refresh_rate(video_st->data);

   return 0.0f;
}

void video_driver_set_gpu_api_version_string(const char *str)
{
   video_driver_state_t *video_st           = &video_driver_st;
   strlcpy(video_st->gpu_api_version_string, str,
         sizeof(video_st->gpu_api_version_string));
}

const char* video_driver_get_gpu_api_version_string(void)
{
   video_driver_state_t *video_st           = &video_driver_st;
   return video_st->gpu_api_version_string;
}

bool video_driver_init_internal(bool *video_is_threaded, bool verbosity_enabled)
{
   video_info_t video;
   unsigned max_dim, scale, width, height;
   video_viewport_t *custom_vp            = NULL;
   input_driver_t *tmp                    = NULL;
   static uint16_t dummy_pixels[32]       = {0};
   runloop_state_t *runloop_st            = runloop_state_get_ptr();
   settings_t       *settings             = config_get_ptr();
   input_driver_state_t *input_st         = input_state_get_ptr();
   video_driver_state_t *video_st         = &video_driver_st;
   struct retro_game_geometry *geom       = &video_st->av_info.geometry;
   const enum retro_pixel_format
      video_driver_pix_fmt                = video_st->pix_fmt;
#ifdef HAVE_VIDEO_FILTER
   const char *path_softfilter_plugin     = settings->paths.path_softfilter_plugin;

   /* Init video filter only when game is running */
   if ((     runloop_st->current_core.flags & RETRO_CORE_FLAG_GAME_LOADED)
         && !string_is_empty(path_softfilter_plugin))
      video_driver_init_filter(video_driver_pix_fmt, settings);
#endif

   max_dim   = MAX(geom->max_width, geom->max_height);
   scale     = next_pow2(max_dim) / RARCH_SCALE_BASE;
   scale     = MAX(scale, 1);

#ifdef HAVE_VIDEO_FILTER
   if (video_st->state_filter)
      scale  = video_st->state_scale;
#endif

   strlcpy(aspectratio_lut[ASPECT_RATIO_CONFIG].name,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CONFIG),
         sizeof(aspectratio_lut[ASPECT_RATIO_CONFIG].name));
   strlcpy(aspectratio_lut[ASPECT_RATIO_CORE].name,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CORE_PROVIDED),
         sizeof(aspectratio_lut[ASPECT_RATIO_CORE].name));
   strlcpy(aspectratio_lut[ASPECT_RATIO_CUSTOM].name,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_CUSTOM),
         sizeof(aspectratio_lut[ASPECT_RATIO_CUSTOM].name));
   strlcpy(aspectratio_lut[ASPECT_RATIO_FULL].name,
         msg_hash_to_str(MENU_ENUM_LABEL_VALUE_VIDEO_ASPECT_RATIO_FULL),
         sizeof(aspectratio_lut[ASPECT_RATIO_FULL].name));

   /* Update core-dependent aspect ratio values. */
   video_driver_set_viewport_square_pixel(geom);
   video_driver_set_viewport_core();
   video_driver_set_viewport_config(geom,
         settings->floats.video_aspect_ratio,
         settings->bools.video_aspect_ratio_auto);

   /* Update CUSTOM viewport. */
   custom_vp = &settings->video_viewport_custom;

   if (settings->uints.video_aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
   {
      float default_aspect = aspectratio_lut[ASPECT_RATIO_CORE].value;
      aspectratio_lut[ASPECT_RATIO_CUSTOM].value =
         (custom_vp->width && custom_vp->height) ?
         (float)custom_vp->width / custom_vp->height : default_aspect;
   }

   {
      /* Guard against aspect ratio index possibly being out of bounds */
      unsigned new_aspect_idx = settings->uints.video_aspect_ratio_idx;
      if (new_aspect_idx > ASPECT_RATIO_END)
         new_aspect_idx       = settings->uints.video_aspect_ratio_idx = 0;
      video_st->aspect_ratio  = aspectratio_lut[new_aspect_idx].value;
   }

   if (     settings->bools.video_fullscreen
         || (video_st->flags & VIDEO_FLAG_FORCE_FULLSCREEN))
   {
      width  = settings->uints.video_fullscreen_x;
      height = settings->uints.video_fullscreen_y;
   }
   else
   {
#ifdef __WINRT__
      if (is_running_on_xbox())
      {
         width = uwp_get_width();
         height = uwp_get_height();
      }
      else
#endif
      {
#if (defined(_WIN32) && !defined(_XBOX) && !defined(__WINRT__)) ||  \
    (defined(HAVE_COCOA_METAL) && !defined(HAVE_COCOATOUCH))
         bool window_custom_size_enable = settings->bools.video_window_save_positions;
#else
         bool window_custom_size_enable = settings->bools.video_window_custom_size_enable;
#endif
         /* TODO/FIXME: remove when the new window resizing core is hooked */
         if (  window_custom_size_enable
            && settings->uints.window_position_width
            && settings->uints.window_position_height)
         {
            width  = settings->uints.window_position_width;
            height = settings->uints.window_position_height;
         }
         else
         {
            unsigned video_scale    = settings->uints.video_scale;
            /* Determine maximum allowed window dimensions
             * NOTE: We cannot read the actual display
             * metrics here, because the context driver
             * has not yet been initialised... */
             /* > Try explicitly configured values */
            unsigned max_win_width  = settings->uints.window_auto_width_max;
            unsigned max_win_height = settings->uints.window_auto_height_max;

            /* > Handle invalid settings */
            if ((max_win_width == 0) || (max_win_height == 0))
            {
               /* > Try configured fullscreen width/height */
               max_win_width = settings->uints.video_fullscreen_x;
               max_win_height = settings->uints.video_fullscreen_y;

               if ((max_win_width == 0) || (max_win_height == 0))
               {
                  /* Maximum window width/size *must* be non-zero;
                   * if all else fails, used defined default
                   * maximum window size */
                  max_win_width  = DEFAULT_WINDOW_AUTO_WIDTH_MAX;
                  max_win_height = DEFAULT_WINDOW_AUTO_HEIGHT_MAX;
               }
            }

            /* rotated games send unrotated width/height and
             * require special handling here because of it */
            if ((retroarch_get_rotation() % 2))
            {
                /* Determine nominal window size based on
                 * core geometry */
                if (settings->bools.video_force_aspect)
                {
                   /* Do rounding here to simplify integer
                    * scale correctness. */
                   unsigned base_width = roundf(geom->base_width *
                      video_st->aspect_ratio);
                   width = base_width * video_scale;
                }
                else
                   width = geom->base_height * video_scale;

                height = geom->base_width * video_scale;
            }
            else
            {
                /* Determine nominal window size based on
                 * core geometry */
                if (settings->bools.video_force_aspect)
                {
                   /* Do rounding here to simplify integer
                    * scale correctness. */
                   unsigned base_width = roundf(geom->base_height *
                      video_st->aspect_ratio);
                   width = base_width * video_scale;
                }
                else
                   width = geom->base_width * video_scale;

                height = geom->base_height * video_scale;
            }

            /* Cap window size to maximum allowed values */
            if ((width > max_win_width) || (height > max_win_height))
            {
               unsigned geom_width  = (width  > 0) ? width  : 1;
               unsigned geom_height = (height > 0) ? height : 1;
               float geom_aspect    = (float)geom_width    / (float)geom_height;
               float max_win_aspect = (float)max_win_width / (float)max_win_height;

               if (geom_aspect > max_win_aspect)
               {
                  width     = max_win_width;
                  height    = geom_height * max_win_width / geom_width;
                  /* Account for any possible rounding errors... */
                  if (height < 1)
                     height = 1;
                  else if (height > max_win_height)
                     height = max_win_height;
               }
               else
               {
                  height    = max_win_height;
                  width     = geom_width * max_win_height / geom_height;
                  /* Account for any possible rounding errors... */
                  if (width < 1)
                     width  = 1;
                  else if (width > max_win_width)
                     width  = max_win_width;
               }
            }
         }
      }
   }

   if (width && height)
      RARCH_LOG("[Video]: Set video size to: %ux%u.\n", width, height);
   else
      RARCH_LOG("[Video]: Set video size to: fullscreen.\n");

   video_st->display_type     = RARCH_DISPLAY_NONE;
   video_st->display          = 0;
   video_st->display_userdata = 0;
   video_st->window           = 0;

   video_st->scaler_ptr       = video_driver_pixel_converter_init(
         video_st->pix_fmt,
         VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st),
         RARCH_SCALE_BASE * scale);

   video.width                       = width;
   video.height                      = height;
   video.fullscreen                  = settings->bools.video_fullscreen
         || (video_st->flags & VIDEO_FLAG_FORCE_FULLSCREEN);
   video.vsync                       = settings->bools.video_vsync
         && (!(runloop_st->flags & RUNLOOP_FLAG_FORCE_NONBLOCK));
   video.force_aspect                = settings->bools.video_force_aspect;
   video.swap_interval               = runloop_get_video_swap_interval(
         settings->uints.video_swap_interval);
   video.adaptive_vsync              = settings->bools.video_adaptive_vsync;
#ifdef GEKKO
   video.viwidth                     = settings->uints.video_viwidth;
   video.vfilter                     = settings->bools.video_vfilter;
#endif
   video.smooth                      = settings->bools.video_smooth;
   video.ctx_scaling                 = settings->bools.video_ctx_scaling;
   video.input_scale                 = scale;
   video.font_enable                 = settings->bools.video_font_enable;
   video.font_size                   = settings->floats.video_font_size;
   video.path_font                   = settings->paths.path_font;
#ifdef HAVE_VIDEO_FILTER
   video.rgb32                       = video_st->state_filter
         ? (video_st->flags & VIDEO_FLAG_STATE_OUT_RGB32)
         : (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);
#else
   video.rgb32                       =
         (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_XRGB8888);
#endif
   video.parent                      = 0;

   if (video.fullscreen)
      video_st->flags |=  VIDEO_FLAG_STARTED_FULLSCREEN;
   else
      video_st->flags &= ~VIDEO_FLAG_STARTED_FULLSCREEN;
   /* Reset video frame count */
   video_st->frame_count             = 0;

   tmp                               = input_state_get_ptr()->current_driver;
   /* Need to grab the "real" video driver interface on a reinit. */
   video_driver_find_driver(settings, "video driver", verbosity_enabled);

#ifdef HAVE_THREADS
   video.is_threaded                 = VIDEO_DRIVER_IS_THREADED_INTERNAL(video_st);
   *video_is_threaded                = video.is_threaded;

   if (video.is_threaded)
   {
      bool ret;
      /* Can't do hardware rendering with threaded driver currently. */
      RARCH_LOG("[Video]: Starting threaded video driver..\n");

      ret = video_init_thread(
            (const video_driver_t**)&video_st->current_video,
            &video_st->data,
            &input_state_get_ptr()->current_driver,
            (void**)&input_state_get_ptr()->current_data,
            video_st->current_video,
            video);
      if (!ret)
      {
         RARCH_ERR("[Video]: Cannot open threaded video driver.. Exiting..\n");
         return false;
      }
   }
   else
#endif
      video_st->data = video_st->current_video->init(
            &video,
            &input_state_get_ptr()->current_driver,
            (void**)&input_state_get_ptr()->current_data);

   if (!video_st->data)
   {
      RARCH_ERR("[Video]: Cannot open video driver.. Exiting..\n");
      return false;
   }

   video_st->poke = NULL;
   if (video_st->current_video->poke_interface)
      video_st->current_video->poke_interface(
            video_st->data, &video_st->poke);

   if (video_st->current_video->viewport_info &&
         (!custom_vp->width  ||
          !custom_vp->height))
   {
      /* Force custom viewport to have sane parameters. */
      custom_vp->width = width;
      custom_vp->height = height;

      video_st->current_video->viewport_info(video_st->data, custom_vp);
   }

   video_driver_set_rotation(retroarch_get_rotation() % 4);

   video_st->current_video->suppress_screensaver(video_st->data,
         settings->bools.ui_suspend_screensaver_enable);

   if (!video_driver_init_input(tmp, settings, verbosity_enabled))
         return false;

#ifdef HAVE_OVERLAY
   input_overlay_unload();
   input_overlay_init();
#endif

   if (!(runloop_st->current_core.flags & RETRO_CORE_FLAG_GAME_LOADED))
   {
      video_st->frame_cache_data    = &dummy_pixels;
      video_st->frame_cache_width   = 4;
      video_st->frame_cache_height  = 4;
      video_st->frame_cache_pitch   = 8;
   }

#if defined(PSP)
   if (     video_st->poke
         && video_st->poke->set_texture_frame)
      video_st->poke->set_texture_frame(video_st->data,
            &dummy_pixels, false, 1, 1, 1.0f);
#endif

   video_context_driver_reset();

   video_display_server_init(video_st->display_type);

   if ((enum rotation)settings->uints.screen_orientation != ORIENTATION_NORMAL)
      video_display_server_set_screen_orientation((enum rotation)settings->uints.screen_orientation);

   /* Ensure that we preserve the 'grab mouse'
    * state if it was enabled prior to driver
    * (re-)initialisation */
   if (input_st->flags & INP_FLAG_GRAB_MOUSE_STATE)
   {
      if (     video_st->poke
            && video_st->poke->show_mouse)
         video_st->poke->show_mouse(video_st->data, false);
      if (input_driver_grab_mouse())
         input_st->flags |= INP_FLAG_GRAB_MOUSE_STATE;
   }
   else if (video.fullscreen)
   {
      if (     video_st->poke
            && video_st->poke->show_mouse)
         video_st->poke->show_mouse(video_st->data, false);
      if (!settings->bools.video_windowed_fullscreen)
         if (input_driver_grab_mouse())
            input_st->flags |= INP_FLAG_GRAB_MOUSE_STATE;
   }

#ifdef HAVE_OVERLAY
   input_overlay_check_mouse_cursor();
#endif

   return true;
}

void video_driver_frame(const void *data, unsigned width,
      unsigned height, size_t pitch)
{
   char status_text[128];
   static char video_driver_msg[256];
   static retro_time_t last_time;
   static retro_time_t curr_time;
   static retro_time_t fps_time;
   static retro_time_t frame_time_accumulator;
   static float last_fps, frame_time;
   static uint64_t last_used_memory, last_total_memory;
   /* Mark the start of nonblock state for
    * ignoring initial previous frame time */
   static int8_t nonblock_active;
   /* Initialise 'last_frame_duped' to 'true'
    * to ensure that the first frame is rendered */
   static bool last_frame_duped   = true;
   bool render_frame              = true;
   retro_time_t new_time;
   video_frame_info_t video_info;
   size_t buf_pos                 = 0;
   video_driver_state_t *video_st = &video_driver_st;
   runloop_state_t *runloop_st    = runloop_state_get_ptr();
   const enum retro_pixel_format
      video_driver_pix_fmt        = video_st->pix_fmt;
   bool runloop_idle              = (runloop_st->flags & RUNLOOP_FLAG_IDLE) ? true : false;
   bool video_driver_active       = (video_st->flags   & VIDEO_FLAG_ACTIVE) ? true : false;
#if defined(HAVE_GFX_WIDGETS)
   dispgfx_widget_t *p_dispwidget = dispwidget_get_ptr();
   bool widgets_active            = p_dispwidget->active;
#endif
   recording_state_t *recording_st= recording_state_get_ptr();

   status_text[0]                 = '\0';
   video_driver_msg[0]            = '\0';

   if (!video_driver_active)
      return;

   new_time                      = cpu_features_get_time_usec();

   if (data)
      video_st->frame_cache_data = data;
   video_st->frame_cache_width   = width;
   video_st->frame_cache_height  = height;
   video_st->frame_cache_pitch   = pitch;

   if (
            video_st->scaler_ptr
         && data
         && (video_driver_pix_fmt == RETRO_PIXEL_FORMAT_0RGB1555)
         && (data != RETRO_HW_FRAME_BUFFER_VALID)
         && video_pixel_frame_scale(
            video_st->scaler_ptr->scaler,
            video_st->scaler_ptr->scaler_out,
            data, width, height, pitch)
      )
   {
      data                = video_st->scaler_ptr->scaler_out;
      pitch               = video_st->scaler_ptr->scaler->out_stride;
   }

   video_driver_build_info(&video_info);

   /* Take target refresh rate as initial FPS value instead of 0.00 */
   if (!last_fps)
      last_fps = video_info.refresh_rate;

   /* If fast forward is active and fast forward
    * frame skipping is enabled, drop any frames
    * that occur at a rate higher than the core-set
    * refresh rate. However: We must always render
    * the current frame when:
    * - The menu is open
    * - The last frame was NULL and the
    *   current frame is not (i.e. if core was
    *   previously sending duped frames, ensure
    *   that the next frame update is captured) */
   if (   video_info.input_driver_nonblock_state
       && video_info.fastforward_frameskip
       &&  !((video_info.menu_st_flags & MENU_ST_FLAG_ALIVE)
       ||   (last_frame_duped && !!data)))
   {
      retro_time_t frame_time_accumulator_prev = frame_time_accumulator;
      retro_time_t frame_time_delta            = new_time - last_time;
      retro_time_t frame_time_target           = 1000000.0f / video_info.refresh_rate;

      /* Ignore initial previous frame time
       * to prevent rubber band startup */
      if (!nonblock_active)
         nonblock_active = -1;
      else if (nonblock_active < 0)
         nonblock_active = 1;

      /* Accumulate the elapsed time since the last frame */
      if (nonblock_active > 0)
         frame_time_accumulator += frame_time_delta;

      /* Render frame if the accumulated time is
       * greater than or equal to the expected
       * core frame time */
      render_frame = frame_time_accumulator >= frame_time_target;

      /* If frame is to be rendered, subtract
       * expected frame time from accumulator */
      if (render_frame)
      {
         frame_time_accumulator -= frame_time_target;

         /* Prevent external frame limiters from
          * pushing fast forward ratio down to 1x */
         if (frame_time_accumulator_prev - frame_time_accumulator >= frame_time_delta)
            frame_time_accumulator -= frame_time_delta;

         if (frame_time_accumulator < 0)
            frame_time_accumulator = 0;

         /* If fast forward is working correctly,
          * the actual frame time will always be
          * less than the expected frame time.
          * But if the host cannot run the core
          * fast enough to achieve at least 1x
          * speed then the frame time accumulator
          * will never empty and may potentially
          * overflow. If a 'runaway' accumulator
          * is detected, we simply reset it */
         if (frame_time_accumulator > frame_time_target)
            frame_time_accumulator = 0;
      }
   }
   else
   {
      nonblock_active        = 0;
      frame_time_accumulator = 0;
   }

   last_time        = new_time;
   last_frame_duped = !data;

   /* Get the amount of frames per seconds. */
   if (video_st->frame_count)
   {
      unsigned fps_update_interval              = video_info.fps_update_interval;
      unsigned memory_update_interval           = video_info.memory_update_interval;
      /* set this to 1 to avoid an offset issue */
      unsigned write_index                      = video_st->frame_time_count++
                                               & (MEASURE_FRAME_TIME_SAMPLES_COUNT - 1);
      frame_time                                = new_time - fps_time;
      video_st->frame_time_samples[write_index] = frame_time;
      fps_time                                  = new_time;

      if (video_info.fps_show)
      {
         status_text[  buf_pos] = 'F';
         status_text[++buf_pos] = 'P';
         status_text[++buf_pos] = 'S';
         status_text[++buf_pos] = ':';
         status_text[++buf_pos] = ' ';
         status_text[++buf_pos] = '\0';
         buf_pos               += snprintf(
               status_text         + buf_pos,
               sizeof(status_text) - buf_pos,
               "%6.2f", last_fps);
      }

      if (video_info.framecount_show)
      {
         if (status_text[buf_pos-1] != '\0')
         {
            status_text[  buf_pos] = ' ';
            status_text[++buf_pos] = '|';
            status_text[++buf_pos] = '|';
            status_text[++buf_pos] = ' ';
            status_text[++buf_pos] = '\0';
         }
         buf_pos += strlcpy(status_text + buf_pos,
               msg_hash_to_str(MSG_FRAMES),
               sizeof(status_text)      - buf_pos);
         status_text[buf_pos  ]    = ':';
         status_text[++buf_pos]    = ' ';
         status_text[++buf_pos]    = '\0';
         buf_pos                  += snprintf(status_text + buf_pos,
               sizeof(status_text) - buf_pos,
               "%" PRIu64, (uint64_t)video_st->frame_count);
      }

      if (video_info.memory_show)
      {
         if ((video_st->frame_count % memory_update_interval) == 0)
         {
            last_total_memory = frontend_driver_get_total_memory();
            last_used_memory  = last_total_memory - frontend_driver_get_free_memory();
         }

         if (status_text[buf_pos-1] != '\0')
         {
            status_text[buf_pos]   = ' ';
            status_text[++buf_pos] = '|';
            status_text[++buf_pos] = '|';
            status_text[++buf_pos] = ' ';
            status_text[++buf_pos] = '\0';
         }
         status_text[buf_pos  ]    = 'M';
         status_text[++buf_pos]    = 'E';
         status_text[++buf_pos]    = 'M';
         status_text[++buf_pos]    = ':';
         status_text[++buf_pos]    = ' ';
         status_text[++buf_pos]    = '\0';
         buf_pos                  += snprintf(
               status_text + buf_pos, sizeof(status_text) - buf_pos, "%.2f/%.2f", last_used_memory / (1024.0f * 1024.0f),
               last_total_memory / (1024.0f * 1024.0f));
         status_text[buf_pos  ]   = 'M';
         status_text[++buf_pos]   = 'B';
         status_text[++buf_pos]   = '\0';
      }

      if ((video_st->frame_count % fps_update_interval) == 0)
      {
         size_t new_len;
         last_fps = TIME_TO_FPS(curr_time, new_time,
               fps_update_interval);

         new_len = strlcpy(video_st->window_title, video_st->title_buf,
               sizeof(video_st->window_title));

         if (!string_is_empty(status_text))
         {
            video_st->window_title[  new_len  ] = ' ';
            video_st->window_title[++new_len  ] = '|';
            video_st->window_title[++new_len  ] = '|';
            video_st->window_title[++new_len  ] = ' ';
            video_st->window_title[++new_len  ] = '\0';
            new_len += strlcpy(
                  video_st->window_title         + new_len,
                  status_text,
                  sizeof(video_st->window_title) - new_len);
         }

         curr_time                  = new_time;
         video_st->window_title_len = new_len;
         video_st->flags           |= VIDEO_FLAG_WINDOW_TITLE_UPDATE;
      }
   }
   else
   {
      curr_time = fps_time = new_time;

      video_st->window_title_len = strlcpy(
            video_st->window_title,
            video_st->title_buf,
            sizeof(video_st->window_title));

      if (video_info.fps_show)
         buf_pos = strlcpy(status_text,
               msg_hash_to_str(MENU_ENUM_LABEL_VALUE_NOT_AVAILABLE),
               sizeof(status_text));

      video_st->flags |= VIDEO_FLAG_WINDOW_TITLE_UPDATE;
   }

   /* Add core status message to status text */
   if (video_info.core_status_msg_show)
   {
      /* Note: We need to lock a mutex here. Strictly
       * speaking, runloop_core_status_msg is not part
       * of the message queue, but:
       * - It may be implemented as a queue in the future
       * - It seems unnecessary to create a new slock_t
       *   object for this type of message when
       *   _runloop_msg_queue_lock is already available
       * We therefore just call runloop_msg_queue_lock()/
       * runloop_msg_queue_unlock() in this case */
      RUNLOOP_MSG_QUEUE_LOCK(runloop_st);

      /* Check whether duration timer has elapsed */
      runloop_st->core_status_msg.duration -= anim_get_ptr()->delta_time;

      if (runloop_st->core_status_msg.duration < 0.0f)
      {
         runloop_st->core_status_msg.str[0]   = '\0';
         runloop_st->core_status_msg.priority = 0;
         runloop_st->core_status_msg.duration = 0.0f;
         runloop_st->core_status_msg.set      = false;
      }
      else
      {
         /* If status text is already set, add status
          * message at the end */
         if (!string_is_empty(status_text))
         {
            status_text[buf_pos    ] = ' ';
            status_text[++buf_pos  ] = '|';
            status_text[++buf_pos  ] = '|';
            status_text[++buf_pos  ] = ' ';
            status_text[++buf_pos  ] = '\0';
            strlcpy(
                  status_text         + buf_pos,
                  runloop_st->core_status_msg.str,
                  sizeof(status_text) - buf_pos);
         }
         else
            strlcpy(status_text,
                  runloop_st->core_status_msg.str,
                  sizeof(status_text));
      }

      RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st);
   }

   /* Slightly messy code,
    * but we really need to do processing before blocking on VSync
    * for best possible scheduling.
    */
   if (
         (
#ifdef HAVE_VIDEO_FILTER
             !video_st->state_filter ||
#endif
             !video_info.post_filter_record
          || !data
          || video_st->record_gpu_buffer
         ) && recording_st->data
           && recording_st->driver
           && recording_st->driver->push_video)
      recording_dump_frame(
            data, width, height,
            pitch, runloop_idle);

#ifdef HAVE_VIDEO_FILTER
   if (render_frame && data && video_st->state_filter)
   {
      unsigned output_width                             = 0;
      unsigned output_height                            = 0;
      unsigned output_pitch                             = 0;

      rarch_softfilter_get_output_size(video_st->state_filter,
            &output_width, &output_height, width, height);

      output_pitch = (output_width) * video_st->state_out_bpp;

      rarch_softfilter_process(video_st->state_filter,
            video_st->state_buffer, output_pitch,
            data, width, height, pitch);

      if (     video_info.post_filter_record
            && recording_st->data
            && recording_st->driver
            && recording_st->driver->push_video)
         recording_dump_frame(
               video_st->state_buffer,
               output_width, output_height, output_pitch,
               runloop_idle);

      data   = video_st->state_buffer;
      width  = output_width;
      height = output_height;
      pitch  = output_pitch;
   }
#endif

   if (runloop_st->msg_queue_delay > 0)
      runloop_st->msg_queue_delay--;
   else if (runloop_st->msg_queue_size > 0)
   {
      /* If widgets are currently enabled, then
       * messages were pushed to the queue before
       * widgets were initialised - in this case, the
       * first item in the message queue should be
       * extracted and pushed to the widget message
       * queue instead */
#if defined(HAVE_GFX_WIDGETS)
      if (widgets_active)
      {
         msg_queue_entry_t msg_entry;
         bool msg_found                  = false;

         RUNLOOP_MSG_QUEUE_LOCK(runloop_st);
         msg_found                       = msg_queue_extract(
               &runloop_st->msg_queue, &msg_entry);
         runloop_st->msg_queue_size      = msg_queue_size(
               &runloop_st->msg_queue);
         RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st);

         if (msg_found)
            gfx_widgets_msg_queue_push(
                  NULL,
                  msg_entry.msg,
                  roundf((float)msg_entry.duration / 60.0f * 1000.0f),
                  msg_entry.title,
                  msg_entry.icon,
                  msg_entry.category,
                  msg_entry.prio,
                  false,
                  (video_info.menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false
            );
      }
      /* ...otherwise, just output message via
       * regular OSD notification text (if enabled) */
      else if (video_info.font_enable)
#else
      if (video_info.font_enable)
#endif
      {
         const char *msg                 = NULL;
         RUNLOOP_MSG_QUEUE_LOCK(runloop_st);
         msg                             = msg_queue_pull(&runloop_st->msg_queue);
         runloop_st->msg_queue_size      = msg_queue_size(&runloop_st->msg_queue);

         if (msg)
            strlcpy(video_driver_msg, msg, sizeof(video_driver_msg));
         RUNLOOP_MSG_QUEUE_UNLOCK(runloop_st);
      }
   }

   if (render_frame && video_info.statistics_show)
   {
      audio_statistics_t audio_stats;
      char throttle_stats[128];
      char latency_stats[128];
      char tmp[128];
      size_t len;
      double stddev                          = 0.0;
      float font_size_scale                  = video_info.font_size / 100;
      struct retro_system_av_info *av_info   = &video_st->av_info;
      unsigned red                           = 235;
      unsigned green                         = 235;
      unsigned blue                          = 235;
      unsigned alpha                         = 255;
      float scale                            = ((float)video_info.height / 480)
            * 0.50f * (DEFAULT_FONT_SIZE / video_info.font_size);
      /* Clamp scale */
      if (scale < font_size_scale)
         scale                               = font_size_scale;
      if (scale > 1.00f)
         scale                               =  1.00f;

      if (scale > font_size_scale)
      {
         scale *= 100;
         scale  = ceil(scale);
         scale /= 100;
      }

      audio_stats.samples                    = 0;
      audio_stats.average_buffer_saturation  = 0.0f;
      audio_stats.std_deviation_percentage   = 0.0f;
      audio_stats.close_to_underrun          = 0.0f;
      audio_stats.close_to_blocking          = 0.0f;

      video_monitor_fps_statistics(NULL, &stddev, NULL);

      video_info.osd_stat_params.x           = 0.008f;
      video_info.osd_stat_params.y           = 0.960f;
      video_info.osd_stat_params.text_align  = TEXT_ALIGN_LEFT;
      video_info.osd_stat_params.scale       = scale;
      video_info.osd_stat_params.full_screen = true;
      video_info.osd_stat_params.drop_x      = (video_info.font_size / DEFAULT_FONT_SIZE) * 3;
      video_info.osd_stat_params.drop_y      = (video_info.font_size / DEFAULT_FONT_SIZE) * -3;
      video_info.osd_stat_params.drop_mod    = 0.1f;
      video_info.osd_stat_params.drop_alpha  = 0.9f;
      video_info.osd_stat_params.color       = COLOR_ABGR(
            alpha, blue, green, red);

      audio_compute_buffer_statistics(&audio_stats);

      throttle_stats[0] = '\0';
      latency_stats[0]  = '\0';
      tmp[0]            = '\0';
      len               = 0;

      if (video_info.frame_rest)
         len = snprintf(tmp + len, sizeof(throttle_stats),
               " Frame Rest:  %2u.00 ms\n"
               " - Rested:    %5.2f %%\n",
               video_st->frame_rest,
               (float)video_st->frame_rest_time_count / runloop_st->core_runtime_usec * 100);

      if (len)
      {
         /* TODO/FIXME - localize */
         size_t _len = strlcpy(throttle_stats, "THROTTLE\n", sizeof(throttle_stats));
         strlcpy(throttle_stats + _len, tmp, sizeof(throttle_stats) - _len);
      }

      tmp[0]            = '\0';
      len               = 0;

      /* TODO/FIXME - localize */
      if (video_st->frame_delay_target > 0)
         len = snprintf(tmp, sizeof(latency_stats),
               " Frame Delay: %2u ms\n"
               " - Target:    %2u ms\n",
               video_st->frame_delay_effective,
               video_st->frame_delay_target);

      if (video_info.runahead && !video_info.runahead_second_instance)
         len = snprintf(tmp + len, sizeof(latency_stats),
               " Run-Ahead:   %2u frames\n"
               " - Single Instance\n",
               video_info.runahead_frames);
      else if (video_info.runahead && video_info.runahead_second_instance)
         len = snprintf(tmp + len, sizeof(latency_stats),
               " Run-Ahead:   %2u frames\n"
               " - Second Instance\n",
               video_info.runahead_frames);
      else if (video_info.preemptive_frames)
         len = snprintf(tmp + len, sizeof(latency_stats),
               " Run-Ahead:   %2u frames\n"
               " - Preemptive Frames\n",
               video_info.runahead_frames);

      if (len)
      {
         /* TODO/FIXME - localize */
         size_t _len = strlcpy(latency_stats, "LATENCY\n", sizeof(latency_stats));
         strlcpy(latency_stats + _len, tmp, sizeof(latency_stats) - _len);
      }

      /* TODO/FIXME - localize */
      snprintf(video_info.stat_text,
            sizeof(video_info.stat_text),
            "CORE AV_INFO\n"
            " Size:        %u x %u\n"
            " Max Size:    %u x %u\n"
            " Aspect:      %3.3f\n"
            " FPS:         %3.2f\n"
            " Sample Rate: %6.2f\n"
            "VIDEO: %s\n"
            " Viewport:    %d x %d\n"
            " Refresh:     %5.2f hz\n"
            " Frame Rate:  %5.2f fps\n"
            " Frame Time:  %5.2f ms\n"
            " - Deviation: %5.2f %%\n"
            " Frames:      %5" PRIu64"\n"
            "AUDIO: %s\n"
            " Saturation:  %5.2f %%\n"
            " Deviation:   %5.2f %%\n"
            " Underrun:    %5.2f %%\n"
            " Blocking:    %5.2f %%\n"
            " Samples:     %5d\n"
            "%s"
            "%s",
            av_info->geometry.base_width,
            av_info->geometry.base_height,
            av_info->geometry.max_width,
            av_info->geometry.max_height,
            av_info->geometry.aspect_ratio,
            av_info->timing.fps,
            av_info->timing.sample_rate,
            video_st->current_video->ident,
            video_info.width,
            video_info.height,
            video_info.refresh_rate,
            last_fps,
            frame_time / 1000.0f,
            100.0f * stddev,
            video_st->frame_count,
            audio_state_get_ptr()->current_audio->ident,
            audio_stats.average_buffer_saturation,
            audio_stats.std_deviation_percentage,
            audio_stats.close_to_underrun,
            audio_stats.close_to_blocking,
            audio_stats.samples,
            throttle_stats,
            latency_stats);

      /* TODO/FIXME - add OSD chat text here */
   }

   if (render_frame
         && video_st->current_video
         && video_st->current_video->frame)
   {
      if (video_st->current_video->frame(
               video_st->data, data, width, height,
               video_st->frame_count, (unsigned)pitch,
                  ((video_info.menu_st_flags & MENU_ST_FLAG_SCREENSAVER_ACTIVE) > 0)
               || video_info.notifications_hidden ? "" : video_driver_msg,
               &video_info))
         video_st->flags |=  VIDEO_FLAG_ACTIVE;
      else
         video_st->flags &= ~VIDEO_FLAG_ACTIVE;
   }

   video_st->frame_count++;

   /* Display the status text, with a higher priority. */
   if (  (   video_info.fps_show
          || video_info.framecount_show
          || video_info.memory_show
          || video_info.core_status_msg_show
         )
       && !((video_info.menu_st_flags & MENU_ST_FLAG_SCREENSAVER_ACTIVE))
       && !video_info.notifications_hidden
      )
   {
#if defined(HAVE_GFX_WIDGETS)
      if (widgets_active)
         strlcpy(
               p_dispwidget->gfx_widgets_status_text,
               status_text,
               sizeof(p_dispwidget->gfx_widgets_status_text)
               );
      else
#endif
      {
         runloop_msg_queue_push(status_text, 2, 1, true, NULL,
               MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
      }
   }

#if defined(HAVE_CRTSWITCHRES)
   /* trigger set resolution*/
   if (video_info.crt_switch_resolution)
   {
      unsigned native_width     = width;
      bool dynamic_super_width  = false;

      video_st->flags          |= VIDEO_FLAG_CRT_SWITCHING_ACTIVE;

      switch (video_info.crt_switch_resolution_super)
      {
         case 2560:
         case 3840:
         case 1920:
            width               = video_info.crt_switch_resolution_super;
            break;
         case 1:
            dynamic_super_width = true;
            break;
         default:
            break;
      }

      crt_switch_res_core(
            &video_st->crt_switch_st,
            native_width, width,
            height,
            video_st->core_hz,
            retroarch_get_rotation() & 1,
            video_info.crt_switch_resolution,
            video_info.crt_switch_center_adjust,
            video_info.crt_switch_porch_adjust,
            video_info.monitor_index,
            dynamic_super_width,
            video_info.crt_switch_resolution_super,
            video_info.crt_switch_hires_menu);
   }
   else if (!video_info.crt_switch_resolution)
#endif
      video_st->flags          &= ~VIDEO_FLAG_CRT_SWITCHING_ACTIVE;
}

static void video_driver_reinit_context(settings_t *settings, int flags)
{
   /* RARCH_DRIVER_CTL_UNINIT clears the callback struct so we
    * need to make sure to keep a copy */
   struct retro_hw_render_callback hwr_copy;
   video_driver_state_t *video_st       = &video_driver_st;
   struct retro_hw_render_callback *hwr =
      VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st);
   const struct retro_hw_render_context_negotiation_interface *iface =
      video_st->hw_render_context_negotiation;
   memcpy(&hwr_copy, hwr, sizeof(hwr_copy));

   driver_uninit(flags, DRIVER_LIFETIME_RESET);

   memcpy(hwr, &hwr_copy, sizeof(*hwr));
   video_st->hw_render_context_negotiation = iface;

   drivers_init(settings, flags, DRIVER_LIFETIME_RESET, verbosity_is_enabled());
}

void video_driver_reinit(int flags)
{
   settings_t *settings                    = config_get_ptr();
   video_driver_state_t *video_st          = &video_driver_st;
   struct retro_hw_render_callback *hwr    =
         VIDEO_DRIVER_GET_HW_CONTEXT_INTERNAL(video_st);

   if (hwr->cache_context != false)
      video_st->flags                     |=  VIDEO_FLAG_CACHE_CONTEXT;
   else
      video_st->flags                     &= ~VIDEO_FLAG_CACHE_CONTEXT;
   video_st->flags                        &= ~VIDEO_FLAG_CACHE_CONTEXT_ACK;
   video_driver_reinit_context(settings, flags);
   video_st->flags                        &= ~VIDEO_FLAG_CACHE_CONTEXT;

   video_st->window_title_prev[0]          = '\0';
}

void video_frame_delay(video_driver_state_t *video_st,
      settings_t *settings,
      bool core_paused)
{
   runloop_state_t *runloop_st          = runloop_state_get_ptr();
   unsigned video_frame_delay           = settings->uints.video_frame_delay;
   unsigned video_frame_delay_effective = video_st->frame_delay_effective;
   bool skip_delay                      = core_paused
         || (runloop_st->flags & RUNLOOP_FLAG_SLOWMOTION)
         || (runloop_st->flags & RUNLOOP_FLAG_FASTMOTION);

   if (settings->bools.video_frame_delay_auto)
   {
      float refresh_rate          = settings->floats.video_refresh_rate;
      uint8_t video_swap_interval = runloop_get_video_swap_interval(
            settings->uints.video_swap_interval);
      uint8_t video_bfi           = settings->uints.video_black_frame_insertion;
      uint8_t frame_time_interval = 8;
      static uint8_t skip_update  = 0;
      static bool skip_delay_prev = false;
      bool frame_time_update      =
            /* Skip some initial frames for stabilization */
               video_st->frame_count > frame_time_interval
            /* Only update when there are enough frames for averaging */
            && video_st->frame_count % frame_time_interval == 0;

      /* A few frames must be ignored after slow+fastmotion/pause
       * is disabled or geometry change is triggered */
      if (     (!skip_delay && skip_delay_prev)
            || video_st->frame_delay_pause)
      {
         skip_update = frame_time_interval * 4;
         video_st->frame_delay_pause = false;
      }

      if (skip_update)
         skip_update--;

      skip_delay_prev = skip_delay;

      /* Always skip when slow+fastmotion/pause is active */
      if (skip_delay_prev)
         skip_update = 1;

      if (skip_update)
         frame_time_update = false;

      /* Black frame insertion + swap interval multiplier */
      refresh_rate = (refresh_rate / (video_bfi + 1.0f) / video_swap_interval);

      /* Set target moderately as half frame time with 0 (Auto) delay */
      if (video_frame_delay == 0)
         video_frame_delay = 1 / refresh_rate * 1000 / 2;

      /* Reset new desired delay target */
      if (video_st->frame_delay_target != video_frame_delay)
      {
         frame_time_update             = false;
         video_st->frame_delay_target  = video_frame_delay_effective = video_frame_delay;
         RARCH_LOG("[Video]: Frame delay reset to %d ms.\n", video_frame_delay);
      }

      /* Decide what should happen to effective delay */
      if (video_frame_delay_effective > 0 && frame_time_update)
      {
         video_frame_delay_auto_t vfda   = {0};
         vfda.frame_time_interval        = frame_time_interval;
         vfda.refresh_rate               = refresh_rate;

         video_frame_delay_auto(video_st, &vfda);
         if (vfda.delay_decrease > 0)
         {
            video_frame_delay_effective -= vfda.delay_decrease;
            RARCH_LOG("[Video]: Frame delay decrease by %d ms to %d ms due to frame time average: %d > %d.\n",
                  vfda.delay_decrease, video_frame_delay_effective, vfda.frame_time_avg, vfda.frame_time_target);
         }
      }
   }
   else
      video_st->frame_delay_target = video_frame_delay_effective = video_frame_delay;

   video_st->frame_delay_effective = video_frame_delay_effective;

   /* Never apply frame delay when slow+fastmotion/pause is active */
   if (video_frame_delay_effective > 0 && !skip_delay)
      retro_sleep(video_frame_delay_effective);
}

void video_frame_delay_auto(video_driver_state_t *video_st, video_frame_delay_auto_t *vfda)
{
   int i;
   retro_time_t frame_time_avg        = 0;
   retro_time_t frame_time_delta      = 0;
   uint16_t frame_time_index          = (video_st->frame_time_count & (MEASURE_FRAME_TIME_SAMPLES_COUNT - 1));
   retro_time_t frame_time_target     = 1000000.0f / vfda->refresh_rate;
   retro_time_t frame_time_limit_mar  = frame_time_target * 1.15f;
   retro_time_t frame_time_limit_min  = frame_time_target * 1.30f;
   retro_time_t frame_time_limit_med  = frame_time_target * 1.50f;
   retro_time_t frame_time_limit_max  = frame_time_target * 1.85f;
   retro_time_t frame_time_limit_cap  = frame_time_target * 3.00f;
   retro_time_t frame_time_limit_ign  = frame_time_target * 3.25f;
   uint8_t  frame_time_frames         = vfda->frame_time_interval;
   uint8_t  frame_time_count_pos      = 0;
   uint8_t  frame_time_count_min      = 0;
   uint8_t  frame_time_count_med      = 0;
   uint8_t  frame_time_count_max      = 0;
   uint8_t  frame_time_count_ign      = 0;
   retro_time_t frame_time_min        = frame_time_target;
   retro_time_t frame_time_max        = frame_time_target;

   /* Calculate average frame time */
   for (i = 1; i < frame_time_frames + 1; i++)
   {
      retro_time_t frame_time_i = 0;

      if (i > frame_time_index)
         continue;

      frame_time_i = video_st->frame_time_samples[frame_time_index - i];

      if (frame_time_max < frame_time_i)
         frame_time_max = frame_time_i;
      if (frame_time_min > frame_time_i)
         frame_time_min = frame_time_i;

      /* Count frames over the target */
      if (frame_time_i > frame_time_target)
      {
         frame_time_count_pos++;
         if (frame_time_i > frame_time_limit_min)
            frame_time_count_min++;
         if (frame_time_i > frame_time_limit_med)
            frame_time_count_med++;
         if (frame_time_i > frame_time_limit_max)
            frame_time_count_max++;
         if (frame_time_i > frame_time_limit_ign)
            frame_time_count_ign++;

         /* Limit maximum to prevent false positives */
         if (frame_time_i > frame_time_limit_cap)
            frame_time_i = frame_time_limit_cap;
      }

      frame_time_avg  += frame_time_i;
   }

   frame_time_avg     /= frame_time_frames;
   frame_time_delta    = frame_time_max - frame_time_min;

   /* Ignore values when core is doing internal frame skipping */
   if (frame_time_count_ign > 0)
      frame_time_avg = frame_time_target;

   /* Special handlings for different video driver frame timings */
   if (     (  frame_time_avg > frame_time_target
            && frame_time_avg < frame_time_limit_med)
         || (frame_time_delta > frame_time_limit_max))
   {
      uint8_t  frame_time_frames_half = frame_time_frames / 2;
      /* Ensure outcome on certain conditions */
      int8_t   mode                   = 0;

      /* All interval frames are above the target */
      if (frame_time_count_pos == frame_time_frames)
         mode = 1;
      /* At least half of interval frames are above minimum level */
      else if (frame_time_count_min >= frame_time_frames_half)
         mode = 2;
      /* D3Dx stripe equalizer */
      else if (
               (frame_time_count_pos == frame_time_frames_half)
            && (  frame_time_count_min > 0
               || frame_time_count_med > 0)
            && (frame_time_delta > frame_time_target / 2.50f)
         )
         mode = 3;
      /* Boost med/max spikes */
      else if (
               (frame_time_count_pos > frame_time_frames_half)
            && (  frame_time_count_med > 1
               || frame_time_count_max > 0)
            && (frame_time_avg   > frame_time_limit_mar)
            && (frame_time_delta < frame_time_limit_min)
         )
         mode = 4;
      /* Boost min/med spikes */
      else if (
               (frame_time_count_pos >= frame_time_frames_half)
            && (  frame_time_count_min > 1
               || frame_time_count_med > 0)
            && (frame_time_count_max == 0)
            && (frame_time_avg > frame_time_limit_mar)
         )
         mode = 5;
      /* Ignore */
      else if (
               (frame_time_delta > frame_time_target)
            && (frame_time_count_med == 0)
         )
         mode = -1;

      if (mode > 0 && frame_time_avg < frame_time_limit_med)
      {
#if FRAME_DELAY_AUTO_DEBUG
         RARCH_LOG("[Video]: Frame delay nudge %d by mode %d.\n", frame_time_avg, mode);
#endif
         frame_time_avg = frame_time_limit_med;
      }
      else if (mode < 0)
      {
#if FRAME_DELAY_AUTO_DEBUG
         RARCH_LOG("[Video]: Frame delay ignore %d.\n", frame_time_avg);
#endif
         frame_time_avg = frame_time_target;
      }
   }

   /* Final output decision */
   if (frame_time_avg > frame_time_limit_min)
   {
      uint8_t delay_decrease = 1;

      /* Increase decrease the more frame time is off target */
      if (     frame_time_avg > frame_time_limit_med
            && video_st->frame_delay_effective > delay_decrease)
      {
         delay_decrease++;
         if (     frame_time_avg > frame_time_limit_max
               && video_st->frame_delay_effective > delay_decrease)
            delay_decrease++;
      }

      vfda->delay_decrease = delay_decrease;
   }

   vfda->frame_time_avg     = frame_time_avg;
   vfda->frame_time_target  = frame_time_target;

#if FRAME_DELAY_AUTO_DEBUG
   if (frame_time_index > frame_time_frames)
      RARCH_LOG("[Video]: %5d / pos:%d min:%d med:%d max:%d ign:%d / delta:%5d = %5d %5d %5d %5d %5d %5d %5d %5d\n",
            frame_time_avg,
            frame_time_count_pos,
            frame_time_count_min,
            frame_time_count_med,
            frame_time_count_max,
            frame_time_count_ign,
            frame_time_max - frame_time_min,
            video_st->frame_time_samples[frame_time_index - 1],
            video_st->frame_time_samples[frame_time_index - 2],
            video_st->frame_time_samples[frame_time_index - 3],
            video_st->frame_time_samples[frame_time_index - 4],
            video_st->frame_time_samples[frame_time_index - 5],
            video_st->frame_time_samples[frame_time_index - 6],
            video_st->frame_time_samples[frame_time_index - 7],
            video_st->frame_time_samples[frame_time_index - 8]
      );
#endif
}

void video_frame_rest(video_driver_state_t *video_st,
      settings_t *settings,
      retro_time_t current_time)
{
#ifdef HAVE_MENU
   bool menu_is_pausing              = settings->bools.menu_pause_libretro && (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE);
#else
   bool menu_is_pausing              = false;
#endif
   runloop_state_t *runloop_st       = runloop_state_get_ptr();
   retro_time_t latest_time          = cpu_features_get_time_usec();
   retro_time_t frame_time_delta     = latest_time - current_time;
   retro_time_t frame_time_target    = 1000000.0f / settings->floats.video_refresh_rate;
   retro_time_t frame_time           = 0;
   static retro_time_t after_present = 0;
   int sleep_max                     = frame_time_target / 1000 / 2;
   int sleep                         = 0;
   int frame_time_near_req_count     = ceil(settings->floats.video_refresh_rate / 2);
   static int frame_time_over_count  = 0;
   static int frame_time_near_count  = 0;
   static int frame_time_try_count   = 0;
   double video_stddev               = 0;
   audio_statistics_t audio_stats;

   /* Must require video and audio deviation standards */
   video_monitor_fps_statistics(NULL, &video_stddev, NULL);
   audio_compute_buffer_statistics(&audio_stats);

   /* Don't care about deviations when core is not running */
   if (      (runloop_st->flags & RUNLOOP_FLAG_PAUSED)
         || !(runloop_st->flags & RUNLOOP_FLAG_CORE_RUNNING)
         || menu_is_pausing)
      video_stddev = audio_stats.std_deviation_percentage = 0;

   /* Compare to previous timestamp */
   frame_time                        = latest_time - after_present;

   /* Count running timers */
   if (frame_time > frame_time_target)
      frame_time_over_count++;
   else if (frame_time < frame_time_target)
      frame_time_over_count--;

   if (labs(frame_time - frame_time_target) < frame_time_target * 1.002f - frame_time_target)
      frame_time_near_count++;
   else
      frame_time_near_count--;

   /* Take new timestamp */
   after_present                     = latest_time;

   /* Ignore unreasonable frame times */
   if (     frame_time < frame_time_target / 2
         || frame_time > frame_time_target * 2)
      return;

   /* Carry the extra */
   frame_time_delta                 -= frame_time_target - frame_time;
   sleep                             = (frame_time_delta > 0) ? frame_time_delta : 0;

   /* No rest with bogus values */
   if (     sleep < 0
         || (  frame_time_target < frame_time_delta
            && frame_time_target < frame_time))
      sleep                          = 0;

   /* Reset over the target counter */
   if (!sleep)
      frame_time_over_count          = 0;

   frame_time_try_count++;
   if (     frame_time_try_count > frame_time_near_req_count * 2
         || frame_time_try_count < frame_time_near_count)
      frame_time_over_count          = frame_time_near_count = frame_time_try_count = 0;

   /* Increase */
   if (sleep
         && (frame_time_over_count < 2)
         && (video_stddev * 100.0f < 25.00f)
         && (audio_stats.std_deviation_percentage < 25.00f)
         && (frame_time_near_count > frame_time_try_count / 2)
         && (frame_time_near_count > frame_time_near_req_count)
      )
   {
#if FRAME_REST_DEBUG
      RARCH_LOG("+ frame=%5d delta=%5d sleep=%2d over=%3d near=%3d try=%3d\n", frame_time, frame_time_delta, video_st->frame_rest, frame_time_over_count, frame_time_near_count, frame_time_try_count);
#endif
      video_st->frame_rest++;
      frame_time_over_count          = frame_time_near_count = frame_time_try_count = 0;
   }
   /* Decrease */
   else if (     sleep
         && frame_time_over_count != 0
         && frame_time_try_count > 10
         && (  (frame_time_near_count < -2 && -frame_time_near_count > frame_time_try_count)
            || (frame_time_over_count > frame_time_near_req_count / 2)
            || (frame_time_over_count < -(frame_time_near_req_count / 2))
            )
      )
   {
#if FRAME_REST_DEBUG
      RARCH_LOG("- frame=%5d delta=%5d sleep=%2d over=%3d near=%3d try=%3d\n", frame_time, frame_time_delta, video_st->frame_rest, frame_time_over_count, frame_time_near_count, frame_time_try_count);
#endif
      if (video_st->frame_rest)
         video_st->frame_rest--;
      frame_time_over_count          = frame_time_near_count = frame_time_try_count = 0;
   }

   /* Limit to maximum sleep */
   if (video_st->frame_rest > sleep_max)
      video_st->frame_rest           = sleep_max;

#if FRAME_REST_DEBUG
   RARCH_LOG("  frame=%5d delta=%5d sleep=%2d over=%3d near=%3d try=%3d %f %f\n", frame_time, frame_time_delta, video_st->frame_rest, frame_time_over_count, frame_time_near_count, frame_time_try_count, video_stddev, audio_stats.std_deviation_percentage);
#endif

   /* Do what is promised and add to statistics */
   if (video_st->frame_rest > 0)
   {
      if (!menu_is_pausing)
         video_st->frame_rest_time_count += video_st->frame_rest * 1000;
      retro_sleep(video_st->frame_rest);
   }
}

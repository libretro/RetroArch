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

/* KMS/DRM context, running without any window manager.
 * Based on kmscube example by Rob Clark.
 */

#include <stdint.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <sched.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>

#include <libdrm/drm.h>
#include <gbm.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <string/stdstring.h>

#include "../../configuration.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"
#include "../common/drm_common.h"

#include <go2/display.h>
#include <drm/drm_fourcc.h>

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#ifdef HAVE_OPENGLES

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

#endif

#ifndef EGL_PLATFORM_GBM_KHR
#define EGL_PLATFORM_GBM_KHR 0x31D7
#endif

typedef struct gfx_ctx_go2_drm_data
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
#endif
   go2_display_t* display;
   go2_presenter_t* presenter;
   go2_context_t* context;
   unsigned fb_width;
   unsigned fb_height;
   unsigned ctx_w;
   unsigned ctx_h;
   unsigned native_width;
   unsigned native_height;
   bool core_hw_context_enable;
} gfx_ctx_go2_drm_data_t;

/* TODO/FIXME - static global */
static enum gfx_ctx_api drm_api           = GFX_CTX_NONE;

/* Function callback */
void (*swap_buffers)(void*);

static void gfx_ctx_go2_drm_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
#ifdef HAVE_UDEV
   /* Try to set it to udev instead */
   void *udev = input_driver_init_wrap(&input_udev, joypad_name);
   if (udev)
   {
      *input       = &input_udev;
      *input_data  = udev;
      return;
   }
#endif

   *input      = NULL;
   *input_data = NULL;
}

static void *gfx_ctx_go2_drm_init(void *video_driver)
{
   gfx_ctx_go2_drm_data_t *drm = (gfx_ctx_go2_drm_data_t*)
      calloc(1, sizeof(gfx_ctx_go2_drm_data_t));

   if (!drm)
      return NULL;

   drm->display       = go2_display_create();

   drm->native_width  = go2_display_height_get(drm->display);
   drm->native_height = go2_display_width_get(drm->display);

   /* This driver should only be used on rotated screens */
   if (drm->native_width < drm->native_height)
   {
      /* This should be fixed by using wayland/weston... */
      go2_display_destroy(drm->display);
      free(drm);
      return NULL;
   }

   drm->presenter     = go2_presenter_create(drm->display,
         DRM_FORMAT_RGB565, 0xff000000, true);

   return drm;
}

static void gfx_ctx_go2_drm_destroy(void *data)
{
   gfx_ctx_go2_drm_data_t *drm = (gfx_ctx_go2_drm_data_t*)data;
   if (!drm) return;

   if (drm->context)
   {
      go2_context_destroy(drm->context);
      drm->context = NULL;
   }

   go2_presenter_destroy(drm->presenter);
   drm->presenter = NULL;

   go2_display_destroy(drm->display);
   drm->display = NULL;
}

static enum gfx_ctx_api gfx_ctx_go2_drm_get_api(void *data) { return drm_api; }

static bool gfx_ctx_go2_drm_bind_api(void *video_driver,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)video_driver;

   drm_api     = api;
#ifdef HAVE_EGL
   g_egl_major = major;
   g_egl_minor = minor;
#endif

   switch (api)
   {
      case GFX_CTX_OPENGL_API:
#if defined(HAVE_EGL) && defined(HAVE_OPENGL)

#ifndef EGL_KHR_create_context
         if ((major * 1000 + minor) >= 3001)
            return false;
#endif
         return egl_bind_api(EGL_OPENGL_API);
#else
         break;
#endif
      case GFX_CTX_OPENGL_ES_API:
#if defined(HAVE_EGL) && defined(HAVE_OPENGLES)

#ifndef EGL_KHR_create_context
         if (major >= 3)
            return false;
#endif
         return egl_bind_api(EGL_OPENGL_ES_API);
#else
         break;
#endif
      case GFX_CTX_OPENVG_API:
#if defined(HAVE_EGL) && defined(HAVE_VG)
         return egl_bind_api(EGL_OPENVG_API);
#endif
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

static void gfx_ctx_go2_drm_swap_interval(void *data, int interval)
{
   (void)data;
   (void)interval;

   if (interval > 1)
      RARCH_WARN("[KMS]: Swap intervals > 1 currently not supported. Will use swap interval of 1.\n");
}

static bool gfx_ctx_go2_drm_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   struct retro_system_av_info *av_info = NULL;
   gfx_ctx_go2_drm_data_t *drm          = (gfx_ctx_go2_drm_data_t*)data;

   if (!drm)
      return false;

   av_info  = video_viewport_get_system_av_info();

   frontend_driver_install_signal_handler();

#ifdef HAVE_MENU
   if (config_get_ptr()->bools.video_ctx_scaling && !menu_state_get_ptr()->alive)
   {
       drm->fb_width  = av_info->geometry.base_width;
       drm->fb_height = av_info->geometry.base_height;
   }
   else
#endif
   {
       drm->fb_width  = drm->native_width;
       drm->fb_height = drm->native_height;
   }

   if (!drm->context)
   {
      go2_context_attributes_t attr;
      attr.major        = 3;
      attr.minor        = 2;
      attr.red_bits     = 8;
      attr.green_bits   = 8;
      attr.blue_bits    = 8;
      attr.alpha_bits   = 8;
      attr.depth_bits   = 0;
      attr.stencil_bits = 0;

      drm->ctx_w        = MAX(av_info->geometry.max_width,  drm->native_width);
      drm->ctx_h        = MAX(av_info->geometry.max_height, drm->native_height);

      drm->context      = go2_context_create(
            drm->display, drm->ctx_w, drm->ctx_h, &attr);
   }

   go2_context_make_current(drm->context);

   gl_clear();

   return true;
}

static void gfx_ctx_go2_drm_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   unsigned w;
   unsigned h;
   gfx_ctx_go2_drm_data_t 
      *drm              = (gfx_ctx_go2_drm_data_t*)data;
#ifdef HAVE_MENU
   settings_t *settings = config_get_ptr();
   bool use_ctx_scaling = settings->bools.video_ctx_scaling;

   if (use_ctx_scaling && !menu_state_get_ptr()->alive)
   {
      struct retro_system_av_info* 
         av_info  = video_viewport_get_system_av_info();
       w          = av_info->geometry.base_width;
       h          = av_info->geometry.base_height;
   }
   else
#endif
   {
       w          = drm->native_width;
       h          = drm->native_height;
   }

   if (*width != w || *height != h)
   {
       *width     = drm->fb_width = w;
       *height    = drm->fb_height = h;
       *resize    = false;
   }

   *quit = (bool)frontend_driver_get_signal_handler_state();
}

static bool gfx_ctx_go2_drm_has_focus(void *data) { return true; }
static bool gfx_ctx_go2_drm_suppress_screensaver(void *data, bool enable) { return false; }

static void gfx_ctx_go2_drm_swap_buffers(void *data)
{
   gfx_ctx_go2_drm_data_t 
      *drm   = (gfx_ctx_go2_drm_data_t*)data;

   int out_w = drm->native_width;
   int out_h = drm->native_height;
   int out_x = 0;
   int out_y = 0;

   int src_w = drm->fb_width;
   int src_h = drm->fb_height;
   int src_x = 0;
   int src_y = drm->ctx_h - drm->fb_height;

   if (out_w != src_w || out_h != src_h)
   {
       out_w = out_h * video_driver_get_aspect_ratio();
       out_w = (out_w > drm->native_width) ? drm->native_width : out_w;
       out_x = (drm->native_width - out_w) / 2;
       if (out_x < 0)
           out_x = 0;
    }

#ifdef HAVE_EGL
   go2_context_swap_buffers(drm->context);

   go2_surface_t* surface = go2_context_surface_lock(drm->context);
   go2_presenter_post(drm->presenter,
         surface,
         src_x, src_y, src_w, src_h,
         out_y, out_x, out_h, out_w,
         GO2_ROTATION_DEGREES_270, 2);
   go2_context_surface_unlock(drm->context, surface);
#endif
}

static uint32_t gfx_ctx_go2_drm_get_flags(void *data)
{
   uint32_t             flags = 0;
   gfx_ctx_go2_drm_data_t    *drm = (gfx_ctx_go2_drm_data_t*)data;

   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES);

   if (drm->core_hw_context_enable)
      BIT32_SET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);

   if (string_is_equal(video_driver_get_ident(), "glcore"))
   {
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
   }
   else
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);

   return flags;
}

static void gfx_ctx_go2_drm_set_flags(void *data, uint32_t flags)
{
   gfx_ctx_go2_drm_data_t *drm     = (gfx_ctx_go2_drm_data_t*)data;
   if (BIT32_GET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT))
      drm->core_hw_context_enable = true;
}

static void gfx_ctx_go2_drm_bind_hw_render(void *data, bool enable)
{
   gfx_ctx_go2_drm_data_t *drm = (gfx_ctx_go2_drm_data_t*)data;

#ifdef HAVE_EGL
   egl_bind_hw_render(&drm->egl, enable);
#endif
}

const gfx_ctx_driver_t gfx_ctx_go2_drm = {
   gfx_ctx_go2_drm_init,
   gfx_ctx_go2_drm_destroy,
   gfx_ctx_go2_drm_get_api,
   gfx_ctx_go2_drm_bind_api,
   gfx_ctx_go2_drm_swap_interval,
   gfx_ctx_go2_drm_set_video_mode,
   NULL, /* get_video_size */
   drm_get_refresh_rate,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   NULL, /* update_title */
   gfx_ctx_go2_drm_check_window,
   NULL, /* set_resize */
   gfx_ctx_go2_drm_has_focus,
   gfx_ctx_go2_drm_suppress_screensaver,
   false, /* has_windowed */
   gfx_ctx_go2_drm_swap_buffers,
   gfx_ctx_go2_drm_input_driver,
#ifdef HAVE_EGL
   egl_get_proc_address,
#else
   NULL,
#endif
   NULL,
   NULL,
   NULL,
   "kms",
   gfx_ctx_go2_drm_get_flags,
   gfx_ctx_go2_drm_set_flags,
   gfx_ctx_go2_drm_bind_hw_render,
   NULL,
   NULL
};

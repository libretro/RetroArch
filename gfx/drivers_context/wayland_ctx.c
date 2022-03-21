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

#include <unistd.h>

#include <wayland-client.h>
#include <wayland-cursor.h>

#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../common/wayland_common.h"
#include "../../frontend/frontend_driver.h"
#include "../../input/common/wayland_common.h"
#include "../../input/input_driver.h"
#include "../../input/input_keymaps.h"
#include "../../verbosity.h"

/* Generated from idle-inhibit-unstable-v1.xml */
#include "../common/wayland/idle-inhibit-unstable-v1.h"

/* Generated from xdg-shell.xml */
#include "../common/wayland/xdg-shell.h"

/* Generated from xdg-decoration-unstable-v1.h */
#include "../common/wayland/xdg-decoration-unstable-v1.h"

#ifdef HAVE_EGL
#include <wayland-egl.h>
#include "../common/egl_common.h"
#endif

static enum gfx_ctx_api wl_api   = GFX_CTX_NONE;

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

#ifndef EGL_PLATFORM_WAYLAND_KHR
#define EGL_PLATFORM_WAYLAND_KHR 0x31D8
#endif

/* Shell surface callbacks. */
static void xdg_toplevel_handle_configure(void *data,
      struct xdg_toplevel *toplevel,
      int32_t width, int32_t height, struct wl_array *states)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   xdg_toplevel_handle_configure_common(wl, toplevel, width, height, states);
#ifdef HAVE_EGL
   if (wl->win)
      wl_egl_window_resize(wl->win, wl->width, wl->height, 0, 0);
   else
      wl->win = wl_egl_window_create(wl->surface,
            wl->width * wl->buffer_scale,
            wl->height * wl->buffer_scale);
#endif

   wl->configured = false;
}

static void gfx_ctx_wl_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   gfx_ctx_wl_get_video_size_common(wl, width, height);
}

static void gfx_ctx_wl_destroy_resources(gfx_ctx_wayland_data_t *wl)
{
   if (!wl)
      return;

#ifdef HAVE_EGL
   egl_destroy(&wl->egl);

   if (wl->win)
      wl_egl_window_destroy(wl->win);
#endif

   gfx_ctx_wl_destroy_resources_common(wl);

#ifdef HAVE_EGL
   wl->win          = NULL;
#endif
}

static void gfx_ctx_wl_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   gfx_ctx_wl_check_window_common(wl, gfx_ctx_wl_get_video_size, quit, resize, width, height);
}

static bool gfx_ctx_wl_set_resize(void *data, unsigned width, unsigned height)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

#ifdef HAVE_EGL
   wl_egl_window_resize(wl->win, width, height, 0, 0);
#endif

   wl_surface_set_buffer_scale(wl->surface, wl->buffer_scale);
   return true;
}

static void gfx_ctx_wl_update_title(void *data)
{
   gfx_ctx_wayland_data_t *wl   = (gfx_ctx_wayland_data_t*)data;
   gfx_ctx_wl_update_title_common(wl);
}

static bool gfx_ctx_wl_get_metrics(void *data,
      enum display_metric_types type, float *value)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   return gfx_ctx_wl_get_metrics_common(wl, type, value);
}

#ifdef HAVE_LIBDECOR_H
static void
libdecor_frame_handle_configure(struct libdecor_frame *frame,
      struct libdecor_configuration *configuration, void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   libdecor_frame_handle_configure_common(frame, configuration, wl);

#ifdef HAVE_EGL
   if (wl->win)
      wl_egl_window_resize(wl->win, wl->width, wl->height, 0, 0);
   else
      wl->win         = wl_egl_window_create(
            wl->surface,
            wl->width  * wl->buffer_scale,
            wl->height * wl->buffer_scale);
#endif

   wl->configured = false;
}
#endif

static const toplevel_listener_t toplevel_listener = {
#ifdef HAVE_LIBDECOR_H
   .libdecor_frame_interface = {
     libdecor_frame_handle_configure,
     libdecor_frame_handle_close,
     libdecor_frame_handle_commit,
   },
#endif
   .xdg_toplevel_listener = {
      xdg_toplevel_handle_configure,
      xdg_toplevel_handle_close,
   },
};

static const toplevel_listener_t xdg_toplevel_listener = {
};

#ifdef HAVE_EGL
#define WL_EGL_ATTRIBS_BASE \
   EGL_SURFACE_TYPE,    EGL_WINDOW_BIT, \
   EGL_RED_SIZE,        1, \
   EGL_GREEN_SIZE,      1, \
   EGL_BLUE_SIZE,       1, \
   EGL_ALPHA_SIZE,      0, \
   EGL_DEPTH_SIZE,      0

static bool gfx_ctx_wl_egl_init_context(gfx_ctx_wayland_data_t *wl)
{
   static const EGLint egl_attribs_gl[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
      EGL_NONE,
   };

#ifdef HAVE_OPENGLES
#ifdef HAVE_OPENGLES2
   static const EGLint egl_attribs_gles[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE,
   };
#endif

#ifdef HAVE_OPENGLES3
#ifdef EGL_KHR_create_context
   static const EGLint egl_attribs_gles3[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
      EGL_NONE,
   };
#endif
#endif

#endif

   static const EGLint egl_attribs_vg[] = {
      WL_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
      EGL_NONE,
   };

   EGLint n;
   EGLint major = 0, minor    = 0;
   const EGLint *attrib_ptr   = NULL;

   switch (wl_api)
   {
      case GFX_CTX_OPENGL_API:
#ifdef HAVE_OPENGL
         attrib_ptr = egl_attribs_gl;
#endif
         break;
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGLES
#ifdef HAVE_OPENGLES3
#ifdef EGL_KHR_create_context
         if (g_egl_major >= 3)
            attrib_ptr = egl_attribs_gles3;
         else
#endif
#endif
#ifdef HAVE_OPENGLES2
            attrib_ptr = egl_attribs_gles;
#endif
#endif
         break;
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_VG
         attrib_ptr = egl_attribs_vg;
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   if (!egl_init_context(&wl->egl,
            EGL_PLATFORM_WAYLAND_KHR,
            (EGLNativeDisplayType)wl->input.dpy,
            &major, &minor, &n, attrib_ptr,
            egl_default_accept_config_cb))
   {
      egl_report_error();
      goto error;
   }

   if (n == 0 || !egl_has_config(&wl->egl))
      goto error;
   return true;

error:
   return false;
}
#endif

static void *gfx_ctx_wl_init(void *video_driver)
{
   int i;
   gfx_ctx_wayland_data_t *wl = NULL;

   if (!gfx_ctx_wl_init_common(video_driver, &toplevel_listener, &wl))
      goto error;

#ifdef HAVE_EGL
   if (!gfx_ctx_wl_egl_init_context(wl))
      goto error;
#endif

   return wl;

error:
   gfx_ctx_wl_destroy_resources(wl);

   if (wl)
      free(wl);

   return NULL;
}

#ifdef HAVE_EGL
static EGLint *egl_fill_attribs(gfx_ctx_wayland_data_t *wl, EGLint *attr)
{
   switch (wl_api)
   {
#ifdef EGL_KHR_create_context
      case GFX_CTX_OPENGL_API:
      {
         bool debug       = false;
#ifdef HAVE_OPENGL
         unsigned version = wl->egl.major * 1000 + wl->egl.minor;
         bool core        = version >= 3001;
#ifndef GL_DEBUG
         struct retro_hw_render_callback *hwr =
            video_driver_get_hw_context();
#endif

#ifdef GL_DEBUG
         debug            = true;
#else
         debug            = hwr->debug_context;
#endif

         if (core)
         {
            *attr++ = EGL_CONTEXT_MAJOR_VERSION_KHR;
            *attr++ = wl->egl.major;
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = wl->egl.minor;
            /* Technically, we don't have core/compat until 3.2
             * Version 3.1 is either compat or not depending
             * on GL_ARB_compatibility. */
            if (version >= 3002)
            {
               *attr++ = EGL_CONTEXT_OPENGL_PROFILE_MASK_KHR;
               *attr++ = EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT_KHR;
            }
         }

         if (debug)
         {
            *attr++ = EGL_CONTEXT_FLAGS_KHR;
            *attr++ = EGL_CONTEXT_OPENGL_DEBUG_BIT_KHR;
         }
#endif

         break;
      }
#endif

      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGLES
         *attr++    = EGL_CONTEXT_CLIENT_VERSION;
         /* Same as EGL_CONTEXT_MAJOR_VERSION */
         *attr++    = wl->egl.major ? (EGLint)wl->egl.major : 2;
#ifdef EGL_KHR_create_context
         if (wl->egl.minor > 0)
         {
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = wl->egl.minor;
         }
#endif
#endif
         break;

      case GFX_CTX_NONE:
      default:
         break;
   }

   *attr = EGL_NONE;
   return attr;
}
#endif

static void gfx_ctx_wl_destroy(void *data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (!wl)
      return;

   gfx_ctx_wl_destroy_resources(wl);

   free(wl);
}

static void gfx_ctx_wl_set_swap_interval(void *data, int swap_interval)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

#ifdef HAVE_EGL
   egl_set_swap_interval(&wl->egl, swap_interval);
#endif
}

static bool gfx_ctx_wl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   gfx_ctx_wayland_data_t *wl   = (gfx_ctx_wayland_data_t*)data;

   if (!gfx_ctx_wl_set_video_mode_common_size(wl, width, height))
      goto error;

#ifdef HAVE_EGL
   EGLint egl_attribs[16];
   EGLint *attr              = egl_fill_attribs(
         (gfx_ctx_wayland_data_t*)data, egl_attribs);

   wl->win = wl_egl_window_create(wl->surface,
      wl->width  * wl->buffer_scale,
      wl->height * wl->buffer_scale);

   if (!egl_create_context(&wl->egl, (attr != egl_attribs)
            ? egl_attribs : NULL))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_create_surface(&wl->egl, (EGLNativeWindowType)wl->win))
      goto error;
   egl_set_swap_interval(&wl->egl, wl->egl.interval);
#endif

   if (!gfx_ctx_wl_set_video_mode_common_fullscreen(wl, fullscreen))
      goto error;

   return true;

error:
   gfx_ctx_wl_destroy(data);
   return false;
}

bool input_wl_init(void *data, const char *joypad_name);

static void gfx_ctx_wl_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   /* Input is heavily tied to the window stuff
    * on Wayland, so just implement the input driver here. */
   if (!input_wl_init(&wl->input, joypad_name))
   {
      wl->input.gfx = NULL;
      *input        = NULL;
      *input_data   = NULL;
   }
   else
   {
      wl->input.gfx = wl;
      *input        = &input_wayland;
      *input_data   = &wl->input;
      input_driver_init_joypads();
   }
}

static enum gfx_ctx_api gfx_ctx_wl_get_api(void *data)
{
   return wl_api;
}

static bool gfx_ctx_wl_bind_api(void *video_driver,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
#ifdef HAVE_EGL
   g_egl_major = major;
   g_egl_minor = minor;
#endif
   wl_api      = api;

   switch (api)
   {
      case GFX_CTX_OPENGL_API:
#ifdef HAVE_OPENGL
#ifndef EGL_KHR_create_context
         if ((major * 1000 + minor) >= 3001)
            return false;
#endif
#ifdef HAVE_EGL
         if (egl_bind_api(EGL_OPENGL_API))
            return true;
#endif
#endif
         break;
      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGLES
#ifndef EGL_KHR_create_context
         if (major >= 3)
            return false;
#endif
#ifdef HAVE_EGL
         if (egl_bind_api(EGL_OPENGL_ES_API))
            return true;
#endif
#endif
         break;
      case GFX_CTX_OPENVG_API:
#ifdef HAVE_VG
#ifdef HAVE_EGL
         if (egl_bind_api(EGL_OPENVG_API))
            return true;
#endif
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

static void gfx_ctx_wl_swap_buffers(void *data)
{
#ifdef HAVE_EGL
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   egl_swap_buffers(&wl->egl);
#endif
}

static void gfx_ctx_wl_bind_hw_render(void *data, bool enable)
{
#ifdef HAVE_EGL
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   egl_bind_hw_render(&wl->egl, enable);
#endif
}

static uint32_t gfx_ctx_wl_get_flags(void *data)
{
   uint32_t             flags = 0;
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;

   if (wl->core_hw_context_enable)
      BIT32_SET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);

   if (string_is_equal(video_driver_get_ident(), "glcore"))
   {
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
   }
   else if (string_is_equal(video_driver_get_ident(), "gl"))
   {
#ifdef HAVE_GLSL
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
#endif
   }

   return flags;
}

static void gfx_ctx_wl_set_flags(void *data, uint32_t flags)
{
   gfx_ctx_wayland_data_t *wl = (gfx_ctx_wayland_data_t*)data;
   if (BIT32_GET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT))
      wl->core_hw_context_enable = true;
}

const gfx_ctx_driver_t gfx_ctx_wayland = {
   gfx_ctx_wl_init,
   gfx_ctx_wl_destroy,
   gfx_ctx_wl_get_api,
   gfx_ctx_wl_bind_api,
   gfx_ctx_wl_set_swap_interval,
   gfx_ctx_wl_set_video_mode,
   gfx_ctx_wl_get_video_size,
   gfx_ctx_wl_get_refresh_rate,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   gfx_ctx_wl_get_metrics,
   NULL,
   gfx_ctx_wl_update_title,
   gfx_ctx_wl_check_window,
   gfx_ctx_wl_set_resize,
   gfx_ctx_wl_has_focus,
   gfx_ctx_wl_suppress_screensaver,
   true, /* has_windowed */
   gfx_ctx_wl_swap_buffers,
   gfx_ctx_wl_input_driver,
#ifdef HAVE_EGL
   egl_get_proc_address,
#else
   NULL,
#endif
   NULL,
   NULL,
   gfx_ctx_wl_show_mouse,
   "wayland",
   gfx_ctx_wl_get_flags,
   gfx_ctx_wl_set_flags,
   gfx_ctx_wl_bind_hw_render,
   NULL,
   NULL,
};

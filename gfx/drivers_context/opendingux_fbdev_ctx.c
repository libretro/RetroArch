/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2014 2015 - Jean-Andre Santoni
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

#include <signal.h>
#include <retroarch.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

#if defined(HAVE_OPENGLES)
#include "../common/gl_common.h"
#endif

#include "../../frontend/frontend_driver.h"
#include "../../verbosity.h"

typedef struct
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
   EGLNativeWindowType native_window;
#endif
   bool resize;
   unsigned width, height;
} opendingux_ctx_data_t;

static enum gfx_ctx_api opendingux_api = GFX_CTX_NONE;

static void gfx_ctx_opendingux_destroy(void *data)
{
   opendingux_ctx_data_t *viv = (opendingux_ctx_data_t*)data;

   if (viv)
   {
#ifdef HAVE_EGL
      egl_destroy(&viv->egl);
#endif

      viv->resize       = false;
      free(viv);
   }
}

static void *gfx_ctx_opendingux_init(video_frame_info_t *video_info, void *video_driver)
{
#ifdef HAVE_EGL
   EGLint n;
   EGLint major, minor;
   EGLint format;
   static const EGLint attribs[] = {
#if 0
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
#endif
      EGL_BLUE_SIZE,  5,
      EGL_GREEN_SIZE, 6,
      EGL_RED_SIZE,   5,
      EGL_ALPHA_SIZE, 0,
      EGL_SAMPLES,    0,
      EGL_NONE
   };
#endif
   opendingux_ctx_data_t *viv = (opendingux_ctx_data_t*)
      calloc(1, sizeof(*viv));

   if (!viv)
      return NULL;

#ifdef HAVE_EGL
   frontend_driver_install_signal_handler();

   if (!egl_init_context(&viv->egl, EGL_NONE, EGL_DEFAULT_DISPLAY,
            &major, &minor,
            &n, attribs, NULL))
      goto error;
#endif

   return viv;

#ifdef HAVE_EGL
error:
   egl_report_error();
#endif
   gfx_ctx_opendingux_destroy(viv);
   return NULL;
}

static void gfx_ctx_opendingux_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   opendingux_ctx_data_t *viv = (opendingux_ctx_data_t*)data;

   *width  = viv->width;
   *height = viv->height;
}

static void gfx_ctx_opendingux_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, bool is_shutdown)
{
   unsigned new_width, new_height;
   opendingux_ctx_data_t *viv = (opendingux_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_get_video_size(&viv->egl, &new_width, &new_height);

   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }
#endif

   *quit   = (bool)frontend_driver_get_signal_handler_state();
}

static bool gfx_ctx_opendingux_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
#ifdef HAVE_EGL
   static const EGLint attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2, /* Use version 2, even for GLES3. */
      EGL_NONE
   };
#endif
   opendingux_ctx_data_t *viv = (opendingux_ctx_data_t*)data;

   /* Pick some arbitrary default. */
   if (!width || !fullscreen)
      width = 1280;
   if (!height || !fullscreen)
      height = 1024;

   viv->width    = width;
   viv->height   = height;

#ifdef HAVE_EGL
   if (!egl_create_context(&viv->egl, attribs))
      goto error;

   viv->native_window = 0;
   if (!egl_create_surface(&viv->egl, viv->native_window))
      goto error;
#endif

   return true;

#ifdef HAVE_EGL
error:
   egl_report_error();
#endif
   gfx_ctx_opendingux_destroy(data);
   return false;
}

static void gfx_ctx_opendingux_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
   *input      = NULL;
   *input_data = NULL;
}

static enum gfx_ctx_api gfx_ctx_opendingux_get_api(void *data)
{
   return opendingux_api;
}

static bool gfx_ctx_opendingux_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;

   opendingux_api = api;

   if (api == GFX_CTX_OPENGL_ES_API)
      return true;
   return false;
}

static bool gfx_ctx_opendingux_has_focus(void *data)
{
   (void)data;
   return true;
}

static bool gfx_ctx_opendingux_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static void gfx_ctx_opendingux_swap_buffers(void *data, void *data2)
{
   opendingux_ctx_data_t *viv = (opendingux_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_swap_buffers(&viv->egl);
#endif
}

static void gfx_ctx_opendingux_set_swap_interval(
      void *data, int swap_interval)
{
   opendingux_ctx_data_t *viv = (opendingux_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_set_swap_interval(&viv->egl, swap_interval);
#endif
}

#ifdef HAVE_EGL
static gfx_ctx_proc_t gfx_ctx_opendingux_get_proc_address(const char *symbol)
{
   return egl_get_proc_address(symbol);
}
#endif

static void gfx_ctx_opendingux_bind_hw_render(void *data, bool enable)
{
   opendingux_ctx_data_t *viv = (opendingux_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_bind_hw_render(&viv->egl, enable);
#endif
}

static uint32_t gfx_ctx_opendingux_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);

   return flags;
}

static void gfx_ctx_opendingux_set_flags(void *data, uint32_t flags)
{
   (void)data;
}

const gfx_ctx_driver_t gfx_ctx_opendingux_fbdev = {
   gfx_ctx_opendingux_init,
   gfx_ctx_opendingux_destroy,
   gfx_ctx_opendingux_get_api,
   gfx_ctx_opendingux_bind_api,
   gfx_ctx_opendingux_set_swap_interval,
   gfx_ctx_opendingux_set_video_mode,
   gfx_ctx_opendingux_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   NULL, /* update_title */
   gfx_ctx_opendingux_check_window,
   NULL, /* set_resize */
   gfx_ctx_opendingux_has_focus,
   gfx_ctx_opendingux_suppress_screensaver,
   false, /* has_windowed */
   gfx_ctx_opendingux_swap_buffers,
   gfx_ctx_opendingux_input_driver,
#ifdef HAVE_EGL
   gfx_ctx_opendingux_get_proc_address,
#else
   NULL,
#endif
   NULL,
   NULL,
   NULL,
   "opendingux-fbdev",
   gfx_ctx_opendingux_get_flags,
   gfx_ctx_opendingux_set_flags,
   gfx_ctx_opendingux_bind_hw_render,
   NULL,
   NULL
};

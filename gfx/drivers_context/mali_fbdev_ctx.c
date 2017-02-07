/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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
#include <unistd.h>

#include <sys/ioctl.h>

/* Includes and defines for framebuffer size retrieval */
#include <linux/fb.h>
#include <linux/vt.h>

#include <streams/file_stream.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include "../common/gl_common.h"
#endif

#include "../../frontend/frontend_driver.h"

typedef struct
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
#endif

   struct mali_native_window native_window;
   bool resize;
   unsigned width, height;
} mali_ctx_data_t;

static enum gfx_ctx_api mali_api           = GFX_CTX_NONE;

static void gfx_ctx_mali_fbdev_destroy(void *data)
{
   int fb;
   RFILE             *fd = NULL;
   mali_ctx_data_t *mali = (mali_ctx_data_t*)data;

   if (mali)
   {
#ifdef HAVE_EGL
       egl_destroy(&mali->egl);
#endif

       mali->resize       = false;
       free(mali);
   }

   /* Clear framebuffer and set cursor on again */
   fd = filestream_open("/dev/tty", RFILE_MODE_READ_WRITE, -1);
   fb = filestream_get_fd(fd);

   ioctl(fb, VT_ACTIVATE,5);
   ioctl(fb, VT_ACTIVATE,1);
   filestream_close(fd);
   system("setterm -cursor on");
}

static void gfx_ctx_mali_fbdev_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   mali_ctx_data_t *mali = (mali_ctx_data_t*)data;

   *width  = mali->width;
   *height = mali->height;
}

static void *gfx_ctx_mali_fbdev_init(video_frame_info_t *video_info, void *video_driver)
{
#ifdef HAVE_EGL
   EGLint n;
   EGLint major, minor;
   EGLint format;
   static const EGLint attribs[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_BLUE_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_RED_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_NONE
   };
#endif

   mali_ctx_data_t *mali = (mali_ctx_data_t*)calloc(1, sizeof(*mali));

   if (!mali)
       return NULL;

#ifdef HAVE_EGL
   frontend_driver_install_signal_handler();
#endif

#ifdef HAVE_EGL
   if (!egl_init_context(&mali->egl, EGL_DEFAULT_DISPLAY,
            &major, &minor, &n, attribs))
   {
      egl_report_error();
      goto error;
   }
#endif

   return mali;

error:
   RARCH_ERR("[Mali fbdev]: EGL error: %d.\n", eglGetError());
   gfx_ctx_mali_fbdev_destroy(video_driver);
   return NULL;
}

static void gfx_ctx_mali_fbdev_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, bool is_shutdown)
{
   unsigned new_width, new_height;

   gfx_ctx_mali_fbdev_get_video_size(data, &new_width, &new_height);

   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }

   *quit   = (bool)frontend_driver_get_signal_handler_state();
}

static bool gfx_ctx_mali_fbdev_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
   struct fb_var_screeninfo vinfo;
   static const EGLint attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2, /* Use version 2, even for GLES3. */
      EGL_NONE
   };
   mali_ctx_data_t *mali = (mali_ctx_data_t*)data;
   RFILE *fd             = filestream_open("/dev/fb0", RFILE_MODE_READ_WRITE, -1);
   int fb                = filestream_get_fd(fd);

   if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) < 0)
   {
      RARCH_ERR("Error obtaining framebuffer info.\n");
      goto error;
   }

   filestream_close(fd);
   
   width                      = vinfo.xres;
   height                     = vinfo.yres;

   mali->width                = width;
   mali->height               = height;

   mali->native_window.width  = vinfo.xres;
   mali->native_window.height = vinfo.yres;

#ifdef HAVE_EGL
   if (!egl_create_context(&mali->egl, attribs))
   {
      egl_report_error();
      goto error;
   }
#endif

#ifdef HAVE_EGL
   if (!egl_create_surface(&mali->egl, &mali->native_window))
      goto error;
#endif

   return true;

error:
   if (fd)
      filestream_close(fd);
   RARCH_ERR("[Mali fbdev]: EGL error: %d.\n", eglGetError());
   gfx_ctx_mali_fbdev_destroy(data);
   return false;
}

static void gfx_ctx_mali_fbdev_input_driver(void *data,
      const char *name,
      const input_driver_t **input, void **input_data)
{
   *input      = NULL;
   *input_data = NULL;
}

static bool gfx_ctx_mali_fbdev_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   mali_api = api;
   return api == GFX_CTX_OPENGL_ES_API;
}

static bool gfx_ctx_mali_fbdev_has_focus(void *data)
{
   (void)data;
   return true;
}

static bool gfx_ctx_mali_fbdev_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static void gfx_ctx_mali_fbdev_set_swap_interval(void *data, unsigned swap_interval)
{
   mali_ctx_data_t *mali = (mali_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_set_swap_interval(&mali->egl, swap_interval);
#endif
}

static void gfx_ctx_mali_fbdev_swap_buffers(void *data, video_frame_info_t *video_info)
{
   mali_ctx_data_t *mali = (mali_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_swap_buffers(&mali->egl);
#endif
}

static gfx_ctx_proc_t gfx_ctx_mali_fbdev_get_proc_address(const char *symbol)
{
#ifdef HAVE_EGL
   return egl_get_proc_address(symbol);
#endif
}

static void gfx_ctx_mali_fbdev_bind_hw_render(void *data, bool enable)
{
   mali_ctx_data_t *mali = (mali_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_bind_hw_render(&mali->egl, enable);
#endif
}

static uint32_t gfx_ctx_mali_fbdev_get_flags(void *data)
{
   uint32_t flags = 0;
   BIT32_SET(flags, GFX_CTX_FLAGS_NONE);

   return flags;
}

static void gfx_ctx_mali_fbdev_set_flags(void *data, uint32_t flags)
{
   (void)data;
}

const gfx_ctx_driver_t gfx_ctx_mali_fbdev = {
   gfx_ctx_mali_fbdev_init,
   gfx_ctx_mali_fbdev_destroy,
   gfx_ctx_mali_fbdev_bind_api,
   gfx_ctx_mali_fbdev_set_swap_interval,
   gfx_ctx_mali_fbdev_set_video_mode,
   gfx_ctx_mali_fbdev_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   NULL, /* update_title */
   gfx_ctx_mali_fbdev_check_window,
   NULL, /* set_resize */
   gfx_ctx_mali_fbdev_has_focus,
   gfx_ctx_mali_fbdev_suppress_screensaver,
   NULL, /* has_windowed */
   gfx_ctx_mali_fbdev_swap_buffers,
   gfx_ctx_mali_fbdev_input_driver,
   gfx_ctx_mali_fbdev_get_proc_address,
   NULL,
   NULL,
   NULL,
   "mali-fbdev",
   gfx_ctx_mali_fbdev_get_flags,
   gfx_ctx_mali_fbdev_set_flags,
   gfx_ctx_mali_fbdev_bind_hw_render,
   NULL,
   NULL
};


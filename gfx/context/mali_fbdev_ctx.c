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

#include "../../driver.h"
#include "../../general.h"
#include "../gfx_common.h"
#include "../gl_common.h"

#include <EGL/egl.h>
#include <signal.h>

//Includes and defines for framebuffer size retrieval
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/vt.h>
#include <fcntl.h>
#include <unistd.h>

struct fbdev_window native_window;
static EGLContext g_egl_ctx;
static EGLSurface g_egl_surf;
static EGLDisplay g_egl_dpy;
static EGLConfig g_config;
static bool g_resize;
static unsigned g_width, g_height;

static volatile sig_atomic_t g_quit;
static void sighandler(int sig)
{
   (void)sig;
   g_quit = 1;
}

static void gfx_ctx_mali_fbdev_set_swap_interval(
      void *data, unsigned interval)
{
   (void)data;
   if (g_egl_dpy)
      eglSwapInterval(g_egl_dpy, interval);
}

static void gfx_ctx_mali_fbdev_destroy(void *data)
{
   (void)data;

   if (g_egl_dpy != EGL_NO_DISPLAY)
   {
      if (g_egl_ctx != EGL_NO_CONTEXT)
      {
         glFlush();
         glFinish();
      }

      eglMakeCurrent(g_egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      if (g_egl_ctx != EGL_NO_CONTEXT)
         eglDestroyContext(g_egl_dpy, g_egl_ctx);
      if (g_egl_surf != EGL_NO_SURFACE)
         eglDestroySurface(g_egl_dpy, g_egl_surf);
      eglTerminate(g_egl_dpy);
   }

   g_egl_dpy  = EGL_NO_DISPLAY;
   g_egl_surf = EGL_NO_SURFACE;
   g_egl_ctx  = EGL_NO_CONTEXT;
   g_config   = 0;
   g_quit     = 0;
   g_resize   = false;

   //Clear framebuffer and set cursor on again
   int fd = open("/dev/tty", O_RDWR);
   ioctl(fd,VT_ACTIVATE,5);
   ioctl(fd,VT_ACTIVATE,1);
   close (fd);
   system("setterm -cursor on");

}

static void gfx_ctx_mali_fbdev_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   (void)data;
   if (g_egl_dpy != EGL_NO_DISPLAY && g_egl_surf != EGL_NO_SURFACE)
   {
      *width  = g_width;
      *height = g_height;
   }
   else
   {
      *width  = 0;
      *height = 0;
   }
}

static bool gfx_ctx_mali_fbdev_init(void *data)
{
   (void)data;

   struct sigaction sa = {{0}};
   sa.sa_handler = sighandler;
   sa.sa_flags   = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);

   static const EGLint attribs[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_BLUE_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_RED_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_NONE
   };
   EGLint num_config;
   EGLint egl_version_major, egl_version_minor;
   EGLint format;
 

   //Disable cursor blinking so it's not visible in RetroArch
   system("setterm -cursor off");
   
   RARCH_LOG("[Mali fbdev]: Initializing context\n");

   if ((g_egl_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY)
   {
      RARCH_ERR("[Mali fbdev]: eglGetDisplay failed.\n");
      goto error;
   }

   if (!eglInitialize(g_egl_dpy, &egl_version_major, &egl_version_minor))
   {
      RARCH_ERR("[Mali fbdev]: eglInitialize failed.\n");
      goto error;
   }

   RARCH_LOG("[Mali fbdev]: EGL version: %d.%d\n", egl_version_major, egl_version_minor);


   if (!eglChooseConfig(g_egl_dpy, attribs, &g_config, 1, &num_config))
   {
      RARCH_ERR("[Mali fbdev]: eglChooseConfig failed.\n");
      goto error;
   }

   return true;

error:
   RARCH_ERR("[Mali fbdev]: EGL error: %d.\n", eglGetError());
   gfx_ctx_mali_fbdev_destroy(data);
   return false;
}

static void gfx_ctx_mali_fbdev_swap_buffers(void *data)
{
   (void)data;
   eglSwapBuffers(g_egl_dpy, g_egl_surf);
}

static void gfx_ctx_mali_fbdev_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)frame_count;

   unsigned new_width, new_height;
   gfx_ctx_mali_fbdev_get_video_size(data, &new_width, &new_height);
   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }

   *quit = g_quit;
}

static void gfx_ctx_mali_fbdev_set_resize(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
}

static void gfx_ctx_mali_fbdev_update_window_title(void *data)
{
   (void)data;
   char buf[128], buf_fps[128];
   bool fps_draw = g_settings.fps_show;
   gfx_get_fps(buf, sizeof(buf), fps_draw ? buf_fps : NULL, sizeof(buf_fps));

   if (fps_draw)
      msg_queue_push(g_extern.msg_queue, buf_fps, 1, 1);
}

static bool gfx_ctx_mali_fbdev_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   struct fb_var_screeninfo vinfo;
   int fb = open("/dev/fb0", O_RDWR, 0);
   if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) < 0)
   {
      RARCH_ERR("Error obtainig framebuffer info.\n");
      goto error;
   }
   close (fb);
   
   width = vinfo.xres;
   height = vinfo.yres;

   g_width = width;
   g_height = height;

   native_window.width = vinfo.xres;
   native_window.height = vinfo.yres;

   static const EGLint attribs[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2, /* Use version 2, even for GLES3. */
      EGL_NONE
   };

   if ((g_egl_surf = eglCreateWindowSurface(g_egl_dpy, g_config, &native_window, 0)) == EGL_NO_SURFACE)
   {
      RARCH_ERR("eglCreateWindowSurface failed.\n");
      goto error;
   }

   if ((g_egl_ctx = eglCreateContext(g_egl_dpy, g_config, 0, attribs)) == EGL_NO_CONTEXT)
   {
      RARCH_ERR("eglCreateContext failed.\n");
      goto error;
   }

   if (!eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx))
   {
      RARCH_ERR("eglMakeCurrent failed.\n");
      goto error;
   }

   return true;

error:
   RARCH_ERR("[Mali fbdev]: EGL error: %d.\n", eglGetError());
   gfx_ctx_mali_fbdev_destroy(data);
   return false;
}

static void gfx_ctx_mali_fbdev_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   (void)data;
   *input = NULL;
   *input_data = NULL;
}

static gfx_ctx_proc_t gfx_ctx_mali_fbdev_get_proc_address(const char *symbol)
{
   rarch_assert(sizeof(void*) == sizeof(void (*)(void)));
   gfx_ctx_proc_t ret;

   void *sym__ = eglGetProcAddress(symbol);
   memcpy(&ret, &sym__, sizeof(void*));

   return ret;
}

static bool gfx_ctx_mali_fbdev_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   return api == GFX_CTX_OPENGL_ES_API;
}

static bool gfx_ctx_mali_fbdev_has_focus(void *data)
{
   (void)data;
   return true;
}

static bool gfx_ctx_mali_fbdev_has_windowed(void *data)
{
   (void)data;
   return false;
}

const gfx_ctx_driver_t gfx_ctx_mali_fbdev = {
   gfx_ctx_mali_fbdev_init,
   gfx_ctx_mali_fbdev_destroy,
   gfx_ctx_mali_fbdev_bind_api,
   gfx_ctx_mali_fbdev_set_swap_interval,
   gfx_ctx_mali_fbdev_set_video_mode,
   gfx_ctx_mali_fbdev_get_video_size,
   NULL,
   gfx_ctx_mali_fbdev_update_window_title,
   gfx_ctx_mali_fbdev_check_window,
   gfx_ctx_mali_fbdev_set_resize,
   gfx_ctx_mali_fbdev_has_focus,
   gfx_ctx_mali_fbdev_has_windowed,
   gfx_ctx_mali_fbdev_swap_buffers,
   gfx_ctx_mali_fbdev_input_driver,
   gfx_ctx_mali_fbdev_get_proc_address,
#ifdef HAVE_EGL
   NULL,
   NULL,
#endif
   NULL,
   "mali-fbdev",
};


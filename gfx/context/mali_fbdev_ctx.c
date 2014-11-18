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


//Custom includes and defines for this hacky driver
#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/vt.h>
#include <fcntl.h>
#include <unistd.h>
#include <EGL/fbdev_window.h>

#ifndef GLOBAL_FBDEV
#define GLOBAL_FBDEV "/dev/fb0"
#endif
//

int fb;
struct fbdev_window native_window;
static EGLContext g_egl_ctx;
static EGLSurface g_egl_surf;
static EGLDisplay g_egl_dpy;
static EGLConfig g_config;
static bool g_resize;
static unsigned g_width, g_height;

static volatile sig_atomic_t g_quit;
/*static void sighandler(int sig)
{
   (void)sig;
   g_quit = 1;
}*/

static void gfx_ctx_clearfb(void){
	int fd = open("/dev/tty", O_RDWR);
	ioctl(fd,VT_ACTIVATE,5);
	ioctl(fd,VT_ACTIVATE,1);
	close (fd);
	system("setterm -cursor on");
}

static void gfx_ctx_mali_fbdev_set_swap_interval(void *data, unsigned interval)
{
   (void)data;
   if (g_egl_dpy)
      eglSwapInterval(g_egl_dpy, interval);
}

static void gfx_ctx_mali_fbdev_destroy(void *data)
{
   RARCH_LOG("gfx_ctx_mali_fbdev_destroy().\n");
   eglMakeCurrent(g_egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
   eglDestroyContext(g_egl_dpy, g_egl_ctx);
   eglDestroySurface(g_egl_dpy, g_egl_surf);
   eglTerminate(g_egl_dpy);

   g_egl_dpy = EGL_NO_DISPLAY;
   g_egl_surf = EGL_NO_SURFACE;
   g_egl_ctx = EGL_NO_CONTEXT;
   g_config   = 0;
   g_resize   = false;
   
   gfx_ctx_clearfb();

   close (fb);
}

static void gfx_ctx_mali_fbdev_get_video_size(void *data, unsigned *width, unsigned *height)
{

   if (g_egl_dpy)
   {
      EGLint gl_width, gl_height;
      eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_WIDTH, &gl_width);
      eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_HEIGHT, &gl_height);
      *width  = gl_width;
      *height = gl_height;
   }
   else
   {
      *width  = 0;
      *height = 0;
   }

}

static bool gfx_ctx_mali_fbdev_init(void *data)
{
   system("setterm -cursor off");
   const EGLint attribs[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_BLUE_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_RED_SIZE, 8,
      EGL_NONE
   };
   EGLint num_config;
   EGLint egl_version_major, egl_version_minor;
   EGLint format;

   EGLint context_attributes[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };

   RARCH_LOG("Initializing context\n");

   if ((g_egl_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY)) == EGL_NO_DISPLAY)
   {
      RARCH_ERR("eglGetDisplay failed.\n");
      goto error;
   }

   if (!eglInitialize(g_egl_dpy, &egl_version_major, &egl_version_minor))
   {
      RARCH_ERR("eglInitialize failed.\n");
      goto error;
   }

   RARCH_LOG("[GLES/EGL]: EGL version: %d.%d\n", egl_version_major, egl_version_minor);

   if (!eglChooseConfig(g_egl_dpy, attribs, &g_config, 1, &num_config))
   {
      RARCH_ERR("eglChooseConfig failed.\n");
      goto error;
   }

   int var = eglGetConfigAttrib(g_egl_dpy, g_config, EGL_NATIVE_VISUAL_ID, &format);

   if (!var)
   {
      RARCH_ERR("eglGetConfigAttrib failed: %d.\n", var);
      goto error;
   }

   struct fb_var_screeninfo vinfo;
   fb = open(GLOBAL_FBDEV, O_RDWR, 0);
   if (ioctl(fb, FBIOGET_VSCREENINFO, &vinfo) < 0)
   {
      RARCH_ERR("error obtainig framebuffer info.\n");
      goto error;
   }
   native_window.width = vinfo.xres;
   native_window.height = vinfo.yres;

   if (!(g_egl_surf = eglCreateWindowSurface(g_egl_dpy, g_config, &native_window, 0)))
   {
      RARCH_ERR("eglCreateWindowSurface failed.\n");
      goto error;
   }

   if (!(g_egl_ctx = eglCreateContext(g_egl_dpy, g_config, 0, context_attributes)))
   {
      RARCH_ERR("eglCreateContext failed.\n");
      goto error;
   }

   if (!eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx))
   {
      RARCH_ERR("eglMakeCurrent failed.\n");
      goto error;
   }


   if ( eglSwapInterval(g_egl_dpy,1) != EGL_TRUE ){
      printf("\neglSwapInterval failed.\n");
      goto error;
   }

   return true;

error:
   RARCH_ERR("EGL error: %d.\n", eglGetError());
   gfx_ctx_mali_fbdev_destroy(data);
   return false;
}

static void gfx_ctx_mali_fbdev_swap_buffers(void *data)
{
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

static void gfx_ctx_mali_fbdev_set_resize(void *data, unsigned width, unsigned height)
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
   (void)width;
   (void)height;
   (void)fullscreen;
   return true;
}

/*
//MAC : Use if we don't want context to initialize an input driver. 
//Otherwise, the best idea is to use udev if available
static void gfx_ctx_input_driver(void *data, const input_driver_t **input, void **input_data)
{
   (void)data;
   *input = NULL;
   *input_data = NULL;
}
*/

static void gfx_ctx_mali_fbdev_input_driver(void *data, const input_driver_t **input, void **input_data)
{
#ifdef HAVE_UDEV
   void *udev = input_udev.init();
   *input = udev ? &input_udev : NULL;
   *input_data = udev;
#else
   void *linuxinput = input_linuxraw.init();
   *input = linuxinput ? &input_linuxraw : NULL;
   *input_data = linuxinput;
#endif
}

static gfx_ctx_proc_t gfx_ctx_mali_fbdev_get_proc_address(const char *symbol)
{
   rarch_assert(sizeof(void*) == sizeof(void (*)(void)));
   gfx_ctx_proc_t ret;

   void *sym__ = eglGetProcAddress(symbol);
   memcpy(&ret, &sym__, sizeof(void*));

   return ret;
}

static bool gfx_ctx_mali_fbdev_bind_api(void *data, enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   return api == GFX_CTX_OPENGL_ES_API;
}

static bool gfx_ctx_mali_fbdev_has_focus(void *data)
{
   (void)data;
   return true;
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
   NULL,
   //gfx_ctx_mali_fbdev_has_windowed,
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

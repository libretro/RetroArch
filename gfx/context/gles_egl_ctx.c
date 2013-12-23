/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2013 - Daniel De Matteis
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

#include "../image.h"

#include "../fonts/gl_font.h"
#include <stdint.h>

#ifdef HAVE_GLSL
#include "../shader_glsl.h"
#endif

#include <sys/ioctl.h>
#include <linux/fb.h>
#include <linux/vt.h>
#include <fcntl.h>
#include <unistd.h>

#ifndef GLOBAL_FBDEV
#define GLOBAL_FBDEV "/dev/fb0"
#endif

static EGLContext g_egl_ctx;
static EGLSurface g_egl_surf;
static EGLDisplay g_egl_dpy;
static EGLConfig g_config;
static bool g_resize;

GLfloat _angle;
int fb;

static enum gfx_ctx_api g_api;

struct mali_native_window native_window;

static void gfx_ctx_set_swap_interval(unsigned interval)
{
   RARCH_LOG("gfx_ctx_set_swap_interval(%d).\n", interval);
   eglSwapInterval(g_egl_dpy, interval);
}

static void gfx_ctx_clearfb(void){
/*	
	char *pixels;
        char *mapped_mem;
	struct fb_fix_screeninfo finfo;
	int mapped_memlen;
        int mapped_offset;      
	 
        ioctl(fb, FBIOGET_FSCREENINFO, &finfo);

	mapped_offset = (((long)finfo.smem_start) -
                        (((long)finfo.smem_start)&~(getpagesize()-1)));
        mapped_memlen = finfo.smem_len+mapped_offset;

	mapped_mem = mmap(NULL, mapped_memlen,
                          PROT_READ|PROT_WRITE, MAP_SHARED, fb, 0);
	pixels = mapped_mem+mapped_offset;

	//TODO: fix to actual bytes per pixel (assumes 4Bpp = 32bpp)
	memset(pixels,0,native_window.height * native_window.width * 4); 
	*/
	int fd = open("/dev/tty", O_RDWR);
	ioctl(fd,VT_ACTIVATE,5);
	ioctl(fd,VT_ACTIVATE,1);
	close (fd);
	system("setterm -cursor on");
}

static void gfx_ctx_destroy(void)
{
   RARCH_LOG("gfx_ctx_destroy().\n");
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

static void gfx_ctx_get_video_size(unsigned *width, unsigned *height)
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

static bool gfx_ctx_init(void)
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
   gfx_ctx_destroy();
   return false;
}

static void gfx_ctx_swap_buffers(void)
{
	eglSwapBuffers(g_egl_dpy, g_egl_surf);
}

static void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)frame_count;

   *quit = false;

   unsigned new_width, new_height;
   gfx_ctx_get_video_size(&new_width, &new_height);
   if (new_width != *width || new_height != *height)
   {
      *width  = new_width;
      *height = new_height;
      *resize = true;
   }

   // Check if we are exiting.
   if (g_extern.lifecycle_state & (1ULL << RARCH_QUIT_KEY))
      *quit = true;
}

static void gfx_ctx_set_resize(unsigned width, unsigned height)
{
   (void)width;
   (void)height;
}

static void gfx_ctx_update_window_title(void)
{
   char buf[128], buf_fps[128];
   bool fps_draw = g_settings.fps_show;
   gfx_get_fps(buf, sizeof(buf), fps_draw ? buf_fps : NULL, sizeof(buf_fps));

   if (fps_draw)
      msg_queue_push(g_extern.msg_queue, buf_fps, 1, 1);
}

static bool gfx_ctx_set_video_mode(
      unsigned width, unsigned height,
      bool fullscreen)
{
   (void)width;
   (void)height;
   (void)fullscreen;
   return true;
}


static void gfx_ctx_input_driver(const input_driver_t **input, void **input_data)
{
#ifdef HAVE_UDEV
   void *udev  = input_udev.init();
   *input      = udev ? &input_udev : NULL;
   *input_data = udev;
#else
   void *linuxinput = input_linuxraw.init();
   *input           = linuxinput ? &input_linuxraw : NULL;
   *input_data      = linuxinput;
#endif
}

/*static unsigned gfx_ctx_get_resolution_width(unsigned resolution_id)
{
   int gl_width;
   eglQuerySurface(g_egl_dpy, g_egl_surf, EGL_WIDTH, &gl_width);

   return gl_width;
}*/

static gfx_ctx_proc_t gfx_ctx_get_proc_address(const char *symbol)
{
   rarch_assert(sizeof(void*) == sizeof(void (*)(void)));
   gfx_ctx_proc_t ret;

   void *sym__ = eglGetProcAddress(symbol);
   memcpy(&ret, &sym__, sizeof(void*));

   return ret;
}

static bool gfx_ctx_bind_api(enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)major;
   (void)minor;
   g_api = api;
   return api == GFX_CTX_OPENGL_ES_API;
}

static bool gfx_ctx_has_focus(void)
{
   return true;
}

#ifdef HAVE_EGL
static bool gfx_ctx_init_egl_image_buffer(const video_info_t *video)
{
   return false;
}

static bool gfx_ctx_write_egl_image(const void *frame, unsigned width, unsigned height, unsigned pitch, bool rgb32, unsigned index, void **image_handle)
{
   return false;
}
#endif

const gfx_ctx_driver_t gfx_ctx_gles_egl = {
   gfx_ctx_init,
   gfx_ctx_destroy,
   gfx_ctx_bind_api,
   gfx_ctx_set_swap_interval,
   gfx_ctx_set_video_mode,
   gfx_ctx_get_video_size,
   NULL,
   gfx_ctx_update_window_title,
   gfx_ctx_check_window,
   gfx_ctx_set_resize,
   gfx_ctx_has_focus,
   gfx_ctx_swap_buffers,
   gfx_ctx_input_driver,
   gfx_ctx_get_proc_address,
#ifdef HAVE_EGL
   gfx_ctx_init_egl_image_buffer,
   gfx_ctx_write_egl_image,
#endif
   NULL,
   "gles-egl",
};

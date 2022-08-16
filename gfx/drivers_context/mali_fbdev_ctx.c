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
#include <sys/stat.h>
#include <fcntl.h>

/* Includes and defines for framebuffer size retrieval */
#include <linux/fb.h>
#include <linux/vt.h>

#include <compat/strl.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

#include "../../frontend/frontend_driver.h"
#include "../../verbosity.h"
#include "../../configuration.h"

#include <streams/file_stream.h>

typedef struct
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
#endif

   struct {
      unsigned short width;
      unsigned short height;
   } native_window;
   bool resize;
   unsigned width, height;
   float refresh_rate;
} mali_ctx_data_t;

mali_ctx_data_t *gfx_ctx_mali_fbdev_global=NULL;
bool gfx_ctx_mali_fbdev_was_threaded=false;
bool gfx_ctx_mali_fbdev_hw_ctx_trigger=false;
bool gfx_ctx_mali_fbdev_restart_pending=false;

static int gfx_ctx_mali_fbdev_get_vinfo(void *data)
{
   struct fb_var_screeninfo vinfo;
   int fd                = open("/dev/fb0", O_RDWR);

   mali_ctx_data_t *mali = (mali_ctx_data_t*)data;

   if (!mali || ioctl(fd, FBIOGET_VSCREENINFO, &vinfo) < 0)
      goto error;

   /*Workaround to reset yoffset when returned >0 from driver */
   if (vinfo.yoffset != 0)
   {
      vinfo.yoffset = 0;
      if (ioctl(fd, FBIOPUT_VSCREENINFO, &vinfo))
         {
            RARCH_ERR("Error resetting yoffset to 0.\n");
      }
   }

   close(fd);
   fd = -1;

   mali->width                = vinfo.xres;
   mali->height               = vinfo.yres;

   mali->native_window.width  = vinfo.xres;
   mali->native_window.height = vinfo.yres;

   if (vinfo.pixclock)
   {
      mali->refresh_rate = 1000000.0f / vinfo.pixclock * 1000000.0f /
           (vinfo.yres + vinfo.upper_margin + vinfo.lower_margin + vinfo.vsync_len) /
           (vinfo.xres + vinfo.left_margin  + vinfo.right_margin + vinfo.hsync_len);
   }else{
      /* Workaround to retrieve current refresh rate if no info is available from IOCTL.
         If this fails as well, 60Hz is assumed... */
      int j=0;
      float k=60.0;
      char temp[32];
      RFILE *fr = filestream_open("/sys/class/display/mode", RETRO_VFS_FILE_ACCESS_READ, RETRO_VFS_FILE_ACCESS_HINT_NONE);
      if (fr){
         if (filestream_gets(fr, temp, sizeof(temp))){
            for (int i=0;i<sizeof(temp);i++){
               if (*(temp+i)=='p' || *(temp+i)=='i')
                  j=i;
               else if (*(temp+i)=='h')
                  *(temp+i)='\0';
            }
            k = j ? atof(temp+j+1) : k;
         }
         filestream_close(fr);
      }
      mali->refresh_rate = k;
   }

   return 0;

error:
   if (fd >= 0)
      close(fd);
   return 1;
}

static void gfx_ctx_mali_fbdev_clear_screen(void)
{
   struct fb_var_screeninfo vinfo;
   void *buffer = NULL;
   int fd                = open("/dev/fb0", O_RDWR);
   ioctl (fd, FBIOGET_VSCREENINFO, &vinfo);
   long buffer_size = vinfo.xres * vinfo.yres * vinfo.bits_per_pixel / 8;
   buffer = calloc(1, buffer_size);
   write(fd,buffer,buffer_size);
   free(buffer);
   close(fd);

   /* Clear framebuffer and set cursor on again */
   if (!system(NULL) && !system("which setterm > /dev/null 2>&1"))
      {
        int fd = open("/dev/tty", O_RDWR);
        ioctl(fd, VT_ACTIVATE, 5);
        ioctl(fd, VT_ACTIVATE, 1);
        close(fd);
        system("setterm -cursor on");
      }
}

static void gfx_ctx_mali_fbdev_destroy_really(void)
{
   if (gfx_ctx_mali_fbdev_global)
   {
#ifdef HAVE_EGL
      egl_destroy(&gfx_ctx_mali_fbdev_global->egl);
#endif
      gfx_ctx_mali_fbdev_global->resize=false;
      free(gfx_ctx_mali_fbdev_global);
      gfx_ctx_mali_fbdev_global=NULL;
   }
}

static void gfx_ctx_mali_fbdev_maybe_restart(void)
{
   runloop_state_t *runloop_st   = runloop_state_get_ptr();

   if (!runloop_st->shutdown_initiated)
      frontend_driver_set_fork(FRONTEND_FORK_RESTART);
}

/*TODO FIXME
As egl_destroy does not work properly with libmali (big fps drop after destroy and initialization/creation of new context/surface), it is not used.
A global pointers is initialized at startup in gfx_ctx_mali_fbdev_init, and returned each time gfx_ctx_mali_fbdev_init is called.
Originally gfx_ctx_mali_fbdev_init initialized a new pointer each time (destroyed each time with egl_destroy), and context/surface creation occurred in gfx_ctx_mali_fbdev_set_video_mode.
With this workaround it's all created once in gfx_ctx_mali_fbdev_init and never destroyed.
Additional workarounds (RA restart) are applied in gfx_ctx_mali_fbdev_destroy in order to avoid segmentation fault when video threaded switch is activated or on exit from cores requesting gfx_ctx_mali_fbdev_hw_ctx_trigger.
All these workarounds should be reverted when and if egl_destroy issues in libmali blobs are fixed.
*/
static void gfx_ctx_mali_fbdev_destroy(void *data)
{
   runloop_state_t *runloop_st   = runloop_state_get_ptr();

   if (runloop_st->shutdown_initiated)
   {
      if (!gfx_ctx_mali_fbdev_restart_pending)
      {
         gfx_ctx_mali_fbdev_destroy_really();
         gfx_ctx_mali_fbdev_clear_screen();
      }
   }
   else
   {
      if (gfx_ctx_mali_fbdev_hw_ctx_trigger || gfx_ctx_mali_fbdev_was_threaded!=*video_driver_get_threaded())
      {
         gfx_ctx_mali_fbdev_destroy_really();
         gfx_ctx_mali_fbdev_restart_pending=true;
         if (!gfx_ctx_mali_fbdev_hw_ctx_trigger)
            gfx_ctx_mali_fbdev_maybe_restart();
      }
   }
}

static void gfx_ctx_mali_fbdev_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   mali_ctx_data_t *mali = (mali_ctx_data_t*)data;

   *width  = mali->width;
   *height = mali->height;
}

static void *gfx_ctx_mali_fbdev_init(void *video_driver)
{
   if (gfx_ctx_mali_fbdev_global)
      return gfx_ctx_mali_fbdev_global;

#ifdef HAVE_EGL
   EGLint n;
   EGLint major, minor;
   EGLint format;
   static const EGLint attribs_init[] = {
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_BLUE_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_RED_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_NONE
   };

   static const EGLint attribs_create[] = {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };

#endif

   mali_ctx_data_t *mali = (mali_ctx_data_t*)calloc(1, sizeof(*mali));

   if (!mali)
       return NULL;
   if (gfx_ctx_mali_fbdev_get_vinfo(mali))
       goto error;

#ifdef HAVE_EGL
   frontend_driver_install_signal_handler();
   mali->egl.use_hw_ctx=true;
   if (!egl_init_context(&mali->egl, EGL_NONE, EGL_DEFAULT_DISPLAY,
            &major, &minor, &n, attribs_init, NULL) ||
   !egl_create_context(&mali->egl, attribs_create) ||
   !egl_create_surface(&mali->egl, &mali->native_window))
      goto error;
#endif

   gfx_ctx_mali_fbdev_global=mali;
   gfx_ctx_mali_fbdev_was_threaded=*video_driver_get_threaded();
   return mali;

error:
   egl_report_error();
   gfx_ctx_mali_fbdev_destroy(mali);
   return NULL;
}

static void gfx_ctx_mali_fbdev_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
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

   if (gfx_ctx_mali_fbdev_restart_pending)
      gfx_ctx_mali_fbdev_maybe_restart();
}

static bool gfx_ctx_mali_fbdev_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   mali_ctx_data_t *mali = (mali_ctx_data_t*)data;

   if (video_driver_is_hw_context())
      gfx_ctx_mali_fbdev_hw_ctx_trigger=true;

   if (gfx_ctx_mali_fbdev_get_vinfo(mali))
      goto error;

   width                      = mali->width;
   height                     = mali->height;

   return true;

error:
   gfx_ctx_mali_fbdev_destroy(data);
   return false;
}

static void gfx_ctx_mali_fbdev_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
   *input      = NULL;
   *input_data = NULL;
}

static enum gfx_ctx_api gfx_ctx_mali_fbdev_get_api(void *data)
{
   return GFX_CTX_OPENGL_ES_API;
}

static bool gfx_ctx_mali_fbdev_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   if (api == GFX_CTX_OPENGL_ES_API)
      return true;
   return false;
}

static bool gfx_ctx_mali_fbdev_has_focus(void *data) { return true; }

static bool gfx_ctx_mali_fbdev_suppress_screensaver(void *data, bool enable) { return false; }

static void gfx_ctx_mali_fbdev_set_swap_interval(void *data,
      int swap_interval)
{
   mali_ctx_data_t *mali = (mali_ctx_data_t*)data;
#ifdef HAVE_EGL
   egl_set_swap_interval(&mali->egl, swap_interval);
#endif
}
static void gfx_ctx_mali_fbdev_swap_buffers(void *data)
{
   mali_ctx_data_t *mali = (mali_ctx_data_t*)data;
#ifdef HAVE_EGL
   egl_swap_buffers(&mali->egl);
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
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);

   return flags;
}

static void gfx_ctx_mali_fbdev_set_flags(void *data, uint32_t flags) { }

static float gfx_ctx_mali_fbdev_get_refresh_rate(void *data)
{
   mali_ctx_data_t *mali = (mali_ctx_data_t*)data;

   return mali->refresh_rate;
}

const gfx_ctx_driver_t gfx_ctx_mali_fbdev = {
   gfx_ctx_mali_fbdev_init,
   gfx_ctx_mali_fbdev_destroy,
   gfx_ctx_mali_fbdev_get_api,
   gfx_ctx_mali_fbdev_bind_api,
   gfx_ctx_mali_fbdev_set_swap_interval,
   gfx_ctx_mali_fbdev_set_video_mode,
   gfx_ctx_mali_fbdev_get_video_size,
   gfx_ctx_mali_fbdev_get_refresh_rate,
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
   false, /* has_windowed */
   gfx_ctx_mali_fbdev_swap_buffers,
   gfx_ctx_mali_fbdev_input_driver,
#ifdef HAVE_EGL
   egl_get_proc_address,
#else
   NULL,
#endif
   NULL,
   NULL,
   NULL,
   "fbdev_mali",
   gfx_ctx_mali_fbdev_get_flags,
   gfx_ctx_mali_fbdev_set_flags,
   gfx_ctx_mali_fbdev_bind_hw_render,
   NULL,
   NULL
};

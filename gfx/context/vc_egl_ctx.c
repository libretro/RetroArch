/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
 *  Copyright (C) 2012 - Michael Lelli
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

// VideoCore context, for Rasperry Pi.

#include "../../driver.h"
#include "../gfx_context.h"
#include "../gl_common.h"
#include "../gfx_common.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <unistd.h>
#include <sched.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>
#include <EGL/eglext_brcm.h>
#include <VG/openvg.h>
#include <bcm_host.h>

static EGLContext g_egl_ctx;
static EGLSurface g_egl_surf;
static EGLDisplay g_egl_dpy;
static EGLConfig g_config;

static volatile sig_atomic_t g_quit;
static bool g_inited;
static unsigned g_interval;
static enum gfx_ctx_api g_api;

static unsigned g_fb_width;
static unsigned g_fb_height;

static EGLImageKHR eglBuffer[MAX_EGLIMAGE_TEXTURES];
static EGLContext g_eglimage_ctx;
static EGLSurface g_pbuff_surf;
static VGImage g_egl_vgimage[MAX_EGLIMAGE_TEXTURES];
static bool g_smooth;
static unsigned g_egl_res;

static PFNEGLCREATEIMAGEKHRPROC peglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC peglDestroyImageKHR;

static inline bool gfx_ctx_egl_query_extension(const char *ext)
{
   const char *str = (const char*)eglQueryString(g_egl_dpy, EGL_EXTENSIONS);
   bool ret = str && strstr(str, ext);
   RARCH_LOG("Querying EGL extension: %s => %s\n",
         ext, ret ? "exists" : "doesn't exist");

   return ret;
}

static void sighandler(int sig)
{
   (void)sig;
   g_quit = 1;
}

static void gfx_ctx_swap_interval(unsigned interval)
{
   // Can be called before initialization.
   // Some contexts require that swap interval is known at startup time.
   g_interval = interval;
   if (g_egl_dpy)
      eglSwapInterval(g_egl_dpy, interval);
}

static void gfx_ctx_check_window(bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)frame_count;
   (void)width;
   (void)height;

   *resize = false;
   *quit   = g_quit;
}

static void gfx_ctx_swap_buffers(void)
{
   eglSwapBuffers(g_egl_dpy, g_egl_surf);
}

static void gfx_ctx_set_resize(unsigned width, unsigned height)
{
   (void)width;
   (void)height;
}

static void gfx_ctx_update_window_title(void)
{
   char buf[128];
   gfx_get_fps(buf, sizeof(buf), false);
}

static void gfx_ctx_get_video_size(unsigned *width, unsigned *height)
{
   *width  = g_fb_width;
   *height = g_fb_height;
}

static void gfx_ctx_destroy(void);

static bool gfx_ctx_init(void)
{
   RARCH_LOG("[VC/EGL]: Initializing...\n");
   if (g_inited)
   {
      RARCH_ERR("[VC/EGL]: Attempted to re-initialize driver.\n");
      return false;
   }

   EGLint num_config;
   static EGL_DISPMANX_WINDOW_T nativewindow;

   DISPMANX_ELEMENT_HANDLE_T dispman_element;
   DISPMANX_DISPLAY_HANDLE_T dispman_display;
   DISPMANX_UPDATE_HANDLE_T dispman_update;
   DISPMANX_MODEINFO_T dispman_modeinfo;
   VC_RECT_T dst_rect;
   VC_RECT_T src_rect;

   static const EGLint attribute_list[] =
   {
      EGL_RED_SIZE, 8,
      EGL_GREEN_SIZE, 8,
      EGL_BLUE_SIZE, 8,
      EGL_ALPHA_SIZE, 8,
      EGL_SURFACE_TYPE, EGL_WINDOW_BIT,
      EGL_NONE
   };

   static const EGLint context_attributes[] =
   {
      EGL_CONTEXT_CLIENT_VERSION, 2,
      EGL_NONE
   };

   bcm_host_init();

   // get an EGL display connection
   g_egl_dpy = eglGetDisplay(EGL_DEFAULT_DISPLAY);
   if (!g_egl_dpy)
      goto error;

   // initialize the EGL display connection
   if (!eglInitialize(g_egl_dpy, NULL, NULL))
      goto error;

   // get an appropriate EGL frame buffer configuration
   if (!eglChooseConfig(g_egl_dpy, attribute_list, &g_config, 1, &num_config))
      goto error;

   // create an EGL rendering context
   g_egl_ctx = eglCreateContext(g_egl_dpy, g_config, EGL_NO_CONTEXT, (g_api == GFX_CTX_OPENGL_ES_API) ? context_attributes : NULL);
   if (!g_egl_ctx)
      goto error;

   // create an EGL window surface
   if (graphics_get_display_size(0 /* LCD */, &g_fb_width, &g_fb_height) < 0)
      goto error;

   dst_rect.x = 0;
   dst_rect.y = 0;
   dst_rect.width = g_fb_width;
   dst_rect.height = g_fb_height;

   src_rect.x = 0;
   src_rect.y = 0;
   src_rect.width = g_fb_width << 16;
   src_rect.height = g_fb_height << 16;

   dispman_display = vc_dispmanx_display_open(0 /* LCD */);
   vc_dispmanx_display_get_info(dispman_display, &dispman_modeinfo);
   dispman_update = vc_dispmanx_update_start(0);

   VC_DISPMANX_ALPHA_T alpha;
   alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
   alpha.opacity = 255;
   alpha.mask = 0;

   dispman_element = vc_dispmanx_element_add(dispman_update, dispman_display,
      0 /*layer*/, &dst_rect, 0 /*src*/,
      &src_rect, DISPMANX_PROTECTION_NONE, &alpha, 0 /*clamp*/, DISPMANX_NO_ROTATE);

   nativewindow.element = dispman_element;
   nativewindow.width = g_fb_width;
   nativewindow.height = g_fb_height;
   vc_dispmanx_update_submit_sync(dispman_update);

   g_egl_surf = eglCreateWindowSurface(g_egl_dpy, g_config, &nativewindow, NULL);
   if (!g_egl_surf)
      goto error;

   // connect the context to the surface
   if (!eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx))
      goto error;

   return true;

error:
   gfx_ctx_destroy();
   return false;
}

static bool gfx_ctx_set_video_mode(
      unsigned width, unsigned height,
      bool fullscreen)
{
   if (g_inited)
      return false;

   struct sigaction sa = {{0}};
   sa.sa_handler = sighandler;
   sa.sa_flags   = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);

   gfx_ctx_swap_interval(g_interval);

   g_inited = true;
   return true;
}

static bool gfx_ctx_bind_api(enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)major;
   (void)minor;
   g_api = api;
   switch (api)
   {
      case GFX_CTX_OPENGL_API:
         return eglBindAPI(EGL_OPENGL_API);
      case GFX_CTX_OPENGL_ES_API:
         return eglBindAPI(EGL_OPENGL_ES_API);
      case GFX_CTX_OPENVG_API:
         return eglBindAPI(EGL_OPENVG_API);
      default:
         return false;
   }
}

static void gfx_ctx_destroy(void)
{
   if (g_egl_dpy)
   {
      for (unsigned i = 0; i < MAX_EGLIMAGE_TEXTURES; i++)
      {
         if (eglBuffer[i] && peglDestroyImageKHR)
         {
            eglBindAPI(EGL_OPENVG_API);
            eglMakeCurrent(g_egl_dpy, g_pbuff_surf, g_pbuff_surf, g_eglimage_ctx);
            peglDestroyImageKHR(g_egl_dpy, eglBuffer[i]);
         }

         if (g_egl_vgimage[i])
         {
            eglBindAPI(EGL_OPENVG_API);
            eglMakeCurrent(g_egl_dpy, g_pbuff_surf, g_pbuff_surf, g_eglimage_ctx);
            vgDestroyImage(g_egl_vgimage[i]);
         }
      }

      if (g_egl_ctx)
      {
         gfx_ctx_bind_api(g_api, 0, 0);
         eglMakeCurrent(g_egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
         eglDestroyContext(g_egl_dpy, g_egl_ctx);
      }

      if (g_eglimage_ctx)
      {
         eglBindAPI(EGL_OPENVG_API);
         eglMakeCurrent(g_egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
         eglDestroyContext(g_egl_dpy, g_eglimage_ctx);
      }

      if (g_egl_surf)
      {
         gfx_ctx_bind_api(g_api, 0, 0);
         eglDestroySurface(g_egl_dpy, g_egl_surf);
      }

      if (g_pbuff_surf)
      {
         eglBindAPI(EGL_OPENVG_API);
         eglDestroySurface(g_egl_dpy, g_pbuff_surf);
      }

      eglBindAPI(EGL_OPENVG_API);
      eglMakeCurrent(g_egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      gfx_ctx_bind_api(g_api, 0, 0);
      eglMakeCurrent(g_egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      eglTerminate(g_egl_dpy);
   }

   g_egl_ctx      = NULL;
   g_eglimage_ctx = NULL;
   g_egl_surf     = NULL;
   g_pbuff_surf   = NULL;
   g_egl_dpy      = NULL;
   g_config       = 0;
   g_inited       = false;

   for (unsigned i = 0; i < MAX_EGLIMAGE_TEXTURES; i++)
   {
      eglBuffer[i]     = NULL;
      g_egl_vgimage[i] = 0;
   }
}

static void gfx_ctx_input_driver(const input_driver_t **input, void **input_data)
{
   void *linuxinput = input_linuxraw.init();
   *input           = linuxinput ? &input_linuxraw : NULL;
   *input_data      = linuxinput;
}

static bool gfx_ctx_has_focus(void)
{
   return g_inited;
}

static gfx_ctx_proc_t gfx_ctx_get_proc_address(const char *symbol)
{
   return eglGetProcAddress(symbol);
}

static float gfx_ctx_translate_aspect(unsigned width, unsigned height)
{
   // check for SD televisions: they should always be 4:3.
   if ((width == 640 || width == 720) && (height == 480 || height == 576))
      return 4.0f / 3.0f;
   else
      return (float)width / height;
}

static bool gfx_ctx_init_egl_image_buffer(const video_info_t *video)
{
   if (g_api == GFX_CTX_OPENVG_API) // don't bother, we just use VGImages for our EGLImage anyway
   {
      return false;
   }

   peglCreateImageKHR = (PFNEGLCREATEIMAGEKHRPROC)gfx_ctx_get_proc_address("eglCreateImageKHR");
   peglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)gfx_ctx_get_proc_address("eglDestroyImageKHR");

   if (!peglCreateImageKHR || !peglDestroyImageKHR || !gfx_ctx_egl_query_extension("KHR_image"))
   {
      return false;
   }

   g_egl_res = video->input_scale * RARCH_SCALE_BASE;

   EGLint pbufsurface_list[] =
   {
      EGL_WIDTH, g_egl_res,
      EGL_HEIGHT, g_egl_res,
      EGL_NONE
   };

   EGLBoolean result;

   eglBindAPI(EGL_OPENVG_API);
   g_pbuff_surf = eglCreatePbufferSurface(g_egl_dpy, g_config, pbufsurface_list);
   if (g_pbuff_surf == EGL_NO_SURFACE)
   {
      RARCH_ERR("[VideoCore:EGLImage] failed to create PbufferSurface\n");
      goto fail;
   }

   g_eglimage_ctx = eglCreateContext(g_egl_dpy, g_config, NULL, NULL);
   if (g_eglimage_ctx == EGL_NO_CONTEXT)
   {
      RARCH_ERR("[VideoCore:EGLImage] failed to create context\n");
      goto fail;
   }

   // test to make sure we can switch context
   result = eglMakeCurrent(g_egl_dpy, g_pbuff_surf, g_pbuff_surf, g_eglimage_ctx);
   if (result == EGL_FALSE)
   {
      RARCH_ERR("[VideoCore:EGLImage] failed to make context current\n");
      goto fail;
   }

   gfx_ctx_bind_api(g_api, 0, 0);
   eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx);

   g_smooth = video->smooth;
   return true;

fail:
   if (g_pbuff_surf != EGL_NO_SURFACE)
   {
      eglDestroySurface(g_egl_dpy, g_pbuff_surf);
      g_pbuff_surf = EGL_NO_SURFACE;
   }

   if (g_eglimage_ctx != EGL_NO_CONTEXT)
   {
      eglDestroyContext(g_egl_dpy, g_eglimage_ctx);
      g_pbuff_surf = EGL_NO_CONTEXT;
   }

   gfx_ctx_bind_api(g_api, 0, 0);
   eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx);

   return false;
}

static bool gfx_ctx_write_egl_image(const void *frame, unsigned width, unsigned height, unsigned pitch, bool rgb32, unsigned index, void **image_handle)
{
   bool ret = false;

   if (index >= MAX_EGLIMAGE_TEXTURES)
   {
      *image_handle = NULL;
      return false;
   }

   eglBindAPI(EGL_OPENVG_API);
   eglMakeCurrent(g_egl_dpy, g_pbuff_surf, g_pbuff_surf, g_eglimage_ctx);

   if (!eglBuffer[index] || !g_egl_vgimage[index])
   {
      g_egl_vgimage[index] = vgCreateImage(rgb32 ? VG_sXRGB_8888 : VG_sRGB_565, g_egl_res, g_egl_res, VG_IMAGE_QUALITY_NONANTIALIASED);
      eglBuffer[index] = peglCreateImageKHR(g_egl_dpy, g_eglimage_ctx, EGL_VG_PARENT_IMAGE_KHR, (EGLClientBuffer)g_egl_vgimage[index], NULL);
      ret = true;
   }

   vgImageSubData(g_egl_vgimage[index], frame, pitch, (rgb32 ? VG_sXRGB_8888 : VG_sRGB_565), 0, 0, width, height);
   *image_handle = eglBuffer[index];

   gfx_ctx_bind_api(g_api, 0, 0);
   eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx);

   return ret;
}

const gfx_ctx_driver_t gfx_ctx_videocore = {
   gfx_ctx_init,
   gfx_ctx_destroy,
   gfx_ctx_bind_api,
   gfx_ctx_swap_interval,
   gfx_ctx_set_video_mode,
   gfx_ctx_get_video_size,
   gfx_ctx_translate_aspect,
   gfx_ctx_update_window_title,
   gfx_ctx_check_window,
   gfx_ctx_set_resize,
   gfx_ctx_has_focus,
   gfx_ctx_swap_buffers,
   gfx_ctx_input_driver,
   gfx_ctx_get_proc_address,
   gfx_ctx_init_egl_image_buffer,
   gfx_ctx_write_egl_image,
   NULL,
   "videocore",
};

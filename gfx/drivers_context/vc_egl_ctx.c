/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2012-2015 - Michael Lelli
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

#include <stdint.h>
#include <errno.h>
#include <signal.h>
#include <unistd.h>

#include <sched.h>

#include <VG/openvg.h>
#include <bcm_host.h>

#include <retro_inline.h>

#include "../../driver.h"
#include "../../runloop.h"
#include "../video_context_driver.h"

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include "../common/gl_common.h"
#endif

#ifdef HAVE_EGL
#include <EGL/eglext_brcm.h>
#endif


#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

typedef struct
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
#endif
   bool resize;
   unsigned fb_width, fb_height;

   EGLImageKHR eglBuffer[MAX_EGLIMAGE_TEXTURES];
   EGLContext eglimage_ctx;
   EGLSurface pbuff_surf;
   VGImage vgimage[MAX_EGLIMAGE_TEXTURES];
   bool smooth;
   unsigned res;
} vc_ctx_data_t;

static enum gfx_ctx_api vc_api;
static PFNEGLCREATEIMAGEKHRPROC peglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC peglDestroyImageKHR;

static INLINE bool gfx_ctx_vc_egl_query_extension(vc_ctx_data_t *vc, const char *ext)
{
   const char *str = (const char*)eglQueryString(vc->egl.dpy, EGL_EXTENSIONS);
   bool ret = str && strstr(str, ext);
   RARCH_LOG("Querying EGL extension: %s => %s\n",
         ext, ret ? "exists" : "doesn't exist");

   return ret;
}

static void gfx_ctx_vc_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)data;
   (void)frame_count;
   (void)width;
   (void)height;

   *resize = false;
   *quit   = g_egl_quit;
}

static bool gfx_ctx_vc_set_resize(void *data, unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
   return false;
}

static void gfx_ctx_vc_update_window_title(void *data)
{
   char buf[128]        = {0};
   char buf_fps[128]    = {0};
   settings_t *settings = config_get_ptr();

   (void)data;

   video_monitor_get_fps(buf, sizeof(buf),
         buf_fps, sizeof(buf_fps));
   if (settings->fps_show)
      runloop_msg_queue_push(buf_fps, 1, 1, false);
}

static void gfx_ctx_vc_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   vc_ctx_data_t *vc = (vc_ctx_data_t*)data;
   settings_t *settings = config_get_ptr();

   /* Use dispmanx upscaling if 
    * fullscreen_x and fullscreen_y are set. */

   if (settings->video.fullscreen_x != 0 &&
      settings->video.fullscreen_y != 0)
   {
      /* Keep input and output aspect ratio equal.
       * There are other aspect ratio settings 
       * which can be used to stretch video output. */

      /*  Calculate source and destination aspect ratios. */

      float srcAspect = (float)settings->video.fullscreen_x / (float)settings->video.fullscreen_y;
      float dstAspect = (float)vc->fb_width / (float)vc->fb_height;

      /* If source and destination aspect ratios are not equal correct source width. */
      if (srcAspect != dstAspect)
         *width = (unsigned)(settings->video.fullscreen_y * dstAspect);
      else
         *width = settings->video.fullscreen_x;
      *height = settings->video.fullscreen_y;
   }
   else
   {
      *width  = vc->fb_width;
      *height = vc->fb_height;
   }
}

static void gfx_ctx_vc_destroy(void *data);

static void *gfx_ctx_vc_init(void *video_driver)
{
   VC_DISPMANX_ALPHA_T alpha;
   EGLint n, major, minor;
   static EGL_DISPMANX_WINDOW_T nativewindow;

   DISPMANX_ELEMENT_HANDLE_T dispman_element;
   DISPMANX_DISPLAY_HANDLE_T dispman_display;
   DISPMANX_UPDATE_HANDLE_T dispman_update;
   DISPMANX_MODEINFO_T dispman_modeinfo;
   VC_RECT_T dst_rect;
   VC_RECT_T src_rect;

#ifdef HAVE_EGL
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
#endif
   settings_t *settings = config_get_ptr();
   vc_ctx_data_t *vc    = NULL;

   if (g_egl_inited)
   {
      RARCH_ERR("[VC/EGL]: Attempted to re-initialize driver.\n");
      return NULL;
   }

   vc = (vc_ctx_data_t*)calloc(1, sizeof(*vc));

   if (!vc)
       return NULL;

   bcm_host_init();

#ifdef HAVE_EGL
   if (!egl_init_context(&vc->egl, EGL_DEFAULT_DISPLAY,
            &major, &minor, &n, attribute_list))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_create_context(&vc->egl, (vc_api == GFX_CTX_OPENGL_ES_API) ? context_attributes : NULL))
   {
      egl_report_error();
      goto error;
   }
#endif

   /* Create an EGL window surface. */
   if (graphics_get_display_size(0 /* LCD */, &vc->fb_width, &vc->fb_height) < 0)
      goto error;

   dst_rect.x = 0;
   dst_rect.y = 0;
   dst_rect.width = vc->fb_width;
   dst_rect.height = vc->fb_height;

   src_rect.x = 0;
   src_rect.y = 0;

   /* Use dispmanx upscaling if fullscreen_x 
    * and fullscreen_y are set. */
   if (settings->video.fullscreen_x != 0 &&
      settings->video.fullscreen_y != 0)
   {
      /* Keep input and output aspect ratio equal.
       * There are other aspect ratio settings which can be used to stretch video output. */

      /* Calculate source and destination aspect ratios. */
      float srcAspect = (float)settings->video.fullscreen_x / (float)settings->video.fullscreen_y;
      float dstAspect = (float)vc->fb_width / (float)vc->fb_height;
      /* If source and destination aspect ratios are not equal correct source width. */
      if (srcAspect != dstAspect)
         src_rect.width = (unsigned)(settings->video.fullscreen_y * dstAspect) << 16;
      else
         src_rect.width = settings->video.fullscreen_x << 16;
      src_rect.height = settings->video.fullscreen_y << 16;
   }
   else
   {
      src_rect.width  = vc->fb_width << 16;
      src_rect.height = vc->fb_height << 16;
   }

   dispman_display = vc_dispmanx_display_open(0 /* LCD */);
   vc_dispmanx_display_get_info(dispman_display, &dispman_modeinfo);
   dispman_update = vc_dispmanx_update_start(0);

   alpha.flags = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
   alpha.opacity = 255;
   alpha.mask = 0;

   dispman_element = vc_dispmanx_element_add(dispman_update, dispman_display,
      0 /*layer*/, &dst_rect, 0 /*src*/,
      &src_rect, DISPMANX_PROTECTION_NONE, &alpha, 0 /*clamp*/, DISPMANX_NO_ROTATE);

   nativewindow.element = dispman_element;

   /* Use dispmanx upscaling if fullscreen_x and fullscreen_y are set. */

   if (settings->video.fullscreen_x != 0 &&
      settings->video.fullscreen_y != 0)
   {
      /* Keep input and output aspect ratio equal.
       * There are other aspect ratio settings which 
       * can be used to stretch video output. */

      /* Calculate source and destination aspect ratios. */
      float srcAspect = (float)settings->video.fullscreen_x / (float)settings->video.fullscreen_y;
      float dstAspect = (float)vc->fb_width / (float)vc->fb_height;

      /* If source and destination aspect ratios are not equal correct source width. */
      if (srcAspect != dstAspect)
         nativewindow.width = (unsigned)(settings->video.fullscreen_y * dstAspect);
      else
         nativewindow.width = settings->video.fullscreen_x;
      nativewindow.height = settings->video.fullscreen_y;
   }
   else
   {
      nativewindow.width = vc->fb_width;
      nativewindow.height = vc->fb_height;
   }
   vc_dispmanx_update_submit_sync(dispman_update);

#ifdef HAVE_EGL
   if (!egl_create_surface(&vc->egl, &nativewindow))
      goto error;
#endif

   return vc;

error:
   gfx_ctx_vc_destroy(video_driver);
   return NULL;
}

static void gfx_ctx_vc_set_swap_interval(void *data, unsigned swap_interval)
{
   vc_ctx_data_t *vc = (vc_ctx_data_t*)data;
   
#ifdef HAVE_EGL
   egl_set_swap_interval(&vc->egl, swap_interval);
#endif
}

static bool gfx_ctx_vc_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
    vc_ctx_data_t *vc = (vc_ctx_data_t*)data;

#ifdef HAVE_EGL
   if (g_egl_inited)
      return false;

   egl_install_sighandlers();
   gfx_ctx_vc_set_swap_interval(&vc->egl, vc->egl.interval);

   g_egl_inited = true;
#endif

   return true;
}

static bool gfx_ctx_vc_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;
   (void)major;
   (void)minor;

   vc_api = api;

   switch (api)
   {
      case GFX_CTX_OPENGL_API:
         return eglBindAPI(EGL_OPENGL_API);
      case GFX_CTX_OPENGL_ES_API:
         return eglBindAPI(EGL_OPENGL_ES_API);
      case GFX_CTX_OPENVG_API:
         return eglBindAPI(EGL_OPENVG_API);
      default:
         break;
   }

   return false;
}

static void gfx_ctx_vc_destroy(void *data)
{
   vc_ctx_data_t *vc = (vc_ctx_data_t*)data;
   unsigned i;

   if (!vc)
   {
       g_egl_inited = false;
       return;
   }

   if (vc->egl.dpy)
   {
      for (i = 0; i < MAX_EGLIMAGE_TEXTURES; i++)
      {
         if (vc->eglBuffer[i] && peglDestroyImageKHR)
         {
            eglBindAPI(EGL_OPENVG_API);
            eglMakeCurrent(vc->egl.dpy,
                  vc->pbuff_surf, vc->pbuff_surf, vc->eglimage_ctx);
            peglDestroyImageKHR(vc->egl.dpy, vc->eglBuffer[i]);
         }

         if (vc->vgimage[i])
         {
            eglBindAPI(EGL_OPENVG_API);
            eglMakeCurrent(vc->egl.dpy,
                  vc->pbuff_surf, vc->pbuff_surf, vc->eglimage_ctx);
            vgDestroyImage(vc->vgimage[i]);
         }
      }

      if (vc->egl.ctx)
      {
         gfx_ctx_vc_bind_api(data, vc_api, 0, 0);
         eglMakeCurrent(vc->egl.dpy,
               EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
         eglDestroyContext(vc->egl.dpy, vc->egl.ctx);
      }

      if (vc->egl.hw_ctx)
         eglDestroyContext(vc->egl.dpy, vc->egl.hw_ctx);

      if (vc->eglimage_ctx)
      {
         eglBindAPI(EGL_OPENVG_API);
         eglMakeCurrent(vc->egl.dpy,
               EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
         eglDestroyContext(vc->egl.dpy, vc->eglimage_ctx);
      }

      if (vc->egl.surf)
      {
         gfx_ctx_vc_bind_api(data, vc_api, 0, 0);
         eglDestroySurface(vc->egl.dpy, vc->egl.surf);
      }

      if (vc->pbuff_surf)
      {
         eglBindAPI(EGL_OPENVG_API);
         eglDestroySurface(vc->egl.dpy, vc->pbuff_surf);
      }

      eglBindAPI(EGL_OPENVG_API);
      eglMakeCurrent(vc->egl.dpy,
            EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      gfx_ctx_vc_bind_api(data, vc_api, 0, 0);
      eglMakeCurrent(vc->egl.dpy,
            EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      eglTerminate(vc->egl.dpy);
   }

   vc->egl.ctx      = NULL;
   vc->egl.hw_ctx   = NULL;
   vc->eglimage_ctx = NULL;
   vc->egl.surf     = NULL;
   vc->pbuff_surf   = NULL;
   vc->egl.dpy      = NULL;
   vc->egl.config   = 0;
   g_egl_inited     = false;

   for (i = 0; i < MAX_EGLIMAGE_TEXTURES; i++)
   {
      vc->eglBuffer[i]     = NULL;
      vc->vgimage[i] = 0;
   }
}

static void gfx_ctx_vc_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   (void)data;
   *input = NULL;
   *input_data = NULL;
}

static bool gfx_ctx_vc_has_focus(void *data)
{
   (void)data;
   return g_egl_inited;
}

static bool gfx_ctx_vc_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool gfx_ctx_vc_has_windowed(void *data)
{
   (void)data;
   return false;
}

static float gfx_ctx_vc_translate_aspect(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   /* Check for SD televisions: they should always be 4:3. */
   if ((width == 640 || width == 720) && (height == 480 || height == 576))
      return 4.0f / 3.0f;
   return (float)width / height;
}

static bool gfx_ctx_vc_image_buffer_init(void *data,
      const video_info_t *video)
{
   vc_ctx_data_t *vc = (vc_ctx_data_t*)data;
   EGLBoolean result;
   EGLint pbufsurface_list[] =
   {
      EGL_WIDTH, vc->res,
      EGL_HEIGHT, vc->res,
      EGL_NONE
   };

   /* Don't bother, we just use VGImages for our EGLImage anyway. */
   if (vc_api == GFX_CTX_OPENVG_API)
      return false;

   peglCreateImageKHR  = (PFNEGLCREATEIMAGEKHRPROC)egl_get_proc_address("eglCreateImageKHR");
   peglDestroyImageKHR = (PFNEGLDESTROYIMAGEKHRPROC)egl_get_proc_address("eglDestroyImageKHR");

   if (!peglCreateImageKHR || !peglDestroyImageKHR 
         || !gfx_ctx_vc_egl_query_extension(vc, "KHR_image"))
      return false;

   vc->res = video->input_scale * RARCH_SCALE_BASE;

   eglBindAPI(EGL_OPENVG_API);
   vc->pbuff_surf = eglCreatePbufferSurface(vc->egl.dpy, vc->egl.config, pbufsurface_list);
   if (vc->pbuff_surf == EGL_NO_SURFACE)
   {
      RARCH_ERR("[VideoCore:EGLImage] failed to create PbufferSurface\n");
      goto fail;
   }

   vc->eglimage_ctx = eglCreateContext(vc->egl.dpy, vc->egl.config, NULL, NULL);
   if (vc->eglimage_ctx == EGL_NO_CONTEXT)
   {
      RARCH_ERR("[VideoCore:EGLImage] failed to create context\n");
      goto fail;
   }

   /* Test to make sure we can switch context. */
   result = eglMakeCurrent(vc->egl.dpy, vc->pbuff_surf, vc->pbuff_surf, vc->eglimage_ctx);
   if (result == EGL_FALSE)
   {
      RARCH_ERR("[VideoCore:EGLImage] failed to make context current\n");
      goto fail;
   }

   gfx_ctx_vc_bind_api(NULL, vc_api, 0, 0);
   eglMakeCurrent(vc->egl.dpy, vc->egl.surf, vc->egl.surf, vc->egl.ctx);

   vc->smooth = video->smooth;
   return true;

fail:
   if (vc->pbuff_surf != EGL_NO_SURFACE)
   {
      eglDestroySurface(vc->egl.dpy, vc->pbuff_surf);
      vc->pbuff_surf = EGL_NO_SURFACE;
   }

   if (vc->eglimage_ctx != EGL_NO_CONTEXT)
   {
      eglDestroyContext(vc->egl.dpy, vc->eglimage_ctx);
      vc->pbuff_surf = EGL_NO_CONTEXT;
   }

   gfx_ctx_vc_bind_api(NULL, vc_api, 0, 0);
   eglMakeCurrent(vc->egl.dpy, vc->egl.surf, vc->egl.surf, vc->egl.ctx);

   return false;
}

static bool gfx_ctx_vc_image_buffer_write(void *data, const void *frame, unsigned width,
      unsigned height, unsigned pitch, bool rgb32, unsigned index, void **image_handle)
{
   bool ret = false;
   vc_ctx_data_t *vc = (vc_ctx_data_t*)data;

   if (index >= MAX_EGLIMAGE_TEXTURES)
      goto error;

   eglBindAPI(EGL_OPENVG_API);
   eglMakeCurrent(vc->egl.dpy, vc->pbuff_surf, vc->pbuff_surf, vc->eglimage_ctx);

   if (!vc->eglBuffer[index] || !vc->vgimage[index])
   {
      vc->vgimage[index] = vgCreateImage(
            rgb32 ? VG_sXRGB_8888 : VG_sRGB_565,
            vc->res,
            vc->res,
            VG_IMAGE_QUALITY_NONANTIALIASED);
      vc->eglBuffer[index] = peglCreateImageKHR(
            vc->egl.dpy,
            vc->eglimage_ctx,
            EGL_VG_PARENT_IMAGE_KHR,
            (EGLClientBuffer)vc->vgimage[index],
            NULL);
      ret = true;
   }

   vgImageSubData(
         vc->vgimage[index],
         frame, pitch,
         (rgb32 ? VG_sXRGB_8888 : VG_sRGB_565),
         0,
         0,
         width,
         height);
   *image_handle = vc->eglBuffer[index];

   gfx_ctx_vc_bind_api(NULL, vc_api, 0, 0);
   eglMakeCurrent(vc->egl.dpy, vc->egl.surf, vc->egl.surf, vc->egl.ctx);

   return ret;

error:
   *image_handle = NULL;
   return false;
}

static void gfx_ctx_vc_swap_buffers(void *data)
{
   vc_ctx_data_t *vc = (vc_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_swap_buffers(&vc->egl);
#endif
}

static void gfx_ctx_vc_bind_hw_render(void *data, bool enable)
{
   vc_ctx_data_t *vc = (vc_ctx_data_t*)data;

#ifdef HAVE_EGL
   egl_bind_hw_render(&vc->egl, enable);
#endif
}

static gfx_ctx_proc_t gfx_ctx_vc_get_proc_address(const char *symbol)
{
#ifdef HAVE_EGL
   return egl_get_proc_address(symbol);
#else
   return NULL;
#endif
}

static uint32_t gfx_ctx_vc_get_flags(void *data)
{
   uint32_t flags = 0;
   BIT32_SET(flags, GFX_CTX_FLAGS_NONE);
   return flags;
}

static void gfx_ctx_vc_set_flags(void *data, uint32_t flags)
{
   (void)data;
}

const gfx_ctx_driver_t gfx_ctx_videocore = {
   gfx_ctx_vc_init,
   gfx_ctx_vc_destroy,
   gfx_ctx_vc_bind_api,
   gfx_ctx_vc_set_swap_interval,
   gfx_ctx_vc_set_video_mode,
   gfx_ctx_vc_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   gfx_ctx_vc_translate_aspect,
   gfx_ctx_vc_update_window_title,
   gfx_ctx_vc_check_window,
   gfx_ctx_vc_set_resize,
   gfx_ctx_vc_has_focus,
   gfx_ctx_vc_suppress_screensaver,
   gfx_ctx_vc_has_windowed,
   gfx_ctx_vc_swap_buffers,
   gfx_ctx_vc_input_driver,
   gfx_ctx_vc_get_proc_address,
   gfx_ctx_vc_image_buffer_init,
   gfx_ctx_vc_image_buffer_write,
   NULL,
   "videocore",
   gfx_ctx_vc_get_flags,
   gfx_ctx_vc_set_flags,
   gfx_ctx_vc_bind_hw_render
};

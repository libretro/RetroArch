/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <rthreads/rthreads.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <retro_inline.h>

#include "../../configuration.h"
#include "../../retroarch.h"

#include "../../frontend/frontend_driver.h"

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

#include "../../verbosity.h"

typedef struct
{
   bool smooth;
   bool vsync_callback_set;
   bool resize;
   unsigned res;
   unsigned fb_width, fb_height;
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
#endif
   EGL_DISPMANX_WINDOW_T native_window;
   DISPMANX_DISPLAY_HANDLE_T dispman_display;
   /* For vsync wait after eglSwapBuffers when max_swapchain < 3 */
   scond_t *vsync_condition;
   slock_t *vsync_condition_mutex;
   EGLImageKHR eglBuffer[MAX_EGLIMAGE_TEXTURES];
   EGLContext eglimage_ctx;
   EGLSurface pbuff_surf;
   VGImage vgimage[MAX_EGLIMAGE_TEXTURES];
} vc_ctx_data_t;

static enum gfx_ctx_api vc_api = GFX_CTX_NONE;
static PFNEGLCREATEIMAGEKHRPROC peglCreateImageKHR;
static PFNEGLDESTROYIMAGEKHRPROC peglDestroyImageKHR;

static INLINE bool gfx_ctx_vc_egl_query_extension(vc_ctx_data_t *vc, const char *ext)
{
   const char *str = (const char*)eglQueryString(vc->egl.dpy, EGL_EXTENSIONS);
   bool        ret = str && strstr(str, ext);
   RARCH_LOG("Querying EGL extension: %s => %s\n",
         ext, ret ? "exists" : "doesn't exist");

   return ret;
}

static void gfx_ctx_vc_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height,
      bool is_shutdown)
{
   (void)data;
   (void)width;
   (void)height;

   *resize = false;
   *quit   = (bool)frontend_driver_get_signal_handler_state();
}

static void gfx_ctx_vc_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   vc_ctx_data_t    *vc = (vc_ctx_data_t*)data;
   settings_t *settings = config_get_ptr();

   /* Use dispmanx upscaling if
    * fullscreen_x and fullscreen_y are set. */

   if (settings->uints.video_fullscreen_x != 0 &&
      settings->uints.video_fullscreen_y != 0)
   {
      /* Keep input and output aspect ratio equal.
       * There are other aspect ratio settings
       * which can be used to stretch video output. */

      /*  Calculate source and destination aspect ratios. */

      float srcAspect = (float)settings->uints.video_fullscreen_x
         / (float)settings->uints.video_fullscreen_y;
      float dstAspect = (float)vc->fb_width / (float)vc->fb_height;

      /* If source and destination aspect ratios
       * are not equal correct source width. */
      if (srcAspect != dstAspect)
         *width = (unsigned)(settings->uints.video_fullscreen_y * dstAspect);
      else
         *width = settings->uints.video_fullscreen_x;
      *height   = settings->uints.video_fullscreen_y;
   }
   else
   {
      *width  = vc->fb_width;
      *height = vc->fb_height;
   }
}

static void dispmanx_vsync_callback(DISPMANX_UPDATE_HANDLE_T u, void *data)
{
   vc_ctx_data_t *vc = (vc_ctx_data_t*)data;

   if (!vc)
      return;

   slock_lock(vc->vsync_condition_mutex);
   scond_signal(vc->vsync_condition);
   slock_unlock(vc->vsync_condition_mutex);
}

static void gfx_ctx_vc_destroy(void *data);

static void *gfx_ctx_vc_init(video_frame_info_t *video_info, void *video_driver)
{
   VC_DISPMANX_ALPHA_T alpha;
   EGLint n, major, minor;

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
      EGL_DEPTH_SIZE, 16,
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

   /* If we set this env variable, Broadcom's EGL implementation will block
    * on vsync with a double buffer when we call eglSwapBuffers. Less input lag!
    * Has to be done before any EGL call.
    * NOTE this is commented out because it should be the right way to do it, but
    * currently it doesn't work, so we are using an vsync callback based solution.*/
   /* if (video_info->max_swapchain_images <= 2)
      setenv("V3D_DOUBLE_BUFFER", "1", 1);
   else
      setenv("V3D_DOUBLE_BUFFER", "0", 1); */

   bcm_host_init();

#ifdef HAVE_EGL
   if (!egl_init_context(&vc->egl, EGL_NONE, EGL_DEFAULT_DISPLAY,
            &major, &minor, &n, attribute_list, NULL))
   {
      egl_report_error();
      goto error;
   }

   if (!egl_create_context(&vc->egl, (vc_api == GFX_CTX_OPENGL_ES_API)
            ? context_attributes : NULL))
   {
      egl_report_error();
      goto error;
   }
#endif

   /* Create an EGL window surface. */
   if (graphics_get_display_size(0 /* LCD */, &vc->fb_width, &vc->fb_height) < 0)
      goto error;

   dst_rect.x      = 0;
   dst_rect.y      = 0;
   dst_rect.width  = vc->fb_width;
   dst_rect.height = vc->fb_height;

   src_rect.x      = 0;
   src_rect.y      = 0;

   /* Use dispmanx upscaling if fullscreen_x
    * and fullscreen_y are set. */
   if ((settings->uints.video_fullscreen_x != 0) &&
       (settings->uints.video_fullscreen_y != 0))
   {
      /* Keep input and output aspect ratio equal.
       * There are other aspect ratio settings which can be used to stretch video output. */

      /* Calculate source and destination aspect ratios. */
      float srcAspect        = (float)settings->uints.video_fullscreen_x / (float)settings->uints.video_fullscreen_y;
      float dstAspect        = (float)vc->fb_width / (float)vc->fb_height;
      /* If source and destination aspect ratios are not equal correct source width. */
      if (srcAspect != dstAspect)
         src_rect.width      = (unsigned)(settings->uints.video_fullscreen_y * dstAspect) << 16;
      else
         src_rect.width      = settings->uints.video_fullscreen_x << 16;
      src_rect.height        = settings->uints.video_fullscreen_y << 16;
   }
   else
   {
      src_rect.width         = vc->fb_width << 16;
      src_rect.height        = vc->fb_height << 16;
   }

   dispman_display           = vc_dispmanx_display_open(0 /* LCD */);
   vc->dispman_display       = dispman_display;

   vc_dispmanx_display_get_info(dispman_display, &dispman_modeinfo);

   dispman_update            = vc_dispmanx_update_start(0);

   alpha.flags               = DISPMANX_FLAGS_ALPHA_FIXED_ALL_PIXELS;
   alpha.opacity             = 255;
   alpha.mask                = 0;

   dispman_element           = vc_dispmanx_element_add(dispman_update, dispman_display,
         0 /*layer*/, &dst_rect, 0 /*src*/,
         &src_rect, DISPMANX_PROTECTION_NONE, &alpha, 0 /*clamp*/, DISPMANX_NO_ROTATE);

   vc->native_window.element = dispman_element;

   /* Use dispmanx upscaling if fullscreen_x and fullscreen_y are set. */

   if (settings->uints.video_fullscreen_x != 0 &&
       settings->uints.video_fullscreen_y != 0)
   {
      /* Keep input and output aspect ratio equal.
       * There are other aspect ratio settings which
       * can be used to stretch video output. */

      /* Calculate source and destination aspect ratios. */
      float srcAspect = (float)settings->uints.video_fullscreen_x
         / (float)settings->uints.video_fullscreen_y;
      float dstAspect = (float)vc->fb_width / (float)vc->fb_height;

      /* If source and destination aspect ratios are not equal correct source width. */
      if (srcAspect != dstAspect)
         vc->native_window.width = (unsigned)(settings->uints.video_fullscreen_y * dstAspect);
      else
         vc->native_window.width = settings->uints.video_fullscreen_x;
      vc->native_window.height   = settings->uints.video_fullscreen_y;
   }
   else
   {
      vc->native_window.width = vc->fb_width;
      vc->native_window.height = vc->fb_height;
   }
   vc_dispmanx_update_submit_sync(dispman_update);

#ifdef HAVE_EGL
   if (!egl_create_surface(&vc->egl, &vc->native_window))
      goto error;
#endif

   /* For vsync after eglSwapBuffers when max_swapchain < 3 */
   vc->vsync_condition       = scond_new();
   vc->vsync_condition_mutex = slock_new();
   vc->vsync_callback_set    = false;

   if (video_info->max_swapchain_images <= 2)
   {
      /* Start sending vsync callbacks so we can wait for vsync after eglSwapBuffers */
      vc_dispmanx_vsync_callback(vc->dispman_display,
            dispmanx_vsync_callback, (void*)vc);
      vc->vsync_callback_set = true;
   }

   return vc;

error:
   gfx_ctx_vc_destroy(video_driver);
   return NULL;
}

static void gfx_ctx_vc_set_swap_interval(void *data, int swap_interval)
{
#ifdef HAVE_EGL
   vc_ctx_data_t *vc = (vc_ctx_data_t*)data;
   if (vc)
      egl_set_swap_interval(&vc->egl, swap_interval);
#endif
}

static bool gfx_ctx_vc_set_video_mode(void *data,
      video_frame_info_t *video_info,
      unsigned width, unsigned height,
      bool fullscreen)
{
#ifdef HAVE_EGL
   vc_ctx_data_t *vc = (vc_ctx_data_t*)data;
   if (!vc || g_egl_inited)
      return false;

   frontend_driver_install_signal_handler();

   gfx_ctx_vc_set_swap_interval(&vc->egl, vc->egl.interval);

   g_egl_inited = true;
#endif

   return true;
}

static enum gfx_ctx_api gfx_ctx_vc_get_api(void *data)
{
   return vc_api;
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
#ifdef HAVE_EGL
      case GFX_CTX_OPENGL_API:
         return egl_bind_api(EGL_OPENGL_API);
      case GFX_CTX_OPENGL_ES_API:
         return egl_bind_api(EGL_OPENGL_ES_API);
      case GFX_CTX_OPENVG_API:
         return egl_bind_api(EGL_OPENVG_API);
#endif
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
            egl_bind_api(EGL_OPENVG_API);
            eglMakeCurrent(vc->egl.dpy,
                  vc->pbuff_surf, vc->pbuff_surf, vc->eglimage_ctx);
            peglDestroyImageKHR(vc->egl.dpy, vc->eglBuffer[i]);
         }

         if (vc->vgimage[i])
         {
            egl_bind_api(EGL_OPENVG_API);
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
         egl_bind_api(EGL_OPENVG_API);
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
         egl_bind_api(EGL_OPENVG_API);
         eglDestroySurface(vc->egl.dpy, vc->pbuff_surf);
      }

      egl_bind_api(EGL_OPENVG_API);
      eglMakeCurrent(vc->egl.dpy,
            EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      gfx_ctx_vc_bind_api(data, vc_api, 0, 0);
      eglMakeCurrent(vc->egl.dpy,
            EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
      egl_terminate(vc->egl.dpy);
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
      vc->eglBuffer[i] = NULL;
      vc->vgimage[i]   = 0;
   }

   /* Stop generating vsync callbacks if we are doing so.
    * Don't destroy the context while cbs are being generated! */
   if (vc->vsync_callback_set)
      vc_dispmanx_vsync_callback(vc->dispman_display, NULL, NULL);

   /* Destroy mutexes and conditions. */
   slock_free(vc->vsync_condition_mutex);
   scond_free(vc->vsync_condition);
}

static void gfx_ctx_vc_input_driver(void *data,
      const char *name,
      input_driver_t **input, void **input_data)
{
   *input      = NULL;
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
   EGLBoolean result;
   vc_ctx_data_t *vc = (vc_ctx_data_t*)data;
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

   if (  !peglCreateImageKHR  ||
         !peglDestroyImageKHR ||
         !gfx_ctx_vc_egl_query_extension(vc, "KHR_image")
      )
      return false;

   vc->res        = video->input_scale * RARCH_SCALE_BASE;

   egl_bind_api(EGL_OPENVG_API);
   vc->pbuff_surf = eglCreatePbufferSurface(
         vc->egl.dpy, vc->egl.config, pbufsurface_list);

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

   if (!vc || index >= MAX_EGLIMAGE_TEXTURES)
      goto error;

   egl_bind_api(EGL_OPENVG_API);
   eglMakeCurrent(vc->egl.dpy, vc->pbuff_surf,
         vc->pbuff_surf, vc->eglimage_ctx);

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

static void gfx_ctx_vc_swap_buffers(void *data, void *data2)
{
#ifdef HAVE_EGL
   vc_ctx_data_t              *vc = (vc_ctx_data_t*)data;
   video_frame_info_t *video_info = (video_frame_info_t*)data2;

   if (!vc)
      return;

   egl_swap_buffers(&vc->egl);

   /* Wait for vsync immediately if we don't
    * want egl_swap_buffers to triple-buffer */
   if (video_info->max_swapchain_images <= 2)
   {
      /* We DON'T wait to wait without callback function ready! */
      if (!vc->vsync_callback_set)
      {
         vc_dispmanx_vsync_callback(vc->dispman_display,
               dispmanx_vsync_callback, (void*)vc);
         vc->vsync_callback_set = true;
      }
      slock_lock(vc->vsync_condition_mutex);
      scond_wait(vc->vsync_condition, vc->vsync_condition_mutex);
      slock_unlock(vc->vsync_condition_mutex);
   }
   /* Stop generating vsync callbacks from now on */
   else if (vc->vsync_callback_set)
      vc_dispmanx_vsync_callback(vc->dispman_display, NULL, NULL);
#endif
}

static void gfx_ctx_vc_bind_hw_render(void *data, bool enable)
{
   vc_ctx_data_t *vc = (vc_ctx_data_t*)data;

   if (!vc)
      return;

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
   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES);
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
   return flags;
}

static void gfx_ctx_vc_set_flags(void *data, uint32_t flags)
{
   (void)data;
}

const gfx_ctx_driver_t gfx_ctx_videocore = {
   gfx_ctx_vc_init,
   gfx_ctx_vc_destroy,
   gfx_ctx_vc_get_api,
   gfx_ctx_vc_bind_api,
   gfx_ctx_vc_set_swap_interval,
   gfx_ctx_vc_set_video_mode,
   gfx_ctx_vc_get_video_size,
   NULL, /* get_refresh_rate */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   gfx_ctx_vc_translate_aspect,
   NULL, /* update_title */
   gfx_ctx_vc_check_window,
   NULL, /* set_resize */
   gfx_ctx_vc_has_focus,
   gfx_ctx_vc_suppress_screensaver,
   false, /* has_windowed */
   gfx_ctx_vc_swap_buffers,
   gfx_ctx_vc_input_driver,
   gfx_ctx_vc_get_proc_address,
   gfx_ctx_vc_image_buffer_init,
   gfx_ctx_vc_image_buffer_write,
   NULL,
   "videocore",
   gfx_ctx_vc_get_flags,
   gfx_ctx_vc_set_flags,
   gfx_ctx_vc_bind_hw_render,
   NULL,
   NULL
};

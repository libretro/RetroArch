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

/* KMS/DRM context, running without any window manager.
 * Based on kmscube example by Rob Clark.
 */

#include <stdint.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <math.h>

#include <sched.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <poll.h>

#include <libdrm/drm.h>
#include <gbm.h>

#include <lists/dir_list.h>
#include <string/stdstring.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../../configuration.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"
#include "../common/drm_common.h"

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

#include "../common/gl_common.h"

#ifdef HAVE_OPENGLES

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

#endif

#ifndef EGL_PLATFORM_GBM_KHR
#define EGL_PLATFORM_GBM_KHR 0x31D7
#endif

typedef struct gfx_ctx_drm_data
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
#endif
   int fd;
   int interval;
   unsigned fb_width;
   unsigned fb_height;

   bool core_hw_context_enable;
   bool waiting_for_flip;
   struct gbm_bo *bo;
   struct gbm_bo *next_bo;
   struct gbm_surface *gbm_surface;
   struct gbm_device  *gbm_dev;
} gfx_ctx_drm_data_t;

struct drm_fb
{
   struct gbm_bo *bo;
   uint32_t fb_id;
};

/*
 * https://github.com/libretro/RetroArch/pull/11590
 * https://www.raspberrypi.org/documentation/configuration/config-txt/video.md
 */
typedef struct hdmi_timings
{
   int h_active_pixels; /* horizontal pixels (width) */
   int h_sync_polarity; /* invert hsync polarity */
   int h_front_porch;   /* horizontal forward padding from DE acitve edge */
   int h_sync_pulse;    /* hsync pulse width in pixel clocks */
   int h_back_porch;    /* vertical back padding from DE active edge */
   int v_active_lines;  /* vertical pixels height (lines) */
   int v_sync_polarity; /* invert vsync polarity */
   int v_front_porch;   /* vertical forward padding from DE active edge */
   int v_sync_pulse;    /* vsync pulse width in pixel clocks */
   int v_back_porch;    /* vertical back padding from DE active edge */
   int v_sync_offset_a; /* leave at zero */
   int v_sync_offset_b; /* leave at zero */
   int pixel_rep;       /* leave at zero */
   int frame_rate;      /* screen refresh rate in Hz */
   int interlaced;      /* leave at zero */
   int pixel_freq;      /* clock frequency (width*height*framerate) */
   int aspect_ratio;
} hdmi_timings_t;

static enum gfx_ctx_api drm_api           = GFX_CTX_NONE;
static drmModeModeInfo gfx_ctx_crt_switch_mode;

/* Load custom hdmi timings from config */
bool gfx_ctx_drm_load_mode(drmModeModeInfoPtr modeInfo)
{
   int ret;
   hdmi_timings_t timings;
   settings_t *settings = config_get_ptr();
   char *crt_switch_timings = settings->arrays.crt_switch_timings;

   if(modeInfo != NULL && !string_is_empty(crt_switch_timings)) {
      ret = sscanf(crt_switch_timings, "%d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d %d",
                   &timings.h_active_pixels, &timings.h_sync_polarity, &timings.h_front_porch,
                   &timings.h_sync_pulse, &timings.h_back_porch,
                   &timings.v_active_lines, &timings.v_sync_polarity, &timings.v_front_porch,
                   &timings.v_sync_pulse, &timings.v_back_porch,
                   &timings.v_sync_offset_a, &timings.v_sync_offset_b, &timings.pixel_rep, &timings.frame_rate,
                   &timings.interlaced, &timings.pixel_freq, &timings.aspect_ratio);
      if (ret != 17) {
         RARCH_ERR("[DRM]: malformed mode requested: %s\n", crt_switch_timings);
         return false;
      }

      memset(modeInfo, 0, sizeof(drmModeModeInfo));
      modeInfo->clock = timings.pixel_freq / 1000;
      modeInfo->hdisplay = timings.h_active_pixels;
      modeInfo->hsync_start = modeInfo->hdisplay + timings.h_front_porch;
      modeInfo->hsync_end = modeInfo->hsync_start + timings.h_sync_pulse;
      modeInfo->htotal = modeInfo->hsync_end + timings.h_back_porch;
      modeInfo->hskew = 0;
      modeInfo->vdisplay = timings.v_active_lines;
      modeInfo->vsync_start = modeInfo->vdisplay + (timings.v_front_porch * (timings.interlaced ? 2 : 1));
      modeInfo->vsync_end = modeInfo->vsync_start + (timings.v_sync_pulse * (timings.interlaced ? 2 : 1));
      modeInfo->vtotal = modeInfo->vsync_end + (timings.v_back_porch * (timings.interlaced ? 2 : 1));
      modeInfo->vscan = 0; /* TODO: ?? */
      modeInfo->vrefresh = timings.frame_rate;
      modeInfo->flags = timings.interlaced ? DRM_MODE_FLAG_INTERLACE : 0;
      modeInfo->flags |= timings.v_sync_polarity ? DRM_MODE_FLAG_NVSYNC : DRM_MODE_FLAG_PVSYNC;
      modeInfo->flags |= timings.h_sync_polarity ? DRM_MODE_FLAG_NHSYNC : DRM_MODE_FLAG_PHSYNC;
      modeInfo->type = 0;
      snprintf(modeInfo->name, DRM_DISPLAY_MODE_LEN, "CRT_%ux%u_%u",
               modeInfo->hdisplay, modeInfo->vdisplay, modeInfo->vrefresh);

      return true;
   }

   return false;
}

static void drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
   struct drm_fb *fb = (struct drm_fb*)data;

   if (fb && fb->fb_id)
      drmModeRmFB(g_drm_fd, fb->fb_id);

   free(fb);
}

static struct drm_fb *drm_fb_get_from_bo(struct gbm_bo *bo)
{
   int ret;
   unsigned width, height, stride, handle;
   struct drm_fb *fb = (struct drm_fb*)calloc(1, sizeof(*fb));

   fb->bo = bo;

   width  = gbm_bo_get_width(bo);
   height = gbm_bo_get_height(bo);
   stride = gbm_bo_get_stride(bo);
   handle = gbm_bo_get_handle(bo).u32;

   RARCH_LOG("[KMS]: New FB: %ux%u (stride: %u).\n",
         width, height, stride);

   ret = drmModeAddFB(g_drm_fd, width, height, 24, 32,
         stride, handle, &fb->fb_id);
   if (ret < 0)
      goto error;

   gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);
   return fb;

error:
   RARCH_ERR("[KMS]: Failed to create FB: %s\n", strerror(errno));
   free(fb);
   return NULL;
}

static void gfx_ctx_drm_swap_interval(void *data, int interval)
{
   gfx_ctx_drm_data_t *drm = (gfx_ctx_drm_data_t*)data;
   drm->interval           = interval;

   if (interval > 1)
      RARCH_WARN("[KMS]: Swap intervals > 1 currently not supported. Will use swap interval of 1.\n");
}

static void gfx_ctx_drm_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height)
{
   *resize = false;
   *quit   = (bool)frontend_driver_get_signal_handler_state();
}

static void drm_flip_handler(int fd, unsigned frame,
      unsigned sec, unsigned usec, void *data)
{
#if 0
   static unsigned first_page_flip;
   static unsigned last_page_flip;

   if (!first_page_flip)
      first_page_flip = frame;

   if (last_page_flip)
   {
      unsigned missed = frame - last_page_flip - 1;
      if (missed)
         RARCH_LOG("[KMS]: Missed %u VBlank(s) (Frame: %u, DRM frame: %u).\n",
               missed, frame - first_page_flip, frame);
   }

   last_page_flip = frame;
#endif

   *(bool*)data = false;
}

static bool gfx_ctx_drm_wait_flip(gfx_ctx_drm_data_t *drm, bool block)
{
   int timeout = 0;

   if (!drm->waiting_for_flip)
      return false;

   if (block)
      timeout = -1;

   while (drm->waiting_for_flip)
   {
      if (!drm_wait_flip(timeout))
         break;
   }

   if (drm->waiting_for_flip)
      return true;

   /* Page flip has taken place. */

   /* This buffer is not on-screen anymore. Release it to GBM. */
   gbm_surface_release_buffer(drm->gbm_surface, drm->bo);
   /* This buffer is being shown now. */
   drm->bo = drm->next_bo;

   return false;
}

static bool gfx_ctx_drm_queue_flip(gfx_ctx_drm_data_t *drm)
{
   struct drm_fb *fb = NULL;

   drm->next_bo      = gbm_surface_lock_front_buffer(drm->gbm_surface);
   fb                = (struct drm_fb*)gbm_bo_get_user_data(drm->next_bo);

   if (!fb)
      fb             = (struct drm_fb*)drm_fb_get_from_bo(drm->next_bo);

   if (drmModePageFlip(g_drm_fd, g_crtc_id, fb->fb_id,
         DRM_MODE_PAGE_FLIP_EVENT, &drm->waiting_for_flip) == 0)
      return true;

   /* Failed to queue page flip. */
   return false;
}

static void gfx_ctx_drm_swap_buffers(void *data)
{
   gfx_ctx_drm_data_t        *drm = (gfx_ctx_drm_data_t*)data;
   settings_t *settings           = config_get_ptr();
   unsigned max_swapchain_images  = settings->uints.video_max_swapchain_images;

#ifdef HAVE_EGL
   egl_swap_buffers(&drm->egl);
#endif

   /* I guess we have to wait for flip to have taken
    * place before another flip can be queued up.
    *
    * If true, we are still waiting for a flip
    * (nonblocking mode, so just drop the frame). */
   if (gfx_ctx_drm_wait_flip(drm, drm->interval))
      return;

   drm->waiting_for_flip = gfx_ctx_drm_queue_flip(drm);

   /* Triple-buffered page flips */
   if (max_swapchain_images >= 3 &&
         gbm_surface_has_free_buffers(drm->gbm_surface))
      return;

   gfx_ctx_drm_wait_flip(drm, true);
}

static void gfx_ctx_drm_get_video_size(void *data,
      unsigned *width, unsigned *height)
{
   gfx_ctx_drm_data_t *drm = (gfx_ctx_drm_data_t*)data;

   if (!drm)
      return;

   *width  = drm->fb_width;
   *height = drm->fb_height;
}

static void free_drm_resources(gfx_ctx_drm_data_t *drm)
{
   if (!drm)
      return;

   /* Restore original CRTC. */
   drm_restore_crtc();

   if (drm->gbm_surface)
      gbm_surface_destroy(drm->gbm_surface);

   if (drm->gbm_dev)
      gbm_device_destroy(drm->gbm_dev);

   drm_free();

   if (drm->fd >= 0)
   {
      if (g_drm_fd >= 0)
      {
         drmDropMaster(g_drm_fd);
         close(drm->fd);
      }
   }

   drm->gbm_surface   = NULL;
   drm->gbm_dev       = NULL;
   g_drm_fd           = -1;
}

static void gfx_ctx_drm_destroy_resources(gfx_ctx_drm_data_t *drm)
{
   if (!drm)
      return;

   /* Make sure we acknowledge all page-flips. */
   gfx_ctx_drm_wait_flip(drm, true);

#ifdef HAVE_EGL
   egl_destroy(&drm->egl);
#endif

   free_drm_resources(drm);

   g_drm_mode          = NULL;
   g_crtc_id           = 0;
   g_connector_id      = 0;

   drm->fb_width       = 0;
   drm->fb_height      = 0;

   drm->bo             = NULL;
   drm->next_bo        = NULL;
}

static void *gfx_ctx_drm_init(void *video_driver)
{
   int fd, i;
   unsigned monitor_index;
   unsigned gpu_index                   = 0;
   const char *gpu                      = NULL;
   struct string_list *gpu_descriptors  = NULL;
   settings_t *settings                 = config_get_ptr();
   gfx_ctx_drm_data_t *drm              = (gfx_ctx_drm_data_t*)
      calloc(1, sizeof(gfx_ctx_drm_data_t));
   unsigned video_monitor_index         = settings->uints.video_monitor_index;

   if (!drm)
      return NULL;
   drm->fd = -1;

   gpu_descriptors = dir_list_new("/dev/dri", NULL, false, true, false, false);

nextgpu:
   free_drm_resources(drm);

   if (!gpu_descriptors || gpu_index == gpu_descriptors->size)
   {
      RARCH_ERR("[KMS]: Couldn't find a suitable DRM device.\n");
      goto error;
   }
   gpu = gpu_descriptors->elems[gpu_index++].data;

   drm->fd    = open(gpu, O_RDWR);
   if (drm->fd < 0)
   {
      RARCH_WARN("[KMS]: Couldn't open DRM device.\n");
      goto nextgpu;
   }

   fd = drm->fd;

   if (!drm_get_resources(fd))
      goto nextgpu;

   if (!drm_get_connector(fd, video_monitor_index))
      goto nextgpu;

   if (!drm_get_encoder(fd))
      goto nextgpu;

   drm_setup(fd);

   /* Choose the optimal video mode for get_video_size():
     - custom timings from configuration
     - else the current video mode from the CRTC
     - otherwise pick first connector mode */
   if(gfx_ctx_drm_load_mode(&gfx_ctx_crt_switch_mode))
   {
      drm->fb_width  = gfx_ctx_crt_switch_mode.hdisplay;
      drm->fb_height = gfx_ctx_crt_switch_mode.vdisplay;
   }
   else if (g_orig_crtc->mode_valid)
   {
      drm->fb_width  = g_orig_crtc->mode.hdisplay;
      drm->fb_height = g_orig_crtc->mode.vdisplay;
   }
   else
   {
      drm->fb_width  = g_drm_connector->modes[0].hdisplay;
      drm->fb_height = g_drm_connector->modes[0].vdisplay;
   }

   drmSetMaster(g_drm_fd);

   drm->gbm_dev      = gbm_create_device(fd);

   if (!drm->gbm_dev)
   {
      RARCH_WARN("[KMS]: Couldn't create GBM device.\n");
      goto nextgpu;
   }

   dir_list_free(gpu_descriptors);

   /* Setup the flip handler. */
   g_drm_fds.fd                   = fd;
   g_drm_fds.events               = POLLIN;
   g_drm_evctx.version            = DRM_EVENT_CONTEXT_VERSION;
   g_drm_evctx.page_flip_handler  = drm_flip_handler;

   g_drm_fd                       = fd;

   return drm;

error:
   dir_list_free(gpu_descriptors);

   gfx_ctx_drm_destroy_resources(drm);

   if (drm)
      free(drm);

   return NULL;
}

static EGLint *gfx_ctx_drm_egl_fill_attribs(
      gfx_ctx_drm_data_t *drm, EGLint *attr)
{
   switch (drm_api)
   {
#ifdef EGL_KHR_create_context
      case GFX_CTX_OPENGL_API:
      {
         bool debug       = false;
#ifdef HAVE_OPENGL
         unsigned version = drm->egl.major * 1000 + drm->egl.minor;
         bool core        = version >= 3001;
#ifdef GL_DEBUG
         debug            = true;
#else
         struct retro_hw_render_callback *hwr = video_driver_get_hw_context();
         debug           = hwr->debug_context;
#endif

         if (core)
         {
            *attr++ = EGL_CONTEXT_MAJOR_VERSION_KHR;
            *attr++ = drm->egl.major;
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = drm->egl.minor;

            /* Technically, we don't have core/compat until 3.2.
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
         break;
#endif
      }
#endif

      case GFX_CTX_OPENGL_ES_API:
#ifdef HAVE_OPENGLES
         *attr++ = EGL_CONTEXT_CLIENT_VERSION;
         *attr++ = drm->egl.major
            ? (EGLint)drm->egl.major : 2;
#ifdef EGL_KHR_create_context
         if (drm->egl.minor > 0)
         {
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = drm->egl.minor;
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

#ifdef HAVE_EGL
static bool gbm_choose_xrgb8888_cb(void *display_data, EGLDisplay dpy, EGLConfig config)
{
   EGLint r, g, b, id;
   (void)display_data;

   /* Makes sure we have 8 bit color. */
   if (!egl_get_config_attrib(dpy, config, EGL_RED_SIZE, &r))
      return false;
   if (!egl_get_config_attrib(dpy, config, EGL_GREEN_SIZE, &g))
      return false;
   if (!egl_get_config_attrib(dpy, config, EGL_BLUE_SIZE, &b))
      return false;

   if (r != 8 || g != 8 || b != 8)
      return false;

   if (!egl_get_config_attrib(dpy, config, EGL_NATIVE_VISUAL_ID, &id))
      return false;

   return id == GBM_FORMAT_XRGB8888;
}

#define DRM_EGL_ATTRIBS_BASE \
   EGL_SURFACE_TYPE,    EGL_WINDOW_BIT, \
   EGL_RED_SIZE,        1, \
   EGL_GREEN_SIZE,      1, \
   EGL_BLUE_SIZE,       1, \
   EGL_ALPHA_SIZE,      0, \
   EGL_DEPTH_SIZE,      0

static bool gfx_ctx_drm_egl_set_video_mode(gfx_ctx_drm_data_t *drm)
{
   const EGLint *attrib_ptr    = NULL;
   static const EGLint egl_attribs_gl[] = {
      DRM_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
      EGL_NONE,
   };

   static const EGLint egl_attribs_gles[] = {
      DRM_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE,
   };

#ifdef EGL_KHR_create_context
   static const EGLint egl_attribs_gles3[] = {
      DRM_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
      EGL_NONE,
   };
#endif

#ifdef HAVE_VG
   static const EGLint egl_attribs_vg[] = {
      DRM_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
      EGL_NONE,
   };
#endif
   EGLint major;
   EGLint minor;
   EGLint n;
   EGLint egl_attribs[16];
   EGLint *egl_attribs_ptr     = NULL;
   EGLint *attr                = NULL;

   switch (drm_api)
   {
      case GFX_CTX_OPENGL_API:
#ifdef HAVE_OPENGL
         attrib_ptr = egl_attribs_gl;
         break;
      case GFX_CTX_OPENGL_ES_API:
#ifdef EGL_KHR_create_context
         if (drm->egl.major >= 3)
            attrib_ptr = egl_attribs_gles3;
         else
#endif
         attrib_ptr = egl_attribs_gles;
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

#ifdef HAVE_EGL
   if (!egl_init_context(&drm->egl, EGL_PLATFORM_GBM_KHR,
            (EGLNativeDisplayType)drm->gbm_dev, &major,
            &minor, &n, attrib_ptr, gbm_choose_xrgb8888_cb))
      goto error;

   attr            = gfx_ctx_drm_egl_fill_attribs(drm, egl_attribs);
   egl_attribs_ptr = &egl_attribs[0];

   if (!egl_create_context(&drm->egl, (attr != egl_attribs_ptr)
            ? egl_attribs_ptr : NULL))
      goto error;

   if (!egl_create_surface(&drm->egl, (EGLNativeWindowType)drm->gbm_surface))
      return false;

   switch (drm_api)
   {
      case GFX_CTX_OPENGL_API:
      case GFX_CTX_OPENGL_ES_API:
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
         gl_clear();
#endif
         break;
      case GFX_CTX_NONE:
      default:
         break;
   }
#endif

   egl_swap_buffers(drm);

   return true;

error:
   egl_report_error();
   return false;
}
#endif

static bool gfx_ctx_drm_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   float refresh_mod;
   int i, ret                  = 0;
   struct drm_fb *fb           = NULL;
   gfx_ctx_drm_data_t *drm     = (gfx_ctx_drm_data_t*)data;
   settings_t *settings        = config_get_ptr();
   unsigned black_frame_insertion  = settings->uints.video_black_frame_insertion;
   float video_refresh_rate    = settings->floats.video_refresh_rate;

   if (!drm)
      return false;

   frontend_driver_install_signal_handler();

   /* If we use black frame insertion,
    * we fake a 60 Hz monitor for 120 Hz one,
    * etc, so try to match that. */
   refresh_mod = 1.0f / (black_frame_insertion + 1.0f);

   /* Find desired video mode, and use that.
    * If not fullscreen, we get desired windowed size,
    * which is not appropriate. */
   if ((width == 0 && height == 0) || !fullscreen)
      g_drm_mode = &g_drm_connector->modes[0];
   else
   {
      /* check if custom hdmi timings were asked */
      if(gfx_ctx_crt_switch_mode.vdisplay > 0)
      {
         RARCH_LOG("[DRM]: custom mode requested: %s\n", gfx_ctx_crt_switch_mode.name);
         g_drm_mode = &gfx_ctx_crt_switch_mode;
      }
      else
      {
         /* Try to match refresh_rate as closely as possible.
          *
          * Lower resolutions tend to have multiple supported
          * refresh rates as well.
          */
         float minimum_fps_diff = 0.0f;
         float mode_vrefresh    = 0.0f;
         drmModeModeInfo *mode;

         /* Find best match. */
         for (i = 0; i < g_drm_connector->count_modes; i++) {
            float diff;
            mode = &g_drm_connector->modes[i];

            if (width != mode->hdisplay ||
               height != mode->vdisplay)
               continue;

            mode_vrefresh = drm_calc_refresh_rate(mode);

            diff = fabsf(refresh_mod * mode_vrefresh - video_refresh_rate);

            if (!g_drm_mode || diff < minimum_fps_diff) {
               g_drm_mode = mode;
               minimum_fps_diff = diff;
            }
         }
      }
   }

   if (!g_drm_mode)
   {
      RARCH_ERR("[KMS/EGL]: Did not find suitable video mode for %u x %u.\n",
            width, height);
      goto error;
   }

   drm->fb_width    = g_drm_mode->hdisplay;
   drm->fb_height   = g_drm_mode->vdisplay;

   /* Create GBM surface. */
   drm->gbm_surface = gbm_surface_create(
         drm->gbm_dev,
         drm->fb_width,
         drm->fb_height,
         GBM_FORMAT_XRGB8888,
         GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

   if (!drm->gbm_surface)
   {
      RARCH_ERR("[KMS/EGL]: Couldn't create GBM surface.\n");
      goto error;
   }

#ifdef HAVE_EGL
   if (!gfx_ctx_drm_egl_set_video_mode(drm))
      goto error;
#endif

   drm->bo   = gbm_surface_lock_front_buffer(drm->gbm_surface);

   fb        = (struct drm_fb*)gbm_bo_get_user_data(drm->bo);

   if (!fb)
      fb   = drm_fb_get_from_bo(drm->bo);

   ret     = drmModeSetCrtc(g_drm_fd,
         g_crtc_id, fb->fb_id, 0, 0, &g_connector_id, 1, g_drm_mode);
   if (ret < 0)
      goto error;

   return true;

error:
   gfx_ctx_drm_destroy_resources(drm);

   if (drm)
      free(drm);

   return false;
}

static void gfx_ctx_drm_destroy(void *data)
{
   gfx_ctx_drm_data_t *drm = (gfx_ctx_drm_data_t*)data;

   if (!drm)
      return;

   gfx_ctx_drm_destroy_resources(drm);
   free(drm);
}

static void gfx_ctx_drm_input_driver(void *data,
      const char *joypad_name,
      input_driver_t **input, void **input_data)
{
#ifdef HAVE_X11
   settings_t *settings = config_get_ptr();

   /* We cannot use the X11 input driver for DRM/KMS */
   if (string_is_equal(settings->arrays.input_driver, "x"))
   {
#ifdef HAVE_UDEV
      {
         /* Try to set it to udev instead */
         void *udev = input_driver_init_wrap(&input_udev, joypad_name);
         if (udev)
         {
            *input       = &input_udev;
            *input_data  = udev;
            return;
         }
      }
#endif
#if defined(__linux__) && !defined(ANDROID)
      {
         /* Try to set it to linuxraw instead */
         void *linuxraw = input_driver_init_wrap(&input_linuxraw, joypad_name);
         if (linuxraw)
         {
            *input       = &input_linuxraw;
            *input_data  = linuxraw;
            return;
         }
      }
#endif
   }
#endif

   *input      = NULL;
   *input_data = NULL;
}

static bool gfx_ctx_drm_has_focus(void *data) { return true; }

static bool gfx_ctx_drm_suppress_screensaver(void *data, bool enable) { return false; }

static enum gfx_ctx_api gfx_ctx_drm_get_api(void *data) { return drm_api; }

static bool gfx_ctx_drm_bind_api(void *video_driver,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   drm_api     = api;
#ifdef HAVE_EGL
   g_egl_major = major;
   g_egl_minor = minor;
#endif

   switch (api)
   {
      case GFX_CTX_OPENGL_API:
#if defined(HAVE_EGL) && defined(HAVE_OPENGL)

#ifndef EGL_KHR_create_context
         if ((major * 1000 + minor) >= 3001)
            return false;
#endif
         return egl_bind_api(EGL_OPENGL_API);
#else
         break;
#endif
      case GFX_CTX_OPENGL_ES_API:
#if defined(HAVE_EGL) && defined(HAVE_OPENGLES)

#ifndef EGL_KHR_create_context
         if (major >= 3)
            return false;
#endif
         return egl_bind_api(EGL_OPENGL_ES_API);
#else
         break;
#endif
      case GFX_CTX_OPENVG_API:
#if defined(HAVE_EGL) && defined(HAVE_VG)
         return egl_bind_api(EGL_OPENVG_API);
#endif
      case GFX_CTX_NONE:
      default:
         break;
   }

   return false;
}

static void gfx_ctx_drm_bind_hw_render(void *data, bool enable)
{
   gfx_ctx_drm_data_t *drm     = (gfx_ctx_drm_data_t*)data;

#ifdef HAVE_EGL
   egl_bind_hw_render(&drm->egl, enable);
#endif
}

static uint32_t gfx_ctx_drm_get_flags(void *data)
{
   uint32_t             flags = 0;
   gfx_ctx_drm_data_t    *drm = (gfx_ctx_drm_data_t*)data;

   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES);

   if (drm->core_hw_context_enable)
      BIT32_SET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);

   if (string_is_equal(video_driver_get_ident(), "glcore"))
   {
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
   }
   else
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);

   return flags;
}

static void gfx_ctx_drm_set_flags(void *data, uint32_t flags)
{
   gfx_ctx_drm_data_t *drm     = (gfx_ctx_drm_data_t*)data;
   if (BIT32_GET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT))
      drm->core_hw_context_enable = true;
}

const gfx_ctx_driver_t gfx_ctx_drm = {
   gfx_ctx_drm_init,
   gfx_ctx_drm_destroy,
   gfx_ctx_drm_get_api,
   gfx_ctx_drm_bind_api,
   gfx_ctx_drm_swap_interval,
   gfx_ctx_drm_set_video_mode,
   gfx_ctx_drm_get_video_size,
   drm_get_refresh_rate,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   NULL, /* update_title */
   gfx_ctx_drm_check_window,
   NULL, /* set_resize */
   gfx_ctx_drm_has_focus,
   gfx_ctx_drm_suppress_screensaver,
   false, /* has_windowed */
   gfx_ctx_drm_swap_buffers,
   gfx_ctx_drm_input_driver,
#ifdef HAVE_EGL
   egl_get_proc_address,
#else
   NULL,
#endif
   NULL,
   NULL,
   NULL,
   "kms",
   gfx_ctx_drm_get_flags,
   gfx_ctx_drm_set_flags,
   gfx_ctx_drm_bind_hw_render,
   NULL,
   NULL
};

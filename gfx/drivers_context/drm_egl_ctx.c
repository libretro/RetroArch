/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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
#include <signal.h>
#include <unistd.h>
#include <math.h>

#include <sched.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>

#include <libdrm/drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>

#include <file/dir_list.h>
#include <retro_file.h>

#include "../../driver.h"
#include "../../runloop.h"
#include "../common/egl_common.h"
#include "../common/gl_common.h"
#include "../video_monitor.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

typedef struct gfx_ctx_drm_egl_data
{
   RFILE *g_drm;
   int g_drm_fd;
   uint32_t g_crtc_id;
   uint32_t g_connector_id;
   unsigned g_fb_width;
   unsigned g_fb_height;
   unsigned g_interval;

   drmModeModeInfo *g_drm_mode;
   drmModeCrtcPtr g_orig_crtc;
   drmModeRes *g_resources;
   drmModeConnector *g_connector;
   drmModeEncoder *g_encoder;

   struct gbm_bo *g_bo;
   struct gbm_bo *g_next_bo;
   struct gbm_device *g_gbm_dev;
   struct gbm_surface *g_gbm_surface;
} gfx_ctx_drm_egl_data_t;

static volatile sig_atomic_t g_quit;

static enum gfx_ctx_api g_api;

static unsigned g_major;

static unsigned g_minor;

struct drm_fb
{
   struct gbm_bo *bo;
   uint32_t fb_id;
};

static void drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
   driver_t *driver = driver_get_ptr();
   struct drm_fb *fb = (struct drm_fb*)data;
   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)driver->video_context_data;

   if (drm && fb->fb_id)
      drmModeRmFB(drm->g_drm_fd, fb->fb_id);

   free(fb);
}

static struct drm_fb *drm_fb_get_from_bo(
      gfx_ctx_drm_egl_data_t *drm,
      struct gbm_bo *bo)
{
   int ret;
   unsigned width, height, stride, handle;
   struct drm_fb *fb = (struct drm_fb*)gbm_bo_get_user_data(bo);
   if (fb)
      return fb;

   fb = (struct drm_fb*)calloc(1, sizeof(*fb));
   fb->bo = bo;

   width  = gbm_bo_get_width(bo);
   height = gbm_bo_get_height(bo);
   stride = gbm_bo_get_stride(bo);
   handle = gbm_bo_get_handle(bo).u32;

   RARCH_LOG("[KMS/EGL]: New FB: %ux%u (stride: %u).\n", width, height, stride);

   ret = drmModeAddFB(drm->g_drm_fd, width, height, 24, 32, stride, handle, &fb->fb_id);
   if (ret < 0)
   {
      RARCH_ERR("[KMS/EGL]: Failed to create FB: %s\n", strerror(errno));
      free(fb);
      return NULL;
   }

   gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);
   return fb;
}

static void sighandler(int sig)
{
   (void)sig;
   g_quit = 1;
}

static void gfx_ctx_drm_egl_swap_interval(void *data, unsigned interval)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)driver->video_context_data;

   (void)data;

   if (drm)
      drm->g_interval = interval;
   if (interval > 1)
      RARCH_WARN("[KMS/EGL]: Swap intervals > 1 currently not supported. Will use swap interval of 1.\n");
}

static void gfx_ctx_drm_egl_check_window(void *data, bool *quit,
      bool *resize, unsigned *width, unsigned *height, unsigned frame_count)
{
   (void)data;
   (void)frame_count;
   (void)width;
   (void)height;

   *resize = false;
   *quit   = g_quit;
}

static unsigned first_page_flip;
static unsigned last_page_flip;
static bool waiting_for_flip;

static void page_flip_handler(int fd, unsigned frame,
      unsigned sec, unsigned usec, void *data)
{
   (void)fd;
   (void)sec;
   (void)usec;

   if (!first_page_flip)
      first_page_flip = frame;

   if (last_page_flip)
   {
      unsigned missed = frame - last_page_flip - 1;
      if (missed)
         RARCH_LOG("[KMS/EGL]: Missed %u VBlank(s) (Frame: %u, DRM frame: %u).\n",
               missed, frame - first_page_flip, frame);
   }

   last_page_flip = frame;
   *(bool*)data = false;
}

static void wait_flip(bool block)
{
   int timeout = 0;
   struct pollfd fds = {0};
   drmEventContext evctx   = {0};
   driver_t *driver = driver_get_ptr();
   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)
      driver->video_context_data;

   fds.fd     = drm->g_drm_fd;
   fds.events = POLLIN;

   evctx.version           = DRM_EVENT_CONTEXT_VERSION;
   evctx.page_flip_handler = page_flip_handler;
   
   if (block)
      timeout = -1;

   while (waiting_for_flip)
   {
      fds.revents = 0;

      if (poll(&fds, 1, timeout) < 0)
         break;

      if (fds.revents & (POLLHUP | POLLERR))
         break;

      if (fds.revents & POLLIN)
         drmHandleEvent(drm->g_drm_fd, &evctx);
      else
         break;
   }

   if (waiting_for_flip)
      return;

   /* Page flip has taken place. */

   /* This buffer is not on-screen anymore. Release it to GBM. */
   gbm_surface_release_buffer(drm->g_gbm_surface, drm->g_bo);
   /* This buffer is being shown now. */
   drm->g_bo = drm->g_next_bo; 
}

static void queue_flip(void)
{
   int ret;
   struct drm_fb *fb = NULL;
   driver_t *driver = driver_get_ptr();
   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)
   driver->video_context_data;

   drm->g_next_bo = gbm_surface_lock_front_buffer(drm->g_gbm_surface);

   fb = (struct drm_fb*)drm_fb_get_from_bo(drm, drm->g_next_bo);

   ret = drmModePageFlip(drm->g_drm_fd, drm->g_crtc_id, fb->fb_id,
         DRM_MODE_PAGE_FLIP_EVENT, &waiting_for_flip);

   if (ret < 0)
   {
      RARCH_ERR("[KMS/EGL]: Failed to queue page flip.\n");
      return;
   }

   waiting_for_flip = true;
}

static void gfx_ctx_drm_egl_swap_buffers(void *data)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)
   driver->video_context_data;

   (void)data;

   eglSwapBuffers(g_egl_dpy, g_egl_surf);

   /* I guess we have to wait for flip to have taken 
    * place before another flip can be queued up. */
   if (waiting_for_flip)
   {
      wait_flip(drm->g_interval);

      /* We are still waiting for a flip 
       * (nonblocking mode, just drop the frame).
       */
      if (waiting_for_flip)
         return;
   }

   queue_flip();

   /* We have to wait for this flip to finish. 
    * This shouldn't happen as we have triple buffered page-flips. */
   if (!gbm_surface_has_free_buffers(drm->g_gbm_surface))
   {
      RARCH_WARN("[KMS/EGL]: Triple buffering is not working correctly ...\n");
      wait_flip(true);  
   }
}

static void gfx_ctx_drm_egl_set_resize(void *data,
      unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
}

static void gfx_ctx_drm_egl_update_window_title(void *data)
{
   char buf[128]        = {0};
   char buf_fps[128]    = {0};
   settings_t *settings = config_get_ptr();

   video_monitor_get_fps(buf, sizeof(buf),
         buf_fps, sizeof(buf_fps));
   if (settings->fps_show)
      rarch_main_msg_queue_push( buf_fps, 1, 1, false);
}

static void gfx_ctx_drm_egl_get_video_size(void *data, unsigned *width, unsigned *height)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)driver->video_context_data;

   if (!drm)
      return;

   (void)data;
   *width  = drm->g_fb_width;
   *height = drm->g_fb_height;
}

static void free_drm_resources(gfx_ctx_drm_egl_data_t *drm)
{
   if (!drm)
      return;

   if (drm->g_gbm_surface)
      gbm_surface_destroy(drm->g_gbm_surface);

   if (drm->g_gbm_dev)
      gbm_device_destroy(drm->g_gbm_dev);

   if (drm->g_encoder)
      drmModeFreeEncoder(drm->g_encoder);

   if (drm->g_connector)
      drmModeFreeConnector(drm->g_connector);

   if (drm->g_resources)
      drmModeFreeResources(drm->g_resources);

   if (drm->g_orig_crtc)
      drmModeFreeCrtc(drm->g_orig_crtc);

   if (drm->g_drm_fd >= 0)
      retro_fclose(drm->g_drm);

   drm->g_gbm_surface = NULL;
   drm->g_gbm_dev     = NULL;
   drm->g_encoder     = NULL;
   drm->g_connector   = NULL;
   drm->g_resources   = NULL;
   drm->g_orig_crtc   = NULL;
   drm->g_drm_fd      = -1;
}

static void gfx_ctx_drm_egl_destroy_resources(gfx_ctx_drm_egl_data_t *drm)
{
   if (!drm)
      return;

   /* Make sure we acknowledge all page-flips. */

   if (waiting_for_flip)
      wait_flip(true);

   egl_destroy();

   /* Restore original CRTC. */
   if (drm->g_orig_crtc)
   {
      drmModeSetCrtc(drm->g_drm_fd, drm->g_orig_crtc->crtc_id,
            drm->g_orig_crtc->buffer_id,
            drm->g_orig_crtc->x,
            drm->g_orig_crtc->y,
            &drm->g_connector_id, 1, &drm->g_orig_crtc->mode);
   }

   free_drm_resources(drm);

   drm->g_drm_mode = NULL;
   g_quit         = 0;
   drm->g_crtc_id      = 0;
   drm->g_connector_id = 0;

   drm->g_fb_width  = 0;
   drm->g_fb_height = 0;

   drm->g_bo      = NULL;
   drm->g_next_bo = NULL;
}

static bool gfx_ctx_drm_egl_init(void *data)
{
   int i;
   unsigned monitor_index;
   unsigned gpu_index                   = 0;
   const char *gpu                      = NULL;
   struct string_list *gpu_descriptors  = NULL;
   settings_t *settings                 = config_get_ptr();
   unsigned monitor = max(settings->video.monitor_index, 1);

   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)calloc(1, sizeof(gfx_ctx_drm_egl_data_t));
   driver_t *driver = driver_get_ptr();

   if (!drm)
      return false;

   drm->g_drm_fd   = -1;
   gpu_descriptors = dir_list_new("/dev/dri", NULL, false, false);

nextgpu:
   free_drm_resources(drm);

   if (!gpu_descriptors || gpu_index == gpu_descriptors->size)
   {
      RARCH_ERR("[KMS/EGL]: Couldn't find a suitable DRM device.\n");
      goto error;
   }
   gpu = gpu_descriptors->elems[gpu_index++].data;

   drm->g_drm    = retro_fopen(gpu, RFILE_MODE_READ_WRITE, -1);
   if (!drm->g_drm)
   {
      RARCH_WARN("[KMS/EGL]: Couldn't open DRM device.\n");
      goto nextgpu;
   }

   drm->g_drm_fd = retro_get_fd(drm->g_drm);

   drm->g_resources = drmModeGetResources(drm->g_drm_fd);
   if (!drm->g_resources)
   {
      RARCH_WARN("[KMS/EGL]: Couldn't get device resources.\n");
      goto nextgpu;
   }

   /* Enumerate all connectors. */
   monitor_index = 0;
   RARCH_LOG("[KMS/EGL]: Found %d connectors.\n",
         drm->g_resources->count_connectors);

   for (i = 0; i < drm->g_resources->count_connectors; i++)
   {
      drmModeConnectorPtr conn = drmModeGetConnector(
            drm->g_drm_fd, drm->g_resources->connectors[i]);

      if (conn)
      {
         bool connected = conn->connection == DRM_MODE_CONNECTED;
         RARCH_LOG("[KMS/EGL]: Connector %d connected: %s\n", i, connected ? "yes" : "no");
         RARCH_LOG("[KMS/EGL]: Connector %d has %d modes.\n", i, conn->count_modes);
         if (connected && conn->count_modes > 0)
         {
            monitor_index++;
            RARCH_LOG("[KMS/EGL]: Connector %d assigned to monitor index: #%u.\n", i, monitor_index);
         }
         drmModeFreeConnector(conn);
      }
   }

   monitor_index = 0;
   for (i = 0; i < drm->g_resources->count_connectors; i++)
   {
      drm->g_connector = drmModeGetConnector(drm->g_drm_fd,
            drm->g_resources->connectors[i]);

      if (!drm->g_connector)
         continue;
      if (drm->g_connector->connection == DRM_MODE_CONNECTED
            && drm->g_connector->count_modes > 0)
      {
         monitor_index++;
         if (monitor_index == monitor)
            break;
      }

      drmModeFreeConnector(drm->g_connector);
      drm->g_connector = NULL;
   }

   if (!drm->g_connector)
   {
      RARCH_WARN("[KMS/EGL]: Couldn't get device connector.\n");
      goto nextgpu;
   }

   for (i = 0; i < drm->g_resources->count_encoders; i++)
   {
      drm->g_encoder = drmModeGetEncoder(drm->g_drm_fd,
            drm->g_resources->encoders[i]);

      if (!drm->g_encoder)
         continue;
      if (drm->g_encoder->encoder_id == drm->g_connector->encoder_id)
         break;

      drmModeFreeEncoder(drm->g_encoder);
      drm->g_encoder = NULL;
   }

   if (!drm->g_encoder)
   {
      RARCH_WARN("[KMS/EGL]: Couldn't find DRM encoder.\n");
      goto nextgpu;
   }

   for (i = 0; i < drm->g_connector->count_modes; i++)
   {
      RARCH_LOG("[KMS/EGL]: Mode %d: (%s) %d x %d, %u Hz\n",
            i,
            drm->g_connector->modes[i].name,
            drm->g_connector->modes[i].hdisplay,
            drm->g_connector->modes[i].vdisplay,
            drm->g_connector->modes[i].vrefresh);
   }

   drm->g_crtc_id   = drm->g_encoder->crtc_id;
   drm->g_orig_crtc = drmModeGetCrtc(drm->g_drm_fd, drm->g_crtc_id);
   if (!drm->g_orig_crtc)
      RARCH_WARN("[KMS/EGL]: Cannot find original CRTC.\n");

   drm->g_connector_id = drm->g_connector->connector_id;

   /* First mode is assumed to be the "optimal" 
    * one for get_video_size() purposes. */
   drm->g_fb_width  = drm->g_connector->modes[0].hdisplay;
   drm->g_fb_height = drm->g_connector->modes[0].vdisplay;

   drm->g_gbm_dev = gbm_create_device(drm->g_drm_fd);

   if (!drm->g_gbm_dev)
   {
      RARCH_WARN("[KMS/EGL]: Couldn't create GBM device.\n");
      goto nextgpu;
   }

   dir_list_free(gpu_descriptors);

   driver->video_context_data = drm;

   return true;

error:
   dir_list_free(gpu_descriptors);

   gfx_ctx_drm_egl_destroy_resources(drm);

   if (drm)
      free(drm);

   return false;
}



static EGLint *egl_fill_attribs(EGLint *attr)
{
   switch (g_api)
   {
#ifdef EGL_KHR_create_context
      case GFX_CTX_OPENGL_API:
      {
         unsigned version = g_major * 1000 + g_minor;
         bool core        = version >= 3001;
#ifdef GL_DEBUG
         bool debug       = true;
#else
         const struct retro_hw_render_callback *hw_render = 
            (const struct retro_hw_render_callback*)video_driver_callback();
         bool debug       = hw_render->debug_context;
#endif

         if (core)
         {
            *attr++ = EGL_CONTEXT_MAJOR_VERSION_KHR;
            *attr++ = g_major;
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = g_minor;

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
      }
#endif

      case GFX_CTX_OPENGL_ES_API:
         *attr++ = EGL_CONTEXT_CLIENT_VERSION;
         *attr++ = g_major ? (EGLint)g_major : 2;
#ifdef EGL_KHR_create_context
         if (g_minor > 0)
         {
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = g_minor;
         }
#endif
         break;

      default:
         break;
   }

   *attr = EGL_NONE;
   return attr;
}

#define DRM_EGL_ATTRIBS_BASE \
   EGL_SURFACE_TYPE,    EGL_WINDOW_BIT, \
   EGL_RED_SIZE,        1, \
   EGL_GREEN_SIZE,      1, \
   EGL_BLUE_SIZE,       1, \
   EGL_ALPHA_SIZE,      0, \
   EGL_DEPTH_SIZE,      0

static bool gfx_ctx_drm_egl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
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

   static const EGLint egl_attribs_vg[] = {
      DRM_EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
      EGL_NONE,
   };

   const EGLint *attrib_ptr;
   EGLint major, minor, n, egl_attribs[16], *attr;
   float refresh_mod;
   int i, ret = 0;
   struct sigaction sa = {{0}};
   struct drm_fb *fb = NULL;
   driver_t *driver     = driver_get_ptr();
   settings_t *settings = config_get_ptr();
   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)
      driver->video_context_data;

   if (!drm)
      return false;

   sa.sa_handler = sighandler;
   sa.sa_flags   = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);

   switch (g_api)
   {
      case GFX_CTX_OPENGL_API:
         attrib_ptr = egl_attribs_gl;
         break;
      case GFX_CTX_OPENGL_ES_API:
#ifdef EGL_KHR_create_context
         if (g_major >= 3)
            attrib_ptr = egl_attribs_gles3;
         else
#endif
         attrib_ptr = egl_attribs_gles;
         break;
      case GFX_CTX_OPENVG_API:
         attrib_ptr = egl_attribs_vg;
         break;
      default:
         attrib_ptr = NULL;
   }

   /* If we use black frame insertion, 
    * we fake a 60 Hz monitor for 120 Hz one, etc, so try to match that. */
   refresh_mod = settings->video.black_frame_insertion ? 0.5f : 1.0f;

   /* Find desired video mode, and use that.
    * If not fullscreen, we get desired windowed size, 
    * which is not appropriate. */
   if ((width == 0 && height == 0) || !fullscreen)
      drm->g_drm_mode = &drm->g_connector->modes[0];
   else
   {
      /* Try to match settings->video.refresh_rate as closely as possible.
       * Lower resolutions tend to have multiple supported 
       * refresh rates as well.
       */
      float minimum_fps_diff = 0.0f;

      /* Find best match. */
      for (i = 0; i < drm->g_connector->count_modes; i++)
      {
         float diff;
         if (width != drm->g_connector->modes[i].hdisplay || 
               height != drm->g_connector->modes[i].vdisplay)
            continue;

         diff = fabsf(refresh_mod * drm->g_connector->modes[i].vrefresh
               - settings->video.refresh_rate);

         if (!drm->g_drm_mode || diff < minimum_fps_diff)
         {
            drm->g_drm_mode = &drm->g_connector->modes[i];
            minimum_fps_diff = diff;
         }
      }
   }

   if (!drm->g_drm_mode)
   {
      RARCH_ERR("[KMS/EGL]: Did not find suitable video mode for %u x %u.\n", width, height);
      goto error;
   }

   drm->g_fb_width  = drm->g_drm_mode->hdisplay;
   drm->g_fb_height = drm->g_drm_mode->vdisplay;

   /* Create GBM surface. */
   drm->g_gbm_surface = gbm_surface_create(
         drm->g_gbm_dev,
         drm->g_fb_width,
         drm->g_fb_height,
         GBM_FORMAT_XRGB8888,
         GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

   if (!drm->g_gbm_surface)
   {
      RARCH_ERR("[KMS/EGL]: Couldn't create GBM surface.\n");
      goto error;
   }

   g_egl_dpy = eglGetDisplay((EGLNativeDisplayType)drm->g_gbm_dev);
   if (!g_egl_dpy)
   {
      RARCH_ERR("[KMS/EGL]: Couldn't get EGL display.\n");
      goto error;
   }

   if (!eglInitialize(g_egl_dpy, &major, &minor))
      goto error;

   if (!eglChooseConfig(g_egl_dpy, attrib_ptr, &g_egl_config, 1, &n) || n != 1)
      goto error;

   attr = egl_fill_attribs(egl_attribs);

   g_egl_ctx = eglCreateContext(g_egl_dpy, g_egl_config, EGL_NO_CONTEXT,
         attr != egl_attribs ? egl_attribs : NULL);

   if (g_egl_ctx == EGL_NO_CONTEXT)
      goto error;

   if (g_use_hw_ctx)
   {
      g_egl_hw_ctx = eglCreateContext(g_egl_dpy, g_egl_config, g_egl_ctx,
            attr != egl_attribs ? egl_attribs : NULL);
      RARCH_LOG("[KMS/EGL]: Created shared context: %p.\n", (void*)g_egl_hw_ctx);

      if (g_egl_hw_ctx == EGL_NO_CONTEXT)
         goto error;
   }

   g_egl_surf = eglCreateWindowSurface(g_egl_dpy,
         g_egl_config, (EGLNativeWindowType)drm->g_gbm_surface, NULL);
   if (!g_egl_surf)
      goto error;

   if (!eglMakeCurrent(g_egl_dpy,
            g_egl_surf, g_egl_surf, g_egl_ctx))
      goto error;

   glClear(GL_COLOR_BUFFER_BIT);
   eglSwapBuffers(g_egl_dpy, g_egl_surf);

   drm->g_bo = gbm_surface_lock_front_buffer(drm->g_gbm_surface);
   fb = drm_fb_get_from_bo(drm, drm->g_bo);

   ret = drmModeSetCrtc(drm->g_drm_fd,
         drm->g_crtc_id, fb->fb_id, 0, 0, &drm->g_connector_id, 1, drm->g_drm_mode);
   if (ret < 0)
      goto error;

   return true;

error:
   gfx_ctx_drm_egl_destroy_resources(drm);

   if (drm)
      free(drm);

   return false;
}


static void gfx_ctx_drm_egl_destroy(void *data)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)
      driver->video_context_data;

   if (!drm)
      return;

   (void)data;

   gfx_ctx_drm_egl_destroy_resources(drm);

   if (driver->video_context_data)
      free(driver->video_context_data);
   driver->video_context_data = NULL;
}

static void gfx_ctx_drm_egl_input_driver(void *data,
      const input_driver_t **input, void **input_data)
{
   (void)data;
   *input = NULL;
   *input_data = NULL;
}

static bool gfx_ctx_drm_egl_has_focus(void *data)
{
   driver_t *driver = driver_get_ptr();
   gfx_ctx_drm_egl_data_t *drm = (gfx_ctx_drm_egl_data_t*)
      driver->video_context_data;
   (void)data;

   if (!drm)
      return false;
   return true;
}

static bool gfx_ctx_drm_egl_suppress_screensaver(void *data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static bool gfx_ctx_drm_egl_has_windowed(void *data)
{
   (void)data;
   return false;
}

static bool gfx_ctx_drm_egl_bind_api(void *data,
      enum gfx_ctx_api api, unsigned major, unsigned minor)
{
   (void)data;

   g_major = major;
   g_minor = minor;
   g_api = api;

   switch (api)
   {
      case GFX_CTX_OPENGL_API:
#ifndef EGL_KHR_create_context
         if ((major * 1000 + minor) >= 3001)
            return false;
#endif
         return eglBindAPI(EGL_OPENGL_API);
      case GFX_CTX_OPENGL_ES_API:
#ifndef EGL_KHR_create_context
         if (major >= 3)
            return false;
#endif
         return eglBindAPI(EGL_OPENGL_ES_API);
      case GFX_CTX_OPENVG_API:
         return eglBindAPI(EGL_OPENVG_API);
      default:
         break;
   }

   return false;
}


const gfx_ctx_driver_t gfx_ctx_drm_egl = {
   gfx_ctx_drm_egl_init,
   gfx_ctx_drm_egl_destroy,
   gfx_ctx_drm_egl_bind_api,
   gfx_ctx_drm_egl_swap_interval,
   gfx_ctx_drm_egl_set_video_mode,
   gfx_ctx_drm_egl_get_video_size,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   gfx_ctx_drm_egl_update_window_title,
   gfx_ctx_drm_egl_check_window,
   gfx_ctx_drm_egl_set_resize,
   gfx_ctx_drm_egl_has_focus,
   gfx_ctx_drm_egl_suppress_screensaver,
   gfx_ctx_drm_egl_has_windowed,
   gfx_ctx_drm_egl_swap_buffers,
   gfx_ctx_drm_egl_input_driver,
   egl_get_proc_address,
   NULL,
   NULL,
   NULL,
   "kms-egl",
   egl_bind_hw_render,
};

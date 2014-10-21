/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
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

// KMS/DRM context, running without any window manager.
// Based on kmscube example by Rob Clark.

#include "../../driver.h"
#include "../gfx_context.h"
#include "../gl_common.h"
#include "../gfx_common.h"
#include "../../dir_list.h"

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <errno.h>
#include <signal.h>
#include <stdint.h>
#include <signal.h>
#include <unistd.h>
#include <sched.h>
#include <sys/time.h>
#include <math.h>

#include <EGL/egl.h>
#include <EGL/eglext.h>

#include <libdrm/drm.h>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <gbm.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/poll.h>
#include <fcntl.h>

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

static bool g_use_hw_ctx;
static EGLContext g_egl_hw_ctx;
static EGLContext g_egl_ctx;
static EGLSurface g_egl_surf;
static EGLDisplay g_egl_dpy;
static EGLConfig g_config;

static volatile sig_atomic_t g_quit;
static bool g_inited;
static unsigned g_interval;
static enum gfx_ctx_api g_api;
static unsigned g_major;
static unsigned g_minor;

static struct gbm_device *g_gbm_dev;
static struct gbm_surface *g_gbm_surface;

static int g_drm_fd;
static drmModeModeInfo *g_drm_mode;
static uint32_t g_crtc_id;
static uint32_t g_connector_id;

static drmModeCrtcPtr g_orig_crtc;

static unsigned g_fb_width; // Just use something for now.
static unsigned g_fb_height;

static struct gbm_bo *g_bo, *g_next_bo;

static drmModeRes *g_resources;
static drmModeConnector *g_connector;
static drmModeEncoder *g_encoder;

struct drm_fb
{
   struct gbm_bo *bo;
   uint32_t fb_id;
};

static struct drm_fb *drm_fb_get_from_bo(struct gbm_bo *bo);

static void gfx_ctx_drm_egl_destroy(void *data);

static void sighandler(int sig)
{
   (void)sig;
   g_quit = 1;
}

static void gfx_ctx_drm_egl_swap_interval(void *data, unsigned interval)
{
   (void)data;
   g_interval = interval;
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

static void page_flip_handler(int fd, unsigned frame, unsigned sec, unsigned usec, void *data)
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

static bool waiting_for_flip;

static void wait_flip(bool block)
{
   struct pollfd fds = {0};
   fds.fd     = g_drm_fd;
   fds.events = POLLIN;

   drmEventContext evctx   = {0};
   evctx.version           = DRM_EVENT_CONTEXT_VERSION;
   evctx.page_flip_handler = page_flip_handler;
   
   int timeout = block ? -1 : 0;

   while (waiting_for_flip)
   {
      fds.revents = 0;
      if (poll(&fds, 1, timeout) < 0)
         break;

      if (fds.revents & (POLLHUP | POLLERR))
         break;

      if (fds.revents & POLLIN)
         drmHandleEvent(g_drm_fd, &evctx);
      else
         break;
   }

   if (!waiting_for_flip) // Page flip has taken place.
   {
      gbm_surface_release_buffer(g_gbm_surface, g_bo); // This buffer is not on-screen anymore. Release it to GBM.
      g_bo = g_next_bo; // This buffer is being shown now.
   }
}

static void queue_flip(void)
{
   g_next_bo = gbm_surface_lock_front_buffer(g_gbm_surface);
   struct drm_fb *fb = drm_fb_get_from_bo(g_next_bo);

   int ret = drmModePageFlip(g_drm_fd, g_crtc_id, fb->fb_id,
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
   (void)data;
   eglSwapBuffers(g_egl_dpy, g_egl_surf);

   // I guess we have to wait for flip to have taken place before another flip can be queued up.
   if (waiting_for_flip)
   {
      wait_flip(g_interval);
      if (waiting_for_flip) // We are still waiting for a flip (nonblocking mode, just drop the frame).
         return;
   }

   queue_flip();

   // We have to wait for this flip to finish. This shouldn't happen as we have triple buffered page-flips.
   if (!gbm_surface_has_free_buffers(g_gbm_surface))
   {
      RARCH_WARN("[KMS/EGL]: Triple buffering is not working correctly ...\n");
      wait_flip(true);  
   }
}

static void gfx_ctx_drm_egl_set_resize(void *data, unsigned width, unsigned height)
{
   (void)data;
   (void)width;
   (void)height;
}

static void gfx_ctx_drm_egl_update_window_title(void *data)
{
   (void)data;
   char buf[128], buf_fps[128];
   bool fps_draw = g_settings.fps_show;
   gfx_get_fps(buf, sizeof(buf), fps_draw ? buf_fps : NULL, sizeof(buf_fps));

   if (fps_draw)
      msg_queue_push(g_extern.msg_queue, buf_fps, 1, 1);
}

static void gfx_ctx_drm_egl_get_video_size(void *data, unsigned *width, unsigned *height)
{
   (void)data;
   *width  = g_fb_width;
   *height = g_fb_height;
}

static void free_drm_resources(void)
{
   if (g_gbm_surface)
      gbm_surface_destroy(g_gbm_surface);

   if (g_gbm_dev)
      gbm_device_destroy(g_gbm_dev);

   if (g_encoder)
      drmModeFreeEncoder(g_encoder);

   if (g_connector)
      drmModeFreeConnector(g_connector);

   if (g_resources)
      drmModeFreeResources(g_resources);

   if (g_orig_crtc)
      drmModeFreeCrtc(g_orig_crtc);

   if (g_drm_fd >= 0)
      close(g_drm_fd);

   g_gbm_surface = NULL;
   g_gbm_dev     = NULL;
   g_encoder     = NULL;
   g_connector   = NULL;
   g_resources   = NULL;
   g_orig_crtc   = NULL;
   g_drm_fd      = -1;
}

static bool gfx_ctx_drm_egl_init(void *data)
{
   int i;
   unsigned monitor_index;
   unsigned gpu_index = 0;
   unsigned monitor = max(g_settings.video.monitor_index, 1);

   const char *gpu;
   if (g_inited)
      return false;

   g_drm_fd = -1;
   struct string_list *gpu_descriptors = dir_list_new("/dev/dri", NULL, false);

nextgpu:
   free_drm_resources();

   if (!gpu_descriptors || gpu_index == gpu_descriptors->size)
   {
      RARCH_ERR("[KMS/EGL]: Couldn't find a suitable DRM device.\n");
      goto error;
   }
   gpu = gpu_descriptors->elems[gpu_index++].data;

   g_drm_fd = open(gpu, O_RDWR);
   if (g_drm_fd < 0)
   {
      RARCH_WARN("[KMS/EGL]: Couldn't open DRM device.\n");
      goto nextgpu;
   }

   g_resources = drmModeGetResources(g_drm_fd);
   if (!g_resources)
   {
      RARCH_WARN("[KMS/EGL]: Couldn't get device resources.\n");
      goto nextgpu;
   }

   // Enumerate all connectors.
   monitor_index = 0;
   RARCH_LOG("[KMS/EGL]: Found %d connectors.\n", g_resources->count_connectors);
   for (i = 0; i < g_resources->count_connectors; i++)
   {
      drmModeConnectorPtr conn = drmModeGetConnector(g_drm_fd, g_resources->connectors[i]);
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
   for (i = 0; i < g_resources->count_connectors; i++)
   {
      g_connector = drmModeGetConnector(g_drm_fd, g_resources->connectors[i]);

      if (!g_connector)
         continue;
      if (g_connector->connection == DRM_MODE_CONNECTED && g_connector->count_modes > 0)
      {
         monitor_index++;
         if (monitor_index == monitor)
            break;
      }

      drmModeFreeConnector(g_connector);
      g_connector = NULL;
   }

   if (!g_connector)
   {
      RARCH_WARN("[KMS/EGL]: Couldn't get device connector.\n");
      goto nextgpu;
   }

   for (i = 0; i < g_resources->count_encoders; i++)
   {
      g_encoder = drmModeGetEncoder(g_drm_fd, g_resources->encoders[i]);

      if (!g_encoder)
         continue;
      if (g_encoder->encoder_id == g_connector->encoder_id)
         break;

      drmModeFreeEncoder(g_encoder);
      g_encoder = NULL;
   }

   if (!g_encoder)
   {
      RARCH_WARN("[KMS/EGL]: Couldn't find DRM encoder.\n");
      goto nextgpu;
   }

   for (i = 0; i < g_connector->count_modes; i++)
   {
      RARCH_LOG("[KMS/EGL]: Mode %d: (%s) %d x %d, %u Hz\n", i,
            g_connector->modes[i].name,
            g_connector->modes[i].hdisplay, g_connector->modes[i].vdisplay,
            g_connector->modes[i].vrefresh);
   }

   g_crtc_id   = g_encoder->crtc_id;
   g_orig_crtc = drmModeGetCrtc(g_drm_fd, g_crtc_id);
   if (!g_orig_crtc)
      RARCH_WARN("[KMS/EGL]: Cannot find original CRTC.\n");

   g_connector_id = g_connector->connector_id;

   // First mode is assumed to be the "optimal" one for get_video_size() purposes.
   g_fb_width  = g_connector->modes[0].hdisplay;
   g_fb_height = g_connector->modes[0].vdisplay;

   g_gbm_dev = gbm_create_device(g_drm_fd);
   if (!g_gbm_dev)
   {
      RARCH_WARN("[KMS/EGL]: Couldn't create GBM device.\n");
      goto nextgpu;
   }

   dir_list_free(gpu_descriptors);
   return true;

error:
   dir_list_free(gpu_descriptors);
   gfx_ctx_drm_egl_destroy(data);
   return false;
}

static void drm_fb_destroy_callback(struct gbm_bo *bo, void *data)
{
   struct drm_fb *fb = (struct drm_fb*)data;

   if (fb->fb_id)
      drmModeRmFB(g_drm_fd, fb->fb_id);

   free(fb);
}

static struct drm_fb *drm_fb_get_from_bo(struct gbm_bo *bo)
{
   struct drm_fb *fb = (struct drm_fb*)gbm_bo_get_user_data(bo);
   if (fb)
      return fb;

   fb = (struct drm_fb*)calloc(1, sizeof(*fb));
   fb->bo = bo;

   unsigned width  = gbm_bo_get_width(bo);
   unsigned height = gbm_bo_get_height(bo);
   unsigned stride = gbm_bo_get_stride(bo);
   unsigned handle = gbm_bo_get_handle(bo).u32;

   RARCH_LOG("[KMS/EGL]: New FB: %ux%u (stride: %u).\n", width, height, stride);

   int ret = drmModeAddFB(g_drm_fd, width, height, 24, 32, stride, handle, &fb->fb_id);
   if (ret < 0)
   {
      RARCH_ERR("[KMS/EGL]: Failed to create FB: %s\n", strerror(errno));
      free(fb);
      return NULL;
   }

   gbm_bo_set_user_data(bo, fb, drm_fb_destroy_callback);
   return fb;
}

static EGLint *egl_fill_attribs(EGLint *attr)
{
   switch (g_api)
   {
#ifdef EGL_KHR_create_context
      case GFX_CTX_OPENGL_API:
      {
         unsigned version = g_major * 1000 + g_minor;
         bool core = version >= 3001;
#ifdef GL_DEBUG
         bool debug = true;
#else
         bool debug = g_extern.system.hw_render_callback.debug_context;
#endif

         if (core)
         {
            *attr++ = EGL_CONTEXT_MAJOR_VERSION_KHR;
            *attr++ = g_major;
            *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
            *attr++ = g_minor;
            // Technically, we don't have core/compat until 3.2.
            // Version 3.1 is either compat or not depending on GL_ARB_compatibility.
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

static bool gfx_ctx_drm_egl_set_video_mode(void *data,
      unsigned width, unsigned height,
      bool fullscreen)
{
   if (g_inited)
      return false;

   int i;
   int ret = 0;
   struct drm_fb *fb = NULL;

   struct sigaction sa = {{0}};
   sa.sa_handler = sighandler;
   sa.sa_flags   = SA_RESTART;
   sigemptyset(&sa.sa_mask);
   sigaction(SIGINT, &sa, NULL);
   sigaction(SIGTERM, &sa, NULL);

#define EGL_ATTRIBS_BASE \
   EGL_SURFACE_TYPE,    EGL_WINDOW_BIT, \
   EGL_RED_SIZE,        1, \
   EGL_GREEN_SIZE,      1, \
   EGL_BLUE_SIZE,       1, \
   EGL_ALPHA_SIZE,      0, \
   EGL_DEPTH_SIZE,      0

   static const EGLint egl_attribs_gl[] = {
      EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
      EGL_NONE,
   };

   static const EGLint egl_attribs_gles[] = {
      EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES2_BIT,
      EGL_NONE,
   };

#ifdef EGL_KHR_create_context
   static const EGLint egl_attribs_gles3[] = {
      EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
      EGL_NONE,
   };
#endif

   static const EGLint egl_attribs_vg[] = {
      EGL_ATTRIBS_BASE,
      EGL_RENDERABLE_TYPE, EGL_OPENVG_BIT,
      EGL_NONE,
   };

   const EGLint *attrib_ptr;
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

   // If we use black frame insertion, we fake a 60 Hz monitor for 120 Hz one, etc, so try to match that.
   float refresh_mod = g_settings.video.black_frame_insertion ? 0.5f : 1.0f;

   // Find desired video mode, and use that.
   // If not fullscreen, we get desired windowed size, which is not appropriate.
   if ((width == 0 && height == 0) || !fullscreen)
      g_drm_mode = &g_connector->modes[0];
   else
   {
      // Try to match g_settings.video.refresh_rate as closely as possible.
      // Lower resolutions tend to have multiple supported refresh rates as well.
      float minimum_fps_diff = 0.0f;

      // Find best match.
      for (i = 0; i < g_connector->count_modes; i++)
      {
         if (width != g_connector->modes[i].hdisplay || height != g_connector->modes[i].vdisplay)
            continue;

         float diff = fabsf(refresh_mod * g_connector->modes[i].vrefresh - g_settings.video.refresh_rate);
         if (!g_drm_mode || diff < minimum_fps_diff)
         {
            g_drm_mode = &g_connector->modes[i];
            minimum_fps_diff = diff;
         }
      }
   }

   if (!g_drm_mode)
   {
      RARCH_ERR("[KMS/EGL]: Did not find suitable video mode for %u x %u.\n", width, height);
      goto error;
   }

   g_fb_width = g_drm_mode->hdisplay;
   g_fb_height = g_drm_mode->vdisplay;

   // Create GBM surface.
   g_gbm_surface = gbm_surface_create(g_gbm_dev,
         g_fb_width, g_fb_height,
         GBM_FORMAT_XRGB8888,
         GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING);

   if (!g_gbm_surface)
   {
      RARCH_ERR("[KMS/EGL]: Couldn't create GBM surface.\n");
      goto error;
   }

   g_egl_dpy = eglGetDisplay((EGLNativeDisplayType)g_gbm_dev);
   if (!g_egl_dpy)
   {
      RARCH_ERR("[KMS/EGL]: Couldn't get EGL display.\n");
      goto error;
   }

   EGLint major, minor;
   if (!eglInitialize(g_egl_dpy, &major, &minor))
      goto error;

   EGLint n;
   if (!eglChooseConfig(g_egl_dpy, attrib_ptr, &g_config, 1, &n) || n != 1)
      goto error;

   EGLint egl_attribs[16];
   EGLint *attr;
   attr = egl_fill_attribs(egl_attribs);

   g_egl_ctx = eglCreateContext(g_egl_dpy, g_config, EGL_NO_CONTEXT,
         attr != egl_attribs ? egl_attribs : NULL);

   if (g_egl_ctx == EGL_NO_CONTEXT)
      goto error;

   if (g_use_hw_ctx)
   {
      g_egl_hw_ctx = eglCreateContext(g_egl_dpy, g_config, g_egl_ctx,
            attr != egl_attribs ? egl_attribs : NULL);
      RARCH_LOG("[KMS/EGL]: Created shared context: %p.\n", (void*)g_egl_hw_ctx);

      if (g_egl_hw_ctx == EGL_NO_CONTEXT)
         goto error;
   }

   g_egl_surf = eglCreateWindowSurface(g_egl_dpy, g_config, (EGLNativeWindowType)g_gbm_surface, NULL);
   if (!g_egl_surf)
      goto error;

   if (!eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, g_egl_ctx))
      goto error;

   glClear(GL_COLOR_BUFFER_BIT);
   eglSwapBuffers(g_egl_dpy, g_egl_surf);

   g_bo = gbm_surface_lock_front_buffer(g_gbm_surface);
   fb = drm_fb_get_from_bo(g_bo);

   ret = drmModeSetCrtc(g_drm_fd, g_crtc_id, fb->fb_id, 0, 0, &g_connector_id, 1, g_drm_mode);
   if (ret < 0)
      goto error;

   g_inited = true;
   return true;

error:
   gfx_ctx_drm_egl_destroy(data);
   return false;
}

static void gfx_ctx_drm_egl_destroy(void *data)
{
   (void)data;
   // Make sure we acknowledge all page-flips.
   if (waiting_for_flip)
      wait_flip(true);

   if (g_egl_dpy)
   {
      if (g_egl_ctx)
      {
         eglMakeCurrent(g_egl_dpy, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
         eglDestroyContext(g_egl_dpy, g_egl_ctx);
      }

      if (g_egl_hw_ctx)
         eglDestroyContext(g_egl_dpy, g_egl_hw_ctx);

      if (g_egl_surf)
         eglDestroySurface(g_egl_dpy, g_egl_surf);
      eglTerminate(g_egl_dpy);
   }

   // Be as careful as possible in deinit.
   // If we screw up, the KMS tty will not restore.

   g_egl_ctx     = NULL;
   g_egl_hw_ctx  = NULL;
   g_egl_surf    = NULL;
   g_egl_dpy     = NULL;
   g_config      = 0;

   // Restore original CRTC.
   if (g_orig_crtc)
   {
      drmModeSetCrtc(g_drm_fd, g_orig_crtc->crtc_id,
            g_orig_crtc->buffer_id,
            g_orig_crtc->x,
            g_orig_crtc->y,
            &g_connector_id, 1, &g_orig_crtc->mode);
   }

   free_drm_resources();

   g_drm_mode = NULL;

   g_quit         = 0;
   g_crtc_id      = 0;
   g_connector_id = 0;

   g_fb_width  = 0;
   g_fb_height = 0;

   g_bo      = NULL;
   g_next_bo = NULL;

   g_inited = false;
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
   (void)data;
   return g_inited;
}

static bool gfx_ctx_drm_egl_has_windowed(void *data)
{
   (void)data;
   return false;
}

static gfx_ctx_proc_t gfx_ctx_drm_egl_get_proc_address(const char *symbol)
{
   return eglGetProcAddress(symbol);
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
         return false;
   }
}

static void gfx_ctx_drm_egl_bind_hw_render(void *data, bool enable)
{
   (void)data;
   g_use_hw_ctx = enable;
   if (g_egl_dpy && g_egl_surf)
      eglMakeCurrent(g_egl_dpy, g_egl_surf, g_egl_surf, enable ? g_egl_hw_ctx : g_egl_ctx);
}

const gfx_ctx_driver_t gfx_ctx_drm_egl = {
   gfx_ctx_drm_egl_init,
   gfx_ctx_drm_egl_destroy,
   gfx_ctx_drm_egl_bind_api,
   gfx_ctx_drm_egl_swap_interval,
   gfx_ctx_drm_egl_set_video_mode,
   gfx_ctx_drm_egl_get_video_size,
   NULL,
   gfx_ctx_drm_egl_update_window_title,
   gfx_ctx_drm_egl_check_window,
   gfx_ctx_drm_egl_set_resize,
   gfx_ctx_drm_egl_has_focus,
   gfx_ctx_drm_egl_has_windowed,
   gfx_ctx_drm_egl_swap_buffers,
   gfx_ctx_drm_egl_input_driver,
   gfx_ctx_drm_egl_get_proc_address,
   NULL,
   NULL,
   NULL,
   "kms-egl",
   gfx_ctx_drm_egl_bind_hw_render,
};


/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
 *  Copyright (C) 2020      - OtherCrashOverride (libgo2 display code,
 *                            LGPL-2.1-or-later, taken in under the GPL)
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

/* KMS/DRM context for the ODROID-GO Advance and the other RK3326
 * handhelds whose panel scans out portrait under a landscape picture.
 *
 * GL renders into a landscape GBM surface. Each frame Rockchip's RGA
 * scales and rotates the frame's buffer straight into one of three
 * portrait dumb buffers, and that buffer is page-flipped onto the CRTC.
 * The GPU's buffer is read once by the RGA and never by the CPU: no
 * copy, map or readback anywhere on the path.
 *
 * There is no thread, queue or lock. The page flip is asynchronous in
 * the kernel already, so the only wait is for the previous flip to land
 * before the next one is queued, which is the vsync throttle itself;
 * the RGA work for the new frame runs before that wait, under it.
 * Everything - EGL, GBM, the three scanout buffers and their prime fds,
 * each GBM buffer's prime fd - is made once and nothing is allocated
 * per frame. */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <unistd.h>
#include <fcntl.h>
#include <poll.h>

#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm/drm_fourcc.h>
#include <gbm.h>
#include <rga/RgaApi.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include <string/stdstring.h>

#include "../../configuration.h"
#include "../../verbosity.h"
#include "../../frontend/frontend_driver.h"
#include "../../retroarch.h"
#include "../video_driver.h"

#ifdef HAVE_EGL
#include "../common/egl_common.h"
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#include "../common/gl_common.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#ifdef HAVE_OPENGLES

#ifndef EGL_OPENGL_ES3_BIT_KHR
#define EGL_OPENGL_ES3_BIT_KHR 0x0040
#endif

#endif

#ifndef EGL_PLATFORM_GBM_KHR
#define EGL_PLATFORM_GBM_KHR 0x31D7
#endif

#ifndef EGL_CONTEXT_CLIENT_VERSION
#define EGL_CONTEXT_CLIENT_VERSION 0x3098
#endif

/* Scanout buffers: one on screen, one queued to flip, one the RGA
 * writes. */
#define GO2_SCANOUT_COUNT  3
/* GBM surfaces hand out two to four buffers; more than this is served
 * by a per-frame prime fd instead of failing. */
#define GO2_GBM_SLOTS      4
/* Bicubic coefficient set for the RGA scaler (Hermite), as before. */
#define GO2_RGA_SCALE_MODE 2
/* Bound on one wait for a flip, so a lost event cannot hang. */
#define GO2_FLIP_TIMEOUT_MS 100

typedef struct go2_scanout
{
   uint32_t handle;     /* dumb buffer GEM handle */
   uint32_t fb_id;
   int      fd;         /* prime fd, for the RGA */
   unsigned clear_gen;  /* rect_gen its borders were cleared for */
} go2_scanout_t;

typedef struct go2_gbm_slot
{
   int fd;              /* prime fd of the GBM buffer, -1 when free */
} go2_gbm_slot_t;

typedef struct gfx_ctx_go2_drm_data
{
#ifdef HAVE_EGL
   egl_ctx_data_t egl;
#endif
   drmModeModeInfo mode;
   drmModeCrtc *orig_crtc;
   struct gbm_device *gbm_dev;
   struct gbm_surface *gbm_surface;
   go2_scanout_t scanout[GO2_SCANOUT_COUNT];
   go2_gbm_slot_t gbm_slot[GO2_GBM_SLOTS];
   int fd;
   uint32_t connector_id;
   uint32_t crtc_id;
   int scan_wstride;    /* scanout row, in pixels */
   int src_rk_format;   /* RGA format of the GBM surface */
   /* Packed (VIDEO_SCALE_PACK) sizes: the landscape picture the panel
    * shows, and the frame GL draws into it. */
   uint32_t native_dims;
   uint32_t fb_dims;
   unsigned ctx_h;
   /* Packed landscape x/width of the picture on the panel; a change
    * bumps rect_gen so each scanout buffer clears its borders once. */
   uint32_t dst_xw;
   unsigned rect_gen;
   unsigned back;       /* scanout buffer the next frame goes into */
   int interval;
   bool flip_pending;
   bool crtc_set;
   bool no_flip;        /* the driver refused page flips */
   bool core_hw_context_enable;
} gfx_ctx_go2_drm_data_t;

/* TODO/FIXME - static global */
static enum gfx_ctx_api drm_api           = GFX_CTX_NONE;

static void go2_page_flip_handler(int fd, unsigned frame,
      unsigned sec, unsigned usec, void *data)
{
   ((gfx_ctx_go2_drm_data_t*)data)->flip_pending = false;
}

static drmEventContext go2_evctx = {
   2,                      /* DRM_EVENT_CONTEXT_VERSION for this handler */
   NULL,                   /* vblank_handler */
   go2_page_flip_handler
};

/* Waits up to timeout_ms (0 polls) for the queued flip to land.
 * Returns true once nothing is pending. */
static bool go2_wait_flip(gfx_ctx_go2_drm_data_t *drm, int timeout_ms)
{
   while (drm->flip_pending)
   {
      int ret;
      struct pollfd p;
      p.fd      = drm->fd;
      p.events  = POLLIN;
      p.revents = 0;
      ret       = poll(&p, 1, timeout_ms);
      if (ret < 0 && errno == EINTR)
         continue;
      if (ret <= 0)
      {
         /* A timed-out wait gives the flip up for lost; a poll leaves
          * it pending. */
         if (timeout_ms > 0)
            drm->flip_pending = false;
         break;
      }
      drmHandleEvent(drm->fd, &go2_evctx);
   }
   return !drm->flip_pending;
}

static void go2_set_crtc(gfx_ctx_go2_drm_data_t *drm, uint32_t fb_id)
{
   if (drmModeSetCrtc(drm->fd, drm->crtc_id, fb_id, 0, 0,
            &drm->connector_id, 1, &drm->mode))
      RARCH_ERR("[KMS] drmModeSetCrtc failed.\n");
   drm->crtc_set = true;
}

static void go2_present(gfx_ctx_go2_drm_data_t *drm, uint32_t fb_id)
{
   if (!drm->crtc_set || drm->no_flip)
   {
      go2_set_crtc(drm, fb_id);
      return;
   }

   if (!drmModePageFlip(drm->fd, drm->crtc_id, fb_id,
            DRM_MODE_PAGE_FLIP_EVENT, drm))
      drm->flip_pending = true;
   else if (errno != EBUSY)
   {
      RARCH_WARN("[KMS] Page flip refused, presenting with drmModeSetCrtc.\n");
      drm->no_flip = true;
      go2_set_crtc(drm, fb_id);
   }
}

static bool go2_find_output(gfx_ctx_go2_drm_data_t *drm)
{
   int i;
   drmModeConnector *conn = NULL;
   drmModeEncoder   *enc  = NULL;
   drmModeRes       *res  = drmModeGetResources(drm->fd);

   if (!res)
      return false;

   for (i = 0; i < res->count_connectors; i++)
   {
      conn = drmModeGetConnector(drm->fd, res->connectors[i]);
      if (conn && conn->connection == DRM_MODE_CONNECTED)
         break;
      if (conn)
         drmModeFreeConnector(conn);
      conn = NULL;
   }

   if (!conn)
      goto error;

   for (i = 0; i < conn->count_modes; i++)
      if (conn->modes[i].type & DRM_MODE_TYPE_PREFERRED)
         break;
   if (i == conn->count_modes)
      goto error;

   drm->mode         = conn->modes[i];
   drm->connector_id = conn->connector_id;

   if (!(enc = drmModeGetEncoder(drm->fd, conn->encoder_id)))
      goto error;
   drm->crtc_id      = enc->crtc_id;

   drmModeFreeEncoder(enc);
   drmModeFreeConnector(conn);
   drmModeFreeResources(res);
   return true;

error:
   if (conn)
      drmModeFreeConnector(conn);
   drmModeFreeResources(res);
   return false;
}

static void go2_scanout_free(gfx_ctx_go2_drm_data_t *drm)
{
   int i;
   for (i = 0; i < GO2_SCANOUT_COUNT; i++)
   {
      go2_scanout_t *s = &drm->scanout[i];
      if (s->fd >= 0)
         close(s->fd);
      if (s->fb_id)
         drmModeRmFB(drm->fd, s->fb_id);
      if (s->handle)
      {
         struct drm_mode_destroy_dumb d;
         d.handle = s->handle;
         drmIoctl(drm->fd, DRM_IOCTL_MODE_DESTROY_DUMB, &d);
      }
      s->fd     = -1;
      s->fb_id  = 0;
      s->handle = 0;
   }
}

static bool go2_scanout_init(gfx_ctx_go2_drm_data_t *drm)
{
   int i;
   unsigned w = drm->mode.hdisplay;
   unsigned h = drm->mode.vdisplay;

   for (i = 0; i < GO2_SCANOUT_COUNT; i++)
   {
      uint32_t handles[4], pitches[4], offsets[4];
      struct drm_mode_create_dumb c;
      go2_scanout_t *s = &drm->scanout[i];

      memset(&c, 0, sizeof(c));
      c.width  = w;
      c.height = h;
      c.bpp    = 16; /* DRM_FORMAT_RGB565 */
      if (drmIoctl(drm->fd, DRM_IOCTL_MODE_CREATE_DUMB, &c) < 0)
         return false;
      s->handle         = c.handle;
      drm->scan_wstride = c.pitch / 2;

      handles[0] = c.handle; handles[1] = handles[2] = handles[3] = 0;
      pitches[0] = c.pitch;  pitches[1] = pitches[2] = pitches[3] = 0;
      offsets[0] = offsets[1] = offsets[2] = offsets[3] = 0;
      if (drmModeAddFB2(drm->fd, w, h, DRM_FORMAT_RGB565,
               handles, pitches, offsets, &s->fb_id, 0))
         return false;
      if (drmPrimeHandleToFD(drm->fd, c.handle,
               DRM_RDWR | DRM_CLOEXEC, &s->fd) < 0)
         return false;
      /* rect_gen starts at 1: every buffer clears before first use. */
      s->clear_gen = 0;
   }
   return true;
}

/* GBM frees a buffer's user data with it, which is where its fd is
 * given back. */
static void go2_gbm_slot_release(struct gbm_bo *bo, void *data)
{
   go2_gbm_slot_t *slot = (go2_gbm_slot_t*)data;
   if (slot->fd >= 0)
      close(slot->fd);
   slot->fd = -1;
}

/* The GBM buffer's prime fd: exported the first time the buffer comes
 * round and kept with it. *temp is set when the slots ran out and the
 * fd is the caller's to close. */
static int go2_gbm_fd(gfx_ctx_go2_drm_data_t *drm, struct gbm_bo *bo,
      bool *temp)
{
   int i, fd                 = -1;
   go2_gbm_slot_t *slot      = (go2_gbm_slot_t*)gbm_bo_get_user_data(bo);

   *temp = false;
   if (slot)
      return slot->fd;

   if (drmPrimeHandleToFD(drm->fd, gbm_bo_get_handle(bo).u32,
            DRM_CLOEXEC, &fd) < 0)
      return -1;

   for (i = 0; i < GO2_GBM_SLOTS; i++)
   {
      if (drm->gbm_slot[i].fd < 0)
      {
         drm->gbm_slot[i].fd = fd;
         gbm_bo_set_user_data(bo, &drm->gbm_slot[i], go2_gbm_slot_release);
         return fd;
      }
   }

   *temp = true;
   return fd;
}

static void go2_drm_free(gfx_ctx_go2_drm_data_t *drm)
{
   go2_wait_flip(drm, GO2_FLIP_TIMEOUT_MS);

#ifdef HAVE_EGL
   if (drm->egl.dpy)
      egl_destroy(&drm->egl);
#endif
   /* Gives the GBM buffers' fds back through go2_gbm_slot_release. */
   if (drm->gbm_surface)
      gbm_surface_destroy(drm->gbm_surface);
   if (drm->gbm_dev)
      gbm_device_destroy(drm->gbm_dev);

   if (drm->orig_crtc)
   {
      drmModeSetCrtc(drm->fd, drm->orig_crtc->crtc_id,
            drm->orig_crtc->buffer_id,
            drm->orig_crtc->x, drm->orig_crtc->y,
            &drm->connector_id, 1, &drm->orig_crtc->mode);
      drmModeFreeCrtc(drm->orig_crtc);
   }

   go2_scanout_free(drm);
   close(drm->fd);
   free(drm);
}

static void gfx_ctx_go2_drm_input_driver(void *data,
      const char *joypad_name)
{
   /* no input driver of this driver's own: there is no window system
    * here, and the frontend starts the one that goes with that */
   input_driver_video_window(INPUT_WINDOW_KMS, NULL);
}

static void *gfx_ctx_go2_drm_init(void *video_driver)
{
   int i;
   gfx_ctx_go2_drm_data_t *drm = (gfx_ctx_go2_drm_data_t*)
      calloc(1, sizeof(gfx_ctx_go2_drm_data_t));

   if (!drm)
      return NULL;

   for (i = 0; i < GO2_SCANOUT_COUNT; i++)
      drm->scanout[i].fd = -1;
   for (i = 0; i < GO2_GBM_SLOTS; i++)
      drm->gbm_slot[i].fd = -1;
   drm->rect_gen = 1;
   drm->interval = 1;

   if ((drm->fd = open("/dev/dri/card0", O_RDWR | O_CLOEXEC)) < 0)
   {
      free(drm);
      return NULL;
   }

   /* The panel is portrait; the picture on it is landscape. */
   if (     !go2_find_output(drm)
         /* This driver should only be used on rotated screens */
         || drm->mode.vdisplay < drm->mode.hdisplay)
   {
      close(drm->fd);
      free(drm);
      return NULL;
   }

   drm->native_dims = VIDEO_SCALE_PACK(drm->mode.vdisplay,
         drm->mode.hdisplay);
   drm->fb_dims     = drm->native_dims;
   drm->orig_crtc   = drmModeGetCrtc(drm->fd, drm->crtc_id);

   if (     !go2_scanout_init(drm)
         || !(drm->gbm_dev = gbm_create_device(drm->fd)))
   {
      go2_drm_free(drm);
      return NULL;
   }

   return drm;
}

static void gfx_ctx_go2_drm_destroy(void *data)
{
   if (data)
      go2_drm_free((gfx_ctx_go2_drm_data_t*)data);
}

static enum gfx_ctx_api gfx_ctx_go2_drm_get_api(void *data) { return drm_api; }

static bool gfx_ctx_go2_drm_bind_api(void *video_driver,
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

static void gfx_ctx_go2_drm_swap_interval(void *data, int interval)
{
   gfx_ctx_go2_drm_data_t *drm = (gfx_ctx_go2_drm_data_t*)data;
   /* 0 drops a frame rather than wait for the flip; anything else
    * waits for one vblank. */
   if (drm)
      drm->interval = interval;
}

#ifdef HAVE_EGL
/* 8-bit RGB whose GBM format the RGA reads. */
static bool go2_accept_config_cb(void *display_data, EGLDisplay dpy,
      EGLConfig config)
{
   EGLint r, g, b, id;

   if (     !egl_get_config_attrib(dpy, config, EGL_RED_SIZE,   &r)
         || !egl_get_config_attrib(dpy, config, EGL_GREEN_SIZE, &g)
         || !egl_get_config_attrib(dpy, config, EGL_BLUE_SIZE,  &b)
         || !egl_get_config_attrib(dpy, config, EGL_NATIVE_VISUAL_ID, &id))
      return false;

   return r == 8 && g == 8 && b == 8
      && (id == GBM_FORMAT_ARGB8888 || id == GBM_FORMAT_XRGB8888);
}

static bool go2_create_context(gfx_ctx_go2_drm_data_t *drm,
      unsigned ctx_w, unsigned ctx_h)
{
   EGLint major, minor, n, id;
   EGLint ctx_attribs[8];
   EGLint *attr                     = ctx_attribs;
   const EGLint *cfg_attribs        = NULL;
   static const EGLint attribs_gl[] = {
      EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
      EGL_RED_SIZE,        8,
      EGL_GREEN_SIZE,      8,
      EGL_BLUE_SIZE,       8,
      EGL_DEPTH_SIZE,      0,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
      EGL_NONE
   };
#ifdef HAVE_OPENGLES
   static const EGLint attribs_gles3[] = {
      EGL_SURFACE_TYPE,    EGL_WINDOW_BIT,
      EGL_RED_SIZE,        8,
      EGL_GREEN_SIZE,      8,
      EGL_BLUE_SIZE,       8,
      EGL_DEPTH_SIZE,      0,
      EGL_RENDERABLE_TYPE, EGL_OPENGL_ES3_BIT_KHR,
      EGL_NONE
   };
#endif

   if (drm_api == GFX_CTX_OPENGL_ES_API)
   {
#ifdef HAVE_OPENGLES
      /* GLES 3, which is what this context has always been. */
      cfg_attribs = attribs_gles3;
      *attr++     = EGL_CONTEXT_CLIENT_VERSION;
      *attr++     = 3;
#endif
   }
   else if (drm_api == GFX_CTX_OPENGL_API)
   {
      cfg_attribs = attribs_gl;
#ifdef EGL_KHR_create_context
      if (g_egl_major * 1000 + g_egl_minor >= 3001)
      {
         *attr++ = EGL_CONTEXT_MAJOR_VERSION_KHR;
         *attr++ = g_egl_major;
         *attr++ = EGL_CONTEXT_MINOR_VERSION_KHR;
         *attr++ = g_egl_minor;
      }
#endif
   }
   *attr = EGL_NONE;

   if (!cfg_attribs)
      return false;

   if (!egl_init_context(&drm->egl, EGL_PLATFORM_GBM_KHR,
            (EGLNativeDisplayType)drm->gbm_dev, &major, &minor, &n,
            cfg_attribs, go2_accept_config_cb))
      return false;

   if (!egl_get_native_visual_id(&drm->egl, &id))
      return false;
   /* Both read as BGRA to the RGA; alpha is ignored. */
   drm->src_rk_format = RK_FORMAT_BGRA_8888;

   if (!(drm->gbm_surface = gbm_surface_create(drm->gbm_dev,
               ctx_w, ctx_h, (uint32_t)id,
               GBM_BO_USE_SCANOUT | GBM_BO_USE_RENDERING)))
      return false;

   if (!egl_create_context(&drm->egl,
            (attr != ctx_attribs) ? ctx_attribs : NULL))
      return false;

   return egl_create_surface(&drm->egl,
         (EGLNativeWindowType)drm->gbm_surface);
}
#endif

static bool gfx_ctx_go2_drm_set_video_mode(void *data,
      unsigned dims,
      bool fullscreen)
{
   video_driver_state_t *video_st       = video_state_get_ptr();
   struct retro_system_av_info *av_info = &video_st->av_info;
   gfx_ctx_go2_drm_data_t *drm          = (gfx_ctx_go2_drm_data_t*)data;

   if (!drm)
      return false;

   frontend_driver_install_signal_handler();

#ifdef HAVE_MENU
   if (      config_get_ptr()->bools.video_ctx_scaling
         && !(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
      drm->fb_dims = VIDEO_SCALE_PACK(av_info->geometry.base_width,
            av_info->geometry.base_height);
   else
#endif
      drm->fb_dims = drm->native_dims;

#ifdef HAVE_EGL
   if (!drm->gbm_surface)
   {
      unsigned ctx_w = MAX(av_info->geometry.max_width,
            VIDEO_SCALE_W(drm->native_dims));
      unsigned ctx_h = MAX(av_info->geometry.max_height,
            VIDEO_SCALE_H(drm->native_dims));

      if (!go2_create_context(drm, ctx_w, ctx_h))
      {
         RARCH_ERR("[KMS] Failed to create the EGL/GBM context.\n");
         return false;
      }
      drm->ctx_h = ctx_h;
   }
#endif

#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
   gl_clear();
#endif

   return true;
}

static void gfx_ctx_go2_drm_get_video_size(void *data,
unsigned *dims)
{
   gfx_ctx_go2_drm_data_t *drm = (gfx_ctx_go2_drm_data_t*)data;

   if (!drm)
      return;

   *dims = drm->fb_dims;
}

static void gfx_ctx_go2_drm_check_window(void *data, bool *quit,
      bool *resize, unsigned *dims)
{
   uint32_t want;
   gfx_ctx_go2_drm_data_t
      *drm              = (gfx_ctx_go2_drm_data_t*)data;
#ifdef HAVE_MENU
   settings_t *settings = config_get_ptr();
   bool use_ctx_scaling = settings->bools.video_ctx_scaling;

   if (      use_ctx_scaling
         && !(menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE))
   {
      video_driver_state_t *video_st       = video_state_get_ptr();
      struct retro_system_av_info *av_info = &video_st->av_info;
      want = VIDEO_SCALE_PACK(av_info->geometry.base_width,
            av_info->geometry.base_height);
   }
   else
#endif
      want = drm->native_dims;

   if (*dims != want)
   {
      *dims        = drm->fb_dims = want;
      *resize      = false;
   }
   *quit           = (bool)frontend_driver_get_signal_handler_state();
}

static bool gfx_ctx_go2_drm_has_focus(void *data) { return true; }
static bool gfx_ctx_go2_drm_suppress_screensaver(void *data, bool enable) { return false; }

static void gfx_ctx_go2_drm_swap_buffers(void *data)
{
#ifdef HAVE_EGL
   rga_info_t src, dst;
   bool temp_fd;
   int src_fd;
   uint32_t dst_xw;
   go2_scanout_t *s;
   struct gbm_bo *bo;
   gfx_ctx_go2_drm_data_t
      *drm   = (gfx_ctx_go2_drm_data_t*)data;
   int nat_w = VIDEO_SCALE_W(drm->native_dims);
   int nat_h = VIDEO_SCALE_H(drm->native_dims);
   int src_w = VIDEO_SCALE_W(drm->fb_dims);
   int src_h = VIDEO_SCALE_H(drm->fb_dims);
   int out_w = nat_w;
   int out_x = 0;

   if (nat_w != src_w || nat_h != src_h)
   {
      out_w = (int)(nat_h * video_driver_get_aspect_ratio());
      if (out_w > nat_w)
         out_w = nat_w;
      out_x = (nat_w - out_w) / 2;
   }

   egl_swap_buffers(&drm->egl);

   if (!(bo = gbm_surface_lock_front_buffer(drm->gbm_surface)))
      return;

   /* Vsync off: with the last flip still queued, this frame is not
    * shown rather than waited for. The buffer it would have gone into
    * stays free for the next one. */
   if (     drm->interval == 0
         && drm->flip_pending
         && !go2_wait_flip(drm, 0))
   {
      gbm_surface_release_buffer(drm->gbm_surface, bo);
      return;
   }

   if ((src_fd = go2_gbm_fd(drm, bo, &temp_fd)) < 0)
   {
      gbm_surface_release_buffer(drm->gbm_surface, bo);
      return;
   }

   s      = &drm->scanout[drm->back];
   dst_xw = ((uint32_t)out_x << 16) | (uint32_t)out_w;
   if (dst_xw != drm->dst_xw)
   {
      drm->dst_xw = dst_xw;
      drm->rect_gen++;
   }

   memset(&dst, 0, sizeof(dst));
   dst.fd           = s->fd;
   dst.mmuFlag      = 1;
   dst.rect.wstride = drm->scan_wstride;
   dst.rect.hstride = drm->mode.vdisplay;
   dst.rect.format  = RK_FORMAT_RGB_565;

   /* Only the borders beside the picture need black, and only once per
    * buffer after the picture moves: the blit covers the rest. */
   if (s->clear_gen != drm->rect_gen)
   {
      if (out_w != nat_w)
      {
         dst.rect.width  = drm->mode.hdisplay;
         dst.rect.height = drm->mode.vdisplay;
         dst.color       = 0;
         c_RkRgaColorFill(&dst);
      }
      s->clear_gen = drm->rect_gen;
   }

   /* Landscape -> portrait: the picture's x runs down the panel. */
   dst.rect.xoffset = 0;
   dst.rect.yoffset = out_x;
   dst.rect.width   = nat_h;
   dst.rect.height  = out_w;

   memset(&src, 0, sizeof(src));
   src.fd           = src_fd;
   src.mmuFlag      = 1;
   src.rotation     = HAL_TRANSFORM_ROT_270;
   src.scale_mode   = GO2_RGA_SCALE_MODE;
   src.rect.xoffset = 0;
   src.rect.yoffset = drm->ctx_h - src_h;
   src.rect.width   = src_w;
   src.rect.height  = src_h;
   src.rect.wstride = gbm_bo_get_stride(bo) / 4;
   src.rect.hstride = gbm_bo_get_height(bo);
   src.rect.format  = drm->src_rk_format;

   if (c_RkRgaBlit(&src, &dst, NULL))
      RARCH_ERR("[KMS] c_RkRgaBlit failed.\n");

   /* The blit is synchronous: GL can have its buffer back now. */
   if (temp_fd)
      close(src_fd);
   gbm_surface_release_buffer(drm->gbm_surface, bo);

   /* The RGA work above ran while the last flip was landing. */
   go2_wait_flip(drm, GO2_FLIP_TIMEOUT_MS);
   go2_present(drm, s->fb_id);
   drm->back = (drm->back + 1) % GO2_SCANOUT_COUNT;
#endif
}

static uint32_t gfx_ctx_go2_drm_get_flags(void *data)
{
   uint32_t             flags = 0;
   gfx_ctx_go2_drm_data_t    *drm = (gfx_ctx_go2_drm_data_t*)data;

   BIT32_SET(flags, GFX_CTX_FLAGS_CUSTOMIZABLE_SWAPCHAIN_IMAGES);

   if (drm->core_hw_context_enable)
      BIT32_SET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT);

   if (string_is_equal(video_driver_get_ident(), "glcore"))
   {
#if defined(HAVE_SLANG) && (defined(HAVE_SPIRV_CROSS) || defined(HAVE_RSLC))
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif
   }
   else
   {
#ifdef HAVE_GLSL
      BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_GLSL);
#endif
   }

   return flags;
}

static void gfx_ctx_go2_drm_set_flags(void *data, uint32_t flags)
{
   gfx_ctx_go2_drm_data_t *drm     = (gfx_ctx_go2_drm_data_t*)data;
   if (BIT32_GET(flags, GFX_CTX_FLAGS_GL_CORE_CONTEXT))
      drm->core_hw_context_enable = true;
}

static void gfx_ctx_go2_drm_bind_hw_render(void *data, bool enable)
{
#ifdef HAVE_EGL
   gfx_ctx_go2_drm_data_t *drm = (gfx_ctx_go2_drm_data_t*)data;
   egl_bind_hw_render(&drm->egl, enable);
#endif
}

const gfx_ctx_driver_t gfx_ctx_go2_drm = {
   gfx_ctx_go2_drm_init,
   gfx_ctx_go2_drm_destroy,
   gfx_ctx_go2_drm_get_api,
   gfx_ctx_go2_drm_bind_api,
   gfx_ctx_go2_drm_swap_interval,
   gfx_ctx_go2_drm_set_video_mode,
   gfx_ctx_go2_drm_get_video_size,
   NULL, /* refresh_rate - handled by display server */
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_metrics */
   NULL,
   NULL, /* update_title */
   gfx_ctx_go2_drm_check_window,
   NULL, /* set_resize */
   gfx_ctx_go2_drm_has_focus,
   gfx_ctx_go2_drm_suppress_screensaver,
   false, /* has_windowed */
   gfx_ctx_go2_drm_swap_buffers,
   gfx_ctx_go2_drm_input_driver,
#ifdef HAVE_EGL
   egl_get_proc_address,
#else
   NULL,
#endif
   NULL,
   NULL,
   NULL,
   "kms",
   gfx_ctx_go2_drm_get_flags,
   gfx_ctx_go2_drm_set_flags,
   gfx_ctx_go2_drm_bind_hw_render,
   NULL,
   NULL,
   NULL, /* create_surface */
   NULL  /* destroy_surface */
};

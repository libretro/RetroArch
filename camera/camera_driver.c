/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2021 - Daniel De Matteis
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

#include <libretro.h>
#include <string/stdstring.h>
#include <compat/strl.h>

#include "../configuration.h"
#include "../driver.h"
#include "../list_special.h"
#include "../runloop.h"
#include "../verbosity.h"

#include "camera_driver.h"

#if defined(HAVE_FFMPEG) && defined(HAVE_AVFORMAT) && defined(HAVE_AVCODEC) && \
   defined(HAVE_AVDEVICE) && defined(HAVE_AVUTIL) && defined(HAVE_SWSCALE)
/* FFMPEG consists of several libraries, and the camera driver needs most of them.
 * The camera driver uses APIs introduced in avformat 58 (ffmpeg 4.0). */
#include <libavformat/version.h>
#if LIBAVFORMAT_VERSION_MAJOR >= 58
#define HAVE_FFMPEG_CAMERA
#endif
#endif

static void *nullcamera_init(const char *device, uint64_t caps,
      unsigned dims) { return (void*)-1; }
static void nullcamera_free(void *data) { }
static void nullcamera_stop(void *data) { }
static bool nullcamera_start(void *data) { return true; }
static bool nullcamera_poll(void *a,
      retro_camera_frame_raw_framebuffer_t b,
      retro_camera_frame_opengl_texture_t c,
      retro_camera_frame_planar_t d) { return true; }

static camera_driver_t camera_null = {
   nullcamera_init,
   nullcamera_free,
   nullcamera_start,
   nullcamera_stop,
   nullcamera_poll,
   "null",
};

const camera_driver_t *camera_drivers[] = {
#ifdef HAVE_V4L2
   &camera_v4l2,
#endif
#if defined(HAVE_PIPEWIRE) && defined(HAVE_PIPEWIRE_STABLE)
   &camera_pipewire,
#endif
#ifdef __EMSCRIPTEN__
   &camera_rwebcam,
#endif
#ifdef ANDROID
   &camera_android,
#endif
#ifdef HAVE_AVF
   &camera_avfoundation,
#endif
#ifdef HAVE_FFMPEG_CAMERA
   &camera_ffmpeg,
#endif
   &camera_null,
   NULL,
};

static camera_driver_state_t camera_driver_st;

camera_driver_state_t *camera_state_get_ptr(void)
{
   return &camera_driver_st;
}

/**
 * config_get_camera_driver_options:
 *
 * Get an enumerated list of all camera driver names,
 * separated by '|'.
 *
 * Returns: string listing of all camera driver names,
 * separated by '|'.
 **/
const char *config_get_camera_driver_options(void)
{
   return char_list_new_special(STRING_LIST_CAMERA_DRIVERS, NULL);
}

bool driver_camera_start(void)
{
   camera_driver_state_t *camera_st = &camera_driver_st;
   if (     camera_st
         && camera_st->data
         && camera_st->driver
         && camera_st->driver->start)
   {
      settings_t *settings = config_get_ptr();
      bool camera_allow    = settings->bools.camera_allow;
      if (camera_allow)
      {
         bool ok = camera_st->driver->start(camera_st->data);
         /* The bit means "poll this every frame": up only when the
          * driver can be polled and the core gave it somewhere to
          * deliver. Everything the iterate used to re-test lives
          * here, once, at the edge. */
         runloop_frame_work_set(RUNLOOP_WORK_CAMERA,
                  ok
               && camera_st->driver->poll
               && camera_st->cb.caps);
         return ok;
      }

      runloop_msg_queue_push(
            "Camera is explicitly disabled.\n",
            STRLEN_CONST("Camera is explicitly disabled.\n"),
            1, 180, false,
            NULL, MESSAGE_QUEUE_ICON_DEFAULT, MESSAGE_QUEUE_CATEGORY_INFO);
   }
   return true;
}

/* The camera's frames, to the core when this poll is the core's and to
 * an open preview. */
static void RETRO_CALLCONV camera_dispatch_raw(const uint32_t *buf,
      unsigned width, unsigned height, size_t pitch)
{
   camera_driver_state_t *st = &camera_driver_st;
   if (st->forward_core && st->cb.frame_raw_framebuffer)
      st->cb.frame_raw_framebuffer(buf, width, height, pitch);
   if (st->preview_on && st->preview.raw)
      st->preview.raw(st->preview.user, buf, width, height, pitch);
}

/* @f turned upright into the preview's NV12 buffer, as @out: the
 * rotation is clockwise, the sample read for each one written. NULL
 * when the buffer cannot be had. */
static const struct retro_camera_planar_frame *camera_preview_upright(
      camera_driver_state_t *st, const struct retro_camera_planar_frame *f,
      struct retro_camera_planar_frame *out)
{
   unsigned rot = f->rotation;
   unsigned w   = f->width, h = f->height;
   unsigned ow  = (rot == 90 || rot == 270) ? h : w;
   unsigned oh  = (rot == 90 || rot == 270) ? w : h;
   unsigned cw  = (w + 1) >> 1, ch = (h + 1) >> 1;
   unsigned ocw = (ow + 1) >> 1, och = (oh + 1) >> 1;
   size_t   cs  = (size_t)ocw * 2;
   size_t   need = (size_t)ow * oh + cs * och;
   unsigned x, y;
   uint8_t *luma, *chroma;

   if (need > st->preview_upright_size)
   {
      uint8_t *buf = (uint8_t*)realloc(st->preview_upright, need);
      if (!buf)
         return NULL;
      st->preview_upright      = buf;
      st->preview_upright_size = need;
   }
   luma   = st->preview_upright;
   chroma = luma + (size_t)ow * oh;
   for (y = 0; y < oh; y++)
      for (x = 0; x < ow; x++)
      {
         unsigned sx, sy;
         switch (rot)
         {
            case 90:  sx = y;          sy = h - 1 - x; break;
            case 180: sx = w - 1 - x;  sy = h - 1 - y; break;
            default:  sx = w - 1 - y;  sy = x;         break;
         }
         luma[(size_t)y * ow + x] = f->planes[0][(size_t)sy * f->strides[0] + sx];
      }
   for (y = 0; y < och; y++)
      for (x = 0; x < ocw; x++)
      {
         unsigned sx, sy;
         size_t   cb, cr;
         switch (rot)
         {
            case 90:  sx = y;           sy = ch - 1 - x; break;
            case 180: sx = cw - 1 - x;  sy = ch - 1 - y; break;
            default:  sx = cw - 1 - y;  sy = x;          break;
         }
         cb = (size_t)sy * f->strides[1] + sx * f->chroma_step;
         cr = (size_t)sy * f->strides[2] + sx * f->chroma_step;
         chroma[(size_t)y * cs + 2 * x]     = f->planes[1][cb];
         chroma[(size_t)y * cs + 2 * x + 1] = f->planes[2][cr];
      }
   out->planes[0]   = luma;
   out->planes[1]   = chroma;
   out->planes[2]   = chroma + 1;
   out->strides[0]  = ow;
   out->strides[1]  = out->strides[2] = cs;
   out->chroma_step = 2;
   out->width       = ow;
   out->height      = oh;
   out->rotation    = 0;
   out->flags       = f->flags;
   return out;
}

static void RETRO_CALLCONV camera_dispatch_planar(
      const struct retro_camera_planar_frame *f)
{
   camera_driver_state_t *st = &camera_driver_st;
   if (st->forward_core && st->cb.frame_planar)
      st->cb.frame_planar(f);
   if (st->preview_on && st->preview.planar)
   {
      struct retro_camera_planar_frame upright;
      const struct retro_camera_planar_frame *p = f;
      if (     (f->rotation == 90 || f->rotation == 180 || f->rotation == 270)
            && !(p = camera_preview_upright(st, f, &upright)))
         return;
      st->preview.planar(st->preview.user, p);
   }
}

static void camera_driver_poll_with(camera_driver_state_t *st,
      const camera_driver_t *drv, void *data, bool forward)
{
   st->forward_core = forward;
   drv->poll(data,
           ((forward && st->cb.frame_raw_framebuffer) || st->preview.raw)
         ? camera_dispatch_raw : NULL,
         forward ? st->cb.frame_opengl_texture : NULL,
           ((forward && st->cb.frame_planar) || st->preview.planar)
         ? camera_dispatch_planar : NULL);
   st->forward_core = false;
}

void camera_driver_poll(void)
{
   camera_driver_state_t *st = &camera_driver_st;
   if (!st->preview_on)
   {
      st->driver->poll(st->data,
            st->cb.frame_raw_framebuffer,
            st->cb.frame_opengl_texture,
            st->cb.frame_planar);
      return;
   }
   st->core_polled = true;
   camera_driver_poll_with(st, st->driver, st->data, true);
}

void camera_driver_preview_open(const camera_preview_sink_t *sink)
{
   camera_driver_state_t *st = &camera_driver_st;
   st->preview        = *sink;
   st->preview_on     = true;
   st->preview_failed = false;
   st->core_polled    = false;
}

void camera_driver_preview_release(void)
{
   camera_driver_state_t *st = &camera_driver_st;
   if (st->preview_data && st->preview_driver)
   {
      if (st->preview_driver->stop)
         st->preview_driver->stop(st->preview_data);
      if (st->preview_driver->free)
         st->preview_driver->free(st->preview_data);
   }
   st->preview_data   = NULL;
   st->preview_driver = NULL;
}

void camera_driver_preview_close(void)
{
   camera_driver_state_t *st = &camera_driver_st;
   camera_driver_preview_release();
   free(st->preview_upright);
   st->preview_upright      = NULL;
   st->preview_upright_size = 0;
   st->preview.planar = NULL;
   st->preview.raw    = NULL;
   st->preview.user   = NULL;
   st->preview_on     = false;
   st->preview_failed = false;
}

enum camera_preview_state camera_driver_preview_poll(void)
{
   camera_driver_state_t *st = &camera_driver_st;
   settings_t *settings      = config_get_ptr();
   const char *device        = settings->arrays.camera_device;

   if (!st->preview_on)
      return CAMERA_PREVIEW_OFF;
   if (!settings->bools.camera_allow)
   {
      camera_driver_preview_release();
      return CAMERA_PREVIEW_DISALLOWED;
   }

   /* The core's camera is the core's: watched, never started or
    * stopped for it, and polled here only while the runloop is not. */
   if (st->active || st->data)
   {
      camera_driver_preview_release();
      if (!st->data || !st->driver || !st->driver->poll
            || !(runloop_state_get_ptr()->frame_work & RUNLOOP_WORK_CAMERA))
         return CAMERA_PREVIEW_CORE_IDLE;
      if (st->core_polled)
         st->core_polled = false;
      else
         camera_driver_poll_with(st, st->driver, st->data, false);
      return CAMERA_PREVIEW_SHARED;
   }

   /* A changed driver or device choice reopens. */
   if (st->preview_data
         && (  !string_is_equal(st->preview_device, device)
            || !string_is_equal(st->preview_driver->ident,
               settings->arrays.camera_driver)))
   {
      camera_driver_preview_release();
      st->preview_failed = false;
   }

   if (!st->preview_data)
   {
      const camera_driver_t *drv;
      if (st->preview_failed)
         return CAMERA_PREVIEW_FAILED;
      if (!camera_driver_find_driver("camera driver", false)
            || !(drv = st->driver) || !drv->init || !drv->poll)
      {
         st->preview_failed = true;
         return CAMERA_PREVIEW_FAILED;
      }
      strlcpy(st->preview_device, device, sizeof(st->preview_device));
      st->preview_data = drv->init(*device ? device : NULL,
              (UINT64_C(1) << RETRO_CAMERA_BUFFER_PLANAR)
            | (UINT64_C(1) << RETRO_CAMERA_BUFFER_RAW_FRAMEBUFFER), 0);
      if (!st->preview_data)
      {
         st->preview_failed = true;
         return CAMERA_PREVIEW_FAILED;
      }
      if (drv->start && !drv->start(st->preview_data))
      {
         /* Never started: freed without a stop */
         if (drv->free)
            drv->free(st->preview_data);
         st->preview_data   = NULL;
         st->preview_failed = true;
         return CAMERA_PREVIEW_FAILED;
      }
      st->preview_driver = drv;
   }

   camera_driver_poll_with(st, st->preview_driver, st->preview_data, false);
   return CAMERA_PREVIEW_OWN;
}

void driver_camera_stop(void)
{
   camera_driver_state_t *camera_st = &camera_driver_st;
   if (     camera_st->driver
         && camera_st->driver->stop
         && camera_st->data)
      camera_st->driver->stop(camera_st->data);
   runloop_frame_work_set(RUNLOOP_WORK_CAMERA, false);
}

bool camera_driver_find_driver(const char *prefix,
      bool verbosity_enabled)
{
   settings_t *settings         = config_get_ptr();
   camera_driver_state_t
      *camera_st                = &camera_driver_st;
   int i                        = (int)driver_find_index(
         "camera_driver",
         settings->arrays.camera_driver);

   if (i >= 0)
      camera_st->driver = (const camera_driver_t*)camera_drivers[i];
   else
   {
      if (verbosity_enabled)
      {
         unsigned d;
         RARCH_ERR("Couldn't find any %s named \"%s\"\n", prefix,
               settings->arrays.camera_driver);
         RARCH_LOG_OUTPUT("Available %ss are:\n", prefix);
         for (d = 0; camera_drivers[d]; d++)
         {
            if (camera_drivers[d])
            {
               RARCH_LOG_OUTPUT("\t%s\n", camera_drivers[d]->ident);
            }
         }

         RARCH_WARN("Going to default to first %s...\n", prefix);
      }

      if (!(camera_st->driver = (const camera_driver_t*)camera_drivers[0]))
         return false;
   }
   return true;
}

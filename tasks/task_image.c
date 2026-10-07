/*  RetroArch - A frontend for libretro.
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

#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include <formats/data_transfer.h>
#include <formats/image.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <features/features_cpu.h>

#include "task_file_transfer.h"
#include "tasks_internal.h"

#include "../configuration.h"
#include "../gfx/gfx_surface.h"

enum image_flags_enum
{
   /* The decode is through; the task completes once the read is too
    * (a still decoded from a prefix declares the read done itself) */
   IMAGE_FLAG_IS_FINISHED                = (1 << 0),
   /* The caller takes half floats (TASK_IMAGE_LOAD_HDR): an HDR video
    * still is asked for them where the driver offers FP16. */
   IMAGE_FLAG_WANT_HDR                   = (1 << 1)
};

struct nbio_image_handle
{
   image_loader_t *loader;
   unsigned frame_duration;
   unsigned upscale_threshold;
   unsigned downscale_cap;
   enum image_type_enum type;
   uint8_t flags;
   bool supports_rgba;
};

/* Decode-time window, shared by every image task in a tick.
 *
 * The decode steps below run against frame_duration per task, and
 * retro_task_regular_gather() runs every running task's handler once
 * per frame, so a per-task budget multiplies exactly the way the
 * file-I/O slice in task_file_transfer.c used to: eight concurrent
 * large decodes could each legitimately consume a frame of CPU in a
 * single gather.  Same fix, same shape: a shared allowance of
 * frame_duration per window, with each task keeping its first
 * iteration (the loader runs one before consulting the deadline), so
 * a queue whose window is already spent still progresses.
 *
 * The period is twice the allowance, and the ratio is load-bearing.
 * With period == allowance, a task that consumes its whole allowance
 * has by definition advanced wall time a full period, so the next
 * task in the same gather finds the window expired, resets it, and
 * takes a full allowance too: the multiplication survives intact.
 * (Caught by simulation, not inspection.  The I/O slice never hits
 * this only because its allowance is a quarter of its period.)  At
 * allowance <= period/2 a mid-gather reset is impossible - the
 * spender cannot age the window past the period on its own - while
 * gathers a frame apart still find it expired and replenished.
 *
 * A single task is unchanged: the same frame_duration per gather it
 * always had, at the same sustained rate - the elapsed frame plus
 * the allowance it spent age the window past the period by the next
 * gather.
 *
 * Declined under a threaded task queue for the reason the I/O slice
 * declines: there the handler runs on the worker thread, there is no
 * frame to protect, and a shared static across threads would be a
 * race for no benefit. */
static retro_time_t image_decode_slice_start;
static retro_time_t image_decode_slice_used;

static retro_time_t task_image_decode_slice_open(
      unsigned frame_duration, retro_time_t *start)
{
   retro_time_t now = cpu_features_get_time_usec();
   *start           = now;
   if (task_queue_is_threaded())
      return (retro_time_t)frame_duration;
   if (now - image_decode_slice_start
         >= (retro_time_t)frame_duration * 2)
   {
      image_decode_slice_start = now;
      image_decode_slice_used  = 0;
   }
   return (image_decode_slice_used < (retro_time_t)frame_duration)
      ? (retro_time_t)frame_duration - image_decode_slice_used
      : 0;
}

static void task_image_decode_slice_close(retro_time_t start)
{
   if (task_queue_is_threaded())
      return;
   image_decode_slice_used += cpu_features_get_time_usec() - start;
}

static void task_image_cleanup(nbio_handle_t *nbio)
{
   struct nbio_image_handle *image = (struct nbio_image_handle*)nbio->data;

   if (image)
   {
      image_loader_free(image->loader);
      image->loader = NULL;
   }
   if (nbio->path)
      free(nbio->path);
   if (nbio->data)
      free(nbio->data);
   /* The video early-completion path can finish the task with the
    * read still in flight; both spines cancel it before releasing -
    * for an unwanted (never-detached) thumbnail that is the pay-off:
    * the rest of the file is simply never read. */
   nbio_xfer_close(nbio);
   nbio->path        = NULL;
   nbio->data        = NULL;
}

static void task_image_load_free(retro_task_t *task)
{
   nbio_handle_t *nbio  = task ? (nbio_handle_t*)task->state : NULL;

   if (nbio)
   {
      task_image_cleanup(nbio);
      free(nbio);
   }
}

/* Open the loader over the nbio buffer and start the decode.  Called
 * from cb_nbio_image_thumbnail when the read completes, and - for the
 * types whose decoders can work against a growing buffer - early,
 * while the read is still running ('partial'): the buffer pointer is
 * stable (every backend sizes or maps it up front), the loader is
 * told how much has arrived, and nbio->is_finished is left unset so
 * the task keeps pumping the file. */
static int task_image_thumbnail_setup(nbio_handle_t *nbio, bool partial)
{
   image_texture_request_t req;
   gfx_surface_requirements_t want;
   void *ptr                       = NULL;
   size_t len                      = 0;
   size_t avail                    = 0;
   struct nbio_image_handle *image = nbio  ? (struct nbio_image_handle*)nbio->data : NULL;
   settings_t *settings            = config_get_ptr();
   float refresh_rate              = 0.0f;

   if (!image)
      return -1;

   /* The decoders bake the output channel order from the request.
    * It was captured when this load was queued, but the driver's
    * wanted order is reset on every video reinit (core start/stop),
    * so a value sampled in that window can disagree with the driver's
    * actual upload format and yield R/B-swapped images.  Re-sample it
    * here, once, at decode start (after any reinit has settled). */
   gfx_surface_query_requirements(0, &want);
   req.rgba            = want.rgba;
   image->supports_rgba = want.rgba;
   /* Native 10-bit output is worth asking the decoder for only when
    * the driver can sample it; otherwise the buffer would be narrowed
    * again at upload for nothing. The decoders emit 8-bit or 10-bit
    * today, so a driver that takes something wider still gets 10-bit
    * from here. */
   req.want_10bit      = (want.formats & GFX_SURFACE_PIXFMT_2101010) ? true : false;
   /* Half floats only for a caller that said it takes them: nothing
    * narrows them, and a consumer that reads the pixels itself (RGUI)
    * has no use for linear light. */
   req.want_fp16       = (image->flags & IMAGE_FLAG_WANT_HDR)
      && (want.formats & GFX_SURFACE_PIXFMT_FP16);
   /* A compressed payload would decode on the main thread at upload
    * for a driver that cannot sample it; this task decodes here. */
   req.want_compressed = false;

   if (!(image->loader = image_loader_new(image->type, &req)))
      return -1;

   ptr   = (void*)nbio_xfer_ptr(nbio, &len);
   avail = len;
   if (partial)
   {
      size_t done = 0, total = 0;
      nbio_xfer_progress(nbio, &done, &total);
      avail = done;
   }
   if (!image_loader_start(image->loader, ptr, len, avail))
   {
      task_image_cleanup(nbio);
      return -1;
   }

   /* Set task iteration duration */
   if (settings)
      refresh_rate = settings->floats.video_refresh_rate;

   if (refresh_rate <= 0.0f)
      refresh_rate = 60.0f;
   image->frame_duration = (unsigned)((1.0 / refresh_rate) * 1000000.0f);

   image->flags                   &= ~IMAGE_FLAG_IS_FINISHED;
   if (!partial)
      nbio->is_finished            = true;

   return 0;
}

static int cb_nbio_image_thumbnail(void *data, size_t len)
{
   nbio_handle_t *nbio             = (nbio_handle_t*)data;
   struct nbio_image_handle *image = nbio ? (struct nbio_image_handle*)nbio->data : NULL;

   if (!image)
      return -1;
   if (image->loader)
   {
      /* The early path already started the decode while the read was
       * running; the read has now completed - tell the loader the
       * whole buffer is valid and let the task's normal completion
       * gate see the read as done.  The decoder may still be mid-walk
       * (stalled at the byte frontier when the last bytes landed);
       * the next tick resumes and completes it. */
      image_loader_set_avail(image->loader, (size_t)-1);
      nbio->is_finished = true;
      return 0;
   }
   return task_image_thumbnail_setup(nbio, false);
}

bool task_image_load_handler(retro_task_t *task)
{
   uint8_t flg;
   nbio_handle_t            *nbio  = NULL;
   struct nbio_image_handle *image = NULL;

   if (!task || !task->state)
      return false;

   nbio  = (nbio_handle_t*)task->state;
   image = (struct nbio_image_handle*)nbio->data;

   if (image)
   {
      if (!image->loader)
      {
         /* Start decoding while the file is still being read where
          * the decoder can: the still usually needs only the header
          * and the first keyframe (video) or paints incrementally
          * (PNG/JPEG), so the decode overlaps the read instead of
          * waiting for the completion callback.  Cheap: two field
          * reads and a header check on the resident bytes. */
         if (nbio->xfer && !nbio->is_finished)
         {
            size_t done = 0, total = 0;
            if (     nbio_xfer_progress(nbio, &done, &total) && done > 0
                  && image_loader_ready(image->type,
                     nbio_xfer_ptr(nbio, NULL), done))
               task_image_thumbnail_setup(nbio, true);
         }
         return true;
      }

      if (!(image->flags & IMAGE_FLAG_IS_FINISHED))
      {
         retro_time_t start_time;
         retro_time_t allowance;
         enum image_loader_state st;

         /* A decoder painting from a prefix walls at the resident
          * frontier: raise it each tick as the read advances.  A no-op
          * once the read has completed.  WEBP sees the bytes it was
          * started on and no more; it has no wall. */
         if (     !nbio->is_finished && nbio->xfer
               && image->type != IMAGE_TYPE_WEBP)
         {
            size_t done = 0, total = 0;
            if (nbio_xfer_progress(nbio, &done, &total))
               image_loader_set_avail(image->loader, done);
         }

         allowance = task_image_decode_slice_open(
               image->frame_duration, &start_time);
         st        = image_loader_step(image->loader,
               cpu_features_get_time_usec, start_time + allowance);
         task_image_decode_slice_close(start_time);

         if (st == IMAGE_LOADER_ERROR)
            return false;
         if (st == IMAGE_LOADER_DONE)
         {
            /* A still decoded from a prefix needs no more of the
             * file: the task completes now and the read is cancelled
             * with it. */
            image->flags     |= IMAGE_FLAG_IS_FINISHED;
            nbio->is_finished = true;
         }
      }
   }

   flg = task_get_flags(task);

   if (     nbio->is_finished
         && (image && (image->flags & IMAGE_FLAG_IS_FINISHED))
         && ((!((flg & RETRO_TASK_FLG_CANCELLED) > 0))))
   {
      struct texture_image *img = (struct texture_image*)malloc(sizeof(struct texture_image));

      if (img)
      {
         if (image_loader_finish(image->loader, img))
            image_texture_scale(img,
                  image->upscale_threshold, image->downscale_cap);
         else
         {
            free(img);
            img = NULL;
         }
      }

      task_set_data(task, img);

      return false;
   }

   return true;
}

bool task_image_detach_video_stream(retro_task_t *task,
      void **stream, enum image_type_enum *type,
      struct data_transfer **xfer_owner, void **buf, size_t *len)
{
   nbio_handle_t *nbio;
   struct nbio_image_handle *image;
   void *s;
   void *ptr;
   size_t l                        = 0;

   if (!task || !stream || !type || !xfer_owner || !buf || !len)
      return false;
   if (!(nbio = (nbio_handle_t*)task->state))
      return false;
   /* nbio->data is only a struct nbio_image_handle for image-load
    * tasks; the audio mixer tasks share nbio_handle_t with a different
    * payload. */
   if (!BIT32_GET(nbio->status_flags, NBIO_FLAG_IMAGE_TASK))
      return false;
   if (!(image = (struct nbio_image_handle*)nbio->data))
      return false;
   if (!image->loader)
      return false;
   /* The stream borrows the transfer's buffer, so without a transfer
    * to hand over there is nothing to detach onto: leave the stream
    * attached (the loader's free closes it).  Video loads always
    * travel the data_transfer spine. */
   if (!nbio->xfer)
      return false;
   if (!(ptr = (void*)data_transfer_ptr(nbio->xfer, &l)) || !l)
      return false;

   if (!(s = image_loader_detach_anim_stream(image->loader)))
      return false;

   *stream       = s;
   *type         = image->type;
   *buf          = ptr;
   *len          = l;
   /* Hand over the transfer: the buffer the stream borrows lives
    * inside it, and its fill may still be in flight (the adopter
    * pumps it on).  task_image_cleanup tolerates the NULL. */
   *xfer_owner   = nbio->xfer;
   nbio->xfer    = NULL;
   return true;
}

int task_image_png_probe(retro_task_t *task)
{
   nbio_handle_t *nbio;
   struct nbio_image_handle *image;

   if (!task || !(nbio = (nbio_handle_t*)task->state))
      return -1;
   if (!BIT32_GET(nbio->status_flags, NBIO_FLAG_IMAGE_TASK))
      return -1;
   if (!(image = (struct nbio_image_handle*)nbio->data))
      return -1;
   /* Only a completed read is a verdict: a short or detached transfer
    * reports unknown and the caller probes the file, as it always
    * did. */
   if (!nbio->xfer || !data_transfer_complete(nbio->xfer))
      return -1;
   return image_loader_png_probe(image->loader);
}

bool task_push_image_load(const char *fullpath,
      bool supports_rgba, unsigned upscale_threshold,
      unsigned downscale_cap,
      retro_task_callback_t cb, void *user_data)
{
   return task_push_image_load_ex(fullpath,
         supports_rgba ? TASK_IMAGE_LOAD_RGBA : 0,
         upscale_threshold, downscale_cap, cb, user_data);
}

bool task_push_image_load_ex(const char *fullpath, unsigned load_flags,
      unsigned upscale_threshold, unsigned downscale_cap,
      retro_task_callback_t cb, void *user_data)
{
   bool supports_rgba = (load_flags & TASK_IMAGE_LOAD_RGBA) ? true : false;
   nbio_handle_t             *nbio   = NULL;
   struct nbio_image_handle   *image = NULL;
   retro_task_t                   *t = task_init();

   if (!t)
      return false;

   if (!(nbio = (nbio_handle_t*)malloc(sizeof(*nbio))))
   {
      free(t);
      return false;
   }

   nbio->type          = NBIO_TYPE_NONE;
   nbio->is_finished   = false;
   nbio->status        = NBIO_STATUS_INIT;
   nbio->status_flags  = 0;
   nbio->data          = NULL;
   nbio->xfer          = NULL;
   nbio->cb            = &cb_nbio_image_thumbnail;

   if (supports_rgba)
      BIT32_SET(nbio->status_flags, NBIO_FLAG_IMAGE_SUPPORTS_RGBA);
   BIT32_SET(nbio->status_flags, NBIO_FLAG_IMAGE_TASK);

   if (!(image = (struct nbio_image_handle*)malloc(sizeof(*image))))
   {
      free(nbio);
      free(t);
      return false;
   }

   nbio->path                        = strdup(fullpath);
   if (!nbio->path)
   {
      free(image);
      free(nbio);
      free(t);
      return false;
   }

   image->type                       = image_texture_get_type(fullpath);
   image->loader                     = NULL;
   image->frame_duration             = 0;
   image->upscale_threshold          = upscale_threshold;
   image->downscale_cap              = downscale_cap;

   image->flags                      = (load_flags & TASK_IMAGE_LOAD_HDR)
      ? IMAGE_FLAG_WANT_HDR : 0;

   image->supports_rgba              = supports_rgba;

   switch (image->type)
   {
      case IMAGE_TYPE_PNG:
         nbio->type = NBIO_TYPE_PNG;
         break;
      case IMAGE_TYPE_JPEG:
         nbio->type = NBIO_TYPE_JPEG;
         break;
      case IMAGE_TYPE_BMP:
         nbio->type = NBIO_TYPE_BMP;
         break;
      case IMAGE_TYPE_TGA:
         nbio->type = NBIO_TYPE_TGA;
         break;
      case IMAGE_TYPE_WEBP:
         nbio->type = NBIO_TYPE_WEBP;
         break;
      case IMAGE_TYPE_WEBM:
         nbio->type = NBIO_TYPE_WEBM;
         break;
      case IMAGE_TYPE_MP4:
         nbio->type = NBIO_TYPE_MP4;
         break;
      default:
         nbio->type = NBIO_TYPE_NONE;
         break;
   }

   nbio->data          = (struct nbio_image_handle*)image;

   t->state           = nbio;
   t->handler         = task_file_load_handler;
   t->cleanup         = task_image_load_free;
   t->callback        = cb;
   t->user_data       = user_data;

   task_queue_push(t);

   return true;
}


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
#include <math.h>

#include <file/nbio.h>
#include <formats/image.h>
#include <compat/strl.h>
#include <retro_miscellaneous.h>
#include <features/features_cpu.h>

#include "task_file_transfer.h"
#include "tasks_internal.h"

#include "../configuration.h"

enum image_status_enum
{
   IMAGE_STATUS_WAIT = 0,
   IMAGE_STATUS_TRANSFER,
   IMAGE_STATUS_TRANSFER_PARSE,
   IMAGE_STATUS_PROCESS_TRANSFER,
   IMAGE_STATUS_PROCESS_TRANSFER_PARSE
};

enum image_flags_enum
{
   IMAGE_FLAG_IS_BLOCKING                = (1 << 0),
   IMAGE_FLAG_IS_BLOCKING_ON_PROCESSING  = (1 << 1),
   IMAGE_FLAG_IS_FINISHED                = (1 << 2)
};

struct nbio_image_handle
{
   void *handle;
   transfer_cb_t  cb;
   struct texture_image ti; /* ptr alignment */
   size_t size;
   int processing_final_state;
   unsigned frame_duration;
   unsigned upscale_threshold;
   enum image_type_enum type;
   enum image_status_enum status;
   uint8_t flags;
};

#define UPSCALE_MAX_PIXELS (256u * 1024u * 1024u)

static int cb_image_upload_generic(void *data, size_t len)
{
   nbio_handle_t             *nbio = (nbio_handle_t*)data;
   struct nbio_image_handle *image = (struct nbio_image_handle*)nbio->data;

   if (!image)
      return -1;

   switch (image->processing_final_state)
   {
      case IMAGE_PROCESS_ERROR:
      case IMAGE_PROCESS_ERROR_END:
         return -1;
      default:
         break;
   }

   /* JPEG already outputs in the correct channel order (BGRA/ARGB)
    * after its internal YCbCr→RGB conversion, so skip the swap.
    * For other formats (PNG, BMP, TGA), apply R↔B conversion
    * if the platform requires RGBA byte order. */
   if (image->type != IMAGE_TYPE_JPEG)
      image_texture_color_convert(&image->ti);

   image->flags                   &= ~IMAGE_FLAG_IS_BLOCKING_ON_PROCESSING;
   image->flags                   |=  IMAGE_FLAG_IS_BLOCKING;
   image->flags                   |=  IMAGE_FLAG_IS_FINISHED;
   nbio->is_finished               = true;

   return 0;
}

static int task_image_process(
      struct nbio_image_handle *image,
      unsigned *width,
      unsigned *height)
{
   int retval;

   if (!image_transfer_is_valid(image->handle, image->type))
      return IMAGE_PROCESS_ERROR;

   if ((retval = image_transfer_process(
         image->handle,
         image->type,
         &image->ti.pixels, image->size, width, height)) == IMAGE_PROCESS_ERROR)
      return IMAGE_PROCESS_ERROR;

   image->ti.width  = *width;
   image->ti.height = *height;

   return retval;
}

static int cb_image_thumbnail(void *data, size_t len)
{
   unsigned width                   = 0;
   unsigned height                  = 0;
   nbio_handle_t        *nbio       = (nbio_handle_t*)data;
   struct nbio_image_handle *image  = (struct nbio_image_handle*)nbio->data;
   int retval                       = image ? task_image_process(image, &width, &height) : IMAGE_PROCESS_ERROR;

   if (   (retval == IMAGE_PROCESS_ERROR)
       || (retval == IMAGE_PROCESS_ERROR_END)
      )
      return -1;

   if (retval != IMAGE_PROCESS_END)
      image->flags                 |=  IMAGE_FLAG_IS_BLOCKING_ON_PROCESSING;
   else
      image->flags                 &= ~IMAGE_FLAG_IS_BLOCKING_ON_PROCESSING;
   if (retval == IMAGE_PROCESS_END)
      image->flags                 |=  IMAGE_FLAG_IS_FINISHED;
   else
      image->flags                 &= ~IMAGE_FLAG_IS_FINISHED;
   image->cb                        = &cb_image_upload_generic;

   return 0;
}

static int task_image_iterate_process_transfer(struct nbio_image_handle *image)
{
   int retval                      = 0;
   unsigned width                  = 0;
   unsigned height                 = 0;
   retro_time_t start_time         = cpu_features_get_time_usec();

   do
   {
      if ((retval = task_image_process(image, &width, &height)) 
          != IMAGE_PROCESS_NEXT)
         break;
   }while (cpu_features_get_time_usec() - start_time
         < image->frame_duration);

   if (retval == IMAGE_PROCESS_NEXT)
      return 0;

   image->processing_final_state = retval;
   return -1;
}

static void task_image_cleanup(nbio_handle_t *nbio)
{
   struct nbio_image_handle *image = (struct nbio_image_handle*)nbio->data;

   if (image)
   {
      image_transfer_free(image->handle, image->type);

      if (image->ti.pixels)
      {
         free(image->ti.pixels);
         image->ti.pixels = NULL;
      }

      image->handle  = NULL;
      image->cb      = NULL;
   }
   if (nbio->path)
      free(nbio->path);
   if (nbio->data)
      free(nbio->data);
   nbio_free(nbio->handle);
   nbio->path        = NULL;
   nbio->data        = NULL;
   nbio->handle      = NULL;
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

static int cb_nbio_image_thumbnail(void *data, size_t len)
{
   void *ptr                       = NULL;
   nbio_handle_t *nbio             = (nbio_handle_t*)data;
   struct nbio_image_handle *image = nbio  ? (struct nbio_image_handle*)nbio->data : NULL;
   void *handle                    = image ? image_transfer_new(image->type)       : NULL;
   settings_t *settings            = config_get_ptr();
   float refresh_rate              = 0.0f;

   if (!handle)
      return -1;

   image->status                   = IMAGE_STATUS_TRANSFER;
   image->handle                   = handle;
   image->cb                       = &cb_image_thumbnail;

   ptr                             = nbio_get_ptr(nbio->handle, &len);

   image_transfer_set_buffer_ptr(image->handle, image->type, ptr, len);

   /* Set image size */
   image->size                     = len;

   /* Set task iteration duration */
   if (settings)
      refresh_rate = settings->floats.video_refresh_rate;

   if (refresh_rate <= 0.0f)
      refresh_rate = 60.0f;
   image->frame_duration = (unsigned)((1.0 / refresh_rate) * 1000000.0f);

   if (!image_transfer_start(image->handle, image->type))
   {
      task_image_cleanup(nbio);
      return -1;
   }

   image->flags                   &= ~IMAGE_FLAG_IS_BLOCKING;
   image->flags                   &= ~IMAGE_FLAG_IS_FINISHED;
   nbio->is_finished               = true;

   return 0;
}

static bool upscale_image(
      unsigned scale_factor,
      struct texture_image *image_src,
      struct texture_image *image_dst)
{
   unsigned y_src;
   size_t total_pixels;

   /* Sanity check */
   if ((scale_factor < 1) || !image_src || !image_dst)
      return false;

   if (!image_src->pixels || (image_src->width < 1) || (image_src->height < 1))
      return false;

   /* Get output dimensions */
   image_dst->width  = image_src->width  * scale_factor;
   image_dst->height = image_src->height * scale_factor;

   total_pixels = (size_t)image_dst->width * (size_t)image_dst->height;
   if (total_pixels == 0 || total_pixels > UPSCALE_MAX_PIXELS)
      return false;

   /* Allocate pixel buffer */
   if (!(image_dst->pixels = (uint32_t*)calloc(total_pixels, sizeof(uint32_t))))
      return false;

   /* Fast path for integer scale factors: expand each source pixel
    * into a scale_factor-wide run, then memcpy to duplicate rows */
   for (y_src = 0; y_src < image_src->height; y_src++)
   {
      unsigned x_src;
      uint32_t *src_row     = image_src->pixels
                            + ((size_t)y_src * image_src->width);
      uint32_t *dst_first   = image_dst->pixels
                            + ((size_t)y_src * scale_factor * image_dst->width);
      size_t dst_row_bytes  = (size_t)image_dst->width * sizeof(uint32_t);

      /* Build the first scaled row by expanding each source pixel */
      for (x_src = 0; x_src < image_src->width; x_src++)
      {
         unsigned k;
         uint32_t px        = src_row[x_src];
         uint32_t *dst_px   = dst_first + (size_t)x_src * scale_factor;

         for (k = 0; k < scale_factor; k++)
            dst_px[k] = px;
      }

      /* Duplicate the first scaled row for the remaining (scale_factor-1) rows */
      {
         unsigned row_copy;
         for (row_copy = 1; row_copy < scale_factor; row_copy++)
         {
            uint32_t *dst_dup = dst_first + (size_t)row_copy * image_dst->width;
            memcpy(dst_dup, dst_first, dst_row_bytes);
         }
      }
   }

   return true;
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
      switch (image->status)
      {
         case IMAGE_STATUS_WAIT:
            return true;
         case IMAGE_STATUS_PROCESS_TRANSFER:
            if (task_image_iterate_process_transfer(image) == -1)
               image->status = IMAGE_STATUS_PROCESS_TRANSFER_PARSE;
            break;
         case IMAGE_STATUS_TRANSFER_PARSE:
            if (image->handle && image->cb)
            {
               size_t _len = 0;
               if (image->cb(nbio, _len) == -1)
                  return false;
            }
            if (image->flags & IMAGE_FLAG_IS_BLOCKING_ON_PROCESSING)
               image->status = IMAGE_STATUS_PROCESS_TRANSFER;
            break;
         case IMAGE_STATUS_TRANSFER:
            if (     !(image->flags & IMAGE_FLAG_IS_BLOCKING)
                  && !(image->flags & IMAGE_FLAG_IS_FINISHED))
            {
               retro_time_t start_time = cpu_features_get_time_usec();
               do
               {
                  if (!image_transfer_iterate(image->handle, image->type))
                  {
                     image->status = IMAGE_STATUS_TRANSFER_PARSE;
                     break;
                  }
               }while (cpu_features_get_time_usec() - start_time
                     < image->frame_duration);
            }
            break;
         case IMAGE_STATUS_PROCESS_TRANSFER_PARSE:
            if (image->handle && image->cb)
            {
               size_t _len = 0;
               if (image->cb(nbio, _len) == -1)
                  return false;
            }
            if (!(image->flags & IMAGE_FLAG_IS_FINISHED))
               break;
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
         if (image->upscale_threshold > 0)
         {
            if (   ((image->ti.width  > 0)
                &&  (image->ti.height > 0))
                && ((image->ti.width  < image->upscale_threshold)
                ||  (image->ti.height < image->upscale_threshold)))
            {
               unsigned min_size = (image->ti.width < image->ti.height)
                                  ? image->ti.width : image->ti.height;
               unsigned scale_factor_int = ((image->upscale_threshold 
               + min_size - 1) / min_size);
               struct texture_image img_resampled = {
                  NULL,
                  0,
                  0,
                  false
               };

               if (upscale_image(scale_factor_int, &image->ti, &img_resampled))
               {
                  image->ti.width  = img_resampled.width;
                  image->ti.height = img_resampled.height;

                  if (image->ti.pixels)
                     free(image->ti.pixels);
                  image->ti.pixels = img_resampled.pixels;
               }
            }
         }

         img->width         = image->ti.width;
         img->height        = image->ti.height;
         img->pixels        = image->ti.pixels;
         img->supports_rgba = image->ti.supports_rgba;

         /* Transfer pixel ownership to the output image so
          * cleanup does not double-free */
         image->ti.pixels   = NULL;
      }

      task_set_data(task, img);

      return false;
   }

   return true;
}

bool task_push_image_load(const char *fullpath,
      bool supports_rgba, unsigned upscale_threshold,
      retro_task_callback_t cb, void *user_data)
{
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
   nbio->pos_increment = 0;
   nbio->status_flags  = 0;
   nbio->data          = NULL;
   nbio->handle        = NULL;
   nbio->cb            = &cb_nbio_image_thumbnail;

   if (supports_rgba)
      BIT32_SET(nbio->status_flags, NBIO_FLAG_IMAGE_SUPPORTS_RGBA);

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
   image->status                     = IMAGE_STATUS_WAIT;
   image->processing_final_state     = 0;
   image->frame_duration             = 0;
   image->size                       = 0;
   image->upscale_threshold          = upscale_threshold;
   image->handle                     = NULL;
   image->cb                         = NULL;

   image->flags                      = 0;

   image->ti.width                   = 0;
   image->ti.height                  = 0;
   image->ti.pixels                  = NULL;
   /* NOTE: Come back to this if this causes problems */
   image->ti.supports_rgba           = supports_rgba;

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

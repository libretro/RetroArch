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
#include <errno.h>

#include <file/nbio.h>
#include <formats/image.h>
#include <compat/strl.h>
#include <string/stdstring.h>
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

struct nbio_image_handle
{
   enum image_type_enum type;
   enum image_status_enum status;
   bool is_blocking;
   bool is_blocking_on_processing;
   bool is_finished;
   int processing_final_state;
   unsigned frame_duration;
   size_t size;
   unsigned upscale_threshold;
   void *handle;
   transfer_cb_t  cb;
   struct texture_image ti;
};

static int cb_image_upload_generic(void *data, size_t len)
{
   unsigned r_shift, g_shift, b_shift, a_shift;
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

   image_texture_set_color_shifts(&r_shift, &g_shift, &b_shift,
         &a_shift, &image->ti);

   image_texture_color_convert(r_shift, g_shift, b_shift,
         a_shift, &image->ti);

   image->is_blocking_on_processing         = false;
   image->is_blocking                       = true;
   image->is_finished                       = true;
   nbio->is_finished                        = true;

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

   retval = image_transfer_process(
         image->handle,
         image->type,
         &image->ti.pixels, image->size, width, height);

   if (retval == IMAGE_PROCESS_ERROR)
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

   if ((retval == IMAGE_PROCESS_ERROR)    ||
       (retval == IMAGE_PROCESS_ERROR_END)
      )
      return -1;

   image->is_blocking_on_processing = (retval != IMAGE_PROCESS_END);
   image->is_finished               = (retval == IMAGE_PROCESS_END);
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
      retval = task_image_process(image, &width, &height);

      if (retval != IMAGE_PROCESS_NEXT)
         break;
   }
   while (cpu_features_get_time_usec() - start_time < image->frame_duration);

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

      image->handle                 = NULL;
      image->cb                     = NULL;
   }
   if (!string_is_empty(nbio->path))
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
   nbio_handle_t       *nbio  = task ? (nbio_handle_t*)task->state : NULL;

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

   image->is_blocking              = false;
   image->is_finished              = false;
   nbio->is_finished               = true;

   return 0;
}

static bool upscale_image(
      unsigned scale_factor,
      struct texture_image *image_src,
      struct texture_image *image_dst)
{
   uint32_t x_ratio, y_ratio;
   unsigned x_src, y_src;
   unsigned x_dst, y_dst;

   /* Sanity check */
   if ((scale_factor < 1) || !image_src || !image_dst)
      return false;

   if (!image_src->pixels || (image_src->width < 1) || (image_src->height < 1))
      return false;

   /* Get output dimensions */
   image_dst->width = image_src->width * scale_factor;
   image_dst->height = image_src->height * scale_factor;

   /* Allocate pixel buffer */
   image_dst->pixels = (uint32_t*)calloc(image_dst->width * image_dst->height, sizeof(uint32_t));
   if (!image_dst->pixels)
      return false;

   /* Perform nearest neighbour resampling */
   x_ratio = ((image_src->width  << 16) / image_dst->width);
   y_ratio = ((image_src->height << 16) / image_dst->height);

   for (y_dst = 0; y_dst < image_dst->height; y_dst++)
   {
      y_src = (y_dst * y_ratio) >> 16;
      for (x_dst = 0; x_dst < image_dst->width; x_dst++)
      {
         x_src = (x_dst * x_ratio) >> 16;
         image_dst->pixels[(y_dst * image_dst->width) + x_dst] = image_src->pixels[(y_src * image_src->width) + x_src];
      }
   }

   return true;
}

bool task_image_load_handler(retro_task_t *task)
{
   nbio_handle_t            *nbio  = (nbio_handle_t*)task->state;
   struct nbio_image_handle *image = (struct nbio_image_handle*)nbio->data;

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
               size_t len = 0;
               if (image->cb(nbio, len) == -1)
                  return false;
            }
            if (image->is_blocking_on_processing)
               image->status = IMAGE_STATUS_PROCESS_TRANSFER;
            break;
         case IMAGE_STATUS_TRANSFER:
            if (!image->is_blocking && !image->is_finished)
            {
               retro_time_t start_time = cpu_features_get_time_usec();
               do
               {
                  if (!image_transfer_iterate(image->handle, image->type))
                  {
                     image->status = IMAGE_STATUS_TRANSFER_PARSE;
                     break;
                  }
               }
               while (cpu_features_get_time_usec() - start_time < image->frame_duration);
            }
            break;
         case IMAGE_STATUS_PROCESS_TRANSFER_PARSE:
            if (image->handle && image->cb)
            {
               size_t len = 0;
               if (image->cb(nbio, len) == -1)
                  return false;
            }
            if (!image->is_finished)
               break;
      }
   }

   if (     nbio->is_finished
         && (image && image->is_finished)
         && (!task_get_cancelled(task)))
   {
      struct texture_image *img = (struct texture_image*)malloc(sizeof(struct texture_image));

      if (img)
      {
         /* Upscale image, if required */
         if (image->upscale_threshold > 0)
         {
            if (((image->ti.width > 0) && (image->ti.height > 0)) &&
                ((image->ti.width  < image->upscale_threshold) ||
                 (image->ti.height < image->upscale_threshold)))
            {
               unsigned min_size                  = (image->ti.width < image->ti.height) ?
                                                      image->ti.width : image->ti.height;
               float scale_factor                 = (float)image->upscale_threshold /
                                                      (float)min_size;
               unsigned scale_factor_int          = (unsigned)scale_factor;
               struct texture_image img_resampled = {
                  0,
                  0,
                  NULL,
                  false
               };

               if (scale_factor - (float)scale_factor_int > 0.0f)
                  scale_factor_int += 1;

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

   nbio                = (nbio_handle_t*)malloc(sizeof(*nbio));

   if (!nbio)
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
   nbio->msg_queue     = NULL;
   nbio->cb            = &cb_nbio_image_thumbnail;

   if (supports_rgba)
      BIT32_SET(nbio->status_flags, NBIO_FLAG_IMAGE_SUPPORTS_RGBA);

   image              = (struct nbio_image_handle*)malloc(sizeof(*image));
   if (!image)
   {
      free(nbio);
      free(t);
      return false;
   }

   nbio->path                        = strdup(fullpath);

   image->type                       = IMAGE_TYPE_NONE;
   image->status                     = IMAGE_STATUS_WAIT;
   image->is_blocking                = false;
   image->is_blocking_on_processing  = false;
   image->is_finished                = false;
   image->processing_final_state     = 0;
   image->frame_duration             = 0;
   image->size                       = 0;
   image->upscale_threshold          = upscale_threshold;
   image->handle                     = NULL;

   image->ti.width                   = 0;
   image->ti.height                  = 0;
   image->ti.pixels                  = NULL;
   /* TODO/FIXME - shouldn't we set this ? */
   image->ti.supports_rgba           = false;

   if (strstr(fullpath, ".png"))
   {
      nbio->type       = NBIO_TYPE_PNG;
      image->type      = IMAGE_TYPE_PNG;
   }
   else if (strstr(fullpath, ".jpeg")
         || strstr(fullpath, ".jpg"))
   {
      nbio->type       = NBIO_TYPE_JPEG;
      image->type      = IMAGE_TYPE_JPEG;
   }
   else if (strstr(fullpath, ".bmp"))
   {
      nbio->type       = NBIO_TYPE_BMP;
      image->type      = IMAGE_TYPE_BMP;
   }
   else if (strstr(fullpath, ".tga"))
   {
      nbio->type       = NBIO_TYPE_TGA;
      image->type      = IMAGE_TYPE_TGA;
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

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

#include "../gfx/video_driver.h"
#include "../file_path_special.h"
#include "../verbosity.h"

#include "tasks_internal.h"

enum image_status_enum
{
   IMAGE_STATUS_TRANSFER = 0,
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
   unsigned processing_pos_increment;
   unsigned pos_increment;
   size_t size;
   void *handle;
   transfer_cb_t  cb;
   struct texture_image ti;
};

static int cb_image_menu_upload_generic(void *data, size_t len)
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
   int retval = image_transfer_process(
         image->handle,
         image->type,
         &image->ti.pixels, image->size, width, height);

   if (retval == IMAGE_PROCESS_ERROR)
      return IMAGE_PROCESS_ERROR;

   image->ti.width  = *width;
   image->ti.height = *height;

   return retval;
}

static int cb_image_menu_thumbnail(void *data, size_t len)
{
   unsigned width                   = 0;
   unsigned height                  = 0;
   nbio_handle_t        *nbio       = (nbio_handle_t*)data;
   struct nbio_image_handle *image  = (struct nbio_image_handle*)nbio->data;
   int retval                       = task_image_process(image, &width, &height);

   if ((retval == IMAGE_PROCESS_ERROR)    ||
       (retval == IMAGE_PROCESS_ERROR_END)
      )
      return -1;

   image->is_blocking_on_processing = (retval != IMAGE_PROCESS_END);
   image->is_finished               = (retval == IMAGE_PROCESS_END);
   image->cb                        = &cb_image_menu_upload_generic;

   return 0;
}

static int task_image_iterate_process_transfer(struct nbio_image_handle *image)
{
   unsigned i;
   int retval                      = 0;
   unsigned width                  = 0;
   unsigned height                 = 0;

   for (i = 0; i < image->processing_pos_increment; i++)
   {
      retval = task_image_process(image,
               &width, &height);
      if (retval != IMAGE_PROCESS_NEXT)
         break;
   }

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

static int cb_nbio_image_menu_thumbnail(void *data, size_t len)
{
   void *ptr                       = NULL;
   nbio_handle_t *nbio             = (nbio_handle_t*)data;
   struct nbio_image_handle *image = nbio ?
      (struct nbio_image_handle*)nbio->data : NULL;
   void *handle                    = image ? image_transfer_new(image->type) : NULL;

   if (!handle)
      return -1;

   image->handle                   = handle;
   image->size                     = len;
   image->cb                       = &cb_image_menu_thumbnail;

   ptr                             = nbio_get_ptr(nbio->handle, &len);

   image_transfer_set_buffer_ptr(image->handle, image->type, ptr);

   image->size                     = len;
   image->pos_increment            = (len / 2) ?
      ((unsigned)(len / 2)) : 1;
   image->processing_pos_increment = (len / 4) ?
       ((unsigned)(len / 4)) : 1;

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

bool task_image_load_handler(retro_task_t *task)
{
   unsigned i;
   nbio_handle_t            *nbio  = (nbio_handle_t*)task->state;
   struct nbio_image_handle *image = (struct nbio_image_handle*)nbio->data;

   if (image)
   {
      switch (image->status)
      {
         case IMAGE_STATUS_PROCESS_TRANSFER:
            if (image && task_image_iterate_process_transfer(image) == -1)
               image->status = IMAGE_STATUS_PROCESS_TRANSFER_PARSE;
            break;
         case IMAGE_STATUS_TRANSFER_PARSE:
            if (image->handle && image->cb)
            {
               size_t len = 0;
               image->cb(nbio, len);
            }
            if (image->is_blocking_on_processing)
               image->status = IMAGE_STATUS_PROCESS_TRANSFER;
            break;
         case IMAGE_STATUS_TRANSFER:
            if (!image->is_blocking && !image->is_finished)
            {
               for (i = 0; i < image->pos_increment; i++)
               {
                  if (!image_transfer_iterate(image->handle, image->type))
                  {
                     image->status = IMAGE_STATUS_TRANSFER_PARSE;
                     break;
                  }
               }
            }
            break;
         case IMAGE_STATUS_PROCESS_TRANSFER_PARSE:
            if (image->handle && image->cb)
            {
               size_t len = 0;
               image->cb(nbio, len);
            }
            if (!image->is_finished)
               break;
      }
   }

   if (     nbio->is_finished
         && (image && image->is_finished )
         && (!task_get_cancelled(task)))
   {
      struct texture_image *img = (struct texture_image*)malloc(sizeof(struct texture_image));

      if (img)
      {
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

bool task_push_image_load(const char *fullpath, retro_task_callback_t cb, void *user_data)
{
   nbio_handle_t             *nbio   = NULL;
   struct nbio_image_handle   *image = NULL;
   retro_task_t                   *t = (retro_task_t*)calloc(1, sizeof(*t));

   if (!t)
      goto error_msg;

   nbio                = (nbio_handle_t*)malloc(sizeof(*nbio));

   if (!nbio)
      goto error;

   nbio->type          = NBIO_TYPE_NONE;
   nbio->is_finished   = false;
   nbio->status        = NBIO_STATUS_INIT;
   nbio->pos_increment = 0;
   nbio->status_flags  = 0;
   nbio->data          = NULL;
   nbio->path          = strdup(fullpath);
   nbio->handle        = NULL;
   nbio->msg_queue     = NULL;
   nbio->cb            = &cb_nbio_image_menu_thumbnail;

   if (video_driver_supports_rgba())
      BIT32_SET(nbio->status_flags, NBIO_FLAG_IMAGE_SUPPORTS_RGBA);

   image              = (struct nbio_image_handle*)malloc(sizeof(*image));
   if (!image)
      goto error;

   image->type                       = IMAGE_TYPE_NONE;
   image->status                     = IMAGE_STATUS_TRANSFER;
   image->is_blocking                = false;
   image->is_blocking_on_processing  = false;
   image->is_finished                = false;
   image->processing_final_state     = 0;
   image->processing_pos_increment   = 0;
   image->pos_increment              = 0;
   image->size                       = 0;
   image->handle                     = NULL;

   image->ti.width                   = 0;
   image->ti.height                  = 0;
   image->ti.pixels                  = NULL;
   image->ti.supports_rgba           = false;

   if (strstr(fullpath, file_path_str(FILE_PATH_PNG_EXTENSION)))
   {
      nbio->type       = NBIO_TYPE_PNG;
      image->type      = IMAGE_TYPE_PNG;
   }
   else if (strstr(fullpath, file_path_str(FILE_PATH_JPEG_EXTENSION))
         || strstr(fullpath, file_path_str(FILE_PATH_JPG_EXTENSION)))
   {
      nbio->type       = NBIO_TYPE_JPEG;
      image->type      = IMAGE_TYPE_JPEG;
   }
   else if (strstr(fullpath, file_path_str(FILE_PATH_BMP_EXTENSION)))
   {
      nbio->type       = NBIO_TYPE_BMP;
      image->type      = IMAGE_TYPE_BMP;
   }
   else if (strstr(fullpath, file_path_str(FILE_PATH_TGA_EXTENSION)))
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

error:
   task_image_load_free(t);
   free(t);
   if (nbio)
   {
      if (!string_is_empty(nbio->path))
         free(nbio->path);
      free(nbio);
   }

error_msg:
   RARCH_ERR("[image load] Failed to open '%s': %s.\n",
         fullpath, strerror(errno));

   return false;
}

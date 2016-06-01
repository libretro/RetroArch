/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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
#include <string.h>
#include <errno.h>

#include <file/nbio.h>
#include <formats/image.h>
#include <compat/strl.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <lists/string_list.h>
#include <rhash.h>

#ifdef HAVE_MENU
#include "../menu/menu_driver.h"
#endif

#include "tasks_internal.h"
#include "../verbosity.h"

#define CB_MENU_WALLPAPER     0xb476e505U
#define CB_MENU_THUMBNAIL     0x82f93a21U

enum image_status_enum
{
   IMAGE_STATUS_POLL = 0,
   IMAGE_STATUS_TRANSFER,
   IMAGE_STATUS_TRANSFER_PARSE,
   IMAGE_STATUS_PROCESS_TRANSFER,
   IMAGE_STATUS_PROCESS_TRANSFER_PARSE,
   IMAGE_STATUS_TRANSFER_PARSE_FREE
};

typedef struct nbio_image_handle nbio_image_handle_t;

struct nbio_image_handle
{
   struct texture_image ti;
   bool is_blocking;
   bool is_blocking_on_processing;
   bool is_finished;
   transfer_cb_t  cb;
   void *handle;
   size_t size;
   unsigned processing_pos_increment;
   unsigned pos_increment;
   int processing_final_state;
   enum image_status_enum status;
};

static int cb_image_menu_upload_generic(void *data, size_t len)
{
   unsigned r_shift, g_shift, b_shift, a_shift;
   nbio_handle_t        *nbio = (nbio_handle_t*)data;
   nbio_image_handle_t *image = (nbio_image_handle_t*)nbio->data;

   if (!image)
      return -1;

   if (image->processing_final_state == IMAGE_PROCESS_ERROR ||
         image->processing_final_state == IMAGE_PROCESS_ERROR_END)
      return -1;

   image_texture_set_color_shifts(&r_shift, &g_shift, &b_shift,
         &a_shift);

   image_texture_color_convert(r_shift, g_shift, b_shift,
         a_shift, &image->ti);

   image->is_blocking_on_processing         = false;
   image->is_blocking                       = true;
   image->is_finished                       = true;
   nbio->is_finished                        = true;

   return 0;
}

static int task_image_iterate_transfer_parse(nbio_handle_t *nbio)
{
   nbio_image_handle_t *image = (nbio_image_handle_t*)nbio->data;

   if (image->handle && image->cb)
   {
      size_t len = 0;
      image->cb(nbio, len);
   }

   return 0;
}

static int task_image_process(
      nbio_handle_t *nbio,
      unsigned *width,
      unsigned *height)
{
   nbio_image_handle_t *image = (nbio_image_handle_t*)nbio->data;

   int retval = image_transfer_process(
         image->handle,
         nbio->image_type,
         &image->ti.pixels, image->size, width, height);

   if (retval == IMAGE_PROCESS_ERROR)
      return IMAGE_PROCESS_ERROR;

   image->ti.width  = *width;
   image->ti.height = *height;

   return retval;
}

static int cb_image_menu_generic(nbio_handle_t *nbio)
{
   int retval;
   unsigned width = 0, height = 0;
   nbio_image_handle_t *image = (nbio_image_handle_t*)nbio->data;

   if (!image)
      return -1;

   retval = task_image_process(nbio, &width, &height);
   if ((retval == IMAGE_PROCESS_ERROR) || (retval == IMAGE_PROCESS_ERROR_END))
      return -1;

   image->is_blocking_on_processing         = (retval != IMAGE_PROCESS_END);
   image->is_finished                       = (retval == IMAGE_PROCESS_END);

   return 0;
}

static int cb_image_menu_thumbnail(void *data, size_t len)
{
   nbio_handle_t        *nbio = (nbio_handle_t*)data; 
   nbio_image_handle_t *image = (nbio_image_handle_t*)nbio->data;

   if (cb_image_menu_generic(nbio) != 0)
      return -1;

   image->cb = &cb_image_menu_upload_generic;

   return 0;
}

static int task_image_iterate_process_transfer(nbio_handle_t *nbio)
{
   int retval = 0;
   unsigned i, width = 0, height = 0;
   nbio_image_handle_t *image = (nbio_image_handle_t*)nbio->data;

   if (!image)
      return -1;

   for (i = 0; i < image->processing_pos_increment; i++)
   {
      retval = task_image_process(nbio,
               &width, &height);
      if (retval != IMAGE_PROCESS_NEXT)
         break;
   }

   if (retval == IMAGE_PROCESS_NEXT)
      return 0;

   image->processing_final_state = retval;
   return -1;
}

static int task_image_iterate_transfer(nbio_handle_t *nbio)
{
   unsigned i;
   nbio_image_handle_t *image = (nbio_image_handle_t*)nbio->data;

   if (!image)
      goto error;

   if (image->is_finished)
      return 0;

   for (i = 0; i < image->pos_increment; i++)
   {
      if (!image_transfer_iterate(image->handle, nbio->image_type))
         goto error;
   }

   return 0;

error:
   return -1;
}

static void task_image_load_free_internal(nbio_handle_t *nbio)
{
   nbio_image_handle_t *image = (nbio_image_handle_t*)nbio->data;

   if (image) {
      image_transfer_free(image->handle, nbio->image_type);

      image->handle                 = NULL;
      image->cb                     = NULL;

      free(image);
   }
}

static int cb_nbio_generic(nbio_handle_t *nbio, size_t *len)
{
   void *ptr                  = NULL;
   nbio_image_handle_t *image = (nbio_image_handle_t*)nbio->data;

   if (!image || !image->handle)
      goto error;

   ptr = nbio_get_ptr(nbio->handle, len);

   if (!ptr)
      goto error;

   image_transfer_set_buffer_ptr(image->handle, nbio->image_type, ptr);

   image->size                     = *len;
   image->pos_increment            = (*len / 2) ? (*len / 2) : 1;
   image->processing_pos_increment = (*len / 4) ? (*len / 4) : 1;

   if (!image_transfer_start(image->handle, nbio->image_type))
      goto error;

   image->is_blocking   = false;
   image->is_finished   = false;
   nbio->is_finished    = true;

   return 0;

error:
   task_image_load_free_internal(nbio);
   return -1;
}

static int cb_nbio_image_menu_thumbnail(void *data, size_t len)
{
   nbio_image_handle_t *image = NULL;
   void *handle               = NULL;
   nbio_handle_t *nbio        = (nbio_handle_t*)data; 

   if (!nbio)
      goto error;

   handle = image_transfer_new(nbio->image_type);

   if (!handle)
      goto error;

   image         = (nbio_image_handle_t*)nbio->data;
 
   image->handle = handle;
   image->size   = len;
   image->cb     = &cb_image_menu_thumbnail;

   return cb_nbio_generic(nbio, &len);

error:
   return -1;
}

bool task_image_load_handler(retro_task_t *task)
{
   nbio_handle_t       *nbio  = (nbio_handle_t*)task->state;
   nbio_image_handle_t *image = (nbio_image_handle_t*)nbio->data;

   switch (image->status)
   {
      case IMAGE_STATUS_PROCESS_TRANSFER:
         if (task_image_iterate_process_transfer(nbio) == -1)
            image->status = IMAGE_STATUS_PROCESS_TRANSFER_PARSE;
         break;
      case IMAGE_STATUS_TRANSFER_PARSE:
         task_image_iterate_transfer_parse(nbio);
         if (image->is_blocking_on_processing)
            image->status = IMAGE_STATUS_PROCESS_TRANSFER;
         break;
      case IMAGE_STATUS_TRANSFER:
         if (!image->is_blocking)
            if (task_image_iterate_transfer(nbio) == -1)
               image->status = IMAGE_STATUS_TRANSFER_PARSE;
         break;
      case IMAGE_STATUS_PROCESS_TRANSFER_PARSE:
         task_image_iterate_transfer_parse(nbio);
         if (!image->is_finished)
            break;
      case IMAGE_STATUS_TRANSFER_PARSE_FREE:
      case IMAGE_STATUS_POLL:
      default:
         break;
   }

   if (     nbio->is_finished 
         && image->is_finished 
         && !task->cancelled)
   {
      task->task_data = malloc(sizeof(image->ti));

      if (task->task_data)
         memcpy(task->task_data, &image->ti, sizeof(image->ti));

      return false;
   }

   return true;
}

bool task_push_image_load(const char *fullpath,
      const char *type, retro_task_callback_t cb, void *user_data)
{
   nbio_handle_t             *nbio   = NULL;
   retro_task_t             *t       = NULL;
   uint32_t             cb_type_hash = djb2_calculate(type);
   struct nbio_t             *handle = NULL;
   nbio_image_handle_t        *image = NULL;

   switch (cb_type_hash)
   {
      case CB_MENU_WALLPAPER:
      case CB_MENU_THUMBNAIL:
         break;
      default:
         goto error_msg;
   }

   t = (retro_task_t*)calloc(1, sizeof(*t));
   if (!t)
      goto error_msg;

   nbio = (nbio_handle_t*)calloc(1, sizeof(*nbio));
   if (!nbio)
      goto error;

   handle = nbio_open(fullpath, NBIO_READ);
   if (!handle)
      goto error;

   nbio->handle       = handle;

   image              = (nbio_image_handle_t*)calloc(1, sizeof(*image));   
   if (!image)
      goto error;

   image->status      = IMAGE_STATUS_TRANSFER;

   nbio->data         = (nbio_image_handle_t*)image;
   nbio->is_finished  = false;
   nbio->cb           = &cb_nbio_image_menu_thumbnail;
   nbio->status       = NBIO_STATUS_TRANSFER;

   if (strstr(fullpath, ".png"))
      nbio->image_type = IMAGE_TYPE_PNG;
   else if (strstr(fullpath, ".jpeg") || strstr(fullpath, ".jpg"))
      nbio->image_type = IMAGE_TYPE_JPEG;
   else if (strstr(fullpath, ".bmp"))
      nbio->image_type = IMAGE_TYPE_BMP;
   else if (strstr(fullpath, ".tga"))
      nbio->image_type = IMAGE_TYPE_TGA;

   nbio_begin_read(handle);

   t->state     = nbio;
   t->handler   = task_file_load_handler;
   t->cleanup   = task_image_load_free;
   t->callback  = cb;
   t->user_data = user_data;

   task_queue_ctl(TASK_QUEUE_CTL_PUSH, t);

   return true;

error:
   nbio_free(handle);
   task_image_load_free(t);
   free(t);

error_msg:
   RARCH_ERR("[image load] Failed to open '%s': %s.\n",
         fullpath, strerror(errno));

   return false;
}

void task_image_load_free(retro_task_t *task)
{
   nbio_handle_t       *nbio  = task ? (nbio_handle_t*)task->state : NULL;

   if (nbio) {
      task_image_load_free_internal(nbio);
      nbio_free(nbio->handle);
      nbio->handle      = NULL;
      free(nbio);
   }
}

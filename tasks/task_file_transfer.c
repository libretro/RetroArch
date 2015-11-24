/*  RetroArch - A frontend for libretro.
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

#include <string.h>

#include <file/nbio.h>
#include <formats/image.h>
#include <formats/rpng.h>
#include <compat/strl.h>
#include <retro_assert.h>
#include <retro_miscellaneous.h>
#include <queues/message_queue.h>
#include <string/string_list.h>
#include <rhash.h>

#include "tasks.h"
#include "../verbosity.h"

#define CB_MENU_WALLPAPER     0xb476e505U
#define CB_MENU_BOXART        0x68b307cdU

enum nbio_image_status_enum
{
   NBIO_IMAGE_STATUS_POLL = 0,
   NBIO_IMAGE_STATUS_TRANSFER,
   NBIO_IMAGE_STATUS_TRANSFER_PARSE,
   NBIO_IMAGE_STATUS_PROCESS_TRANSFER,
   NBIO_IMAGE_STATUS_PROCESS_TRANSFER_PARSE,
   NBIO_IMAGE_STATUS_TRANSFER_PARSE_FREE
};

enum nbio_status_enum
{
   NBIO_STATUS_POLL = 0,
   NBIO_STATUS_TRANSFER,
   NBIO_STATUS_TRANSFER_PARSE,
   NBIO_STATUS_TRANSFER_PARSE_FREE
};

typedef struct nbio_image_handle
{
   struct texture_image ti;
   bool is_blocking;
   bool is_blocking_on_processing;
   bool is_finished;
   transfer_cb_t  cb;
#ifdef HAVE_RPNG
   rpng_t *handle;
#endif
   unsigned processing_pos_increment;
   unsigned pos_increment;
   uint64_t frame_count;
   int processing_final_state;
   unsigned status;
} nbio_image_handle_t;

typedef struct nbio_handle
{
   nbio_image_handle_t image;
   bool is_finished;
   transfer_cb_t  cb;
   struct nbio_t *handle;
   unsigned pos_increment;
   uint64_t frame_count;
   msg_queue_t *msg_queue;
   unsigned status;
} nbio_handle_t;


#ifdef HAVE_MENU
#include "../menu/menu_driver.h"

#ifdef HAVE_RPNG
static void rarch_task_file_load_handler(rarch_task_t *task);
static int cb_image_menu_upload_generic(void *data, size_t len)
{
   nbio_handle_t *nbio = (nbio_handle_t*)data;
   unsigned r_shift, g_shift, b_shift, a_shift;

   if (!nbio)
      return -1;

   if (nbio->image.processing_final_state == IMAGE_PROCESS_ERROR ||
         nbio->image.processing_final_state == IMAGE_PROCESS_ERROR_END)
      return -1;

   texture_image_set_color_shifts(&r_shift, &g_shift, &b_shift,
         &a_shift);

   texture_image_color_convert(r_shift, g_shift, b_shift,
         a_shift, &nbio->image.ti);

   nbio->image.is_blocking_on_processing         = false;
   nbio->image.is_blocking                       = true;
   nbio->image.is_finished                       = true;
   nbio->is_finished                             = true;

   return 0;
}

static int cb_image_menu_generic(nbio_handle_t *nbio)
{
   unsigned width = 0, height = 0;
   int retval;
   if (!nbio)
      return -1;

   if (!rpng_is_valid(nbio->image.handle))
      return -1;

   retval = rpng_nbio_load_image_argb_process(nbio->image.handle,
         &nbio->image.ti.pixels, &width, &height);

   nbio->image.ti.width  = width;
   nbio->image.ti.height = height;

   if (retval == IMAGE_PROCESS_ERROR || retval == IMAGE_PROCESS_ERROR_END)
      return -1;

   nbio->image.is_blocking_on_processing         = true;
   nbio->image.is_finished                       = false;

   return 0;
}

static int cb_image_menu_wallpaper(void *data, size_t len)
{
   nbio_handle_t *nbio = (nbio_handle_t*)data; 

   if (cb_image_menu_generic(nbio) != 0)
      return -1;

   nbio->image.cb = &cb_image_menu_upload_generic;

   return 0;
}

static int cb_image_menu_boxart(void *data, size_t len)
{
   nbio_handle_t *nbio = (nbio_handle_t*)data; 

   if (cb_image_menu_generic(nbio) != 0)
      return -1;

   nbio->image.cb = &cb_image_menu_upload_generic;

   return 0;
}

static int rarch_main_data_image_iterate_transfer(nbio_handle_t *nbio)
{
   unsigned i;

   if (!nbio)
      return -1;

   if (nbio->image.is_finished)
      return 0;

   for (i = 0; i < nbio->image.pos_increment; i++)
   {
      if (!rpng_nbio_load_image_argb_iterate(nbio->image.handle))
         goto error;
   }

   nbio->image.frame_count++;
   return 0;

error:
   return -1;
}

static int rarch_main_data_image_iterate_process_transfer(nbio_handle_t *nbio)
{
   unsigned i, width = 0, height = 0;
   int retval = 0;

   if (!nbio)
      return -1;

   for (i = 0; i < nbio->image.processing_pos_increment; i++)
   {
      retval = rpng_nbio_load_image_argb_process(nbio->image.handle,
            &nbio->image.ti.pixels, &width, &height);

      nbio->image.ti.width  = width;
      nbio->image.ti.height = height;

      if (retval != IMAGE_PROCESS_NEXT)
         break;
   }

   if (retval == IMAGE_PROCESS_NEXT)
      return 0;

   nbio->image.processing_final_state = retval;
   return -1;
}

static int rarch_main_data_image_iterate_process_transfer_parse(nbio_handle_t *nbio)
{
   if (nbio->image.handle && nbio->image.cb)
   {
      size_t len = 0;
      nbio->image.cb(nbio, len);
   }

   return 0;
}

static int rarch_main_data_image_iterate_transfer_parse(nbio_handle_t *nbio)
{
   if (nbio->image.handle && nbio->image.cb)
   {
      size_t len = 0;
      nbio->image.cb(nbio, len);
   }

   return 0;
}

static int cb_nbio_default(void *data, size_t len)
{
   nbio_handle_t *nbio = (nbio_handle_t*)data;

   if (!data)
      return -1;

   (void)len;

   nbio->is_finished   = true;

   return 0;
}

static int cb_nbio_generic(nbio_handle_t *nbio, size_t *len)
{
   void *ptr           = NULL;

   if (!nbio->image.handle)
   {
      nbio->image.cb = NULL;
      return -1;
   }

   ptr = nbio_get_ptr(nbio->handle, len);

   if (!ptr)
   {
      rpng_nbio_load_image_free(nbio->image.handle);
      nbio->image.handle = NULL;
      nbio->image.cb     = NULL;

      return -1;
   }

   rpng_set_buf_ptr(nbio->image.handle, (uint8_t*)ptr);
   nbio->image.pos_increment            = (*len / 2) ? (*len / 2) : 1;
   nbio->image.processing_pos_increment = (*len / 4) ?  (*len / 4) : 1;

   if (!rpng_nbio_load_image_argb_start(nbio->image.handle))
   {
      rpng_nbio_load_image_free(nbio->image.handle);
      return -1;
   }

   nbio->image.is_blocking   = false;
   nbio->image.is_finished   = false;
   nbio->is_finished         = true;

   return 0;
}

static int cb_nbio_image_menu_wallpaper(void *data, size_t len)
{
   nbio_handle_t *nbio = (nbio_handle_t*)data; 

   if (!nbio || !data)
      return -1;
   
   nbio->image.handle = rpng_alloc();
   nbio->image.cb     = &cb_image_menu_wallpaper;

   return cb_nbio_generic(nbio, &len);
}

static int cb_nbio_image_menu_boxart(void *data, size_t len)
{
   nbio_handle_t *nbio = (nbio_handle_t*)data; 

   if (!nbio || !data)
      return -1;
   
   nbio->image.handle = rpng_alloc();
   nbio->image.cb     = &cb_image_menu_boxart;

   return cb_nbio_generic(nbio, &len);
}
#endif
#endif

bool rarch_task_push_image_load(const char *fullpath, const char *type, rarch_task_callback_t cb, void *user_data)
{
#if defined(HAVE_RPNG) && defined(HAVE_MENU)
   rarch_task_t *t;
   nbio_handle_t *nbio;
   uint32_t cb_type_hash        = 0;
   struct nbio_t* handle        = NULL;

   cb_type_hash = djb2_calculate(type);

   handle = nbio_open(fullpath, NBIO_READ);

   if (!handle)
   {
      RARCH_ERR("Could not create new file loading handle.\n");
      return false;
   }

   nbio = (nbio_handle_t*)calloc(1, sizeof(*nbio));
   nbio->handle       = handle;
   nbio->is_finished  = false;
   nbio->cb           = &cb_nbio_default;
   nbio->status       = NBIO_STATUS_TRANSFER;
   nbio->image.status = NBIO_IMAGE_STATUS_TRANSFER;


   if (cb_type_hash == CB_MENU_WALLPAPER)
      nbio->cb = &cb_nbio_image_menu_wallpaper;
   else if (cb_type_hash == CB_MENU_BOXART)
      nbio->cb = &cb_nbio_image_menu_boxart;

   nbio_begin_read(handle);

   t = (rarch_task_t*)calloc(1, sizeof(*t));
   t->state = nbio;
   t->handler = rarch_task_file_load_handler;
   t->callback = cb;
   t->user_data = user_data;

   rarch_task_push(t);
#endif
   return true;
}

/* guarded for rpng/menu use but good for generic use */
#if defined(HAVE_RPNG) && defined(HAVE_MENU)
static int rarch_main_data_nbio_iterate_transfer(nbio_handle_t *nbio)
{
   size_t i;

   if (!nbio)
      return -1;
   
   nbio->pos_increment = 5;

   if (nbio->is_finished)
      return 0;

   for (i = 0; i < nbio->pos_increment; i++)
   {
      if (nbio_iterate(nbio->handle))
         return -1;
   }

   nbio->frame_count++;
   return 0;
}

static int rarch_main_data_nbio_iterate_parse(nbio_handle_t *nbio)
{
   if (!nbio)
      return -1;

   if (nbio->cb)
   {
      int len = 0;
      nbio->cb(nbio, len);
   }

   return 0;
}

static void rarch_task_file_load_handler(rarch_task_t *task)
{
   nbio_handle_t         *nbio  = (nbio_handle_t*)task->state;
   nbio_image_handle_t   *image = nbio    ? &nbio->image   : NULL;

   switch (nbio->status)
   {
      case NBIO_STATUS_TRANSFER_PARSE:
         rarch_main_data_nbio_iterate_parse(nbio);
         nbio->status = NBIO_STATUS_TRANSFER_PARSE_FREE;
         break;
      case NBIO_STATUS_TRANSFER:
         if (rarch_main_data_nbio_iterate_transfer(nbio) == -1)
            nbio->status = NBIO_STATUS_TRANSFER_PARSE;
         break;
      case NBIO_STATUS_TRANSFER_PARSE_FREE:
      case NBIO_STATUS_POLL:
      default:
         break;
   }

   if (nbio->image.handle)
   {
      switch (image->status)
      {
         case NBIO_IMAGE_STATUS_PROCESS_TRANSFER:
            if (rarch_main_data_image_iterate_process_transfer(nbio) == -1)
               image->status = NBIO_IMAGE_STATUS_PROCESS_TRANSFER_PARSE;
            break;
         case NBIO_IMAGE_STATUS_TRANSFER_PARSE:
            rarch_main_data_image_iterate_transfer_parse(nbio);
            if (image->is_blocking_on_processing)
               image->status = NBIO_IMAGE_STATUS_PROCESS_TRANSFER;
            break;
         case NBIO_IMAGE_STATUS_TRANSFER:
            if (!image->is_blocking)
               if (rarch_main_data_image_iterate_transfer(nbio) == -1)
                  image->status = NBIO_IMAGE_STATUS_TRANSFER_PARSE;
            break;
         case NBIO_IMAGE_STATUS_PROCESS_TRANSFER_PARSE:
            rarch_main_data_image_iterate_process_transfer_parse(nbio);
            if (!image->is_finished)
               break;
         case NBIO_IMAGE_STATUS_TRANSFER_PARSE_FREE:
         case NBIO_IMAGE_STATUS_POLL:
         default:
            break;
      }

      if (nbio->is_finished && nbio->image.is_finished)
      {
         task->task_data = malloc(sizeof(nbio->image.ti));
         memcpy(task->task_data, &nbio->image.ti, sizeof(nbio->image.ti));
         goto task_finished;
      }

   } else
      if (nbio->is_finished)
         goto task_finished;

   return;

task_finished:
   task->finished = true;

   if (image->handle)
   {
      rpng_nbio_load_image_free(image->handle);

      image->handle                 = NULL;
      image->frame_count            = 0;
   }

   nbio_free(nbio->handle);
   nbio->handle      = NULL;
   nbio->is_finished = false;
   nbio->frame_count = 0;
   free(nbio);
}
#endif

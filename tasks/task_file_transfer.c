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

#include <retro_miscellaneous.h>
#include <queues/message_queue.h>
#include <string/string_list.h>

#include "../runloop_data.h"
#include "tasks.h"

#ifdef HAVE_MENU
#include "menu/menu_driver.h"

#ifdef HAVE_RPNG
static int cb_image_menu_wallpaper_upload(void *data, size_t len)
{
   unsigned r_shift, g_shift, b_shift, a_shift;
   nbio_handle_t *nbio = (nbio_handle_t*)data; 

   if (!nbio || !data)
      return -1;

   if (nbio->image.processing_final_state == IMAGE_PROCESS_ERROR ||
         nbio->image.processing_final_state == IMAGE_PROCESS_ERROR_END)
      return -1;

   texture_image_set_color_shifts(&r_shift, &g_shift, &b_shift,
         &a_shift);

   texture_image_color_convert(r_shift, g_shift, b_shift,
         a_shift, &nbio->image.ti);

   menu_driver_load_background(&nbio->image.ti);

   texture_image_free(&nbio->image.ti);

   nbio->image.is_blocking_on_processing         = false;
   nbio->image.is_blocking                       = true;
   nbio->image.is_finished                       = true;
   nbio->is_finished                             = true;

   return 0;
}

static int cb_image_menu_wallpaper(void *data, size_t len)
{
   int retval;
   nbio_handle_t *nbio = (nbio_handle_t*)data; 

   if (!nbio || !data)
      return -1;

   if (  !nbio->image.handle->has_ihdr || 
         !nbio->image.handle->has_idat || 
         !nbio->image.handle->has_iend)
      return -1;

   retval = rpng_nbio_load_image_argb_process(nbio->image.handle,
         &nbio->image.ti.pixels, &nbio->image.ti.width, &nbio->image.ti.height);

   if (retval == IMAGE_PROCESS_ERROR || retval == IMAGE_PROCESS_ERROR_END)
      return -1;

   nbio->image.cb = &cb_image_menu_wallpaper_upload;

   nbio->image.is_blocking_on_processing         = true;
   nbio->image.is_finished                       = false;

   return 0;
}


static int rarch_main_data_image_iterate_poll(nbio_handle_t *nbio)
{
   const char *path    = NULL;

   if (!nbio)
      return -1;
   
   path = msg_queue_pull(nbio->image.msg_queue);

   if (!path)
      return -1;

   /* Can only deal with one image transfer at a time for now */
   if (nbio->image.handle)
      return -1; 

   /* We need to load the image file first. */
   msg_queue_clear(nbio->msg_queue);
   msg_queue_push(nbio->msg_queue, path, 0, 1);

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
      unsigned ret;
      if (!rpng_nbio_load_image_argb_iterate(
               nbio->image.handle->buff_data,
               nbio->image.handle, &ret))
         goto error;

      nbio->image.handle->buff_data += ret;
   }

   nbio->image.frame_count++;
   return 0;

error:
   return -1;
}

static int rarch_main_data_image_iterate_process_transfer(nbio_handle_t *nbio)
{
   unsigned i;
   int retval = 0;

   if (!nbio)
      return -1;

   for (i = 0; i < nbio->image.processing_pos_increment; i++)
   {
      retval = rpng_nbio_load_image_argb_process(nbio->image.handle,
            &nbio->image.ti.pixels, &nbio->image.ti.width, &nbio->image.ti.height);

      if (retval != IMAGE_PROCESS_NEXT)
         break;
   }

   nbio->image.processing_frame_count++;

   if (retval == IMAGE_PROCESS_NEXT)
      return 0;

   nbio->image.processing_final_state = retval;
   return -1;
}

static int rarch_main_data_image_iterate_parse_free(nbio_handle_t *nbio)
{
   if (!nbio)
      return -1;

   rpng_nbio_load_image_free(nbio->image.handle);

   nbio->image.handle                 = NULL;
   nbio->image.frame_count            = 0;
   nbio->image.processing_frame_count = 0;

   msg_queue_clear(nbio->image.msg_queue);

   return 0;
}

static int rarch_main_data_image_iterate_process_transfer_parse(nbio_handle_t *nbio)
{
   size_t len = 0;
   if (nbio->image.handle && nbio->image.cb)
      nbio->image.cb(nbio, len);

   return 0;
}

static int rarch_main_data_image_iterate_transfer_parse(nbio_handle_t *nbio)
{
   size_t len = 0;
   if (nbio->image.handle && nbio->image.cb)
      nbio->image.cb(nbio, len);

   return 0;
}

void rarch_main_data_nbio_image_iterate(bool is_thread, void *data)
{
   data_runloop_t      *runloop = (data_runloop_t*)data;
   nbio_handle_t         *nbio  = runloop ? &runloop->nbio : NULL;
   nbio_image_handle_t   *image = nbio    ? &nbio->image   : NULL;

   if (!image || !nbio)
      return;

   (void)is_thread;

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
      default:
      case NBIO_IMAGE_STATUS_POLL:
         if (rarch_main_data_image_iterate_poll(nbio) == 0)
            image->status = NBIO_IMAGE_STATUS_TRANSFER;
         break;
   }
}

void rarch_main_data_nbio_image_upload_iterate(bool is_thread,
      void *data)
{
   data_runloop_t     *runloop  = (data_runloop_t*)data;
   nbio_handle_t         *nbio  = runloop ? &runloop->nbio : NULL;
   nbio_image_handle_t   *image = nbio    ? &nbio->image   : NULL;

   if (!image || !nbio || !runloop)
      return;

   (void)is_thread;

   switch (image->status)
   {
      case NBIO_IMAGE_STATUS_PROCESS_TRANSFER_PARSE:
         rarch_main_data_image_iterate_process_transfer_parse(nbio);
         if (image->is_finished)
            image->status = NBIO_IMAGE_STATUS_TRANSFER_PARSE_FREE;
         break;
      case NBIO_IMAGE_STATUS_TRANSFER_PARSE_FREE:
         rarch_main_data_image_iterate_parse_free(nbio);
         image->status = NBIO_IMAGE_STATUS_POLL;
         break;
   }
}
#endif

#endif

static int cb_nbio_default(void *data, size_t len)
{
   nbio_handle_t *nbio = (nbio_handle_t*)data;

   if (!data)
      return -1;

   (void)len;

   nbio->is_finished   = true;

   return 0;
}

static int cb_nbio_image_menu_wallpaper(void *data, size_t len)
{
   void *ptr           = NULL;
   nbio_handle_t *nbio = (nbio_handle_t*)data; 

   if (!nbio || !data)
      return -1;
   
   nbio->image.handle = (struct rpng_t*)calloc(1, sizeof(struct rpng_t));
   nbio->image.cb     = &cb_image_menu_wallpaper;

   if (!nbio->image.handle)
   {
      nbio->image.cb = NULL;
      return -1;
   }

   ptr = nbio_get_ptr(nbio->handle, &len);

   if (!ptr)
   {
      free(nbio->image.handle);
      nbio->image.handle = NULL;
      nbio->image.cb     = NULL;

      return -1;
   }

   nbio->image.handle->buff_data        = (uint8_t*)ptr;
   nbio->image.pos_increment            = (len / 2) ? (len / 2) : 1;
   nbio->image.processing_pos_increment = (len / 4) ? (len / 4) : 1;

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

static int rarch_main_data_nbio_iterate_poll(nbio_handle_t *nbio)
{
   char elem0[PATH_MAX_LENGTH], elem1[PATH_MAX_LENGTH];
   struct nbio_t* handle        = NULL;
   struct string_list *str_list = NULL;
   const char *path             = NULL;

   if (!nbio)
      return -1;
   
   path                         = msg_queue_pull(nbio->msg_queue);

   if (!path)
      return -1;

   /* Can only deal with one NBIO transfer at a time for now */
   if (nbio->handle)
      return -1; 

   str_list                     = string_split(path, "|"); 

   if (!str_list)
      goto error;

   if (str_list->size > 0)
      strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));
   if (str_list->size > 1)
      strlcpy(elem1, str_list->elems[1].data, sizeof(elem1));

   handle = nbio_open(elem0, NBIO_READ);

   if (!handle)
   {
      RARCH_ERR("Could not create new file loading handle.\n");
      goto error;
   }

   nbio->handle      = handle;
   nbio->is_finished = false;
   nbio->cb          = &cb_nbio_default;

   if (elem1[0] != '\0')
   {
#if defined(HAVE_MENU) && defined(HAVE_RPNG)
      if (!strcmp(elem1, "cb_menu_wallpaper"))
         nbio->cb = &cb_nbio_image_menu_wallpaper;
#endif
   }

   nbio_begin_read(handle);

   string_list_free(str_list);


   return 0;

error:
   if (str_list)
      string_list_free(str_list);

   return -1;
}

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
         goto error;
   }

   nbio->frame_count++;
   return 0;

error:
   return -1;
}

static int rarch_main_data_nbio_iterate_parse_free(nbio_handle_t *nbio)
{
   if (!nbio)
      return -1;
   if (!nbio->is_finished)
      return -1;

   nbio_free(nbio->handle);
   nbio->handle      = NULL;
   nbio->is_finished = false;
   nbio->frame_count = 0;

   msg_queue_clear(nbio->msg_queue);

   return 0;
}

static int rarch_main_data_nbio_iterate_parse(nbio_handle_t *nbio)
{
   int len = 0;

   if (!nbio)
      return -1;

   if (nbio->cb)
      nbio->cb(nbio, len);

   return 0;
}

void rarch_main_data_nbio_iterate(bool is_thread, void *data)
{
   data_runloop_t      *runloop = (data_runloop_t*)data;
   nbio_handle_t          *nbio = runloop ? &runloop->nbio : NULL;
   if (!nbio || !runloop)
      return;

   switch (nbio->status)
   {
      case NBIO_STATUS_TRANSFER:
         if (rarch_main_data_nbio_iterate_transfer(nbio) == -1)
            nbio->status = NBIO_STATUS_TRANSFER_PARSE;
         break;
      case NBIO_STATUS_TRANSFER_PARSE:
         rarch_main_data_nbio_iterate_parse(nbio);
         nbio->status = NBIO_STATUS_TRANSFER_PARSE_FREE;
         break;
      case NBIO_STATUS_TRANSFER_PARSE_FREE:
         rarch_main_data_nbio_iterate_parse_free(nbio);
         nbio->status = NBIO_STATUS_POLL;
         break;
      default:
      case NBIO_STATUS_POLL:
         if (rarch_main_data_nbio_iterate_poll(nbio) == 0)
            nbio->status = NBIO_STATUS_TRANSFER;
         break;
   }
}

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

#include <retro_miscellaneous.h>
#include "runloop_data.h"
#include "general.h"
#include "input/input_overlay.h"

#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

typedef int (*transfer_cb_t               )(void *data, size_t len);

#ifdef HAVE_NETWORKING
#include <net/net_http.h>

enum
{
   HTTP_STATUS_POLL = 0,
   HTTP_STATUS_CONNECTION_TRANSFER,
   HTTP_STATUS_CONNECTION_TRANSFER_PARSE,
   HTTP_STATUS_TRANSFER,
   HTTP_STATUS_TRANSFER_PARSE,
   HTTP_STATUS_TRANSFER_PARSE_FREE,
} http_status_enum;

typedef struct http_handle
{
   struct
   {
      struct http_connection_t *handle;
      transfer_cb_t  cb;
      char elem1[PATH_MAX_LENGTH];
   } connection;
   msg_queue_t *msg_queue;
   struct http_t *handle;
   transfer_cb_t  cb;
   unsigned status;
} http_handle_t;
#endif

typedef struct nbio_image_handle
{
   struct texture_image ti;
   bool is_blocking;
   bool is_blocking_on_processing;
   bool is_finished;
   transfer_cb_t  cb;
   struct rpng_t *handle;
   unsigned processing_pos_increment;
   unsigned pos_increment;
   uint64_t frame_count;
   uint64_t processing_frame_count;
   int processing_final_state;
   msg_queue_t *msg_queue;
   unsigned status;
} nbio_image_handle_t;

enum
{
   NBIO_IMAGE_STATUS_POLL = 0,
   NBIO_IMAGE_STATUS_TRANSFER,
   NBIO_IMAGE_STATUS_TRANSFER_PARSE,
   NBIO_IMAGE_STATUS_PROCESS_TRANSFER,
   NBIO_IMAGE_STATUS_PROCESS_TRANSFER_PARSE,
   NBIO_IMAGE_STATUS_TRANSFER_PARSE_FREE,
} nbio_image_status_enum;

enum
{
   NBIO_STATUS_POLL = 0,
   NBIO_STATUS_TRANSFER,
   NBIO_STATUS_TRANSFER_PARSE,
   NBIO_STATUS_TRANSFER_PARSE_FREE,
} nbio_status_enum;

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

typedef struct db_handle
{
   msg_queue_t *msg_queue;
   unsigned status;
} db_handle_t;

enum
{
   THREAD_CODE_INIT = 0,
   THREAD_CODE_DEINIT,
   THREAD_CODE_ALIVE,
} thread_code_enum;

typedef struct data_runloop
{
#ifdef HAVE_NETWORKING
   http_handle_t http;
#endif

#ifdef HAVE_LIBRETRODB
   db_handle_t db;
#endif

   nbio_handle_t nbio;
   bool inited;

#ifdef HAVE_THREADS
   bool thread_inited;
   unsigned thread_code;
   bool alive;

   slock_t *lock;
   slock_t *cond_lock;
   slock_t *overlay_lock;
   scond_t *cond;
   sthread_t *thread;
#endif
} data_runloop_t;

static char data_runloop_msg[PATH_MAX_LENGTH];

static struct data_runloop *g_data_runloop;

static void *rarch_main_data_get_ptr(void)
{
   return g_data_runloop;
}

#ifdef HAVE_NETWORKING
int cb_core_updater_download(void *data_, size_t len);
int cb_core_updater_list(void *data_, size_t len);

/**
 * rarch_main_data_http_iterate_transfer:
 *
 * Resumes HTTP transfer update.
 *
 * Returns: 0 when finished, -1 when we should continue
 * with the transfer on the next frame.
 **/
static int rarch_main_data_http_iterate_transfer(http_handle_t *http)
{
   size_t pos = 0, tot = 0;
   int percent = 0;
   if (!net_http_update(http->handle, &pos, &tot))
   {
      if(tot != 0)
         percent=(unsigned long long)pos*100/(unsigned long long)tot;
      else
         percent=0;   

      if (percent > 0)
      {
         snprintf(data_runloop_msg,
               sizeof(data_runloop_msg), "Download progress: %d%%", percent);
      }

      return -1;
   }

   return 0;
}

static int rarch_main_data_http_con_iterate_transfer(http_handle_t *http)
{
   if (!net_http_connection_iterate(http->connection.handle))
      return -1;
   return 0;
}

static int rarch_main_data_http_conn_iterate_transfer_parse(http_handle_t *http)
{
   if (net_http_connection_done(http->connection.handle))
   {
      if (http->connection.handle && http->connection.cb)
         http->connection.cb(http, 0);
   }
   
   net_http_connection_free(http->connection.handle);

   http->connection.handle = NULL;

   return 0;
}

static int rarch_main_data_http_iterate_transfer_parse(http_handle_t *http)
{
   size_t len = 0;
   char *data = (char*)net_http_data(http->handle, &len, false);

   if (data && http->cb)
      http->cb(data, len);

   net_http_delete(http->handle);

   http->handle = NULL;
   msg_queue_clear(http->msg_queue);

   return 0;
}

static int cb_http_conn_default(void *data_, size_t len)
{
   http_handle_t *http = (http_handle_t*)data_;

   if (!http)
      return -1;

   http->handle = net_http_new(http->connection.handle);

   if (!http->handle)
   {
      RARCH_ERR("Could not create new HTTP session handle.\n");
      return -1;
   }

   http->cb     = NULL;

   if (http->connection.elem1[0] != '\0')
   {
      if (!strcmp(http->connection.elem1, "cb_core_updater_download"))
         http->cb = &cb_core_updater_download;
      if (!strcmp(http->connection.elem1, "cb_core_updater_list"))
         http->cb = &cb_core_updater_list;
   }

   return 0;
}

/**
 * rarch_main_data_http_iterate_poll:
 *
 * Polls HTTP message queue to see if any new URLs 
 * are pending.
 *
 * If handle is freed, will set up a new http handle. 
 * The transfer will be started on the next frame.
 *
 * Returns: 0 when an URL has been pulled and we will
 * begin transferring on the next frame. Returns -1 if
 * no HTTP URL has been pulled. Do nothing in that case.
 **/
static int rarch_main_data_http_iterate_poll(http_handle_t *http)
{
   char elem0[PATH_MAX_LENGTH];
   struct string_list *str_list = NULL;
   const char *url              = msg_queue_pull(http->msg_queue);

   if (!url)
      return -1;

   /* Can only deal with one HTTP transfer at a time for now */
   if (http->handle)
      return -1; 

   str_list                     = string_split(url, "|");

   if (!str_list)
      return -1;

   if (str_list->size > 0)
      strlcpy(elem0, str_list->elems[0].data, sizeof(elem0));

   http->connection.handle = net_http_connection_new(elem0);

   if (!http->connection.handle)
      return -1;

   http->connection.cb     = &cb_http_conn_default;

   if (str_list->size > 1)
      strlcpy(http->connection.elem1,
            str_list->elems[1].data,
            sizeof(http->connection.elem1));

   string_list_free(str_list);
   
   return 0;
}
#endif

#ifdef HAVE_MENU
static int cb_image_menu_wallpaper_upload(void *data, size_t len)
{
   nbio_handle_t *nbio = (nbio_handle_t*)data; 

   if (!nbio || !data)
      return -1;

   if (nbio->image.processing_final_state == IMAGE_PROCESS_ERROR ||
         nbio->image.processing_final_state == IMAGE_PROCESS_ERROR_END)
      return -1;

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
#endif

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

   RARCH_LOG("Image transfer processing took %u frames.\n", (unsigned)nbio->image.processing_frame_count);

   return 0;
}

static int rarch_main_data_image_iterate_transfer_parse(nbio_handle_t *nbio)
{
   size_t len = 0;
   if (nbio->image.handle && nbio->image.cb)
      nbio->image.cb(nbio, len);

   RARCH_LOG("Image transfer took %u frames.\n", (unsigned)nbio->image.frame_count);

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
#ifdef HAVE_MENU
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

   RARCH_LOG("File transfer took %u frames.\n", (unsigned)nbio->frame_count);

   return 0;
}

#ifdef HAVE_MENU
static void rarch_main_data_db_iterate(bool is_thread,
      data_runloop_t *runloop)
{
   driver_t *driver = driver_get_ptr();

   if (!driver || !driver->menu || !driver->menu->rdl)
      return;

   if (driver->menu->rdl->blocking)
   {
      /* Do nonblocking I/O transfers here. */
      return;
   }

#ifdef HAVE_LIBRETRODB
   if (!driver->menu->rdl->iterating)
   {
      database_info_write_rdl_free(driver->menu->rdl);
      driver->menu->rdl = NULL;
      return;
   }

   database_info_write_rdl_iterate(driver->menu->rdl);
#endif

}
#endif


static void rarch_main_data_nbio_image_iterate(bool is_thread,
      data_runloop_t *runloop)
{
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

static void rarch_main_data_nbio_iterate(bool is_thread, data_runloop_t *runloop)
{
   nbio_handle_t          *nbio = runloop ? &runloop->nbio : NULL;
   if (!nbio)
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

#ifdef HAVE_NETWORKING
static void rarch_main_data_http_iterate(bool is_thread, data_runloop_t *runloop)
{
   http_handle_t *http = runloop ? &runloop->http : NULL;
   if (!http)
      return;

   switch (http->status)
   {
      case HTTP_STATUS_CONNECTION_TRANSFER_PARSE:
         rarch_main_data_http_conn_iterate_transfer_parse(http);
         http->status = HTTP_STATUS_TRANSFER;
         break;
      case HTTP_STATUS_CONNECTION_TRANSFER:
         if (!rarch_main_data_http_con_iterate_transfer(http))
            http->status = HTTP_STATUS_CONNECTION_TRANSFER_PARSE;
         break;
      case HTTP_STATUS_TRANSFER_PARSE:
         rarch_main_data_http_iterate_transfer_parse(http);
         http->status = HTTP_STATUS_POLL;
         break;
      case HTTP_STATUS_TRANSFER:
         if (!rarch_main_data_http_iterate_transfer(http))
            http->status = HTTP_STATUS_TRANSFER_PARSE;
         break;
      case HTTP_STATUS_POLL:
      default:
         if (rarch_main_data_http_iterate_poll(http) == 0)
            http->status = HTTP_STATUS_CONNECTION_TRANSFER;
         break;
   }
}
#endif

#ifdef HAVE_OVERLAY
static void rarch_main_data_overlay_iterate(bool is_thread, data_runloop_t *runloop)
{
   driver_t *driver = driver_get_ptr();

   if (rarch_main_is_idle())
      return;
   if (!driver->overlay)
      return;

#ifdef HAVE_THREADS
   if (is_thread)
      slock_lock(runloop->overlay_lock);
#endif

   switch (driver->overlay->state)
   {
      case OVERLAY_STATUS_NONE:
      case OVERLAY_STATUS_ALIVE:
         break;
      case OVERLAY_STATUS_DEFERRED_LOAD:
         input_overlay_load_overlays(driver->overlay);
         break;
      case OVERLAY_STATUS_DEFERRED_LOADING:
         input_overlay_load_overlays_iterate(driver->overlay);
         break;
      case OVERLAY_STATUS_DEFERRED_LOADING_RESOLVE:
         input_overlay_load_overlays_resolve_iterate(driver->overlay);
         break;
      case OVERLAY_STATUS_DEFERRED_DONE:
         input_overlay_new_done(driver->overlay);
         break;
      case OVERLAY_STATUS_DEFERRED_ERROR:
         input_overlay_free(driver->overlay);
         break;
      default:
         break;
   }

#ifdef HAVE_THREADS
   if (is_thread)
      slock_unlock(runloop->overlay_lock);
#endif
}
#endif

#ifdef HAVE_THREADS
static void data_runloop_thread_deinit(data_runloop_t *runloop)
{
   if (!runloop->thread_inited)
   {
      slock_lock(runloop->cond_lock);
      runloop->alive = false;
      scond_signal(runloop->cond);
      slock_unlock(runloop->cond_lock);
      sthread_join(runloop->thread);

      slock_free(runloop->lock);
      slock_free(runloop->cond_lock);
      slock_free(runloop->overlay_lock);
      scond_free(runloop->cond);
   }

   runloop->thread_inited = false;
}
#endif

void rarch_main_data_deinit(void)
{
   data_runloop_t *data_runloop = (data_runloop_t*)rarch_main_data_get_ptr();

   if (!data_runloop)
      return;

#ifdef HAVE_THREADS
   if (data_runloop->thread_inited)
   {
      data_runloop_thread_deinit(data_runloop);
      data_runloop->thread_code = THREAD_CODE_DEINIT;
   }
#endif

   data_runloop->inited = false;
}

void rarch_main_data_free(void)
{
   data_runloop_t *data_runloop = (data_runloop_t*)rarch_main_data_get_ptr();

   if (data_runloop)
      free(data_runloop);
   data_runloop = NULL;
}

static void data_runloop_iterate(bool is_thread, data_runloop_t *runloop)
{
   rarch_main_data_nbio_iterate       (is_thread, runloop);
   rarch_main_data_nbio_image_iterate (is_thread, runloop);
#ifdef HAVE_NETWORKING
   rarch_main_data_http_iterate       (is_thread, runloop);
#endif
#ifdef HAVE_MENU
   rarch_main_data_db_iterate         (is_thread, runloop);
#endif
}

#ifdef HAVE_THREADS
static void data_thread_loop(void *data)
{
   data_runloop_t *runloop = (data_runloop_t*)data;

   RARCH_LOG("[Data Thread]: Initializing data thread.\n");

   slock_lock(runloop->lock);
   while (!runloop->thread_inited)
      scond_wait(runloop->cond, runloop->lock);
   slock_unlock(runloop->lock);

   RARCH_LOG("[Data Thread]: Starting data thread.\n");

   for (;;)
   {
      slock_lock(runloop->lock);

      if (!runloop->alive)
         break;

      data_runloop_iterate(true, runloop);

      slock_unlock(runloop->lock);
   }

   RARCH_LOG("[Data Thread]: Stopping data thread.\n");
   
   data_runloop_thread_deinit(runloop);
}

static void rarch_main_data_thread_init(void)
{
   data_runloop_t *data_runloop  = (data_runloop_t*)rarch_main_data_get_ptr();

   if (!data_runloop)
      return;

   data_runloop->lock            = slock_new();
   data_runloop->cond_lock       = slock_new();
   data_runloop->overlay_lock    = slock_new();
   data_runloop->cond            = scond_new();

   data_runloop->thread    = sthread_create(data_thread_loop, data_runloop);

   if (!data_runloop->thread)
      goto error;

   data_runloop->thread_inited   = true;
   data_runloop->alive           = true;
   data_runloop->thread_code     = THREAD_CODE_ALIVE;

   return;

error:
   slock_free(data_runloop->lock);
   slock_free(data_runloop->cond_lock);
   slock_free(data_runloop->overlay_lock);
   scond_free(data_runloop->cond);
}
#endif

static void rarch_main_data_nbio_image_upload_iterate(bool is_thread,
      data_runloop_t *runloop)
{
   nbio_handle_t         *nbio  = runloop ? &runloop->nbio : NULL;
   nbio_image_handle_t   *image = nbio    ? &nbio->image   : NULL;

   if (!image || !nbio)
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

void rarch_main_data_iterate(void)
{
   data_runloop_t *data_runloop = (data_runloop_t*)rarch_main_data_get_ptr();
   settings_t     *settings     = config_get_ptr();
   
   (void)settings;
#ifdef HAVE_THREADS
   if (settings->menu.threaded_data_runloop_enable)
   {
      switch (data_runloop->thread_code)
      {
         case THREAD_CODE_INIT:
            rarch_main_data_thread_init();
            break;
         case THREAD_CODE_DEINIT:
         case THREAD_CODE_ALIVE:
            break;
      }
   }
#endif

#ifdef HAVE_OVERLAY
   rarch_main_data_overlay_iterate(false, data_runloop);
#endif
   rarch_main_data_nbio_image_upload_iterate(false, data_runloop);

   if (data_runloop_msg[0] != '\0')
   {
      rarch_main_msg_queue_push(data_runloop_msg, 1, 10, true);
      data_runloop_msg[0] = '\0';
   }

#ifdef HAVE_THREADS
   if (settings->menu.threaded_data_runloop_enable && data_runloop->alive)
      return;
#endif

   data_runloop_iterate(false, data_runloop);
}

static data_runloop_t *rarch_main_data_new(void)
{
   data_runloop_t *data_runloop = (data_runloop_t*)calloc(1, sizeof(data_runloop_t));

   if (!data_runloop)
      return NULL;

#ifdef HAVE_THREADS
   data_runloop->thread_inited = false;
   data_runloop->alive         = false;
#endif

   data_runloop->inited = true;

   return data_runloop;
}

void rarch_main_data_clear_state(void)
{
   rarch_main_data_deinit();
   rarch_main_data_free();
   g_data_runloop = rarch_main_data_new();
}


void rarch_main_data_init_queues(void)
{
   data_runloop_t *data_runloop = (data_runloop_t*)rarch_main_data_get_ptr();
#ifdef HAVE_NETWORKING
   if (!data_runloop->http.msg_queue)
      rarch_assert(data_runloop->http.msg_queue = msg_queue_new(8));
#endif
   if (!data_runloop->nbio.msg_queue)
      rarch_assert(data_runloop->nbio.msg_queue = msg_queue_new(8));
   if (!data_runloop->nbio.image.msg_queue)
      rarch_assert(data_runloop->nbio.image.msg_queue = msg_queue_new(8));
#ifdef HAVE_LIBRETRODB
   if (!data_runloop->db.msg_queue)
      rarch_assert(data_runloop->db.msg_queue = msg_queue_new(8));
#endif
}

void rarch_main_data_msg_queue_push(unsigned type,
      const char *msg, const char *msg2,
      unsigned prio, unsigned duration, bool flush)
{
   char new_msg[PATH_MAX_LENGTH];
   msg_queue_t *queue = NULL;
   data_runloop_t *data_runloop = (data_runloop_t*)rarch_main_data_get_ptr();

   switch(type)
   {
      case DATA_TYPE_NONE:
         break;
      case DATA_TYPE_FILE:
         queue = data_runloop->nbio.msg_queue;
         snprintf(new_msg, sizeof(new_msg), "%s|%s", msg, msg2);
         break;
      case DATA_TYPE_IMAGE:
         queue = data_runloop->nbio.image.msg_queue;
         snprintf(new_msg, sizeof(new_msg), "%s|%s", msg, msg2);
         break;
#ifdef HAVE_NETWORKING
      case DATA_TYPE_HTTP:
         queue = data_runloop->http.msg_queue;
         snprintf(new_msg, sizeof(new_msg), "%s|%s", msg, msg2);
         break;
#endif
#ifdef HAVE_OVERLAY
      case DATA_TYPE_OVERLAY:
         snprintf(new_msg, sizeof(new_msg), "%s|%s", msg, msg2);
         break;
#endif
   }

   if (!queue)
      return;

   if (flush)
      msg_queue_clear(queue);
   msg_queue_push(queue, new_msg, prio, duration);
}

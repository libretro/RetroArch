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

#ifdef HAVE_MENU
#include "menu/menu.h"
#endif

enum
{
   THREAD_CODE_INIT = 0,
   THREAD_CODE_DEINIT,
   THREAD_CODE_ALIVE,
} thread_code_enum;


static char data_runloop_msg[PATH_MAX_LENGTH];

static struct data_runloop *g_data_runloop;

data_runloop_t *rarch_main_data_get_ptr(void)
{
   return g_data_runloop;
}

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
}
#endif

void rarch_main_data_deinit(void)
{
   data_runloop_t *runloop = rarch_main_data_get_ptr();

   if (!runloop)
      return;

#ifdef HAVE_THREADS
   if (runloop->thread_inited)
   {
      data_runloop_thread_deinit(runloop);

      runloop->thread_inited = false;
      runloop->thread_code   = THREAD_CODE_DEINIT;
   }
#endif

   runloop->inited = false;
}

void rarch_main_data_free(void)
{
   data_runloop_t *runloop = rarch_main_data_get_ptr();

   if (runloop)
      free(runloop);
   runloop = NULL;
}

static void data_runloop_iterate(bool is_thread, data_runloop_t *runloop)
{
   rarch_main_data_nbio_iterate       (is_thread, runloop);
#ifdef HAVE_RPNG
   rarch_main_data_nbio_image_iterate (is_thread, runloop);
#endif
#ifdef HAVE_OVERLAY
   rarch_main_data_overlay_iterate    (is_thread, runloop);
#endif
#ifdef HAVE_NETWORKING
   rarch_main_data_http_iterate       (is_thread, runloop);
#endif
#ifdef HAVE_MENU
#ifdef HAVE_LIBRETRODB
   rarch_main_data_db_iterate         (is_thread, runloop);
#endif
#endif
}


bool rarch_main_data_active(data_runloop_t *runloop)
{
   bool                  image_active, nbio_active, http_active,
                         http_conn_active, overlay_active;
   bool                  active = false;
   bool               db_active = false;

   driver_t             *driver = driver_get_ptr();
   nbio_handle_t          *nbio = runloop ? &runloop->nbio : NULL;
#ifdef HAVE_RPNG
   nbio_image_handle_t   *image = nbio ? &nbio->image : NULL;
#endif
#ifdef HAVE_NETWORKING
   http_handle_t          *http = runloop ? &runloop->http : NULL;
   struct http_connection_t *http_conn = http ? http->connection.handle : NULL;
#endif
#ifdef HAVE_LIBRETRODB
#ifdef HAVE_MENU
   menu_handle_t          *menu = menu_driver_get_ptr();
   database_info_handle_t   *db = menu ? menu->db : NULL;
   db_active                    = db && db->status != DATABASE_STATUS_NONE;
   active                       = active || db_active;
#endif
#endif

#ifdef HAVE_OVERLAY
   overlay_active               = driver && driver->overlay && 
      (driver->overlay->state != OVERLAY_STATUS_ALIVE 
       && driver->overlay->state != OVERLAY_STATUS_NONE);
   active                       = active || overlay_active;
#endif
#ifdef HAVE_RPNG
   image_active                 = image && image->handle != NULL;
   active                       = active || image_active;
#endif
   nbio_active                  = nbio->handle != NULL;
   active                       = active || nbio_active;
#ifdef HAVE_NETWORKING
   http_active                  = http && http->handle != NULL;
   active                       = active || http_active;
   http_conn_active             = http_conn != NULL;
   active                       = active || http_conn_active;
#endif

   (void)active;
   (void)image_active;
   (void)nbio_active;
   (void)http_active;
   (void)http_conn_active;
   (void)overlay_active;
   (void)db_active;

#if 0
   RARCH_LOG("runloop nbio : %d, image: %d, http: %d, http conn: %d, overlay: %d\n", nbio_active, image_active,
         http_active, http_conn_active, overlay_active);
   RARCH_LOG("active: %d\n", active);
#endif

   return active;
}

static void data_thread_loop(void *data)
{
   data_runloop_t *runloop = (data_runloop_t*)data;

   RARCH_LOG("[Data Thread]: Initializing data thread.\n");

   slock_lock(runloop->lock);
   while (!runloop->thread_inited)
      scond_wait(runloop->cond, runloop->lock);
   slock_unlock(runloop->lock);

   RARCH_LOG("[Data Thread]: Starting data thread.\n");

   while (runloop->alive)
   {
      slock_lock(runloop->lock);

      if (!runloop->alive)
         break;

      data_runloop_iterate(true, runloop);

      if (!rarch_main_data_active(runloop))
         rarch_sleep(10);

      slock_unlock(runloop->lock);

   }

   RARCH_LOG("[Data Thread]: Stopping data thread.\n");
}

#ifdef HAVE_THREADS
static void rarch_main_data_thread_init(void)
{
   data_runloop_t *runloop  = rarch_main_data_get_ptr();

   if (!runloop)
      return;

   runloop->lock            = slock_new();
   runloop->cond_lock       = slock_new();
   runloop->overlay_lock    = slock_new();
   runloop->cond            = scond_new();

   runloop->thread    = sthread_create(data_thread_loop, runloop);

   if (!runloop->thread)
      goto error;

   slock_lock(runloop->lock);
   runloop->thread_inited   = true;
   runloop->alive           = true;
   runloop->thread_code     = THREAD_CODE_ALIVE;
   slock_unlock(runloop->lock);

   return;

error:
   slock_free(runloop->lock);
   slock_free(runloop->cond_lock);
   slock_free(runloop->overlay_lock);
   scond_free(runloop->cond);
}
#endif

void rarch_main_data_iterate(void)
{
   data_runloop_t *runloop      = rarch_main_data_get_ptr();
   settings_t     *settings     = config_get_ptr();
   
   (void)settings;
#ifdef HAVE_THREADS
   if (settings->menu.threaded_data_runloop_enable)
   {
      switch (runloop->thread_code)
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
   rarch_main_data_overlay_image_upload_iterate(false, runloop);
#endif
#ifdef HAVE_RPNG
   rarch_main_data_nbio_image_upload_iterate(false, runloop);
#endif

   if (data_runloop_msg[0] != '\0')
   {
      rarch_main_msg_queue_push(data_runloop_msg, 1, 10, true);
      data_runloop_msg[0] = '\0';
   }

#ifdef HAVE_MENU
   menu_do_refresh(MENU_ACTION_REFRESH);
#endif

#ifdef HAVE_THREADS
   if (settings->menu.threaded_data_runloop_enable && runloop->alive)
      return;
#endif

   data_runloop_iterate(false, runloop);
}

static data_runloop_t *rarch_main_data_new(void)
{
   data_runloop_t *runloop = (data_runloop_t*)calloc(1, sizeof(data_runloop_t));

   if (!runloop)
      return NULL;

#ifdef HAVE_THREADS
   runloop->thread_inited = false;
   runloop->alive         = false;
#endif

   runloop->inited = true;

   return runloop;
}

void rarch_main_data_clear_state(void)
{
   rarch_main_data_deinit();
   rarch_main_data_free();
   g_data_runloop = rarch_main_data_new();
}

void rarch_main_data_init_queues(void)
{
   data_runloop_t *runloop = rarch_main_data_get_ptr();
#ifdef HAVE_NETWORKING
   if (!runloop->http.msg_queue)
      rarch_assert(runloop->http.msg_queue       = msg_queue_new(8));
#endif
   if (!runloop->nbio.msg_queue)
      rarch_assert(runloop->nbio.msg_queue       = msg_queue_new(8));
   if (!runloop->nbio.image.msg_queue)
      rarch_assert(runloop->nbio.image.msg_queue = msg_queue_new(8));
#ifdef HAVE_LIBRETRODB
   if (!runloop->db.msg_queue)
      rarch_assert(runloop->db.msg_queue         = msg_queue_new(8));
#endif
}

void rarch_main_data_msg_queue_push(unsigned type,
      const char *msg, const char *msg2,
      unsigned prio, unsigned duration, bool flush)
{
   char new_msg[PATH_MAX_LENGTH];
   msg_queue_t *queue = NULL;
   data_runloop_t *runloop = rarch_main_data_get_ptr();

   switch(type)
   {
      case DATA_TYPE_NONE:
         break;
      case DATA_TYPE_FILE:
         queue = runloop->nbio.msg_queue;
         snprintf(new_msg, sizeof(new_msg), "%s|%s", msg, msg2);
         break;
      case DATA_TYPE_IMAGE:
         queue = runloop->nbio.image.msg_queue;
         snprintf(new_msg, sizeof(new_msg), "%s|%s", msg, msg2);
         break;
#ifdef HAVE_NETWORKING
      case DATA_TYPE_HTTP:
         queue = runloop->http.msg_queue;
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

void data_runloop_osd_msg(const char *msg, size_t sizeof_msg)
{
   strlcpy(data_runloop_msg, msg, sizeof_msg);
}

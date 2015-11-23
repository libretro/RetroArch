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
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif
#include <file/file_path.h>

#include "general.h"

#include "tasks/tasks.h"
#include "input/input_overlay.h"

#ifdef HAVE_MENU
#include "menu/menu.h"
#endif

typedef struct data_runloop
{
   bool inited;

#ifdef HAVE_THREADS
   bool thread_sleeping;
   bool thread_inited;
   bool alive;

   slock_t *lock;
   slock_t *cond_lock;
   scond_t *cond;
   sthread_t *thread;
#endif
} data_runloop_t;

static char data_runloop_msg[PATH_MAX_LENGTH];

static data_runloop_t g_data_runloop;

#ifdef HAVE_THREADS
static void data_runloop_thread_deinit(void)
{
   if (!g_data_runloop.thread_inited)
   {
      slock_lock(g_data_runloop.cond_lock);
      g_data_runloop.alive = false;
      scond_signal(g_data_runloop.cond);
      slock_unlock(g_data_runloop.cond_lock);
      sthread_join(g_data_runloop.thread);

      slock_free(g_data_runloop.lock);
      slock_free(g_data_runloop.cond_lock);
      scond_free(g_data_runloop.cond);
   }
}
#endif

void rarch_main_data_deinit(void)
{
#ifdef HAVE_THREADS
   if (g_data_runloop.thread_inited)
   {
      data_runloop_thread_deinit();

      g_data_runloop.thread_inited = false;
   }
#endif

   g_data_runloop.inited = false;
}

void rarch_main_data_free(void)
{
   rarch_main_data_nbio_uninit();
#ifdef HAVE_NETWORKING
   rarch_main_data_http_uninit();
#endif
#ifdef HAVE_LIBRETRODB
   rarch_main_data_db_uninit();
#endif

   memset(&g_data_runloop, 0, sizeof(g_data_runloop));
}

static void data_runloop_iterate(bool is_thread)
{
   rarch_main_data_nbio_iterate       (is_thread);
#ifdef HAVE_MENU
#ifdef HAVE_RPNG
   rarch_main_data_nbio_image_iterate (is_thread);
#endif
#endif
#ifdef HAVE_NETWORKING
   rarch_main_data_http_iterate       (is_thread);
#endif
#ifdef HAVE_LIBRETRODB
   rarch_main_data_db_iterate         (is_thread);
#endif
}


bool rarch_main_data_active(void)
{
#ifdef HAVE_LIBRETRODB
   if (rarch_main_data_db_is_active())
      return true;
#endif

#ifdef HAVE_OVERLAY
   if (input_overlay_data_is_active())
      return true;
#endif
   if (rarch_main_data_nbio_image_get_handle())
      return true;
   if (rarch_main_data_nbio_get_handle())
      return true;
#ifdef HAVE_NETWORKING
   if (rarch_main_data_http_get_handle())
      return true;
   if (rarch_main_data_http_conn_get_handle())
      return true;
#endif

   return false;
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

   while (runloop->alive)
   {
      slock_lock(runloop->lock);

      if (!runloop->alive)
         break;

      data_runloop_iterate(true);

      if (!rarch_main_data_active())
      {
         runloop->thread_sleeping = true;
         while(runloop->thread_sleeping)
            scond_wait(runloop->cond, runloop->lock);
      }

      slock_unlock(runloop->lock);

   }

   RARCH_LOG("[Data Thread]: Stopping data thread.\n");
}
#endif

#ifdef HAVE_THREADS
static void rarch_main_data_thread_init(void)
{
   if (g_data_runloop.thread_inited)
      return;

   g_data_runloop.lock            = slock_new();
   g_data_runloop.cond_lock       = slock_new();
   g_data_runloop.cond            = scond_new();

   g_data_runloop.thread    = sthread_create(data_thread_loop, &g_data_runloop);

   if (!g_data_runloop.thread)
      goto error;

   slock_lock(g_data_runloop.lock);
   g_data_runloop.thread_inited   = true;
   g_data_runloop.alive           = true;
   slock_unlock(g_data_runloop.lock);

   return;

error:
   data_runloop_thread_deinit();
}
#endif

#ifdef HAVE_MENU
static void rarch_main_data_menu_iterate(void)
{
#ifdef HAVE_LIBRETRODB
   if (rarch_main_data_db_pending_scan_finished())
      menu_environment_cb(MENU_ENVIRON_RESET_HORIZONTAL_LIST, NULL);
#endif

   menu_iterate_render();
}
#endif

void rarch_main_data_iterate(void)
{
   settings_t     *settings     = config_get_ptr();
   
   (void)settings;
#ifdef HAVE_THREADS
   if (settings->threaded_data_runloop_enable)
   {
      if (!g_data_runloop.thread_inited)
         rarch_main_data_thread_init();
      else if (g_data_runloop.thread_sleeping)
      {
         slock_lock(g_data_runloop.cond_lock);
         g_data_runloop.thread_sleeping = false;
         scond_signal(g_data_runloop.cond);
         slock_unlock(g_data_runloop.cond_lock);
      }
   }
#endif

#ifdef HAVE_RPNG
#ifdef HAVE_MENU
   rarch_main_data_nbio_image_upload_iterate(false);
#endif
#endif
#ifdef HAVE_OVERLAY
   rarch_main_data_overlay_iterate();
#endif

#ifdef HAVE_MENU
   rarch_main_data_menu_iterate();
#endif

   if (data_runloop_msg[0] != '\0')
   {
      rarch_main_msg_queue_push(data_runloop_msg, 1, 10, true);
      data_runloop_msg[0] = '\0';
   }

#ifdef HAVE_THREADS
   if (settings->threaded_data_runloop_enable && g_data_runloop.alive)
      return;
#endif

   data_runloop_iterate(false);
}

static void rarch_main_data_init(void)
{
#ifdef HAVE_THREADS
   g_data_runloop.thread_inited = false;
   g_data_runloop.alive         = false;
#endif

   g_data_runloop.inited = true;
}

void rarch_main_data_clear_state(void)
{
   rarch_main_data_deinit();
   rarch_main_data_free();
   rarch_main_data_init();

   rarch_main_data_nbio_init();
#ifdef HAVE_NETWORKING
   rarch_main_data_http_init();
#endif
#ifdef HAVE_LIBRETRODB
   rarch_main_data_db_init();
#endif
}


void rarch_main_data_init_queues(void)
{
#ifdef HAVE_NETWORKING
   rarch_main_data_http_init_msg_queue();
#endif
   rarch_main_data_nbio_init_msg_queue();
#ifdef HAVE_LIBRETRODB
   rarch_main_data_db_init_msg_queue();
#endif
}


void rarch_main_data_msg_queue_push(unsigned type,
      const char *msg, const char *msg2,
      unsigned prio, unsigned duration, bool flush)
{
   char new_msg[PATH_MAX_LENGTH];
   msg_queue_t *queue            = NULL;
   settings_t     *settings      = config_get_ptr();

   (void)settings;

   switch(type)
   {
      case DATA_TYPE_NONE:
         break;
      case DATA_TYPE_FILE:
         queue = rarch_main_data_nbio_get_msg_queue_ptr();
         fill_pathname_join_delim(new_msg, msg, msg2, '|', sizeof(new_msg));
         break;
      case DATA_TYPE_IMAGE:
         queue = rarch_main_data_nbio_image_get_msg_queue_ptr();
         fill_pathname_join_delim(new_msg, msg, msg2, '|', sizeof(new_msg));
         break;
#ifdef HAVE_NETWORKING
      case DATA_TYPE_HTTP:
         queue = rarch_main_data_http_get_msg_queue_ptr();
         fill_pathname_join_delim(new_msg, msg, msg2, '|', sizeof(new_msg));
         break;
#endif
#ifdef HAVE_OVERLAY
      case DATA_TYPE_OVERLAY:
         fill_pathname_join_delim(new_msg, msg, msg2, '|', sizeof(new_msg));
         break;
#endif
#ifdef HAVE_LIBRETRODB
      case DATA_TYPE_DB:
         queue = rarch_main_data_db_get_msg_queue_ptr();
         fill_pathname_join_delim(new_msg, msg, msg2, '|', sizeof(new_msg));
         break;
#endif
   }

   if (!queue)
      return;

   if (flush)
      msg_queue_clear(queue);
   msg_queue_push(queue, new_msg, prio, duration);

}

void data_runloop_osd_msg(const char *msg, size_t len)
{
   strlcpy(data_runloop_msg, msg, len);
}

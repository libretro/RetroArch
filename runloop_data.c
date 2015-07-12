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

#include "general.h"

#include "runloop_data.h"
#include "tasks/tasks.h"
#include "input/input_overlay.h"

#ifdef HAVE_MENU
#include "menu/menu.h"
#include "menu/menu_entries.h"
#include "menu/menu_input.h"
#endif

enum
{
   THREAD_CODE_INIT = 0,
   THREAD_CODE_DEINIT,
   THREAD_CODE_ALIVE
} thread_code_enum;

typedef struct data_runloop
{
   bool inited;

#ifdef HAVE_THREADS
   bool thread_inited;
   unsigned thread_code;
   bool alive;

   slock_t *lock;
   slock_t *cond_lock;
   scond_t *cond;
   sthread_t *thread;
#endif
} data_runloop_t;

static char data_runloop_msg[PATH_MAX_LENGTH];

static data_runloop_t *g_data_runloop;

static data_runloop_t *rarch_main_data_get_ptr(void)
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
      rarch_main_data_overlay_thread_uninit();
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

static void rarch_main_data_free_internal(void)
{
   data_runloop_t *runloop = rarch_main_data_get_ptr();

   if (!runloop)
      return;

   if (runloop)
      free(runloop);
   runloop = NULL;
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

   rarch_main_data_free_internal();
}

static void data_runloop_iterate(bool is_thread)
{
   rarch_main_data_nbio_iterate       (is_thread);
#ifdef HAVE_RPNG
   rarch_main_data_nbio_image_iterate (is_thread);
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
   bool                  active = false;
#ifdef HAVE_LIBRETRODB
   if (rarch_main_data_db_is_active())
      active = true;
#endif

#ifdef HAVE_OVERLAY
   if (input_overlay_data_is_active())
      active = true;
#endif
   if (rarch_main_data_nbio_image_get_handle())
      active = true;
   if (rarch_main_data_nbio_get_handle())
      active = true;
#ifdef HAVE_NETWORKING
   if (rarch_main_data_http_get_handle())
      active = true;
   if (rarch_main_data_http_conn_get_handle())
      active = true;
#endif

   return active;
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
         rarch_sleep(10);

      slock_unlock(runloop->lock);

   }

   RARCH_LOG("[Data Thread]: Stopping data thread.\n");
}
#endif

#ifdef HAVE_THREADS
static void rarch_main_data_thread_init(void)
{
   data_runloop_t *runloop  = rarch_main_data_get_ptr();

   if (!runloop)
      return;

   runloop->lock            = slock_new();
   runloop->cond_lock       = slock_new();
   runloop->cond            = scond_new();

#ifdef HAVE_OVERLAY
   rarch_main_data_overlay_thread_init();
#endif

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
   data_runloop_thread_deinit(runloop);
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
   rarch_main_data_overlay_image_upload_iterate(false);
#endif
#ifdef HAVE_RPNG
   rarch_main_data_nbio_image_upload_iterate(false);
#endif
#ifdef HAVE_OVERLAY
   rarch_main_data_overlay_iterate    (false);
#endif

#ifdef HAVE_MENU
#ifdef HAVE_LIBRETRODB
   if (rarch_main_data_db_pending_scan_finished())
      menu_environment_cb(MENU_ENVIRON_RESET_HORIZONTAL_LIST, NULL);
#endif
#endif

   if (data_runloop_msg[0] != '\0')
   {
      rarch_main_msg_queue_push(data_runloop_msg, 1, 10, true);
      data_runloop_msg[0] = '\0';
   }

#ifdef HAVE_MENU
   menu_entries_refresh(MENU_ACTION_REFRESH);
#endif

#ifdef HAVE_THREADS
   if (settings->menu.threaded_data_runloop_enable && runloop->alive)
      return;
#endif

   data_runloop_iterate(false);
}

static data_runloop_t *rarch_main_data_new(void)
{
   data_runloop_t *runloop = (data_runloop_t*)
      calloc(1, sizeof(data_runloop_t));

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

   if (!g_data_runloop)
      return;

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
   char new_msg[PATH_MAX_LENGTH] = {0};
   msg_queue_t *queue            = NULL;

   switch(type)
   {
      case DATA_TYPE_NONE:
         break;
      case DATA_TYPE_FILE:
         queue = rarch_main_data_nbio_get_msg_queue_ptr();
         if (!queue)
            return;
         snprintf(new_msg, sizeof(new_msg), "%s|%s", msg, msg2);
         break;
      case DATA_TYPE_IMAGE:
         queue = rarch_main_data_nbio_image_get_msg_queue_ptr();
         if (!queue)
            return;
         snprintf(new_msg, sizeof(new_msg), "%s|%s", msg, msg2);
         break;
#ifdef HAVE_NETWORKING
      case DATA_TYPE_HTTP:
         queue = rarch_main_data_http_get_msg_queue_ptr();
         snprintf(new_msg, sizeof(new_msg), "%s|%s", msg, msg2);
         break;
#endif
#ifdef HAVE_OVERLAY
      case DATA_TYPE_OVERLAY:
         snprintf(new_msg, sizeof(new_msg), "%s|%s", msg, msg2);
         break;
#endif
#ifdef HAVE_LIBRETRODB
      case DATA_TYPE_DB:
         queue = rarch_main_data_db_get_msg_queue_ptr();
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

void data_runloop_osd_msg(const char *msg, size_t len)
{
   strlcpy(data_runloop_msg, msg, len);
}

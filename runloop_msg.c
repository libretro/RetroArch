/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2012-2015 - Michael Lelli
 *  Copyright (C) 2014-2015 - Jay McCarthy
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

#include <retro_assert.h>
#include <queues/message_queue.h>
#ifdef HAVE_THREADS
#include <rthreads/rthreads.h>
#endif

#include "msg_hash.h"
#include "runloop.h"

static msg_queue_t *g_msg_queue;

#ifdef HAVE_THREADS
static slock_t *mq_lock = NULL;
#endif

const char *rarch_main_msg_queue_pull(void)
{
   const char *ret = NULL;

#ifdef HAVE_THREADS
   slock_lock(mq_lock);
#endif

   ret = msg_queue_pull(g_msg_queue);

#ifdef HAVE_THREADS
   slock_unlock(mq_lock);
#endif

   return ret;
}

void rarch_main_msg_queue_push_new(uint32_t hash, unsigned prio, unsigned duration,
      bool flush)
{
   const char *msg = msg_hash_to_str(hash);

   if (!msg)
      return;

   rarch_main_msg_queue_push(msg, prio, duration, flush);
}

void rarch_main_msg_queue_push(const char *msg, unsigned prio, unsigned duration,
      bool flush)
{
   settings_t *settings;
   settings = config_get_ptr();
   if(!settings->video.font_enable)
      return;
   if (!g_msg_queue)
      return;

#ifdef HAVE_THREADS
   slock_lock(mq_lock);
#endif

   if (flush)
      msg_queue_clear(g_msg_queue);
   msg_queue_push(g_msg_queue, msg, prio, duration);

#ifdef HAVE_THREADS
   slock_unlock(mq_lock);
#endif

   if (ui_companion_is_on_foreground())
   {
      const ui_companion_driver_t *ui = ui_companion_get_ptr();
      if (ui->msg_queue_push)
         ui->msg_queue_push(msg, prio, duration, flush);
   }
}

void rarch_main_msg_queue_free(void)
{
   if (!g_msg_queue)
      return;

#ifdef HAVE_THREADS
   slock_lock(mq_lock);
#endif

   msg_queue_free(g_msg_queue);

#ifdef HAVE_THREADS
   slock_unlock(mq_lock);
   slock_free(mq_lock);
#endif

   g_msg_queue = NULL;
}

void rarch_main_msg_queue_init(void)
{
   if (g_msg_queue)
      return;

   g_msg_queue = msg_queue_new(8);
   retro_assert(g_msg_queue);

#ifdef HAVE_THREADS
   mq_lock = slock_new();
   retro_assert(mq_lock);
#endif
}

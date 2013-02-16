/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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

#include "thread_wrapper.h"
#include "../thread.h"
#include "../general.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

enum thread_cmd
{
   CMD_NONE = 0,
   CMD_INIT,
   CMD_SET_SHADER,
   CMD_FREE,
   CMD_SET_ROTATION,
   CMD_VIEWPORT_INFO,
   CMD_READ_VIEWPORT,
   CMD_SET_NONBLOCK,

   CMD_DUMMY = INT_MAX
};

typedef struct thread_video
{
   slock_t *lock;
   scond_t *cond_cmd;
   scond_t *cond_thread;
   sthread_t *thread;

   video_info_t info;
   const video_driver_t *driver;
   void *driver_data;
   const input_driver_t **input;
   void **input_data;

   bool alive;
   bool focus;

   enum thread_cmd send_cmd;
   enum thread_cmd reply_cmd;
   union
   {
      bool b;
      int i;
      const char *str;
      void *v;
   } cmd_data;

   struct
   {
      slock_t *lock;
      uint8_t *buffer;
      unsigned width;
      unsigned height;
      unsigned pitch;
      bool updated;
      char msg[1024];
   } frame;

} thread_video_t;

static void *thread_init_never_call(const video_info_t *video, const input_driver_t **input, void **input_data)
{
   (void)video;
   (void)input;
   (void)input_data;
   RARCH_ERR("Sanity check fail! Threaded mustn't be reinit.\n");
   abort();
   return NULL;
}

static void thread_reply(thread_video_t *thr, enum thread_cmd cmd)
{
   slock_lock(thr->lock);
   thr->reply_cmd = cmd;
   thr->send_cmd = CMD_NONE;
   scond_signal(thr->cond_cmd);
   slock_unlock(thr->lock);
}

static void thread_loop(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;

   for (;;)
   {
      bool updated = false;
      slock_lock(thr->lock);
      while (thr->send_cmd == CMD_NONE && !thr->frame.updated)
         scond_wait(thr->cond_thread, thr->lock);
      if (thr->frame.updated)
         updated = true;
      slock_unlock(thr->lock);

      switch (thr->send_cmd)
      {
         case CMD_INIT:
            //fprintf(stderr, "CMD_INIT\n");
            thr->driver_data = thr->driver->init(&thr->info, thr->input, thr->input_data);
            thr->cmd_data.b = thr->driver_data;
            thread_reply(thr, CMD_INIT);
            break;

         case CMD_FREE:
            //fprintf(stderr, "CMD_FREE\n");
            if (thr->driver_data)
               thr->driver->free(thr->driver_data);
            thr->driver_data = NULL;
            thread_reply(thr, CMD_FREE);
            return;

         case CMD_SET_NONBLOCK:
            //fprintf(stderr, "CMD_SET_NONBLOCK\n");
            thr->driver->set_nonblock_state(thr->driver_data, thr->cmd_data.b);
            thread_reply(thr, CMD_SET_NONBLOCK);
            break;

         default:
            //fprintf(stderr, "CMD unknown ...\n");
            thread_reply(thr, thr->send_cmd);
            break;
      }

      if (updated)
      {
         //fprintf(stderr, "RUN FRAME\n");
         slock_lock(thr->frame.lock);
         bool ret = thr->driver->frame(thr->driver_data,
               thr->frame.buffer, thr->frame.width, thr->frame.height,
               thr->frame.pitch, *thr->frame.msg ? thr->frame.msg : NULL);
         slock_unlock(thr->frame.lock);

         bool alive = ret && thr->driver->alive(thr->driver_data);
         bool focus = ret && thr->driver->focus(thr->driver_data);
         //fprintf(stderr, "Alive: %d, Focus: %d.\n", alive, focus);

         slock_lock(thr->lock);
         thr->alive = alive;
         thr->focus = focus;
         thr->frame.updated = false;
         slock_unlock(thr->lock);
      }
   }
}

static void thread_send_cmd(thread_video_t *thr, enum thread_cmd cmd)
{
   slock_lock(thr->lock);
   thr->send_cmd = cmd;
   thr->reply_cmd = CMD_NONE;
   scond_signal(thr->cond_thread);
   slock_unlock(thr->lock);
}

static void thread_wait_reply(thread_video_t *thr, enum thread_cmd cmd)
{
   slock_lock(thr->lock);
   while (cmd != thr->reply_cmd)
      scond_wait(thr->cond_cmd, thr->lock);
   slock_unlock(thr->lock);
}

static bool thread_alive(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;
   slock_lock(thr->lock);
   bool ret = thr->alive;
   slock_unlock(thr->lock);
   return ret;
}

static bool thread_focus(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;
   slock_lock(thr->lock);
   bool ret = thr->focus;
   slock_unlock(thr->lock);
   return ret;
}

static bool thread_frame(void *data, const void *frame_,
      unsigned width, unsigned height, unsigned pitch, const char *msg)
{
   if (!frame_)
      return true;

   thread_video_t *thr = (thread_video_t*)data;
   unsigned copy_stride = width * (thr->info.rgb32 ? sizeof(uint32_t) : sizeof(uint16_t));

   const uint8_t *src = (const uint8_t*)frame_;
   uint8_t *dst = thr->frame.buffer;

   slock_lock(thr->lock);
   // Drop frame if updated flag is still set, as thread is still working on last frame.
   if (!thr->frame.updated)
   {
      slock_lock(thr->frame.lock);
      for (unsigned h = 0; h < height; h++, src += pitch, dst += copy_stride)
         memcpy(dst, src, copy_stride);
      thr->frame.updated = true;
      scond_signal(thr->cond_thread);
      thr->frame.width  = width;
      thr->frame.height = height;
      thr->frame.pitch  = copy_stride;

      if (msg)
         strlcpy(thr->frame.msg, msg, sizeof(thr->frame.msg));
      else
         *thr->frame.msg = '\0';

      slock_unlock(thr->frame.lock);
   }
   slock_unlock(thr->lock);

   return true;
}

static void thread_set_nonblock_state(void *data, bool state)
{
   thread_video_t *thr = (thread_video_t*)data;
   thr->cmd_data.b = state;
   thread_send_cmd(thr, CMD_SET_NONBLOCK);
   thread_wait_reply(thr, CMD_SET_NONBLOCK);
}

static bool thread_init(thread_video_t *thr, const video_info_t *info, const input_driver_t **input,
      void **input_data)
{
   thr->lock = slock_new();
   thr->frame.lock = slock_new();
   thr->cond_cmd = scond_new();
   thr->cond_thread = scond_new();
   thr->input = input;
   thr->input_data = input_data;
   thr->info = *info;
   thr->alive = true;
   thr->focus = true;

   thr->frame.buffer = (uint8_t*)malloc((info->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t)) * 
         info->input_scale * info->input_scale * RARCH_SCALE_BASE * RARCH_SCALE_BASE);
   if (!thr->frame.buffer)
      return false;

   thr->thread = sthread_create(thread_loop, thr);
   if (!thr->thread)
      return false;
   thread_send_cmd(thr, CMD_INIT);
   thread_wait_reply(thr, CMD_INIT);
   return thr->cmd_data.b;
}

static void thread_free(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;
   if (!thr)
      return;

   thread_send_cmd(thr, CMD_FREE);
   thread_wait_reply(thr, CMD_FREE);
   sthread_join(thr->thread);

   free(thr->frame.buffer);
   slock_free(thr->frame.lock);
   slock_free(thr->lock);
   scond_free(thr->cond_cmd);
   scond_free(thr->cond_thread);

   free(thr);
}

static const video_driver_t video_thread = {
   thread_init_never_call, // Should never be called directly.
   thread_frame,
   thread_set_nonblock_state,
   thread_alive,
   thread_focus,
   NULL, // set_shader
   thread_free,
   "Thread wrapper",
   NULL, // set_rotation
   NULL, // viewport_info
   NULL, // read_viewport
#ifdef HAVE_OVERLAY
   NULL, // get_overlay_interface
#endif
};

bool rarch_threaded_video_init(const video_driver_t **out_driver, void **out_data,
      const input_driver_t **input, void **input_data,
      const video_driver_t *driver, const video_info_t *info)
{
   thread_video_t *thr = (thread_video_t*)calloc(1, sizeof(*thr));
   if (!thr)
      return false;

   thr->driver = driver;
   *out_driver = &video_thread;
   *out_data   = thr;
   return thread_init(thr, info, input, input_data);
}



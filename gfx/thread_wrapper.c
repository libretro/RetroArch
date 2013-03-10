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
#include "../performance.h"
#include <stdlib.h>
#include <string.h>
#include <limits.h>

enum thread_cmd
{
   CMD_NONE = 0,
   CMD_INIT,
   CMD_SET_SHADER,
   CMD_FREE,
   CMD_ALIVE, // Blocking alive check. Used when paused.
   CMD_SET_ROTATION,
   CMD_READ_VIEWPORT,
   CMD_SET_NONBLOCK,

#ifdef HAVE_OVERLAY
   CMD_OVERLAY_ENABLE,
   CMD_OVERLAY_LOAD,
   CMD_OVERLAY_TEX_GEOM,
   CMD_OVERLAY_VERTEX_GEOM,
   CMD_OVERLAY_FULL_SCREEN,
   CMD_OVERLAY_SET_ALPHA,
#endif

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
#ifdef HAVE_OVERLAY
   const video_overlay_interface_t *overlay;
#endif
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
      float f;
      const char *str;
      void *v;

      struct
      {
         enum rarch_shader_type type;
         const char *path;
         unsigned index;
      } set_shader;

      struct
      {
         float x, y, w, h;
      } rect;

      struct
      {
         const uint32_t *data;
         unsigned width;
         unsigned height;
      } image;
   } cmd_data;

   struct rarch_viewport vp;
   struct rarch_viewport read_vp; // Last viewport reported to caller.

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

   video_driver_t video_thread;

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
            thr->driver_data = thr->driver->init(&thr->info, thr->input, thr->input_data);
            thr->cmd_data.b = thr->driver_data;
            thr->driver->viewport_info(thr->driver_data, &thr->vp);
            thread_reply(thr, CMD_INIT);
            break;

         case CMD_FREE:
            if (thr->driver_data)
               thr->driver->free(thr->driver_data);
            thr->driver_data = NULL;
            thread_reply(thr, CMD_FREE);
            return;

         case CMD_SET_NONBLOCK:
            thr->driver->set_nonblock_state(thr->driver_data, thr->cmd_data.b);
            thread_reply(thr, CMD_SET_NONBLOCK);
            break;

         case CMD_SET_ROTATION:
            thr->driver->set_rotation(thr->driver_data, thr->cmd_data.i);
            thread_reply(thr, CMD_SET_ROTATION);
            break;

         case CMD_READ_VIEWPORT:
         {
            struct rarch_viewport vp = {0};
            thr->driver->viewport_info(thr->driver_data, &vp);
            if (memcmp(&vp, &thr->read_vp, sizeof(vp)) == 0) // We can read safely
            {
               thr->cmd_data.b = thr->driver->read_viewport(thr->driver_data, (uint8_t*)thr->cmd_data.v);
               thread_reply(thr, CMD_READ_VIEWPORT);
            }
            else // Viewport dimensions changed right after main thread read the async value. Cannot read safely.
            {
               thr->cmd_data.b = false;
               thread_reply(thr, CMD_READ_VIEWPORT);
            }
            break;
         }
            
         case CMD_SET_SHADER:
         {
            bool ret = thr->driver->set_shader(thr->driver_data,
                  thr->cmd_data.set_shader.type,
                  thr->cmd_data.set_shader.path,
                  thr->cmd_data.set_shader.index);
            thr->cmd_data.b = ret;
            thread_reply(thr, CMD_SET_SHADER);
            break;
         }

         case CMD_ALIVE:
            thr->cmd_data.b = thr->driver->alive(thr->driver_data);
            thread_reply(thr, CMD_ALIVE);
            break;

#ifdef HAVE_OVERLAY
         case CMD_OVERLAY_ENABLE:
            thr->overlay->enable(thr->driver_data, thr->cmd_data.b);
            thread_reply(thr, CMD_OVERLAY_ENABLE);
            break;

         case CMD_OVERLAY_LOAD:
            thr->cmd_data.b = thr->overlay->load(thr->driver_data,
                  thr->cmd_data.image.data,
                  thr->cmd_data.image.width,
                  thr->cmd_data.image.height);
            thread_reply(thr, CMD_OVERLAY_LOAD);
            break;

         case CMD_OVERLAY_TEX_GEOM:
            thr->overlay->tex_geom(thr->driver_data,
                  thr->cmd_data.rect.x,
                  thr->cmd_data.rect.y,
                  thr->cmd_data.rect.w,
                  thr->cmd_data.rect.h);
            thread_reply(thr, CMD_OVERLAY_TEX_GEOM);
            break;

         case CMD_OVERLAY_VERTEX_GEOM:
            thr->overlay->vertex_geom(thr->driver_data,
                  thr->cmd_data.rect.x,
                  thr->cmd_data.rect.y,
                  thr->cmd_data.rect.w,
                  thr->cmd_data.rect.h);
            thread_reply(thr, CMD_OVERLAY_VERTEX_GEOM);
            break;

         case CMD_OVERLAY_FULL_SCREEN:
            thr->overlay->full_screen(thr->driver_data, thr->cmd_data.b);
            thread_reply(thr, CMD_OVERLAY_FULL_SCREEN);
            break;

         case CMD_OVERLAY_SET_ALPHA:
            thr->overlay->set_alpha(thr->driver_data, thr->cmd_data.f);
            thread_reply(thr, CMD_OVERLAY_SET_ALPHA);
            break;
#endif

         default:
            thread_reply(thr, thr->send_cmd);
            break;
      }

      if (updated)
      {
         slock_lock(thr->frame.lock);
         bool ret = thr->driver->frame(thr->driver_data,
               thr->frame.buffer, thr->frame.width, thr->frame.height,
               thr->frame.pitch, *thr->frame.msg ? thr->frame.msg : NULL);
         slock_unlock(thr->frame.lock);

         bool alive = ret && thr->driver->alive(thr->driver_data);
         bool focus = ret && thr->driver->focus(thr->driver_data);

         struct rarch_viewport vp = {0};
         thr->driver->viewport_info(thr->driver_data, &vp);

         slock_lock(thr->lock);
         thr->alive = alive;
         thr->focus = focus;
         thr->frame.updated = false;
         thr->vp = vp;
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
   if (g_extern.is_paused)
   {
      thread_send_cmd(thr, CMD_ALIVE);
      thread_wait_reply(thr, CMD_ALIVE);
      return thr->cmd_data.b;
   }
   else
   {
      slock_lock(thr->lock);
      bool ret = thr->alive;
      slock_unlock(thr->lock);
      return ret;
   }
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

   RARCH_PERFORMANCE_INIT(thread_frame);
   RARCH_PERFORMANCE_START(thread_frame);

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
      thr->frame.width  = width;
      thr->frame.height = height;
      thr->frame.pitch  = copy_stride;

      if (msg)
         strlcpy(thr->frame.msg, msg, sizeof(thr->frame.msg));
      else
         *thr->frame.msg = '\0';

      scond_signal(thr->cond_thread);
      slock_unlock(thr->frame.lock);
   }
   slock_unlock(thr->lock);

   RARCH_PERFORMANCE_STOP(thread_frame);

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

   size_t max_size = info->input_scale * RARCH_SCALE_BASE;
   max_size *= max_size;
   max_size *= info->rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);
   thr->frame.buffer = (uint8_t*)malloc(max_size);
   if (!thr->frame.buffer)
      return false;

   memset(thr->frame.buffer, 0x80, max_size);

   thr->thread = sthread_create(thread_loop, thr);
   if (!thr->thread)
      return false;
   thread_send_cmd(thr, CMD_INIT);
   thread_wait_reply(thr, CMD_INIT);

   return thr->cmd_data.b;
}

static bool thread_set_shader(void *data, enum rarch_shader_type type, const char *path, unsigned index)
{
   thread_video_t *thr = (thread_video_t*)data;
   thr->cmd_data.set_shader.type = type;
   thr->cmd_data.set_shader.path = path;
   thr->cmd_data.set_shader.index = index;
   thread_send_cmd(thr, CMD_SET_SHADER);
   thread_wait_reply(thr, CMD_SET_SHADER);
   return thr->cmd_data.b;
}

static void thread_set_rotation(void *data, unsigned rotation)
{
   thread_video_t *thr = (thread_video_t*)data;
   thr->cmd_data.i = rotation;
   thread_send_cmd(thr, CMD_SET_ROTATION);
   thread_wait_reply(thr, CMD_SET_ROTATION);
}

// This value is set async as stalling on the video driver for every query is too slow.
// This means this value might not be correct, so viewport reads are not supported for now.
static void thread_viewport_info(void *data, struct rarch_viewport *vp)
{
   thread_video_t *thr = (thread_video_t*)data;
   slock_lock(thr->lock);
   *vp = thr->vp;

   // Explicitly mem-copied so we can use memcmp correctly later.
   memcpy(&thr->read_vp, &thr->vp, sizeof(thr->vp));
   slock_unlock(thr->lock);
}

static bool thread_read_viewport(void *data, uint8_t *buffer)
{
   thread_video_t *thr = (thread_video_t*)data;
   thr->cmd_data.v = buffer;
   thread_send_cmd(thr, CMD_READ_VIEWPORT);
   thread_wait_reply(thr, CMD_READ_VIEWPORT);
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

#ifdef HAVE_OVERLAY
static void thread_overlay_enable(void *data, bool state)
{
   thread_video_t *thr = (thread_video_t*)data;
   thr->cmd_data.b = state;
   thread_send_cmd(thr, CMD_OVERLAY_ENABLE);
   thread_wait_reply(thr, CMD_OVERLAY_ENABLE);
}

static bool thread_overlay_load(void *data, const uint32_t *image, unsigned width, unsigned height)
{
   thread_video_t *thr = (thread_video_t*)data;
   thr->cmd_data.image.data = image;
   thr->cmd_data.image.width = width;
   thr->cmd_data.image.height = height;
   thread_send_cmd(thr, CMD_OVERLAY_LOAD);
   thread_wait_reply(thr, CMD_OVERLAY_LOAD);
   return thr->cmd_data.b;
}

static void thread_overlay_tex_geom(void *data, float x, float y, float w, float h)
{
   thread_video_t *thr = (thread_video_t*)data;
   thr->cmd_data.rect.x = x;
   thr->cmd_data.rect.y = y;
   thr->cmd_data.rect.w = w;
   thr->cmd_data.rect.h = h;
   thread_send_cmd(thr, CMD_OVERLAY_TEX_GEOM);
   thread_wait_reply(thr, CMD_OVERLAY_TEX_GEOM);
}

static void thread_overlay_vertex_geom(void *data, float x, float y, float w, float h)
{
   thread_video_t *thr = (thread_video_t*)data;
   thr->cmd_data.rect.x = x;
   thr->cmd_data.rect.y = y;
   thr->cmd_data.rect.w = w;
   thr->cmd_data.rect.h = h;
   thread_send_cmd(thr, CMD_OVERLAY_VERTEX_GEOM);
   thread_wait_reply(thr, CMD_OVERLAY_VERTEX_GEOM);
}

static void thread_overlay_full_screen(void *data, bool enable)
{
   thread_video_t *thr = (thread_video_t*)data;
   thr->cmd_data.b = enable;
   thread_send_cmd(thr, CMD_OVERLAY_FULL_SCREEN);
   thread_wait_reply(thr, CMD_OVERLAY_FULL_SCREEN);
}

static void thread_overlay_set_alpha(void *data, float mod)
{
   thread_video_t *thr = (thread_video_t*)data;
   thr->cmd_data.f = mod;
   thread_send_cmd(thr, CMD_OVERLAY_SET_ALPHA);
   thread_wait_reply(thr, CMD_OVERLAY_SET_ALPHA);
}

static const video_overlay_interface_t thread_overlay = {
   thread_overlay_enable,
   thread_overlay_load,
   thread_overlay_tex_geom,
   thread_overlay_vertex_geom,
   thread_overlay_full_screen,
   thread_overlay_set_alpha,
};

static void thread_get_overlay_interface(void *data, const video_overlay_interface_t **iface)
{
   thread_video_t *thr = (thread_video_t*)data;
   *iface = &thread_overlay;
   thr->driver->overlay_interface(thr->driver_data, &thr->overlay);
}
#endif

#if defined(HAVE_RMENU)
// all stubs for now, might not have to implement them unless we want to port this to consoles
static void thread_start(void) {}
static void thread_stop(void) {}
static void thread_restart(void) {}
#endif

#if defined(HAVE_RMENU) || defined(HAVE_RGUI)
static void thread_set_aspect_ratio(void *data, unsigned aspectratio_index) {}
#endif

static const video_driver_t video_thread = {
   thread_init_never_call, // Should never be called directly.
   thread_frame,
   thread_set_nonblock_state,
   thread_alive,
   thread_focus,
   thread_set_shader,
   thread_free,
   "Thread wrapper",
#if defined(HAVE_RMENU)
   thread_start,
   thread_stop,
   thread_restart,
#endif
   thread_set_rotation,
   thread_viewport_info,
   thread_read_viewport,
#ifdef HAVE_OVERLAY
   thread_get_overlay_interface, // get_overlay_interface
#endif
};

static void thread_set_callbacks(thread_video_t *thr, const video_driver_t *driver)
{
   thr->video_thread = video_thread;
   // Disable optional features if not present.
   if (!driver->read_viewport)
      thr->video_thread.read_viewport = NULL;
   if (!driver->set_rotation)
      thr->video_thread.set_rotation = NULL;
   if (!driver->set_shader)
      thr->video_thread.set_shader = NULL;
#ifdef HAVE_OVERLAY
   if (!driver->overlay_interface)
      thr->video_thread.overlay_interface = NULL;
#endif
}

bool rarch_threaded_video_init(const video_driver_t **out_driver, void **out_data,
      const input_driver_t **input, void **input_data,
      const video_driver_t *driver, const video_info_t *info)
{
   thread_video_t *thr = (thread_video_t*)calloc(1, sizeof(*thr));
   if (!thr)
      return false;

   thread_set_callbacks(thr, driver);

   thr->driver = driver;
   *out_driver = &thr->video_thread;
   *out_data   = thr;
   return thread_init(thr, info, input, input_data);
}



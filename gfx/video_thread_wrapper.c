/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <math.h>

#include <compat/strl.h>
#include <features/features_cpu.h>
#include <memalign.h>

#ifdef _3DS
#include <3ds/types.h>
#include <3ds/allocator/linear.h> /* linearMemAlign() */
#endif

#include "video_driver.h"
#include "video_thread_wrapper.h"
#include "font_driver.h"

#include "../retroarch.h"
#include "../runloop.h"
#include "../verbosity.h"

#include <retro_assert.h>

/* cond_reply is woken with scond_signal() and carries one command's
 * reply, so at most one thread may wait on it; see the note in
 * video_thread_wrapper.h. Every wait on cond_reply is bracketed by
 * these; thr->lock is held across the wait, so the counter needs no
 * atomics. Ring waits use cond_ring, which is broadcast, and need no
 * bracket. */
#ifdef DEBUG
#define VIDEO_THREAD_CMD_WAIT_ENTER(thr) \
   do { \
      uintptr_t self_ = sthread_get_current_thread_id(); \
      retro_assert(   (thr)->cond_reply_waiters == 0 \
                   || (thr)->cond_reply_waiter  == self_); \
      (thr)->cond_reply_waiter = self_; \
      (thr)->cond_reply_waiters++; \
   } while (0)
#else
/* Release builds pay nothing; the fields exist unconditionally only so
 * that the struct layout does not vary with the build type. */
#define VIDEO_THREAD_CMD_WAIT_ENTER(thr) do { } while (0)
#endif
#ifdef DEBUG
#define VIDEO_THREAD_CMD_WAIT_LEAVE(thr) \
   do { (thr)->cond_reply_waiters--; } while (0)
#else
#define VIDEO_THREAD_CMD_WAIT_LEAVE(thr) do { } while (0)
#endif

static void *video_thread_init_never_call(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   (void)video;
   (void)input;
   (void)input_data;
   RARCH_ERR("Sanity check fail! Threaded mustn't be reinit.\n");
   abort();
   return NULL;
}

/* thread -> user */
static void video_thread_reply(thread_video_t *thr, const thread_packet_t *pkt)
{
   if (thr->inline_reply)
   {
      *thr->inline_reply = *pkt;
      return;
   }

   slock_lock(thr->lock);

   thr->cmd_data  = *pkt;

   thr->reply_cmd = pkt->type;
   thr->send_cmd  = CMD_VIDEO_NONE;

   scond_signal(thr->cond_reply);
   slock_unlock(thr->lock);
}

/* user -> thread */
static void video_thread_send_packet(thread_video_t *thr,
      const thread_packet_t *pkt)
{
   slock_lock(thr->lock);

   thr->cmd_data  = *pkt;

   thr->send_cmd  = pkt->type;
   thr->reply_cmd = CMD_VIDEO_NONE;

   scond_signal(thr->cond_thread);
   slock_unlock(thr->lock);

}

/* As video_thread_send_packet(), but drops the packet and reports
 * failure if the worker is no longer alive.  thr->alive is written by
 * video_thread_loop() under thr->lock, so the test has to happen with
 * that lock held; doing it here reuses the critical section this
 * function enters anyway rather than taking a second one. */
static bool video_thread_send_packet_if_alive(thread_video_t *thr,
      const thread_packet_t *pkt)
{
   slock_lock(thr->lock);

   if (!thr->alive)
   {
      slock_unlock(thr->lock);
      return false;
   }

   thr->cmd_data  = *pkt;

   thr->send_cmd  = pkt->type;
   thr->reply_cmd = CMD_VIDEO_NONE;

   scond_signal(thr->cond_thread);
   slock_unlock(thr->lock);

   return true;
}

/* One condvar-wait iteration that lets a main-thread waiter drain the
 * cocoa main-thread trampoline, so work the worker marshals back via
 * cocoa_main_thread_sync() runs and the handshake does not deadlock (the
 * worker blocks on the main thread while the main thread blocks on the
 * reply).  Returns true if it fully handled this wait iteration; false if
 * the caller should perform a plain blocking scond_wait().  A no-op
 * returning false on non-Apple platforms. */
static bool video_thread_pump_wait(scond_t *cond, slock_t *lock)
{
#ifdef __APPLE__
   bool cocoa_main_thread_cond_wait_pump(scond_t *cond, slock_t *lock);
   return cocoa_main_thread_cond_wait_pump(cond, lock);
#else
   (void)cond;
   (void)lock;
   return false;
#endif
}

/* True when the caller is the video thread itself. A marshalled call
 * that reaches the wrapper from inside driver->frame() - the menu
 * drivers do this with set_viewport - must go to the driver directly:
 * sending a command from the thread that answers commands waits on
 * itself forever. */
static bool video_thread_is_self(thread_video_t *thr)
{
   return thr->thread
       && sthread_get_thread_id(thr->thread) == sthread_get_current_thread_id();
}

/* user -> thread */
static void video_thread_wait_reply(thread_video_t *thr, thread_packet_t *pkt)
{
   slock_lock(thr->lock);

   VIDEO_THREAD_CMD_WAIT_ENTER(thr);
   while (pkt->type != thr->reply_cmd)
      if (!video_thread_pump_wait(thr->cond_reply, thr->lock))
         scond_wait(thr->cond_reply, thr->lock);
   VIDEO_THREAD_CMD_WAIT_LEAVE(thr);

   *pkt               = thr->cmd_data;
   thr->cmd_data.type = CMD_VIDEO_NONE;

   slock_unlock(thr->lock);
}

/* user -> thread: take the poster slot. Waits while another thread's
 * command is in flight, until its reply has been consumed.
 *
 * A main-thread waiter pumps the cocoa trampoline here as it does in
 * video_thread_wait_reply(): the slot holder's command may itself be
 * blocked in cocoa_main_thread_sync() waiting on the main thread, so a
 * plain wait here would close the cycle.
 *
 * The slot is counted rather than exclusive so that a re-entry from the
 * owning thread does not deadlock on itself. That is the only thing the
 * depth buys: a command posted from inside the trampoline while the
 * worker is blocked in cocoa_main_thread_sync() has nobody to service
 * it and overwrites the mailbox under the outstanding command, which is
 * the same failure this slot exists to prevent. Nested posting is a bug
 * in the caller, not a supported path. */
static void video_thread_user_acquire(thread_video_t *thr)
{
   uintptr_t self = sthread_get_current_thread_id();

   slock_lock(thr->lock);
   while (thr->user_depth && thr->user_owner != self)
   {
      if (!video_thread_pump_wait(thr->cond_user, thr->lock))
         scond_wait(thr->cond_user, thr->lock);
   }
   thr->user_owner = self;
   thr->user_depth++;
   slock_unlock(thr->lock);
}

static void video_thread_user_release(thread_video_t *thr)
{
   slock_lock(thr->lock);
   if (--thr->user_depth == 0)
   {
      thr->user_owner = 0;
      scond_broadcast(thr->cond_user);
   }
   slock_unlock(thr->lock);
}

/* user -> thread */
static bool video_thread_handle_packet(thread_video_t *thr,
      const thread_packet_t *incoming);

static void video_thread_send_and_wait_user_to_thread(thread_video_t *thr, thread_packet_t *pkt)
{
   /* On the video thread already - a wrapper entry point reached from
    * inside driver->frame(), as the menu drivers do with set_viewport -
    * the command runs here and now. Sending it would wait for a reply
    * from the only thread that could give one. The reply lands in the
    * caller's packet, leaving the mailbox to whatever the main thread
    * may have sent meanwhile. */
   if (video_thread_is_self(thr))
   {
      thr->inline_reply = pkt;
      video_thread_handle_packet(thr, pkt);
      thr->inline_reply = NULL;
      return;
   }

   video_thread_user_acquire(thr);
   video_thread_send_packet(thr, pkt);
   video_thread_wait_reply(thr, pkt);
   video_thread_user_release(thr);
}

static void thread_update_driver_state(thread_video_t *thr)
{
#ifdef HAVE_MENU
   if (thr->texture.frame_updated)
   {
      if (thr->driver_data && thr->poke && thr->poke->set_texture_frame)
         thr->poke->set_texture_frame(thr->driver_data,
               thr->texture.frame, thr->texture.rgb32,
               thr->texture.width, thr->texture.height,
               thr->texture.alpha);
      thr->texture.frame_updated = false;
   }

   if (thr->driver_data && thr->poke && thr->poke->set_texture_enable)
      thr->poke->set_texture_enable(thr->driver_data,
            thr->texture.enable, thr->texture.full_screen);
#endif

#ifdef HAVE_OVERLAY
   slock_lock(thr->alpha_lock);
   if (thr->alpha_update)
   {
      if (thr->driver_data && thr->overlay && thr->overlay->set_alpha)
      {
         int i;
         for (i = 0; i < (int)thr->alpha_mods; i++)
            thr->overlay->set_alpha(thr->driver_data, i, thr->alpha_mod[i]);
      }
      thr->alpha_update = false;
   }
   slock_unlock(thr->alpha_lock);
#endif

   if (thr->apply_state_changes)
   {
      if (thr->driver_data && thr->poke && thr->poke->apply_state_changes)
         thr->poke->apply_state_changes(thr->driver_data);
      thr->apply_state_changes = false;
   }
}

/* returns true when video_thread_loop should quit */
static bool video_thread_handle_packet(
      thread_video_t *thr,
      const thread_packet_t *incoming)
{
   thread_packet_t pkt = *incoming;

   switch (pkt.type)
   {
      case CMD_INIT:
         if (thr->driver && thr->driver->init)
         {
            thr->driver_data = thr->driver->init(&thr->info,
                  thr->input, thr->input_data);
            if (thr->driver_data && thr->driver->viewport_info)
               thr->driver->viewport_info(thr->driver_data, &thr->vp);
            /* Drivers that have handed the OSD font lifecycle up get
             * it created here rather than in video_driver.c, because
             * this runs on the video thread that owns the graphics
             * context. Unmigrated drivers still do it themselves,
             * also from here, inside their own init(). */
            if (     thr->driver_data
                  && thr->driver->font_backend)
               font_driver_init_osd(thr->driver_data, &thr->info,
                     true, thr->driver->font_backend);
         }
         else
            thr->driver_data = NULL;
         pkt.data.b = (thr->driver_data != NULL);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_FREE:
         /* Before the driver goes: the font owns GPU objects created
          * against it, and this is the thread they belong to. */
         if (     thr->driver
               && thr->driver->font_backend)
            font_driver_free_osd_for(thr->driver_data);
         if (thr->driver_data && thr->driver && thr->driver->free)
            thr->driver->free(thr->driver_data);
         thr->driver_data = NULL;
         video_thread_reply(thr, &pkt);
         return true;

      case CMD_SET_ROTATION:
         if (thr->driver_data && thr->driver && thr->driver->set_rotation)
            thr->driver->set_rotation(thr->driver_data, pkt.data.i);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_SET_VIEWPORT:
         if (thr->driver_data && thr->driver && thr->driver->set_viewport)
            thr->driver->set_viewport(thr->driver_data,
                  pkt.data.set_viewport.width,
                  pkt.data.set_viewport.height,
                  pkt.data.set_viewport.force_full,
                  pkt.data.set_viewport.allow_rotate);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_SET_NONBLOCK:
         /* The swapchain lives on this thread; swap interval and
          * adaptive vsync are its settings and have to be applied here. */
         if (thr->driver_data && thr->driver && thr->driver->set_nonblock_state)
            thr->driver->set_nonblock_state(thr->driver_data,
                  pkt.data.nonblock.nonblock,
                  pkt.data.nonblock.adaptive_vsync,
                  pkt.data.nonblock.swap_interval);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_READ_VIEWPORT:
         if (thr->driver_data && thr->driver &&
               thr->driver->viewport_info && thr->driver->read_viewport)
         {
            struct video_viewport vp;

            vp.x           = 0;
            vp.y           = 0;
            vp.width       = 0;
            vp.height      = 0;
            vp.full_width  = 0;
            vp.full_height = 0;

            thr->driver->viewport_info(thr->driver_data, &vp);
            if (!memcmp(&vp, &thr->read_vp, sizeof(vp)))
            {
               /* We can read safely
                *
                * read_viewport() in GL driver calls
                * 'cached frame render' to be able to read from
                * back buffer.
                *
                * This means frame() callback in threaded wrapper will
                * be called from this thread, causing a timeout, and
                * no frame to be rendered.
                *
                * To avoid this, set a flag so wrapper can see if
                * it's called in this "special" way. */
               thr->frame.within_thread = true;
               pkt.data.b = thr->driver->read_viewport(thr->driver_data,
                     (uint8_t*)pkt.data.v, thr->is_idle);
               thr->frame.within_thread = false;
            }
            else
            {
               /* Viewport dimensions changed right after main
                * thread read the async value. Cannot read safely. */
               pkt.data.b = false;
            }
         }
         else
            pkt.data.b = false;
         video_thread_reply(thr, &pkt);
         break;

      case CMD_SET_SHADER:
         if (thr->driver_data && thr->driver && thr->driver->set_shader)
            pkt.data.b = thr->driver->set_shader(thr->driver_data,
               pkt.data.set_shader.type, pkt.data.set_shader.path);
         else
            pkt.data.b = false;
         video_thread_reply(thr, &pkt);
         break;

      case CMD_ALIVE:
         if (thr->driver_data && thr->driver && thr->driver->alive)
            pkt.data.b = thr->driver->alive(thr->driver_data);
         else
            pkt.data.b = false;
         video_thread_reply(thr, &pkt);
         break;

#ifdef HAVE_OVERLAY
      case CMD_OVERLAY_ENABLE:
         if (thr->driver_data && thr->overlay && thr->overlay->enable)
            thr->overlay->enable(thr->driver_data, pkt.data.b);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_OVERLAY_LOAD:
         {
            unsigned tmp_alpha_mods = pkt.data.image.num;

            if (thr->driver_data && thr->overlay && thr->overlay->load)
               pkt.data.b = thr->overlay->load(thr->driver_data,
                  pkt.data.image.data, pkt.data.image.num);
            else
               pkt.data.b = false;

            if (tmp_alpha_mods > 0)
            {
               float *tmp_alpha_mod = (float*)realloc(thr->alpha_mod,
                  tmp_alpha_mods * sizeof(float));
               if (tmp_alpha_mod)
               {
                  /* Avoid temporary garbage data. */
                  int i;
                  for (i = 0; i < (int)tmp_alpha_mods; i++)
                     tmp_alpha_mod[i] = 1.0f;
                  thr->alpha_mods = tmp_alpha_mods;
                  thr->alpha_mod  = tmp_alpha_mod;
               }
            }
            else
            {
               free(thr->alpha_mod);
               thr->alpha_mods = 0;
               thr->alpha_mod  = NULL;
            }
         }
         video_thread_reply(thr, &pkt);
         break;

      case CMD_OVERLAY_TEX_GEOM:
         if (thr->driver_data && thr->overlay && thr->overlay->tex_geom)
            thr->overlay->tex_geom(thr->driver_data,
                  pkt.data.rect.index,
                  pkt.data.rect.x,
                  pkt.data.rect.y,
                  pkt.data.rect.w,
                  pkt.data.rect.h);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_OVERLAY_VERTEX_GEOM:
         if (thr->driver_data && thr->overlay && thr->overlay->vertex_geom)
            thr->overlay->vertex_geom(thr->driver_data,
                  pkt.data.rect.index,
                  pkt.data.rect.x,
                  pkt.data.rect.y,
                  pkt.data.rect.w,
                  pkt.data.rect.h);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_OVERLAY_FULL_SCREEN:
         if (thr->driver_data && thr->overlay && thr->overlay->full_screen)
            thr->overlay->full_screen(thr->driver_data, pkt.data.b);
         video_thread_reply(thr, &pkt);
         break;
#endif

      case CMD_POKE_SET_VIDEO_MODE:
         if (thr->driver_data && thr->poke && thr->poke->set_video_mode)
            thr->poke->set_video_mode(thr->driver_data,
                  pkt.data.new_mode.width,
                  pkt.data.new_mode.height,
                  pkt.data.new_mode.fullscreen);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_POKE_SET_FILTERING:
         if (thr->driver_data && thr->poke && thr->poke->set_filtering)
            thr->poke->set_filtering(thr->driver_data,
                  pkt.data.filtering.index,
                  pkt.data.filtering.smooth,
                  pkt.data.filtering.ctx_scaling);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_POKE_SET_ASPECT_RATIO:
         if (thr->driver_data && thr->poke && thr->poke->set_aspect_ratio)
            thr->poke->set_aspect_ratio(thr->driver_data, pkt.data.i);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_FONT_INIT:
         if (pkt.data.font_init.method)
            pkt.data.font_init.return_value = pkt.data.font_init.method(
               pkt.data.font_init.font_driver,
               pkt.data.font_init.font_handle,
               pkt.data.font_init.video_data,
               pkt.data.font_init.font_path,
               pkt.data.font_init.font_size,
               pkt.data.font_init.backend,
               pkt.data.font_init.is_threaded
            );
         video_thread_reply(thr, &pkt);
         break;

      case CMD_CUSTOM_COMMAND:
         if (pkt.data.custom_command.method)
         {
            /* The user thread is blocked in video_thread_wait_reply for
             * the whole of this call. On some platforms that waiter is a
             * higher-priority (e.g. main/UI) thread while this worker runs
             * at a lower scheduling class, so the wait is a priority
             * inversion; slock/scond do not propagate priority. Lift this
             * thread for the duration of the synchronous work (no-op where
             * unsupported), covering the heavy GPU uploads the custom
             * command path carries (texture/font resource commands). */
            void *qos_override = sthread_priority_override_begin();
            pkt.data.custom_command.return_value =
               pkt.data.custom_command.method(pkt.data.custom_command.data);
            sthread_priority_override_end(qos_override);
         }
         video_thread_reply(thr, &pkt);
         break;

      case CMD_POKE_SHOW_MOUSE:
         if (thr->driver_data && thr->poke && thr->poke->show_mouse)
            thr->poke->show_mouse(thr->driver_data, pkt.data.b);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_POKE_GRAB_MOUSE_TOGGLE:
         if (thr->driver_data && thr->poke && thr->poke->grab_mouse_toggle)
            thr->poke->grab_mouse_toggle(thr->driver_data);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_VIDEO_NONE:
         /* Never reply on no command. Possible deadlock if
          * thread sends command right after frame update. */
         break;

      case CMD_POKE_SET_HDR_MENU_NITS:
         if (thr->driver_data && thr->poke && thr->poke->set_hdr_menu_nits)
            thr->poke->set_hdr_menu_nits(
               thr->driver_data,
               pkt.data.hdr.menu_nits
            );
         video_thread_reply(thr, &pkt);
         break;

      case CMD_POKE_SET_HDR_PAPER_WHITE_NITS:
         if (thr->driver_data &&
               thr->poke && thr->poke->set_hdr_paper_white_nits)
            thr->poke->set_hdr_paper_white_nits(
               thr->driver_data,
               pkt.data.hdr.paper_white_nits
            );
         video_thread_reply(thr, &pkt);
         break;

      case CMD_POKE_SET_HDR_EXPAND_GAMUT:
         if (thr->driver_data && thr->poke && thr->poke->set_hdr_expand_gamut)
            thr->poke->set_hdr_expand_gamut(
               thr->driver_data,
               pkt.data.hdr.expand_gamut
            );
         video_thread_reply(thr, &pkt);
         break;

      case CMD_POKE_SET_HDR_SCANLINES:
         if (thr->driver_data && thr->poke && thr->poke->set_hdr_scanlines)
            thr->poke->set_hdr_scanlines(
               thr->driver_data,
               pkt.data.hdr.scanlines
            );
         video_thread_reply(thr, &pkt);
         break;

      case CMD_POKE_SET_HDR_SUBPIXEL_LAYOUT:
         if (thr->driver_data && thr->poke && thr->poke->set_hdr_subpixel_layout)
            thr->poke->set_hdr_subpixel_layout(
               thr->driver_data,
               pkt.data.hdr.subpixel_layout
            );

         video_thread_reply(thr, &pkt);
         break;

      default:
         video_thread_reply(thr, &pkt);
         break;
   }

   return false;
}

/* Called on the video thread after a present. Sets when a repeat falls
 * due: a period after the display's own timestamp for that present when
 * the driver reports one, else after now; and never at or before now,
 * so a stale or late report cannot make the next wait fire at once. */
static void video_thread_schedule_next(thread_video_t *thr)
{
   retro_time_t now  = cpu_features_get_time_usec();
   retro_time_t base = 0;
   retro_time_t next;

   if (     thr->present_timing_ask
         && thr->driver_data && thr->poke && thr->poke->get_last_present_time)
      base = thr->poke->get_last_present_time(thr->driver_data);
   thr->phase_from_display = base > 0 && base <= now;
   if (!thr->phase_from_display)
      base = now;

   next = base + thr->present_period;
   if (thr->present_period > 0)
      while (next <= now)
         next += thr->present_period;
   thr->next_present = next;
}

static void video_thread_loop(void *data)
{
   thread_packet_t pkt;
   unsigned slot;
   bool claimed;
   bool have_cmd;
   thread_video_t *thr = (thread_video_t*)data;

   sthread_setname("ra-video");

   for (;;)
   {
      bool repeat_due = false;

      slock_lock(thr->lock);
      while (thr->send_cmd == CMD_VIDEO_NONE && !thr->frame.pending)
      {
         /* With a frame retained, the wait has a deadline: the next
          * display period after the last present. Passing it with
          * nothing new is what a repeat is for. */
         if (thr->present_repeat && thr->present_period > 0)
         {
            retro_time_t now      = cpu_features_get_time_usec();
            retro_time_t deadline = thr->next_present;
            if (now >= deadline || thr->repeat_request)
            {
               thr->repeat_request = false;
               repeat_due          = true;
               break;
            }
            scond_wait_timeout(thr->cond_thread, thr->lock, deadline - now);
         }
         else
            scond_wait(thr->cond_thread, thr->lock);
      }

      /* Claim the oldest filled slot before releasing the lock: from
       * here until completion the slot is this thread's and the main
       * thread routes new frames to the other one. */
      claimed = thr->frame.pending > 0;
      slot    = thr->frame.tail;
      if (claimed)
      {
         thr->frame.tail    = slot ^ 1;
         thr->frame.pending--;
         thr->frame.busy    = true;
         /* The paced wait in video_thread_frame() blocks on pending
          * dropping, which just happened; tell it now rather than a
          * whole render later at completion. */
         scond_broadcast(thr->cond_ring);
      }

      /* Whether there is a command to run is decided here, under the
       * lock, together with the copy of it. cmd_data still holds the
       * previous command's reply until the sender consumes it, and
       * this thread now wakes on its own for repeats: dispatching on
       * the copy alone would run that command a second time. */
      have_cmd = thr->send_cmd != CMD_VIDEO_NONE;
      pkt      = thr->cmd_data;

      slock_unlock(thr->lock);

      if (have_cmd && video_thread_handle_packet(thr, &pkt))
         return;

      if (claimed)
      {
         struct video_viewport vp;
         uint64_t        presents = 0;
         float       refresh_rate = 0.0f;
         bool           ret_frame = false;
         bool               alive = false;
         bool               focus = false;
         bool        has_windowed = false;
         /* True unless the context says otherwise, so a driver without
          * the hook keeps pacing exactly as it did. */
         bool         presentable = true;

         vp.x                     = 0;
         vp.y                     = 0;
         vp.width                 = 0;
         vp.height                = 0;
         vp.full_width            = 0;
         vp.full_height           = 0;

         slock_lock(thr->frame.lock);

         thread_update_driver_state(thr);

         if (thr->driver_data && thr->driver)
         {
            if (thr->driver->frame)
            {
               bool ret;
               video_frame_info_t *video_info = &thr->frame.slot[slot].video_info;

               /* video_driver_build_info() resolves userdata from
                * video_driver_st, and video_thread_free() clears
                * thread_wrapper_active before this thread
                * stops, so a frame built inside that window would carry
                * the thread_video_t wrapper instead of the real driver
                * data.  This thread knows its own. The slot is discarded
                * after this call, so the driver may scribble on it. */
               video_info->userdata   = thr->driver_data;
               /* This thread presents, so it owns the swap counter; the
                * value carried from the main thread is whatever it read
                * when the frame was built and is superseded here. */
               video_info->swap_count = video_state_get_ptr()->swap_count;
               /* Retain what this frame puts on screen when the setting
                * is on and the driver can put it there again. Shader
                * sub-frames opt out: each is a different shader output
                * and only the last would be retained. BFI is fine, the
                * driver replays the whole group. */
               video_info->retain_output =
                     video_info->threaded_present_repeat
                  && thr->poke && thr->poke->present_last
                  && video_info->shader_subframes <= 1;

               ret = thr->driver->frame(thr->driver_data,
                  thr->frame.slot[slot].buffer,
                  thr->frame.slot[slot].width,
                  thr->frame.slot[slot].height,
                  thr->frame.slot[slot].count,
                  thr->frame.slot[slot].pitch,
                  *thr->frame.slot[slot].msg
                     ? thr->frame.slot[slot].msg : NULL,
                  video_info);

               slock_unlock(thr->frame.lock);

               ret_frame = ret;
               if (ret)
               {
                  /* The presenter's clock: this frame just went out,
                   * and the next one is due a display period later. */
                  float hz = thr->driver_refresh_rate > 0.0f
                     ? thr->driver_refresh_rate : video_info->refresh_rate;
                  presents = video_driver_presents_per_frame(video_info);
                  /* A repeat replays the whole group, so it is due a
                   * group's worth of display periods later. */
                  thr->present_period = hz > 0.0f
                     ? (retro_time_t)(1000000.0f * (float)presents / hz) : 0;
                  thr->present_repeat = video_info->retain_output;
                  thr->present_group  = (unsigned)presents;
                  thr->present_timing_ask =
                     video_info->present_timing_from_display;
               }
               else
                  thr->present_repeat = false;

               if (ret)
               {
                  if (thr->driver->alive)
                     alive = thr->driver->alive(thr->driver_data);
                  if (thr->driver->focus)
                     focus = thr->driver->focus(thr->driver_data);
                  if (thr->driver->has_windowed)
                     has_windowed = thr->driver->has_windowed(thr->driver_data);
                  /* Direct: this is the video thread, which owns the
                   * context, and the dispatching call would read back
                   * the value published here on the previous frame. */
                  presentable = video_context_driver_presentable_direct();
                  if (thr->poke && thr->poke->get_refresh_rate)
                     refresh_rate = thr->poke->get_refresh_rate(thr->driver_data);
               }
            }
            else
               slock_unlock(thr->frame.lock);

            if (thr->driver->viewport_info)
               thr->driver->viewport_info(thr->driver_data, &vp);
         }
         else
            slock_unlock(thr->frame.lock);

         slock_lock(thr->lock);
         thr->alive         = alive;
         thr->focus         = focus;
         thr->presentable   = presentable;
         thr->has_windowed  = has_windowed;
         thr->vp            = vp;
         /* Statistics. The viewport maths ran on this thread during
          * thr->driver->frame() above, so publish the result rather
          * than letting the main thread read video_driver_st. */
         thr->scale_width   = video_state_get_ptr()->scale_width;
         thr->scale_height  = video_state_get_ptr()->scale_height;
         /* Under the wrapper this thread owns swap_count; every advance
          * happens here, under lock, so the main thread can read it
          * consistently through video_thread_swap_count(). */
         video_state_get_ptr()->swap_count += presents;
         thr->driver_refresh_rate = refresh_rate;
         /* Under the lock: the phase it records is read by the overlay
          * from the main thread. */
         if (ret_frame)
            video_thread_schedule_next(thr);
         thr->frame.busy    = false;
         scond_broadcast(thr->cond_ring);
         slock_unlock(thr->lock);
      }
      else if (repeat_due)
      {
         /* Nothing new by the deadline: show the retained frame again
          * so the display keeps its cadence. No shader chain runs and
          * no menu texture is touched, so this is not a rendered frame
          * for the purposes of video_thread_wait_idle(). */
         unsigned swaps = 0;
         slock_lock(thr->frame.lock);
         if (thr->driver_data && thr->poke && thr->poke->present_last)
            swaps = thr->poke->present_last(thr->driver_data);
         slock_unlock(thr->frame.lock);

         slock_lock(thr->lock);
         if (swaps)
         {
            video_state_get_ptr()->swap_count += swaps;
            thr->frames_repeated++;
            video_thread_schedule_next(thr);
         }
         else
            thr->present_repeat = false;
         slock_unlock(thr->lock);
      }
   }
}

static bool video_thread_alive(void *data)
{
   bool ret;
   uint32_t runloop_flags;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return false;

   runloop_flags       = runloop_get_flags();

   if (runloop_flags & RUNLOOP_FLAG_PAUSED)
   {
      thread_packet_t pkt;
      pkt.type = CMD_ALIVE;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);

      return pkt.data.b;
   }

   slock_lock(thr->lock);
   ret = thr->alive;
   slock_unlock(thr->lock);

   return ret;
}

static bool video_thread_focus(void *data)
{
   bool ret;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return false;

   slock_lock(thr->lock);
   ret = thr->focus;
   slock_unlock(thr->lock);

   return ret;
}

static bool video_thread_suppress_screensaver(void *data, bool enable)
{
   bool ret;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return false;

   slock_lock(thr->lock);
   ret = thr->suppress_screensaver;
   slock_unlock(thr->lock);

   return ret;
}

static bool video_thread_has_windowed(void *data)
{
   bool ret;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return false;

   slock_lock(thr->lock);
   ret = thr->has_windowed;
   slock_unlock(thr->lock);

   return ret;
}

static bool video_thread_frame(void *data, const void *frame_,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   unsigned slot;
   bool dropped        = false;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return false;

   /* If called from within read_viewport, we're actually in the
    * driver thread, so just render directly. */
   if (thr->frame.within_thread)
   {
      thread_update_driver_state(thr);

      if (thr->driver_data && thr->driver && thr->driver->frame)
         return thr->driver->frame(thr->driver_data, frame_,
            width, height, frame_count, pitch, msg, video_info);

      return false;
   }

   slock_lock(thr->lock);

   if (!thr->nonblock)
   {
      retro_time_t target_frame_time =
         (retro_time_t)roundf(1000000 / video_info->refresh_rate);
      retro_time_t target            = thr->last_time + target_frame_time;

      /* Pace against the worker claiming the previous frame, not
       * finishing it: the copy below then overlaps that render.
       * Ideally, use absolute time, but that is only a good idea on POSIX. */
      while (thr->frame.pending)
      {
         retro_time_t current = cpu_features_get_time_usec();
         retro_time_t delta   = target - current;

         if (delta <= 0)
            break;

         if (!scond_wait_timeout(thr->cond_ring, thr->lock, delta))
            break;
      }
   }

   /* Pick the slot to fill. The worker renders tail ^ 1 while busy and
    * claims tail next, so a slot is free when it is neither. When both
    * are taken, the newest unclaimed frame is replaced rather than the
    * new one dropped, and the worker keeps rendering what it holds. */
   if (!thr->frame.pending)
      slot = thr->frame.tail;
   else if (thr->frame.pending == 1 && !thr->frame.busy)
      slot = thr->frame.tail ^ 1;
   else
   {
      slot = (thr->frame.pending == 2) ? (thr->frame.tail ^ 1) : thr->frame.tail;
      thr->frame.pending--;
      thr->miss_count++;
      dropped = true;
   }

   slock_unlock(thr->lock);

   {
      const uint8_t *src   = (const uint8_t*)frame_;
      uint8_t       *dst   = thr->frame.slot[slot].buffer;
      unsigned copy_stride = width *
         (thr->info.rgb32 ? sizeof(uint32_t) : sizeof(uint16_t));
      /* The slot holds the maximum geometry the core declared at init.
       * A core is free to hand over a bigger frame than that, so publish
       * only the rows that fit: the worker renders slot height rows out
       * of this same buffer, so an unclamped height would be read past
       * the end of the allocation whether or not anything was copied
       * into it. A stride too wide for a single row yields zero. */
      unsigned rows        = copy_stride
         ? (unsigned)(thr->frame.buffer_size / copy_stride)
         : 0;

      if (height > rows)
      {
         if (!thr->clamp_logged)
         {
            RARCH_WARN("[Video] Threaded video: core frame %ux%u exceeds "
                  "the declared maximum, cropping to %u rows.\n",
                  width, height, rows);
            thr->clamp_logged = true;
         }
         height            = rows;
      }

      if (src)
      {
         if (pitch == copy_stride)
            memcpy(dst, src, (size_t)height * copy_stride);
         else
         {
            unsigned i;
            for (i = 0; i < height; i++, src += pitch, dst += copy_stride)
               memcpy(dst, src, copy_stride);
         }
      }

      thr->frame.slot[slot].width  = width;
      thr->frame.slot[slot].height = height;
      thr->frame.slot[slot].count  = frame_count;
      thr->frame.slot[slot].pitch  = copy_stride;

      /* Hand the caller's video_frame_info_t across with the frame data.
       * It was built by video_driver_frame() on this thread; rebuilding
       * it on the worker races the main thread's writes to
       * video_driver_st and runloop_state. */
      if (video_info)
         thr->frame.slot[slot].video_info = *video_info;

      if (msg)
         strlcpy(thr->frame.slot[slot].msg, msg,
               sizeof(thr->frame.slot[slot].msg));
      else
         *thr->frame.slot[slot].msg = '\0';
   }

   slock_lock(thr->lock);
   thr->frame.pending++;
   scond_signal(thr->cond_thread);

#ifdef HAVE_MENU
   if (thr->texture.enable)
   {
      /* Unbounded wait that may run on the main thread; the worker can
       * marshal main-thread-only work (e.g. Vulkan swapchain recreation
       * on resize) via cocoa_main_thread_sync() before completing the
       * frame, so drain the trampoline while waiting. The timed
       * frame-pacing wait above needs no such treatment: it breaks after
       * at most one frame period and the main runloop then drains common
       * modes. */
      do
      {
         if (!video_thread_pump_wait(thr->cond_ring, thr->lock))
            scond_wait(thr->cond_ring, thr->lock);
      } while (thr->frame.pending || thr->frame.busy);
   }
#endif
   if (!dropped)
      thr->hit_count++;

   slock_unlock(thr->lock);

   thr->last_time = cpu_features_get_time_usec();

   return true;
}

static void video_thread_set_nonblock_state(void *data, bool state,
      bool adaptive_vsync_enabled,
      unsigned swap_interval)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      /* nonblock also drives this side's paced wait in
       * video_thread_frame(); the wrapped driver gets all three. */
      thr->nonblock                    = state;
      pkt.type                         = CMD_SET_NONBLOCK;
      pkt.data.nonblock.nonblock       = state;
      pkt.data.nonblock.adaptive_vsync = adaptive_vsync_enabled;
      pkt.data.nonblock.swap_interval  = swap_interval;
      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

static bool video_thread_init(thread_video_t *thr,
      const video_info_t info,
      input_driver_t **input, void **input_data)
{
   thread_packet_t pkt;

   if (!(thr->lock        = slock_new()))
      return false;
   if (!(thr->alpha_lock  = slock_new()))
      return false;
   if (!(thr->frame.lock  = slock_new()))
      return false;
   if (!(thr->cond_reply  = scond_new()))
      return false;
   if (!(thr->cond_ring   = scond_new()))
      return false;
   if (!(thr->cond_thread = scond_new()))
      return false;
   if (!(thr->cond_user   = scond_new()))
      return false;

   {
      unsigned i;
      size_t max_size        = info.input_scale * RARCH_SCALE_BASE;
      max_size              *= max_size;
      max_size              *= info.rgb32 ?
         sizeof(uint32_t) : sizeof(uint16_t);

      /* The main thread copies core frames in here and the video
       * thread reads them back for upload; a cache-line start keeps
       * both copies on aligned rows for the usual pitches. Two slots so
       * the copy of one frame overlaps the upload of the other. */
      for (i = 0; i < 2; i++)
      {
#ifdef _3DS
         thr->frame.slot[i].buffer = linearMemAlign(max_size, 0x80);
#else
         thr->frame.slot[i].buffer = (uint8_t*)memalign_alloc(64, max_size);
#endif
         if (!thr->frame.slot[i].buffer)
            return false;

         memset(thr->frame.slot[i].buffer, 0x80, max_size);
      }

      thr->frame.buffer_size = max_size;
   }

   thr->input                = input;
   thr->input_data           = input_data;
   thr->info                 = info;
   thr->alive                = true;
   thr->focus                = true;
   /* Same default the video thread applies when the context has no
    * answer, so the runloop is not told there is nothing to present to
    * during the frames before the first one completes. */
   thr->presentable          = true;
   thr->has_windowed         = true;
   thr->suppress_screensaver = true;
   thr->last_time            = cpu_features_get_time_usec();

   if (!(thr->thread = sthread_create(video_thread_loop, thr)))
      return false;

   pkt.type                  = CMD_INIT;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);

   return pkt.data.b;
}

static bool video_thread_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   thread_packet_t pkt;
   thread_video_t *thr      = (thread_video_t*)data;

   if (!thr)
      return false;

   pkt.type                 = CMD_SET_SHADER;
   pkt.data.set_shader.type = type;
   pkt.data.set_shader.path = path;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);

   return pkt.data.b;
}

static void video_thread_set_viewport(void *data, unsigned width,
      unsigned height, bool force_full, bool video_allow_rotate)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr && thr->driver_data && thr->driver && thr->driver->set_viewport)
   {
      thread_packet_t pkt;
      pkt.type                         = CMD_SET_VIEWPORT;
      pkt.data.set_viewport.width      = width;
      pkt.data.set_viewport.height     = height;
      pkt.data.set_viewport.force_full = force_full;
      pkt.data.set_viewport.allow_rotate = video_allow_rotate;
      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

static void video_thread_set_rotation(void *data, unsigned rotation)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type   = CMD_SET_ROTATION;
      pkt.data.i = rotation;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

/* This value is set async as stalling on the video driver for
 * every query is too slow.
 *
 * This means this value might not be correct, so viewport
 * reads are not supported for now. */
static void video_thread_viewport_info(void *data, struct video_viewport *vp)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      slock_lock(thr->lock);

      *vp = thr->vp;

      /* Explicitly mem-copied so we can use memcmp correctly later. */
      memcpy(&thr->read_vp, &thr->vp, sizeof(thr->read_vp));

      slock_unlock(thr->lock);
   }
}

static bool video_thread_read_viewport(void *data,
      uint8_t *buffer, bool is_idle)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return false;

   pkt.type            = CMD_READ_VIEWPORT;
   pkt.data.v          = buffer;
   thr->is_idle        = is_idle;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);

   return pkt.data.b;
}

static void video_thread_free(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      if (thr->thread)
      {
         thread_packet_t pkt;
         pkt.type = CMD_FREE;

         video_thread_send_and_wait_user_to_thread(thr, &pkt);

         sthread_join(thr->thread);
      }
      else
      {
         /* If we don't have a thread,
            we must call the driver's free function ourselves. */
         if (thr->driver_data && thr->driver && thr->driver->free)
            thr->driver->free(thr->driver_data);
      }

      /* After the join, not before it: the video thread reads this
       * from inside driver frame callbacks, so clearing it while that
       * thread still runs is a write racing those reads - and it
       * briefly tells the rest of the frontend the wrapper is gone
       * while its thread is still presenting. */
      video_state_get_ptr()->thread_wrapper_active = false;

      free(thr->texture.frame);
#ifdef _3DS
      linearFree(thr->frame.slot[0].buffer);
      linearFree(thr->frame.slot[1].buffer);
#else
      memalign_free(thr->frame.slot[0].buffer);
      memalign_free(thr->frame.slot[1].buffer);
#endif
      free(thr->alpha_mod);

      slock_free(thr->frame.lock);
      slock_free(thr->alpha_lock);
      slock_free(thr->lock);
      scond_free(thr->cond_reply);
      scond_free(thr->cond_ring);
      scond_free(thr->cond_thread);
      scond_free(thr->cond_user);

      RARCH_LOG(
         "Threaded video stats: Frames pushed: %u, Frames dropped: %u, Frames repeated: %llu.\n",
         thr->hit_count, thr->miss_count,
         (unsigned long long)thr->frames_repeated);

      /* video_init_thread() pointed the video state at the vtable
       * embedded in this struct. Point it back at the wrapped driver's
       * static vtable before the struct goes away, so a later
       * video_driver_free_internal() reading current_video sees a live
       * driver, as it does without threading. */
      if (video_state_get_ptr()->current_video == &thr->video_thread)
         video_state_get_ptr()->current_video = (video_driver_t*)thr->driver;

      free(thr);
   }
}

#ifdef HAVE_OVERLAY
static void thread_overlay_enable(void *data, bool state)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type   = CMD_OVERLAY_ENABLE;
      pkt.data.b = state;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

static bool thread_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return false;

   pkt.type            = CMD_OVERLAY_LOAD;
   pkt.data.image.data = (const struct texture_image*)image_data;
   pkt.data.image.num  = num_images;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);

   return pkt.data.b;
}

static void thread_overlay_tex_geom(void *data,
      unsigned idx, float x, float y, float w, float h)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type            = CMD_OVERLAY_TEX_GEOM;
      pkt.data.rect.index = idx;
      pkt.data.rect.x     = x;
      pkt.data.rect.y     = y;
      pkt.data.rect.w     = w;
      pkt.data.rect.h     = h;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

static void thread_overlay_vertex_geom(void *data,
      unsigned idx, float x, float y, float w, float h)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type            = CMD_OVERLAY_VERTEX_GEOM;
      pkt.data.rect.index = idx;
      pkt.data.rect.x     = x;
      pkt.data.rect.y     = y;
      pkt.data.rect.w     = w;
      pkt.data.rect.h     = h;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

static void thread_overlay_full_screen(void *data, bool enable)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type   = CMD_OVERLAY_FULL_SCREEN;
      pkt.data.b = enable;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

/* We cannot wait for this to complete. Totally blocks the main thread. */
static void thread_overlay_set_alpha(void *data, unsigned idx, float mod)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      slock_lock(thr->alpha_lock);
      thr->alpha_mod[idx] = mod;
      thr->alpha_update   = true;
      slock_unlock(thr->alpha_lock);
   }
}

static const video_overlay_interface_t thread_overlay = {
   thread_overlay_enable,
   thread_overlay_load,
   thread_overlay_tex_geom,
   thread_overlay_vertex_geom,
   thread_overlay_full_screen,
   thread_overlay_set_alpha,
};

static void video_thread_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr && thr->driver_data &&
         thr->driver && thr->driver->overlay_interface)
   {
      thr->driver->overlay_interface(thr->driver_data, &thr->overlay);
      *iface = &thread_overlay;
   }
   else
      *iface = NULL;
}
#endif

static void thread_set_video_mode(void *data,
      unsigned width, unsigned height, bool video_fullscreen)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type                     = CMD_POKE_SET_VIDEO_MODE;
      pkt.data.new_mode.width      = width;
      pkt.data.new_mode.height     = height;
      pkt.data.new_mode.fullscreen = video_fullscreen;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

static void thread_set_filtering(void *data,
      unsigned idx, bool smooth, bool ctx_scaling)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type                  = CMD_POKE_SET_FILTERING;
      pkt.data.filtering.index  = idx;
      pkt.data.filtering.smooth = smooth;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

static void thread_set_hdr_menu_nits(void *data, float menu_nits)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type               = CMD_POKE_SET_HDR_MENU_NITS;
      pkt.data.hdr.menu_nits = menu_nits;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

static void thread_set_hdr_paper_white_nits(void *data, float paper_white_nits)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type                      = CMD_POKE_SET_HDR_PAPER_WHITE_NITS;
      pkt.data.hdr.paper_white_nits = paper_white_nits;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

static void thread_set_hdr_expand_gamut(void *data, unsigned expand_gamut)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type                  = CMD_POKE_SET_HDR_EXPAND_GAMUT;
      pkt.data.hdr.expand_gamut = expand_gamut;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

static void thread_set_hdr_scanlines(void *data, bool hdr_scanlines)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type                = CMD_POKE_SET_HDR_SCANLINES;
      pkt.data.hdr.scanlines  = hdr_scanlines;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

static void thread_set_hdr_subpixel_layout(void *data, unsigned hdr_subpixel_layout)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type                        = CMD_POKE_SET_HDR_SUBPIXEL_LAYOUT;
      pkt.data.hdr.subpixel_layout    = hdr_subpixel_layout;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}


static void thread_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr && thr->driver_data &&
         thr->poke && thr->poke->get_video_output_size)
      thr->poke->get_video_output_size(thr->driver_data,
         width, height, desc, desc_len);
}

static void thread_get_video_output_prev(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr && thr->driver_data &&
         thr->poke && thr->poke->get_video_output_prev)
      thr->poke->get_video_output_prev(thr->driver_data);
}

static void thread_get_video_output_next(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr && thr->driver_data &&
         thr->poke && thr->poke->get_video_output_next)
      thr->poke->get_video_output_next(thr->driver_data);
}

static void thread_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type   = CMD_POKE_SET_ASPECT_RATIO;
      pkt.data.i = aspect_ratio_idx;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

static void thread_set_texture_frame(void *data, const void *frame,
      bool rgb32, unsigned width, unsigned height, float alpha)
{
   thread_video_t *thr = (thread_video_t*)data;
   size_t required     = width * height *
      (rgb32 ? sizeof(uint32_t) : sizeof(uint16_t));

   if (!thr)
      return;

   slock_lock(thr->frame.lock);

   if (!thr->texture.frame || required > thr->texture.frame_cap)
   {
      void *tmp_frame = realloc(thr->texture.frame, required);

      if (!tmp_frame)
      {
         slock_unlock(thr->frame.lock);
         return;
      }

      thr->texture.frame     = tmp_frame;
      thr->texture.frame_cap = required;
   }

   memcpy(thr->texture.frame, frame, required);

   thr->texture.rgb32         = rgb32;
   thr->texture.width         = width;
   thr->texture.height        = height;
   thr->texture.alpha         = alpha;
   thr->texture.frame_updated = true;

   slock_unlock(thr->frame.lock);
}

static void thread_set_texture_enable(void *data, bool state, bool full_screen)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      slock_lock(thr->frame.lock);
      thr->texture.enable      = state;
      thr->texture.full_screen = full_screen;
      slock_unlock(thr->frame.lock);
   }
}

static void thread_set_osd_msg(void *data,
      const char *msg, size_t msg_len,
      const struct font_params *params, void *font)
{
   thread_video_t *thr = (thread_video_t*)data;

   /* TODO : find a way to determine if the calling
    * thread is the driver thread or not. */
   if (thr && thr->driver_data && thr->poke && thr->poke->set_osd_msg)
      thr->poke->set_osd_msg(thr->driver_data, msg, msg_len, params, font);
}

static void thread_show_mouse(void *data, bool state)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type   = CMD_POKE_SHOW_MOUSE;
      pkt.data.b = state;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

static void thread_grab_mouse_toggle(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      thread_packet_t pkt;
      pkt.type = CMD_POKE_GRAB_MOUSE_TOGGLE;

      video_thread_send_and_wait_user_to_thread(thr, &pkt);
   }
}

static uintptr_t thread_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   thread_video_t *thr = (thread_video_t*)video_data;

   if (thr && thr->driver_data && thr->poke && thr->poke->load_texture)
      return thr->poke->load_texture(thr->driver_data,
         data, threaded, filter_type);

   return 0;
}

static void thread_unload_texture(void *data,
      bool threaded, uintptr_t id)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr && thr->driver_data && thr->poke && thr->poke->unload_texture)
   {
      /* Releasing a GPU texture while the video thread is mid-frame can
       * free something the in-flight frame still references -- the AI
       * service overlay is drawn straight from
       * dispgfx_widget_t::ai_service_overlay_texture after a plain
       * ai_service_overlay_state test, with no handshake.  Drain any
       * pending frame first; no-op when this is the video thread or
       * when the wrapper is not running. */
      video_thread_wait_idle();
      thr->poke->unload_texture(thr->driver_data, threaded, id);
   }
}

static void thread_apply_state_changes(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr)
   {
      slock_lock(thr->frame.lock);
      thr->apply_state_changes = true;
      slock_unlock(thr->frame.lock);
   }
}

/* This is read-only state which should not
 * have any kind of race condition. */
static struct video_shader *thread_get_current_shader(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr && thr->driver_data && thr->poke && thr->poke->get_current_shader)
      return thr->poke->get_current_shader(thr->driver_data);

   return NULL;
}

static uint32_t thread_get_flags(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr && thr->driver_data && thr->poke && thr->poke->get_flags)
      return thr->poke->get_flags(thr->driver_data);

   return 0;
}

static bool thread_supports_texture_format(void *video_data,
      enum texture_gpu_format fmt)
{
   thread_video_t *thr = (thread_video_t*)video_data;
   if (     thr
         && thr->driver_data
         && thr->poke
         && thr->poke->supports_texture_format)
      return thr->poke->supports_texture_format(thr->driver_data, fmt);
   return false;
}

/* Forward the compressed upload with 'threaded' passed through, exactly as
 * thread_load_texture does. The underlying driver decides whether to marshal
 * the GPU work onto the video thread; the descriptor stays alive because
 * video_thread_texture_handle is synchronous. */
static uintptr_t thread_load_texture_compressed(void *video_data,
      const struct texture_compressed *tc, bool threaded,
      enum texture_filter_type filter_type)
{
   thread_video_t *thr = (thread_video_t*)video_data;
   if (     thr
         && thr->driver_data
         && thr->poke
         && thr->poke->load_texture_compressed)
      return thr->poke->load_texture_compressed(thr->driver_data,
         tc, threaded, filter_type);
   return 0;
}

static float thread_get_refresh_rate(void *data)
{
   float ret;
   thread_video_t *thr = (thread_video_t*)data;
   if (!thr)
      return 0.0f;
   slock_lock(thr->lock);
   ret = thr->driver_refresh_rate;
   slock_unlock(thr->lock);
   return ret;
}

/* Asks the video thread to show the retained frame again as soon as it
 * is idle. Returns whether there is a retained frame to show; the
 * present itself happens on the video thread. */
static unsigned thread_present_last(void *data)
{
   unsigned ret = 0;
   thread_video_t *thr = (thread_video_t*)data;
   if (!thr)
      return 0;
   slock_lock(thr->lock);
   if (thr->present_repeat)
   {
      thr->repeat_request = true;
      scond_signal(thr->cond_thread);
      ret = thr->present_group;
   }
   slock_unlock(thr->lock);
   return ret;
}

static const video_poke_interface_t thread_poke = {
   thread_get_flags,
   thread_load_texture,
   thread_unload_texture,
   thread_set_video_mode,
   thread_get_refresh_rate,
   thread_set_filtering,
   thread_get_video_output_size,
   thread_get_video_output_prev,
   thread_get_video_output_next,
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   thread_set_aspect_ratio,
   thread_apply_state_changes,
   thread_set_texture_frame,
   thread_set_texture_enable,
   thread_set_osd_msg,
   thread_show_mouse,
   thread_grab_mouse_toggle,
   thread_get_current_shader,
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   thread_set_hdr_menu_nits,
   thread_set_hdr_paper_white_nits,
   thread_set_hdr_expand_gamut,
   thread_set_hdr_scanlines,
   thread_set_hdr_subpixel_layout,
   thread_supports_texture_format,
   thread_load_texture_compressed,
   thread_present_last,
   NULL  /* get_last_present_time: consumed on the video thread */
};

static void video_thread_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr && thr->driver_data &&
         thr->driver && thr->driver->poke_interface)
   {
      thr->driver->poke_interface(thr->driver_data, &thr->poke);
      *iface = &thread_poke;
   }
   else
      *iface = NULL;
}

#ifdef HAVE_GFX_WIDGETS
static bool video_thread_wrapper_gfx_widgets_enabled(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr && thr->driver_data &&
         thr->driver && thr->driver->gfx_widgets_enabled)
      return thr->driver->gfx_widgets_enabled(thr->driver_data);

   return false;
}
#endif

static const video_driver_t video_thread = {
   video_thread_init_never_call, /* Should never be called directly. */
   video_thread_frame,
   video_thread_set_nonblock_state,
   video_thread_alive,
   video_thread_focus,
   video_thread_suppress_screensaver,
   video_thread_has_windowed,
   video_thread_set_shader,
   video_thread_free,
   "Thread wrapper",
   video_thread_set_viewport,
   video_thread_set_rotation,
   video_thread_viewport_info,
   video_thread_read_viewport,
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   video_thread_get_overlay_interface,
#endif
   video_thread_get_poke_interface,
   NULL, /* wrap_type_to_enum */
   NULL, /* shader_load_begin */
   NULL, /* shader_load_step */
#ifdef HAVE_GFX_WIDGETS
   video_thread_wrapper_gfx_widgets_enabled
#endif
};

static void video_thread_set_callbacks(thread_video_t *thr,
      const video_driver_t *drv)
{
   thr->video_thread = video_thread;
   thr->driver       = drv;

   if (drv)
   {
      /* Disable optional features if not present. */
      if (!drv->read_viewport)
         thr->video_thread.read_viewport     = NULL;
      if (!drv->set_viewport)
         thr->video_thread.set_viewport      = NULL;
      if (!drv->set_rotation)
         thr->video_thread.set_rotation      = NULL;
      if (!drv->set_shader)
         thr->video_thread.set_shader        = NULL;
#ifdef HAVE_OVERLAY
      if (!drv->overlay_interface)
         thr->video_thread.overlay_interface = NULL;
#endif
      if (!drv->poke_interface)
         thr->video_thread.poke_interface    = NULL;
   }
}

/**
 * video_init_thread:
 * @out_driver                : Output video driver
 * @out_data                  : Output video data
 * @input                     : Input input driver
 * @input_data                : Input input data
 * @driver                    : Input Video driver
 * @info                      : Video info handle.
 *
 * Creates, initializes and starts a video driver in a new thread.
 * Access to video driver will be mediated through this driver.
 *
 * Returns: true (1) if successful, otherwise false (0).
 **/
bool video_init_thread(const video_driver_t **out_driver, void **out_data,
      input_driver_t **input, void **input_data,
      const video_driver_t *drv, const video_info_t info)
{
   thread_video_t *thr = (thread_video_t*)calloc(1, sizeof(*thr));
   if (!thr)
      return false;

   video_thread_set_callbacks(thr, drv);

   thr->driver = drv;
   *out_driver = &thr->video_thread;
   *out_data   = thr;

   /* Mark the wrapper active before running the underlying driver's
    * init(): that init() runs on the worker thread and may query
    * video_driver_get_ident() (e.g. via the context driver's get_flags
    * for shader-backend detection).  current_video already points at the
    * thread wrapper here, so without the flag set get_ident() would
    * resolve to "Thread wrapper" instead of the wrapped driver ("glcore"),
    * causing shader-backend detection to fail. */
   video_state_get_ptr()->thread_wrapper_active = true;
   if (!video_thread_init(thr, info, input, input_data))
   {
      /* video_thread is a member of thr, not a static vtable, so leaving
       * it published hands the caller freed memory once thr goes.
       * Restore drv and NULL the data, as the non-threaded failure path
       * does.  Free via video_thread_free(): init can fail after the
       * worker thread and the frame buffer already exist. */
      video_thread_free(thr);
      *out_driver = drv;
      *out_data   = NULL;
      return false;
   }

   return true;
}

bool video_thread_font_init(const void **font_driver, void **font_handle,
      void *data, const char *font_path, float video_font_size,
      const font_renderer_t *backend, custom_font_command_method_t func,
      bool is_threaded)
{
   thread_packet_t pkt;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t       *thr;

   /* Only safe to interpret video_st->data as a thread_video_t*
    * when the threaded video wrapper is actually active.  During
    * driver reinit, is_threaded may already reflect the new
    * configuration while video_st->data still points to the
    * previous (possibly non-threaded) driver's private state. */
   if (!video_st->thread_wrapper_active)
      return false;

   thr = (thread_video_t*)video_st->data;

   if (!thr)
      return false;

   pkt.type                       = CMD_FONT_INIT;
   pkt.data.font_init.method      = func;
   pkt.data.font_init.font_driver = font_driver;
   pkt.data.font_init.font_handle = font_handle;
   pkt.data.font_init.video_data  = data;
   pkt.data.font_init.font_path   = font_path;
   pkt.data.font_init.font_size   = video_font_size;
   pkt.data.font_init.is_threaded = is_threaded;
   pkt.data.font_init.backend         = backend;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);

   return pkt.data.font_init.return_value;
}

uintptr_t video_thread_texture_handle(void *data, custom_command_method_t func)
{
   thread_packet_t pkt;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t       *thr;

   /* Only safe to interpret video_st->data as a thread_video_t*
    * when the threaded video wrapper is actually active.  During
    * driver reinit, callers' "threaded" flags may already reflect
    * the new configuration while video_st->data still points to
    * the previous driver's private state.  Fall back to calling
    * func directly (same contract as the "already on video
    * thread" branch below). */
   if (!video_st->thread_wrapper_active)
      return func(data);

   thr = (thread_video_t*)video_st->data;

   if (!thr)
      return 0;

   /* if we're already on the video thread, just call the function, otherwise
    * we may deadlock with ourself waiting for the packet to be processed. */
   if (sthread_get_thread_id(thr->thread) == sthread_get_current_thread_id())
      return func(data);

   pkt.type                       = CMD_CUSTOM_COMMAND;
   pkt.data.custom_command.method = func;
   pkt.data.custom_command.data   = data;

   /* Aliveness is tested inside the send, under the lock it already
    * takes.  Reading thr->alive here instead would race the worker's
    * write in video_thread_loop(). */
   video_thread_user_acquire(thr);
   if (!video_thread_send_packet_if_alive(thr, &pkt))
   {
      video_thread_user_release(thr);
      return func(data);
   }

   video_thread_wait_reply(thr, &pkt);
   video_thread_user_release(thr);

   return pkt.data.custom_command.return_value;
}

/* Waits until the video thread has finished processing any
 * pending frame and is idle, waiting on its command condition
 * variable.  After this returns, it is safe to free GPU-backed
 * resources (textures, fonts) owned by the menu driver — no
 * frame can be in-flight referencing them.
 *
 * Must be called from the main thread.  No-op if the video
 * thread is not running or if called from the video thread
 * itself (would deadlock). */
bool video_thread_presentable(void)
{
   bool ret;
   thread_video_t *thr;
   video_driver_state_t *video_st = video_state_get_ptr();

   /* video_st->data is the thread_video_t only while the wrapper is
    * installed. It is taken raw here: video_driver_get_ptr() unwraps
    * the wrapper and hands back the concrete driver's private data,
    * which is not a thread_video_t and has no lock at that offset. */
   if (!video_st->thread_wrapper_active)
      return true;
   if (!(thr = (thread_video_t*)video_st->data) || !thr->thread)
      return true;

   /* The video thread publishes this value under thr->lock at the end
    * of each frame; it reads its own context directly. */
   if (sthread_get_thread_id(thr->thread) == sthread_get_current_thread_id())
      return video_context_driver_presentable_direct();

   slock_lock(thr->lock);
   ret = thr->presentable;
   slock_unlock(thr->lock);
   return ret;
}

/* Presenter statistics for the overlay: repeats made this session, and
 * whether their cadence is phase-locked to the display. Under the lock;
 * false/0 without the wrapper. Returns whether repeats are armed. */
bool video_thread_presenter_stats(uint64_t *repeats, bool *display_phase)
{
   bool armed;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t       *thr;
   *repeats       = 0;
   *display_phase = false;
   if (!video_st->thread_wrapper_active)
      return false;
   if (!(thr = (thread_video_t*)video_st->data) || !thr->thread)
      return false;
   slock_lock(thr->lock);
   armed          = thr->present_repeat;
   *repeats       = thr->frames_repeated;
   *display_phase = thr->phase_from_display;
   slock_unlock(thr->lock);
   return armed;
}

uint64_t video_thread_swap_count(void)
{
   uint64_t ret;
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t       *thr;
   if (!video_st->thread_wrapper_active)
      return video_st->swap_count;
   if (!(thr = (thread_video_t*)video_st->data) || !thr->thread)
      return video_st->swap_count;
   if (sthread_get_thread_id(thr->thread) == sthread_get_current_thread_id())
      return video_st->swap_count;
   slock_lock(thr->lock);
   ret = video_st->swap_count;
   slock_unlock(thr->lock);
   return ret;
}

void video_thread_wait_idle(void)
{
   video_driver_state_t *video_st = video_state_get_ptr();
   thread_video_t       *thr;

   /* Only safe to interpret video_st->data as a thread_video_t*
    * when the threaded video wrapper is actually active.  With
    * non-threaded video, video_st->data points to the raw
    * driver's private state. */
   if (!video_st->thread_wrapper_active)
      return;

   thr = (thread_video_t*)video_st->data;

   if (!thr || !thr->thread)
      return;

   /* Avoid self-deadlock if called from the video thread. */
   if (sthread_get_thread_id(thr->thread) == sthread_get_current_thread_id())
      return;

   slock_lock(thr->lock);
   while (thr->frame.pending || thr->frame.busy)
      scond_wait(thr->cond_ring, thr->lock);
   slock_unlock(thr->lock);
}

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
#include <string/stdstring.h>

#include "video_thread_wrapper.h"
#include "font_driver.h"

#include "../retroarch.h"
#include "../verbosity.h"

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
   slock_lock(thr->lock);

   thr->cmd_data  = *pkt;

   thr->reply_cmd = pkt->type;
   thr->send_cmd  = CMD_VIDEO_NONE;

   scond_signal(thr->cond_cmd);
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

/* user -> thread */
static void video_thread_wait_reply(thread_video_t *thr, thread_packet_t *pkt)
{
   slock_lock(thr->lock);

   while (pkt->type != thr->reply_cmd)
      scond_wait(thr->cond_cmd, thr->lock);

   *pkt               = thr->cmd_data;
   thr->cmd_data.type = CMD_VIDEO_NONE;

   slock_unlock(thr->lock);
}

/* user -> thread */
static void video_thread_send_and_wait_user_to_thread(thread_video_t *thr, thread_packet_t *pkt)
{
   video_thread_send_packet(thr, pkt);
   video_thread_wait_reply(thr, pkt);
}

static void thread_update_driver_state(thread_video_t *thr)
{
#if defined(HAVE_MENU)
   if (thr->texture.frame_updated)
   {
      if (thr->poke && thr->poke->set_texture_frame)
         thr->poke->set_texture_frame(thr->driver_data,
               thr->texture.frame, thr->texture.rgb32,
               thr->texture.width, thr->texture.height,
               thr->texture.alpha);
      thr->texture.frame_updated = false;
   }

   if (thr->poke && thr->poke->set_texture_enable)
      thr->poke->set_texture_enable(thr->driver_data,
            thr->texture.enable, thr->texture.full_screen);
#endif

#if defined(HAVE_OVERLAY)
   slock_lock(thr->alpha_lock);
   if (thr->alpha_update)
   {
      unsigned i;
      for (i = 0; i < thr->alpha_mods; i++)
      {
         if (thr->overlay && thr->overlay->set_alpha)
            thr->overlay->set_alpha(thr->driver_data, i, thr->alpha_mod[i]);
      }
      thr->alpha_update = false;
   }
   slock_unlock(thr->alpha_lock);
#endif

   if (thr->apply_state_changes)
   {
      if (thr->poke && thr->poke->apply_state_changes)
         thr->poke->apply_state_changes(thr->driver_data);
      thr->apply_state_changes = false;
   }
}

/* returns true when video_thread_loop should quit */
static bool video_thread_handle_packet(
      thread_video_t *thr,
      const thread_packet_t *incoming)
{
#ifdef HAVE_OVERLAY
   unsigned i;
#endif
   thread_packet_t pkt = *incoming;
   bool            ret = false;

   switch (pkt.type)
   {
      case CMD_INIT:
         thr->driver_data = thr->driver->init(&thr->info,
               thr->input, thr->input_data);
         pkt.data.b = (thr->driver_data != NULL);
         thr->driver->viewport_info(thr->driver_data, &thr->vp);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_FREE:
         if (thr->driver_data)
         {
            if (thr->driver && thr->driver->free)
               thr->driver->free(thr->driver_data);
         }
         thr->driver_data = NULL;
         video_thread_reply(thr, &pkt);
         return true;

      case CMD_SET_ROTATION:
         if (thr->driver && thr->driver->set_rotation)
            thr->driver->set_rotation(thr->driver_data, pkt.data.i);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_READ_VIEWPORT:
      {
         struct video_viewport vp;

         vp.x                     = 0;
         vp.y                     = 0;
         vp.width                 = 0;
         vp.height                = 0;
         vp.full_width            = 0;
         vp.full_height           = 0;

         thr->driver->viewport_info(thr->driver_data, &vp);

         if (string_is_equal_fast(&vp, &thr->read_vp, sizeof(vp)))
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

            if (thr->driver->read_viewport)
               ret = thr->driver->read_viewport(thr->driver_data,
                     (uint8_t*)pkt.data.v, thr->is_idle);

            pkt.data.b = ret;
            thr->frame.within_thread = false;
         }
         else
         {
            /* Viewport dimensions changed right after main
             * thread read the async value. Cannot read safely. */
            pkt.data.b = false;
         }
         video_thread_reply(thr, &pkt);
         break;
      }

      case CMD_SET_SHADER:
         if (thr->driver && thr->driver->set_shader)
            ret = thr->driver->set_shader(thr->driver_data,
                     pkt.data.set_shader.type,
                     pkt.data.set_shader.path);

         pkt.data.b = ret;
         video_thread_reply(thr, &pkt);
         break;

      case CMD_ALIVE:
         if (thr->driver && thr->driver->alive)
            ret = thr->driver->alive(thr->driver_data);

         pkt.data.b = ret;
         video_thread_reply(thr, &pkt);
         break;

#ifdef HAVE_OVERLAY
      case CMD_OVERLAY_ENABLE:
         if (thr->overlay && thr->overlay->enable)
            thr->overlay->enable(thr->driver_data, pkt.data.b);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_OVERLAY_LOAD:
         {
            if (thr->overlay && thr->overlay->load)
               ret = thr->overlay->load(thr->driver_data,
                     pkt.data.image.data,
                     pkt.data.image.num);

            pkt.data.b         = ret;
            thr->alpha_mods    = pkt.data.image.num;

            if (thr->alpha_mods > 0)
            {
               float *tmp_alpha_mod = (float*)realloc(thr->alpha_mod,
                     thr->alpha_mods * sizeof(float));

               if (tmp_alpha_mod)
                  thr->alpha_mod = tmp_alpha_mod;
            }
            else
            {
               if (thr->alpha_mod)
                  free(thr->alpha_mod);
               thr->alpha_mod = NULL;
            }

            /* Avoid temporary garbage data. */
            for (i = 0; i < thr->alpha_mods; i++)
               thr->alpha_mod[i] = 1.0f;

            video_thread_reply(thr, &pkt);
         }
         break;

      case CMD_OVERLAY_TEX_GEOM:
         if (thr->overlay && thr->overlay->tex_geom)
            thr->overlay->tex_geom(thr->driver_data,
                  pkt.data.rect.index,
                  pkt.data.rect.x,
                  pkt.data.rect.y,
                  pkt.data.rect.w,
                  pkt.data.rect.h);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_OVERLAY_VERTEX_GEOM:
         if (thr->overlay && thr->overlay->vertex_geom)
            thr->overlay->vertex_geom(thr->driver_data,
                  pkt.data.rect.index,
                  pkt.data.rect.x,
                  pkt.data.rect.y,
                  pkt.data.rect.w,
                  pkt.data.rect.h);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_OVERLAY_FULL_SCREEN:
         if (thr->overlay && thr->overlay->full_screen)
            thr->overlay->full_screen(thr->driver_data,
                  pkt.data.b);
         video_thread_reply(thr, &pkt);
         break;
#endif

      case CMD_POKE_SET_VIDEO_MODE:
         if (thr->poke && thr->poke->set_video_mode)
            thr->poke->set_video_mode(thr->driver_data,
                  pkt.data.new_mode.width,
                  pkt.data.new_mode.height,
                  pkt.data.new_mode.fullscreen);
         video_thread_reply(thr, &pkt);
         break;
      case CMD_POKE_SET_FILTERING:
         if (thr->poke && thr->poke->set_filtering)
            thr->poke->set_filtering(thr->driver_data,
                  pkt.data.filtering.index,
                  pkt.data.filtering.smooth,
                  pkt.data.filtering.ctx_scaling);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_POKE_SET_ASPECT_RATIO:
         if (thr->poke && thr->poke->set_aspect_ratio)
            thr->poke->set_aspect_ratio(thr->driver_data,
                  pkt.data.i);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_FONT_INIT:
         if (pkt.data.font_init.method)
            pkt.data.font_init.return_value =
                  pkt.data.font_init.method
                  (pkt.data.font_init.font_driver,
                     pkt.data.font_init.font_handle,
                     pkt.data.font_init.video_data,
                     pkt.data.font_init.font_path,
                     pkt.data.font_init.font_size,
                     pkt.data.font_init.api,
                     pkt.data.font_init.is_threaded);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_CUSTOM_COMMAND:
         if (pkt.data.custom_command.method)
            pkt.data.custom_command.return_value =
                  pkt.data.custom_command.method
                  (pkt.data.custom_command.data);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_POKE_SHOW_MOUSE:
         if (thr->poke && thr->poke->show_mouse)
            thr->poke->show_mouse(thr->driver_data,
                  pkt.data.b);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_POKE_GRAB_MOUSE_TOGGLE:
         if (thr->poke && thr->poke->grab_mouse_toggle)
            thr->poke->grab_mouse_toggle(thr->driver_data);
         video_thread_reply(thr, &pkt);
         break;

      case CMD_VIDEO_NONE:
         /* Never reply on no command. Possible deadlock if
          * thread sends command right after frame update. */
         break;
         
      case CMD_POKE_SET_HDR_MAX_NITS:
         if (thr->poke && thr->poke->set_hdr_max_nits)
            thr->poke->set_hdr_max_nits(thr->driver_data,
                  pkt.data.hdr.max_nits);
         video_thread_reply(thr, &pkt);
         break;
         
      case CMD_POKE_SET_HDR_PAPER_WHITE_NITS:
         if (thr->poke && thr->poke->set_hdr_paper_white_nits)
            thr->poke->set_hdr_paper_white_nits(thr->driver_data,
                  pkt.data.hdr.paper_white_nits);
         video_thread_reply(thr, &pkt);
         break;
         
      case CMD_POKE_SET_HDR_CONTRAST:
         if (thr->poke && thr->poke->set_hdr_contrast)
            thr->poke->set_hdr_contrast(thr->driver_data,
                  pkt.data.hdr.contrast);
         video_thread_reply(thr, &pkt);
         break;
         
      case CMD_POKE_SET_HDR_EXPAND_GAMUT:
         if (thr->poke && thr->poke->set_hdr_expand_gamut)
            thr->poke->set_hdr_expand_gamut(thr->driver_data,
                  pkt.data.hdr.expand_gamut);
         video_thread_reply(thr, &pkt);
         break;
      default:
         video_thread_reply(thr, &pkt);
         break;
   }

   return false;
}

static void video_thread_loop(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;

   for (;;)
   {
      thread_packet_t pkt;
      bool updated = false;

      slock_lock(thr->lock);
      while (thr->send_cmd == CMD_VIDEO_NONE && !thr->frame.updated)
         scond_wait(thr->cond_thread, thr->lock);
      if (thr->frame.updated)
         updated = true;

      /* To avoid race condition where send_cmd is updated
       * right after the switch is checked. */
      pkt = thr->cmd_data;

      slock_unlock(thr->lock);

      if (video_thread_handle_packet(thr, &pkt))
         return;

      if (updated)
      {
         struct video_viewport vp;
         bool                 ret = false;
         bool               alive = false;
         bool               focus = false;
         bool        has_windowed = true;

         vp.x                     = 0;
         vp.y                     = 0;
         vp.width                 = 0;
         vp.height                = 0;
         vp.full_width            = 0;
         vp.full_height           = 0;

         slock_lock(thr->frame.lock);

         thread_update_driver_state(thr);

         if (thr->driver && thr->driver->frame)
         {
            video_frame_info_t video_info;
            /* TODO/FIXME - not thread-safe - should get 
             * rid of this */
            video_driver_build_info(&video_info);

            ret = thr->driver->frame(thr->driver_data,
                  thr->frame.buffer, thr->frame.width, thr->frame.height,
                  thr->frame.count,
                  thr->frame.pitch, *thr->frame.msg ? thr->frame.msg : NULL,
                  &video_info);
         }

         slock_unlock(thr->frame.lock);

         if (thr->driver && thr->driver->alive)
            alive = ret && thr->driver->alive(thr->driver_data);

         if (thr->driver && thr->driver->focus)
            focus = ret && thr->driver->focus(thr->driver_data);

         if (thr->driver && thr->driver->has_windowed)
            has_windowed = ret && thr->driver->has_windowed(thr->driver_data);

         if (thr->driver && thr->driver->viewport_info)
            thr->driver->viewport_info(thr->driver_data, &vp);

         slock_lock(thr->lock);
         thr->alive         = alive;
         thr->focus         = focus;
         thr->has_windowed  = has_windowed;
         thr->frame.updated = false;
         thr->vp            = vp;
         scond_signal(thr->cond_cmd);
         slock_unlock(thr->lock);
      }
   }
}

static bool video_thread_alive(void *data)
{
   bool ret;
   thread_video_t *thr = (thread_video_t*)data;

   if (retroarch_ctl(RARCH_CTL_IS_PAUSED, NULL))
   {
      thread_packet_t pkt;

      pkt.type            = CMD_ALIVE;
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

   slock_lock(thr->lock);
   ret = thr->focus;
   slock_unlock(thr->lock);

   return ret;
}

static bool video_thread_suppress_screensaver(void *data, bool enable)
{
   bool ret;
   thread_video_t *thr = (thread_video_t*)data;

   slock_lock(thr->lock);
   ret = thr->suppress_screensaver;
   slock_unlock(thr->lock);

   return ret;
}

static bool video_thread_has_windowed(void *data)
{
   bool ret;
   thread_video_t *thr = (thread_video_t*)data;

   slock_lock(thr->lock);
   ret = thr->has_windowed;
   slock_unlock(thr->lock);

   return ret;
}

static bool video_thread_frame(void *data, const void *frame_,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   unsigned copy_stride;
   const uint8_t *src                  = NULL;
   uint8_t *dst                        = NULL;
   thread_video_t *thr                 = (thread_video_t*)data;

   /* If called from within read_viewport, we're actually in the
    * driver thread, so just render directly. */
   if (thr->frame.within_thread)
   {
      thread_update_driver_state(thr);

      if (thr->driver && thr->driver->frame)
         return thr->driver->frame(thr->driver_data, frame_,
               width, height, frame_count, pitch, msg, video_info);
      return false;
   }

   copy_stride = width * (thr->info.rgb32
         ? sizeof(uint32_t) : sizeof(uint16_t));

   src = (const uint8_t*)frame_;
   dst = thr->frame.buffer;

   slock_lock(thr->lock);

   if (!thr->nonblock)
   {

      retro_time_t target_frame_time = (retro_time_t)
         roundf(1000000 / video_info->refresh_rate);
      retro_time_t target = thr->last_time + target_frame_time;

      /* Ideally, use absolute time, but that is only a good idea on POSIX. */
      while (thr->frame.updated)
      {
         retro_time_t current = cpu_features_get_time_usec();
         retro_time_t delta   = target - current;

         if (delta <= 0)
            break;

         if (!scond_wait_timeout(thr->cond_cmd, thr->lock, delta))
            break;
      }
   }

   /* Drop frame if updated flag is still set, as thread is
    * still working on last frame. */
   if (!thr->frame.updated)
   {
      if (src)
      {
         unsigned h;
         for (h = 0; h < height; h++, src += pitch, dst += copy_stride)
            memcpy(dst, src, copy_stride);
      }

      thr->frame.updated = true;
      thr->frame.width  = width;
      thr->frame.height = height;
      thr->frame.count  = frame_count;
      thr->frame.pitch  = copy_stride;

      if (msg)
         strlcpy(thr->frame.msg, msg, sizeof(thr->frame.msg));
      else
         *thr->frame.msg = '\0';

      scond_signal(thr->cond_thread);

#if defined(HAVE_MENU)
      if (thr->texture.enable)
      {
         while (thr->frame.updated)
            scond_wait(thr->cond_cmd, thr->lock);
      }
#endif
      thr->hit_count++;
   }
   else
      thr->miss_count++;

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
      thr->nonblock = state;
}

static bool video_thread_init(thread_video_t *thr,
      const video_info_t info,
      input_driver_t **input, void **input_data)
{
   size_t max_size;
   thread_packet_t pkt;

   pkt.type                  = CMD_INIT;

   thr->lock                 = slock_new();
   thr->alpha_lock           = slock_new();
   thr->frame.lock           = slock_new();
   thr->cond_cmd             = scond_new();
   thr->cond_thread          = scond_new();
   thr->input                = input;
   thr->input_data           = input_data;
   thr->info                 = info;
   thr->alive                = true;
   thr->focus                = true;
   thr->has_windowed         = true;
   thr->suppress_screensaver = true;

   max_size                  = info.input_scale * RARCH_SCALE_BASE;
   max_size                 *= max_size;
   max_size                 *= info.rgb32 ? sizeof(uint32_t) : sizeof(uint16_t);
#ifdef _3DS
   thr->frame.buffer         = linearMemAlign(max_size, 0x80);
#else
   thr->frame.buffer         = (uint8_t*)malloc(max_size);
#endif

   if (!thr->frame.buffer)
      return false;

   memset(thr->frame.buffer, 0x80, max_size);

   thr->last_time            = cpu_features_get_time_usec();
   thr->thread               = sthread_create(video_thread_loop, thr);

   if (!thr->thread)
      return false;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);

   thr->send_and_wait        = video_thread_send_and_wait_user_to_thread;
   return pkt.data.b;
}

static bool video_thread_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;
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
   if (!thr)
      return;

   slock_lock(thr->lock);

    if (thr->driver && thr->driver->set_viewport)
        thr->driver->set_viewport(thr->driver_data, width, height,
                                  force_full, video_allow_rotate);

   slock_unlock(thr->lock);
}

static void video_thread_set_rotation(void *data, unsigned rotation)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;

   pkt.type            = CMD_SET_ROTATION;
   pkt.data.i          = rotation;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);
}

/* This value is set async as stalling on the video driver for
 * every query is too slow.
 *
 * This means this value might not be correct, so viewport
 * reads are not supported for now. */
static void video_thread_viewport_info(void *data, struct video_viewport *vp)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;

   slock_lock(thr->lock);
   *vp = thr->vp;

   /* Explicitly mem-copied so we can use memcmp correctly later. */
   memcpy(&thr->read_vp, &thr->vp, sizeof(thr->vp));
   slock_unlock(thr->lock);
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
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;

   pkt.type             = CMD_FREE;
   video_thread_send_and_wait_user_to_thread(thr, &pkt);

   sthread_join(thr->thread);

#if defined(HAVE_MENU)
   free(thr->texture.frame);
#endif
#ifdef _3DS
   linearFree(thr->frame.buffer);
#else
   free(thr->frame.buffer);
#endif
   slock_free(thr->frame.lock);
   slock_free(thr->lock);
   scond_free(thr->cond_cmd);
   scond_free(thr->cond_thread);

   free(thr->alpha_mod);
   slock_free(thr->alpha_lock);

   RARCH_LOG("Threaded video stats: Frames pushed: %u, Frames dropped: %u.\n",
         thr->hit_count, thr->miss_count);

   free(thr);
}

#ifdef HAVE_OVERLAY
static void thread_overlay_enable(void *data, bool state)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;

   pkt.type            = CMD_OVERLAY_ENABLE;
   pkt.data.b          = state;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);
}

static bool thread_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   thread_packet_t pkt;
   thread_video_t *thr                = (thread_video_t*)data;
   const struct texture_image *images =
      (const struct texture_image*)image_data;

   if (!thr)
      return false;

   pkt.type            = CMD_OVERLAY_LOAD;
   pkt.data.image.data = images;
   pkt.data.image.num  = num_images;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);

   return pkt.data.b;
}

static void thread_overlay_tex_geom(void *data,
      unsigned idx, float x, float y, float w, float h)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;
   pkt.type            = CMD_OVERLAY_TEX_GEOM;
   pkt.data.rect.index = idx;
   pkt.data.rect.x     = x;
   pkt.data.rect.y     = y;
   pkt.data.rect.w     = w;
   pkt.data.rect.h     = h;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);
}

static void thread_overlay_vertex_geom(void *data,
      unsigned idx, float x, float y, float w, float h)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;

   pkt.type            = CMD_OVERLAY_VERTEX_GEOM;
   pkt.data.rect.index = idx;
   pkt.data.rect.x     = x;
   pkt.data.rect.y     = y;
   pkt.data.rect.w     = w;
   pkt.data.rect.h     = h;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);
}

static void thread_overlay_full_screen(void *data, bool enable)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   pkt.type            = CMD_OVERLAY_FULL_SCREEN;
   pkt.data.b          = enable;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);
}

/* We cannot wait for this to complete. Totally blocks the main thread. */
static void thread_overlay_set_alpha(void *data, unsigned idx, float mod)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;

   slock_lock(thr->alpha_lock);
   thr->alpha_mod[idx] = mod;
   thr->alpha_update   = true;
   slock_unlock(thr->alpha_lock);
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
   if (!thr)
      return;
   *iface = &thread_overlay;
   thr->driver->overlay_interface(thr->driver_data, &thr->overlay);
}
#endif

static void thread_set_video_mode(void *data, unsigned width, unsigned height,
      bool video_fullscreen)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;

   pkt.type                     = CMD_POKE_SET_VIDEO_MODE;
   pkt.data.new_mode.width      = width;
   pkt.data.new_mode.height     = height;
   pkt.data.new_mode.fullscreen = video_fullscreen;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);
}

static void thread_set_filtering(void *data, unsigned idx, bool smooth, bool ctx_scaling)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;
   pkt.type                  = CMD_POKE_SET_FILTERING;
   pkt.data.filtering.index  = idx;
   pkt.data.filtering.smooth = smooth;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);
}

static void thread_set_hdr_max_nits(void *data, float max_nits)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;
   pkt.type                = CMD_POKE_SET_HDR_MAX_NITS;
   pkt.data.hdr.max_nits   = max_nits;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);
}

static void thread_set_hdr_paper_white_nits(void *data, float paper_white_nits)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;
   pkt.type                         = CMD_POKE_SET_HDR_PAPER_WHITE_NITS;
   pkt.data.hdr.paper_white_nits    = paper_white_nits;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);
}

static void thread_set_hdr_contrast(void *data, float contrast)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;
   pkt.type                = CMD_POKE_SET_HDR_CONTRAST;
   pkt.data.hdr.contrast   = contrast;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);
}

static void thread_set_hdr_expand_gamut(void *data, bool expand_gamut)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;
   pkt.type                   = CMD_POKE_SET_HDR_EXPAND_GAMUT;
   pkt.data.hdr.expand_gamut  = expand_gamut;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);
}

static void thread_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;

   if (thr->poke && thr->poke->get_video_output_size)
      thr->poke->get_video_output_size(thr->driver_data,
            width,
            height, desc, desc_len);
}

static void thread_get_video_output_prev(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;

   if (thr->poke && thr->poke->get_video_output_prev)
      thr->poke->get_video_output_prev(thr->driver_data);
}

static void thread_get_video_output_next(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;

   if (thr->poke && thr->poke->get_video_output_next)
      thr->poke->get_video_output_next(thr->driver_data);
}

static void thread_set_aspect_ratio(void *data, unsigned aspectratio_idx)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;
   pkt.type            = CMD_POKE_SET_ASPECT_RATIO;
   pkt.data.i          = aspectratio_idx;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);
}

static void thread_set_texture_frame(void *data, const void *frame,
      bool rgb32, unsigned width, unsigned height, float alpha)
{
   thread_video_t *thr = (thread_video_t*)data;
   size_t required     = width * height *
      (rgb32 ? sizeof(uint32_t) : sizeof(uint16_t));

   slock_lock(thr->frame.lock);

   if (required > thr->texture.frame_cap)
   {
      thr->texture.frame     = realloc(thr->texture.frame, required);
      thr->texture.frame_cap = required;
   }

   if (thr->texture.frame)
   {
      memcpy(thr->texture.frame, frame, required);
      thr->texture.frame_updated = true;
      thr->texture.rgb32         = rgb32;
      thr->texture.width         = width;
      thr->texture.height        = height;
      thr->texture.alpha         = alpha;
   }
   slock_unlock(thr->frame.lock);
}

static void thread_set_texture_enable(void *data, bool state, bool full_screen)
{
   thread_video_t *thr = (thread_video_t*)data;
   if (!thr)
      return;

   slock_lock(thr->frame.lock);
   thr->texture.enable      = state;
   thr->texture.full_screen = full_screen;
   slock_unlock(thr->frame.lock);
}

static void thread_set_osd_msg(void *data,
      const char *msg,
      const void *params, void *font)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;

   /* TODO : find a way to determine if the calling
    * thread is the driver thread or not. */
   if (thr->poke && thr->poke->set_osd_msg)
      thr->poke->set_osd_msg(thr->driver_data, msg, params, font);
}

static void thread_show_mouse(void *data, bool state)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;
   pkt.type            = CMD_POKE_SHOW_MOUSE;
   pkt.data.b          = state;

   video_thread_send_and_wait_user_to_thread(thr, &pkt);
}

static void thread_grab_mouse_toggle(void *data)
{
   thread_packet_t pkt;
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;

   pkt.type                       = CMD_POKE_GRAB_MOUSE_TOGGLE;
   video_thread_send_and_wait_user_to_thread(thr, &pkt);
}

static uintptr_t thread_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   thread_video_t *thr = (thread_video_t*)video_data;

   if (!thr || !thr->poke || !thr->poke->load_texture)
      return 0;

   return thr->poke->load_texture(thr->driver_data, data, threaded, filter_type);
}

static void thread_unload_texture(void *video_data, bool threaded,
      uintptr_t id)
{
   thread_video_t *thr = (thread_video_t*)video_data;
   if (thr && thr->poke && thr->poke->unload_texture)
      thr->poke->unload_texture(thr->driver_data, threaded,
            id);
}

static void thread_apply_state_changes(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (!thr)
      return;

   slock_lock(thr->frame.lock);
   thr->apply_state_changes = true;
   slock_unlock(thr->frame.lock);
}

/* This is read-only state which should not
 * have any kind of race condition. */
static struct video_shader *thread_get_current_shader(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;
   if (!thr || !thr->poke || !thr->poke->get_current_shader)
      return NULL;
   return thr->poke->get_current_shader(thr->driver_data);
}

static uint32_t thread_get_flags(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;
   if (!thr || !thr->poke || !thr->poke->get_flags)
      return 0;
   return thr->poke->get_flags(thr->driver_data);
}

static const video_poke_interface_t thread_poke = {
   thread_get_flags,
   thread_load_texture,
   thread_unload_texture,
   thread_set_video_mode,
   NULL,
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
   NULL,                      /* get_current_software_framebuffer */
   NULL,                      /* get_hw_render_interface */
   thread_set_hdr_max_nits,
   thread_set_hdr_paper_white_nits,
   thread_set_hdr_contrast,
   thread_set_hdr_expand_gamut
};

static void video_thread_get_poke_interface(
      void *data,
      const video_poke_interface_t **iface)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr->driver->poke_interface)
   {
      *iface = &thread_poke;
      thr->driver->poke_interface(thr->driver_data, &thr->poke);
   }
   else
      *iface = NULL;
}

#ifdef HAVE_GFX_WIDGETS
static bool video_thread_wrapper_gfx_widgets_enabled(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;

   if (thr && thr->driver && thr->driver->gfx_widgets_enabled)
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
   video_thread_get_overlay_interface, /* get_overlay_interface */
#endif
#ifdef HAVE_VIDEO_LAYOUT
   NULL,
#endif
   video_thread_get_poke_interface,
   NULL,
#ifdef HAVE_GFX_WIDGETS
   video_thread_wrapper_gfx_widgets_enabled
#endif
};

static void video_thread_set_callbacks(
      thread_video_t *thr,
      const video_driver_t *drv)
{
   thr->video_thread = video_thread;

   /* Disable optional features if not present. */
   if (!drv->read_viewport)
      thr->video_thread.read_viewport = NULL;
   if (!drv->set_viewport)
      thr->video_thread.set_viewport = NULL;
   if (!drv->set_rotation)
      thr->video_thread.set_rotation = NULL;
   if (!drv->set_shader)
      thr->video_thread.set_shader = NULL;
#ifdef HAVE_OVERLAY
   if (!drv->overlay_interface)
      thr->video_thread.overlay_interface = NULL;
#endif

   /* Might have to optionally disable poke_interface features as well. */
   if (!thr->video_thread.poke_interface)
      thr->video_thread.poke_interface = NULL;
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
bool video_init_thread(const video_driver_t **out_driver,
      void **out_data,  input_driver_t **input, void **input_data,
      const video_driver_t *drv, const video_info_t info)
{
   thread_video_t *thr = (thread_video_t*)calloc(1, sizeof(*thr));
   if (!thr)
      return false;

   video_thread_set_callbacks(thr, drv);

   thr->driver = drv;
   *out_driver = &thr->video_thread;
   *out_data   = thr;
   return video_thread_init(thr, info, input, input_data);
}

static void video_thread_send_and_wait(thread_video_t *thr,
      thread_packet_t *pkt)
{
   if (thr && pkt)
      thr->send_and_wait(thr, pkt);
}

bool video_thread_font_init(const void **font_driver, void **font_handle,
      void *data, const char *font_path, float video_font_size,
      enum font_driver_render_api api, custom_font_command_method_t func,
      bool is_threaded)
{
   thread_packet_t pkt;
   thread_video_t *thr            = (thread_video_t*)
      video_driver_get_data();

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
   pkt.data.font_init.api         = api;

   video_thread_send_and_wait(thr, &pkt);

   return pkt.data.font_init.return_value;
}

unsigned video_thread_texture_load(void *data,
      custom_command_method_t func)
{
   thread_packet_t pkt;
   thread_video_t *thr  = (thread_video_t*)video_driver_get_data();

   if (!thr)
      return 0;

   pkt.type                       = CMD_CUSTOM_COMMAND;
   pkt.data.custom_command.method = func;
   pkt.data.custom_command.data   = (void*)data;

   video_thread_send_and_wait(thr, &pkt);

   return pkt.data.custom_command.return_value;
}

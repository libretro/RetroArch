/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - libretroadmin
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

/* See video_thread_hw.h. */

#include <stdlib.h>
#include <string.h>

#include <string/stdstring.h>

#include "video_driver.h"
#include "video_thread_wrapper.h"
#include "video_thread_hw.h"
#include "../configuration.h"
#include "../verbosity.h"

#ifdef HAVE_THREADS

#ifdef HAVE_VULKAN
#include <libretro_vulkan.h>

typedef struct
{
   /* What the core handed over for this slot. The image is a copy: the
    * core's own struct may be rewritten for its next frame while the
    * video thread is still driving the driver with this one. */
   struct retro_vulkan_image image;
   VkSemaphore     *semaphores;
   VkCommandBuffer *cmd;
   void            *fence;
   unsigned         num_semaphores;
   unsigned         cap_semaphores;
   unsigned         num_cmd;
   unsigned         cap_cmd;
   uint32_t         src_queue_family;
   bool             has_image;
   /* The video thread has driven the driver with this slot and its
    * fence is armed; the next user of the slot waits it first. */
   bool             in_flight;
} hw_slot_t;

typedef struct
{
   struct retro_hw_render_interface_vulkan        iface;
   const struct retro_hw_render_interface_vulkan *real;
   hw_slot_t slot[VIDEO_THREAD_HW_RING];
   /* The core's current sync index: the slot it is rendering into.
    * Main thread only between pushes; the push moves it. */
   unsigned  index;
   thread_video_t *thr;
} hw_ring_t;

static hw_ring_t *hw_ring_of(void *handle)
{
   thread_video_t *thr = (thread_video_t*)handle;
   return thr ? (hw_ring_t*)thr->frame.hw_ring : NULL;
}

/* --- the core's side, main thread ------------------------------------ */

static uint32_t hw_get_sync_index(void *handle)
{
   hw_ring_t *ring = hw_ring_of(handle);
   return ring ? ring->index : 0;
}

static uint32_t hw_get_sync_index_mask(void *handle)
{
   (void)handle;
   return (1u << VIDEO_THREAD_HW_RING) - 1;
}

static void hw_wait_slot(hw_ring_t *ring, unsigned i)
{
   hw_slot_t *s        = &ring->slot[i];
   thread_video_t *thr = ring->thr;
   if (!s->in_flight)
      return;
   if (thr->poke && thr->poke->hw_ring_fence_wait)
      thr->poke->hw_ring_fence_wait(thr->driver_data, s->fence);
   s->in_flight = false;
}

static void hw_wait_sync_index(void *handle)
{
   hw_ring_t *ring = hw_ring_of(handle);
   if (ring)
      hw_wait_slot(ring, ring->index);
}

static void hw_set_image(void *handle,
      const struct retro_vulkan_image *image,
      uint32_t num_semaphores, const VkSemaphore *semaphores,
      uint32_t src_queue_family)
{
   hw_ring_t *ring = hw_ring_of(handle);
   hw_slot_t *s;
   if (!ring)
      return;
   s = &ring->slot[ring->index];
   if (image)
   {
      s->image     = *image;
      s->has_image = true;
   }
   else
      s->has_image = false;
   if (num_semaphores > s->cap_semaphores)
   {
      VkSemaphore *grown = (VkSemaphore*)realloc(s->semaphores,
            sizeof(*grown) * num_semaphores);
      if (!grown)
      {
         s->num_semaphores = 0;
         return;
      }
      s->semaphores     = grown;
      s->cap_semaphores = num_semaphores;
   }
   if (num_semaphores)
      memcpy(s->semaphores, semaphores, sizeof(*semaphores) * num_semaphores);
   s->num_semaphores   = num_semaphores;
   s->src_queue_family = src_queue_family;
}

static void hw_set_command_buffers(void *handle, uint32_t num_cmd,
      const VkCommandBuffer *cmd)
{
   hw_ring_t *ring = hw_ring_of(handle);
   hw_slot_t *s;
   if (!ring)
      return;
   s = &ring->slot[ring->index];
   if (num_cmd > s->cap_cmd)
   {
      VkCommandBuffer *grown = (VkCommandBuffer*)realloc(s->cmd,
            sizeof(*grown) * num_cmd);
      if (!grown)
      {
         s->num_cmd = 0;
         return;
      }
      s->cmd     = grown;
      s->cap_cmd = num_cmd;
   }
   if (num_cmd)
      memcpy(s->cmd, cmd, sizeof(*cmd) * num_cmd);
   s->num_cmd = num_cmd;
}

/* Forwarded: the driver serialises queue use for cores that submit from
 * other threads, which is exactly the situation here. */
static void hw_lock_queue(void *handle)
{
   hw_ring_t *ring = hw_ring_of(handle);
   if (ring && ring->real->lock_queue)
      ring->real->lock_queue(ring->real->handle);
}

static void hw_unlock_queue(void *handle)
{
   hw_ring_t *ring = hw_ring_of(handle);
   if (ring && ring->real->unlock_queue)
      ring->real->unlock_queue(ring->real->handle);
}

static void hw_set_signal_semaphore(void *handle, VkSemaphore semaphore)
{
   hw_ring_t *ring = hw_ring_of(handle);
   if (ring && ring->real->set_signal_semaphore)
      ring->real->set_signal_semaphore(ring->real->handle, semaphore);
}

/* --- the wrapper's side ---------------------------------------------- */

bool video_thread_get_hw_render_interface(void *data,
      const struct retro_hw_render_interface **iface)
{
   thread_video_t *thr = (thread_video_t*)data;
   const struct retro_hw_render_interface *real_base = NULL;
   const struct retro_hw_render_interface_vulkan *real;
   hw_ring_t *ring;
   unsigned i;

   if (!thr || !iface)
      return false;
   if (!thr->poke || !thr->poke->get_hw_render_interface
         || !thr->poke->hw_ring_install
         || !thr->poke->hw_ring_fence_new
         || !thr->poke->hw_ring_fence_wait
         || !thr->poke->hw_ring_fence_signal)
      return false;
   if (!thr->poke->get_hw_render_interface(thr->driver_data, &real_base))
      return false;
   if (!real_base || real_base->interface_type != RETRO_HW_RENDER_INTERFACE_VULKAN)
      return false;
   real = (const struct retro_hw_render_interface_vulkan*)real_base;

   if (!(ring = (hw_ring_t*)thr->frame.hw_ring))
   {
      if (!(ring = (hw_ring_t*)calloc(1, sizeof(*ring))))
         return false;
      ring->thr = thr;
      for (i = 0; i < VIDEO_THREAD_HW_RING; i++)
      {
         if (!thr->poke->hw_ring_fence_new(thr->driver_data,
                  &ring->slot[i].fence))
         {
            unsigned j;
            for (j = 0; j < i; j++)
               thr->poke->hw_ring_fence_free(thr->driver_data,
                     ring->slot[j].fence);
            free(ring);
            return false;
         }
      }
      thr->frame.hw_ring = ring;
      RARCH_LOG("[Video] Threaded video: hardware core on a %u-slot ring.\n",
            VIDEO_THREAD_HW_RING);
   }

   /* The driver's interface, with the bookkeeping redirected here. The
    * handle the core sees is the wrapper. */
   ring->real                       = real;
   ring->iface                      = *real;
   ring->iface.handle               = thr;
   ring->iface.set_image            = hw_set_image;
   ring->iface.get_sync_index       = hw_get_sync_index;
   ring->iface.get_sync_index_mask  = hw_get_sync_index_mask;
   ring->iface.wait_sync_index      = hw_wait_sync_index;
   ring->iface.set_command_buffers  = hw_set_command_buffers;
   ring->iface.lock_queue           = hw_lock_queue;
   ring->iface.unlock_queue         = hw_unlock_queue;
   ring->iface.set_signal_semaphore = hw_set_signal_semaphore;

   *iface = (const struct retro_hw_render_interface*)&ring->iface;
   return true;
}

int video_thread_hw_publish(thread_video_t *thr)
{
   hw_ring_t *ring = (hw_ring_t*)thr->frame.hw_ring;
   unsigned published;
   if (!ring)
      return -1;
   published   = ring->index;
   ring->index = (ring->index + 1) % VIDEO_THREAD_HW_RING;
   /* The slot the core fills next was last driven two frames ago;
    * normally long done, and if not this is where the core waits. */
   hw_wait_slot(ring, ring->index);
   return (int)published;
}

void video_thread_hw_before_frame(thread_video_t *thr, int hw_slot)
{
   hw_ring_t *ring = (hw_ring_t*)thr->frame.hw_ring;
   hw_slot_t *s;
   if (!ring || hw_slot < 0 || hw_slot >= VIDEO_THREAD_HW_RING)
      return;
   s = &ring->slot[hw_slot];
   thr->poke->hw_ring_install(thr->driver_data,
         s->has_image ? &s->image : NULL,
         s->semaphores, s->num_semaphores, s->src_queue_family,
         s->cmd, s->num_cmd);
}

void video_thread_hw_after_frame(thread_video_t *thr, int hw_slot)
{
   hw_ring_t *ring = (hw_ring_t*)thr->frame.hw_ring;
   hw_slot_t *s;
   if (!ring || hw_slot < 0 || hw_slot >= VIDEO_THREAD_HW_RING)
      return;
   s = &ring->slot[hw_slot];
   /* Signalled after the submission the frame call just made; the fence
    * was reset by whoever waited it last, or is fresh. */
   thr->poke->hw_ring_fence_signal(thr->driver_data, s->fence);
   s->in_flight = true;
}

void video_thread_hw_free(thread_video_t *thr)
{
   hw_ring_t *ring = (hw_ring_t*)thr->frame.hw_ring;
   unsigned i;
   if (!ring)
      return;
   for (i = 0; i < VIDEO_THREAD_HW_RING; i++)
   {
      hw_slot_t *s = &ring->slot[i];
      /* Wait anything in flight before its fence goes. */
      hw_wait_slot(ring, i);
      if (thr->poke && thr->poke->hw_ring_fence_free)
         thr->poke->hw_ring_fence_free(thr->driver_data, s->fence);
      free(s->semaphores);
      free(s->cmd);
   }
   free(ring);
   thr->frame.hw_ring = NULL;
}

bool video_thread_hw_allowed(void)
{
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   if (!settings || !settings->bools.video_threaded_hw_vulkan)
      return false;
   if (video_st->hw_render.context_type != RETRO_HW_CONTEXT_VULKAN)
      return false;
   return string_is_equal(settings->arrays.video_driver, "vulkan");
}

#else /* !HAVE_VULKAN */

bool video_thread_get_hw_render_interface(void *data,
      const struct retro_hw_render_interface **iface)
{
   (void)data; (void)iface;
   return false;
}
int  video_thread_hw_publish(thread_video_t *thr) { (void)thr; return -1; }
void video_thread_hw_before_frame(thread_video_t *thr, int hw_slot) { (void)thr; (void)hw_slot; }
void video_thread_hw_after_frame(thread_video_t *thr, int hw_slot)  { (void)thr; (void)hw_slot; }
void video_thread_hw_free(thread_video_t *thr) { (void)thr; }
bool video_thread_hw_allowed(void) { return false; }

#endif /* HAVE_VULKAN */

#endif /* HAVE_THREADS */

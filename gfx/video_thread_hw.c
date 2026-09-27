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
#endif
#ifdef HAVE_D3D12
#include <libretro_d3d12.h>
#include "common/d3d12_hw_interface.h"
#endif
#ifdef HAVE_D3D11
#include <libretro_d3d11.h>
#include "common/d3d11_hw_interface.h"
#endif

#if defined(HAVE_VULKAN) || defined(HAVE_D3D12) || defined(HAVE_D3D11) \
   || defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
#define VIDEO_THREAD_HW_ANY 1
#endif

#ifdef VIDEO_THREAD_HW_ANY

typedef struct
{
#ifdef HAVE_VULKAN
   /* Vulkan: what the core handed over for this slot. The image is a
    * copy: the core's own struct may be rewritten for its next frame
    * while the video thread is still driving the driver with this one. */
   struct retro_vulkan_image image;
   VkSemaphore     *semaphores;
   VkCommandBuffer *cmd;
   unsigned         num_semaphores;
   unsigned         cap_semaphores;
   unsigned         num_cmd;
   unsigned         cap_cmd;
   uint32_t         src_queue_family;
   bool             has_image;
#endif
#ifdef HAVE_D3D12
   /* Direct3D 12: the texture the core set for this frame, and its
    * format. The driver copies it into the slot's own texture at
    * publish; after that the core may do what it likes with its own. */
   const void      *texture;
   unsigned         format;
   /* Version 2 (set_texture_fenced): the driver reads the core's own
    * texture, once `fence` has reached `value`; nothing is copied. The
    * slot holds a reference to both until it is handed over again. */
   ID3D12Resource  *v2_texture;
   ID3D12Fence     *v2_fence;
   UINT64           v2_value;
   bool             v2;
#endif
#ifdef HAVE_D3D11
   /* Direct3D 11, interface version 2: the texture the core named with
    * set_texture for this frame. Not referenced: the driver copies it at
    * publish, inside the video_refresh the core called with it. */
   ID3D11Texture2D *d3d11_texture;
   /* Version 3: the texture is the core's own and is read directly. The
    * slot holds a reference from publish until it is handed over again. */
   ID3D11Texture2D *d3d11_direct;
#endif
   void            *fence;
   /* The video thread has driven the driver with this slot and its
    * fence is armed; the next user of the slot waits it first. */
   bool             in_flight;
} hw_slot_t;

enum hw_api
{
   HW_API_NONE = 0,
   HW_API_VULKAN,
   HW_API_D3D12,
   HW_API_D3D11,
   HW_API_GL
};

typedef struct
{
   union
   {
#ifdef HAVE_VULKAN
      struct retro_hw_render_interface_vulkan vk;
#endif
#ifdef HAVE_D3D12
      struct retro_hw_render_interface_d3d12  d3d12;
#endif
#ifdef HAVE_D3D11
      struct retro_hw_render_interface_d3d11  d3d11;
#endif
      /* OpenGL interposes nothing; this keeps the union non-empty on
       * a build with only OpenGL. */
      struct retro_hw_render_interface        base;
   } iface;
   /* OpenGL: the driver's handle for the core's context; Direct3D 11:
    * the proxy in front of the core's deferred context. Both from
    * hw_ring_context_new. */
   void *core_ctx;
   /* Direct3D 11, interface version 2: the core holds the immediate
    * context, by taking turns with the driver; there is no core_ctx.
    * Version 3 on top of that: frames are not copied into the slots. */
   bool  d3d11_v2;
   /* The interface version last reported in the log. A core may ask for
    * the interface more than once - when it first needs the context, and
    * again when it builds its device - and gets the same answer each
    * time; the log says it once per ring, or again if it ever changed. */
   unsigned logged_version;
   const struct retro_hw_render_interface *real;
   hw_slot_t slot[VIDEO_THREAD_HW_RING];
   /* The core's current sync index: the slot it is rendering into.
    * Main thread only between pushes; the push moves it. */
   unsigned  index;
   enum hw_api api;
   thread_video_t *thr;
} hw_ring_t;

static hw_ring_t *hw_ring_of(void *handle)
{
   thread_video_t *thr = (thread_video_t*)handle;
   return thr ? (hw_ring_t*)thr->frame.hw_ring : NULL;
}

/* Waits a slot's fence on the core's thread. On Apple, in slices with
 * the main-thread pump between: the video thread may need the main
 * thread to run a job before it can signal - a swapchain rebuild
 * marshalled through the Cocoa trampoline - and an unbounded wait here
 * left both threads waiting on each other, reported as the trampoline
 * stalling with a Vulkan core. Everywhere else there is no trampoline
 * and nothing to pump, so the wait is the single unbounded one it was:
 * the slices are a cost only the platform that needs them pays. */
static void hw_wait_slot(hw_ring_t *ring, unsigned i)
{
   hw_slot_t *s        = &ring->slot[i];
   thread_video_t *thr = ring->thr;
   if (!s->in_flight)
      return;
   if (thr->poke && thr->poke->hw_ring_fence_wait)
   {
#ifdef __APPLE__
      while (!thr->poke->hw_ring_fence_wait(thr->driver_data, s->fence, 2000))
         video_thread_main_pump();
#else
      thr->poke->hw_ring_fence_wait(thr->driver_data, s->fence, HW_RING_WAIT_FOREVER);
#endif
   }
   s->in_flight = false;
}

/* --- Vulkan: the core's side, main thread ----------------------------- */
#ifdef HAVE_VULKAN

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
   const struct retro_hw_render_interface_vulkan *real;
   if (!ring)
      return;
   real = (const struct retro_hw_render_interface_vulkan*)ring->real;
   if (real->lock_queue)
      real->lock_queue(real->handle);
}

static void hw_unlock_queue(void *handle)
{
   hw_ring_t *ring = hw_ring_of(handle);
   const struct retro_hw_render_interface_vulkan *real;
   if (!ring)
      return;
   real = (const struct retro_hw_render_interface_vulkan*)ring->real;
   if (real->unlock_queue)
      real->unlock_queue(real->handle);
}

static void hw_set_signal_semaphore(void *handle, VkSemaphore semaphore)
{
   hw_ring_t *ring = hw_ring_of(handle);
   const struct retro_hw_render_interface_vulkan *real;
   if (!ring)
      return;
   real = (const struct retro_hw_render_interface_vulkan*)ring->real;
   if (real->set_signal_semaphore)
      real->set_signal_semaphore(real->handle, semaphore);
}
#endif /* VIDEO_THREAD_HW_ANY */

/* --- Direct3D 12: the core's side, main thread ------------------------ */
#if defined(HAVE_D3D11) || defined(HAVE_D3D12)
/* --- slots that are the core's own texture -----------------------------
 * libretro_d3d12.h version 2 and libretro_d3d11.h version 3: nothing is
 * copied into the slot, the driver reads the core's texture itself. */

/* Under thr->lock: whether a frame the video thread is drawing, or has
 * yet to claim, names ring slot i. The pending frame is tail, both are
 * pending at two, and the one being drawn is tail ^ 1. */
static bool hw_slot_queued(const thread_video_t *thr, unsigned i)
{
   unsigned w;
   for (w = 0; w < 2; w++)
   {
      if (thr->frame.slot[w].hw_slot != (int)i)
         continue;
      if (     thr->frame.pending == 2
            || (thr->frame.pending == 1 && w == thr->frame.tail)
            || (thr->frame.busy && w == (thr->frame.tail ^ 1)))
         return true;
   }
   return false;
}

/* The wait itself, for any slot that is the core's own texture. */
static void hw_wait_queued(hw_ring_t *ring, unsigned i, bool locked)
{
   thread_video_t *thr = ring->thr;
   if (!locked)
      slock_lock(thr->lock);
   while (hw_slot_queued(thr, i))
      scond_wait(thr->cond_ring, thr->lock);
   if (!locked)
      slock_unlock(thr->lock);
}
#endif

#ifdef HAVE_D3D12
static void hw_d3d12_slot_release_v2(hw_slot_t *s);
static void hw_d3d12_wait_queued(hw_ring_t *ring, unsigned i, bool locked);

static void hw_d3d12_set_texture(void *handle, ID3D12Resource *texture,
      DXGI_FORMAT format)
{
   hw_ring_t *ring = hw_ring_of(handle);
   hw_slot_t *s;
   if (!ring)
      return;
   s          = &ring->slot[ring->index];
   s->texture = texture;
   s->format  = (unsigned)format;
   /* A version 1 handoff, whatever the slot carried before. */
   if (s->v2)
   {
      hw_d3d12_wait_queued(ring, ring->index, false);
      hw_wait_slot(ring, ring->index);
      hw_d3d12_slot_release_v2(s);
   }
}

/* --- Direct3D 12, interface version 2 ----------------------------------
 * The core keeps one present texture per ring slot and says when each is
 * complete; the wrapper says when it is done with it. No copy. */

/* The slot's fence says the video thread has driven the driver with the
 * slot and the GPU is done. It says nothing until then: a frame that is
 * queued but not yet claimed, or claimed and still being recorded, has
 * armed no fence. With a copy per slot that window costs nothing - the
 * copy is simply replaced by a newer one. With version 2 the slot IS the
 * core's texture, and a core let back in during that window draws into
 * a texture the frame about to be recorded will read. So for version 2
 * the wait covers the window too: until no queued frame names the slot.
 * The video thread broadcasts cond_ring when it claims a frame and when
 * it finishes one. */
static void hw_d3d12_wait_queued(hw_ring_t *ring, unsigned i, bool locked)
{
   thread_video_t *thr = ring->thr;
   if (!ring->slot[i].v2)
      return;
   hw_wait_queued(ring, i, locked);
}


static void hw_d3d12_slot_release_v2(hw_slot_t *s)
{
   if (s->v2_texture)
      s->v2_texture->lpVtbl->Release(s->v2_texture);
   if (s->v2_fence)
      s->v2_fence->lpVtbl->Release(s->v2_fence);
   s->v2_texture = NULL;
   s->v2_fence   = NULL;
   s->v2_value   = 0;
   s->v2         = false;
}

static unsigned hw_d3d12_get_sync_index(void *handle)
{
   hw_ring_t *ring = hw_ring_of(handle);
   return ring ? ring->index : 0;
}

static unsigned hw_d3d12_get_sync_index_mask(void *handle)
{
   (void)handle;
   return (1u << VIDEO_THREAD_HW_RING) - 1;
}

static void hw_d3d12_wait_sync_index(void *handle)
{
   hw_ring_t *ring = hw_ring_of(handle);
   if (!ring)
      return;
   hw_d3d12_wait_queued(ring, ring->index, false);
   hw_wait_slot(ring, ring->index);
}

/* Main thread. The slot's previous frame is done with - the core called
 * wait_sync_index, and publish waited the slot before moving onto it -
 * so its references can go. */
static void hw_d3d12_set_texture_fenced(void *handle, ID3D12Resource *texture,
      DXGI_FORMAT format, ID3D12Fence *fence, UINT64 value)
{
   hw_ring_t *ring = hw_ring_of(handle);
   hw_slot_t *s;
   if (!ring)
      return;
   s = &ring->slot[ring->index];
   hw_d3d12_wait_queued(ring, ring->index, false);
   hw_wait_slot(ring, ring->index);
   hw_d3d12_slot_release_v2(s);
   if (!texture)
      return;
   texture->lpVtbl->AddRef(texture);
   if (fence)
      fence->lpVtbl->AddRef(fence);
   s->v2_texture = texture;
   s->v2_fence   = fence;
   s->v2_value   = value;
   s->format     = (unsigned)format;
   s->v2         = true;
}
#endif /* HAVE_D3D12 */

#ifdef HAVE_D3D11
/* --- Direct3D 11, interface version 2 ----------------------------------
 * The lock is the driver's, and so is what it reports; the ring only
 * stands where the core expects its handle and remembers the frame. */

static bool hw_d3d11_lock_context(void *handle)
{
   hw_ring_t *ring = hw_ring_of(handle);
   const struct retro_hw_render_interface_d3d11 *real = ring
      ? (const struct retro_hw_render_interface_d3d11*)ring->real : NULL;
   return real ? real->lock_context(real->handle) : false;
}

static void hw_d3d11_unlock_context(void *handle)
{
   hw_ring_t *ring = hw_ring_of(handle);
   const struct retro_hw_render_interface_d3d11 *real = ring
      ? (const struct retro_hw_render_interface_d3d11*)ring->real : NULL;
   if (real)
      real->unlock_context(real->handle);
}

static void hw_d3d11_set_texture(void *handle, ID3D11Texture2D *texture)
{
   hw_ring_t *ring = hw_ring_of(handle);
   if (ring)
      ring->slot[ring->index].d3d11_texture = texture;
}

/* --- Direct3D 11, interface version 3 ----------------------------------
 * The core keeps a present texture per ring slot, the driver reads it
 * where it used to read the slot's copy, and the core waits here before
 * it draws into one again. */

static void hw_d3d11_slot_release_direct(hw_slot_t *s)
{
   if (s->d3d11_direct)
      s->d3d11_direct->lpVtbl->Release(s->d3d11_direct);
   s->d3d11_direct = NULL;
}

static unsigned hw_d3d11_get_sync_index(void *handle)
{
   hw_ring_t *ring = hw_ring_of(handle);
   return ring ? ring->index : 0;
}

static unsigned hw_d3d11_get_sync_index_mask(void *handle)
{
   (void)handle;
   return (1u << VIDEO_THREAD_HW_RING) - 1;
}

/* Until no queued frame names the slot, and the video thread has issued
 * its read of the one that did: see hw_wait_queued. The slot's fence is
 * a CPU event the video thread sets once its frame call has returned,
 * and on one context that is all "done" has to mean. */
static void hw_d3d11_wait_sync_index(void *handle)
{
   hw_ring_t *ring = hw_ring_of(handle);
   if (!ring)
      return;
   hw_wait_queued(ring, ring->index, false);
   hw_wait_slot(ring, ring->index);
}
#endif /* HAVE_D3D11 */

/* --- the wrapper's side ---------------------------------------------- */

static bool hw_ring_setup(thread_video_t *thr, hw_ring_t **out)
{
   hw_ring_t *ring;
   unsigned i;
   if ((ring = (hw_ring_t*)thr->frame.hw_ring))
   {
      *out = ring;
      return true;
   }
   if (!(ring = (hw_ring_t*)calloc(1, sizeof(*ring))))
      return false;
   ring->thr = thr;
   for (i = 0; i < VIDEO_THREAD_HW_RING; i++)
   {
      if (!thr->poke->hw_ring_fence_new(thr->driver_data, &ring->slot[i].fence))
      {
         unsigned j;
         for (j = 0; j < i; j++)
            thr->poke->hw_ring_fence_free(thr->driver_data, ring->slot[j].fence);
         free(ring);
         return false;
      }
   }
   thr->frame.hw_ring = ring;
   RARCH_LOG("[Video] Threaded video: hardware core on a %u-slot ring.\n",
         VIDEO_THREAD_HW_RING);
   *out = ring;
   return true;
}

bool video_thread_get_hw_render_interface(void *data,
      const struct retro_hw_render_interface **iface)
{
   thread_video_t *thr = (thread_video_t*)data;
   const struct retro_hw_render_interface *real = NULL;
#if defined(HAVE_VULKAN) || defined(HAVE_D3D12) || defined(HAVE_D3D11)
   hw_ring_t *ring;
#endif

   if (!thr || !iface)
      return false;
   if (!thr->poke || !thr->poke->get_hw_render_interface
         || !thr->poke->hw_ring_fence_new
         || !thr->poke->hw_ring_fence_wait
         || !thr->poke->hw_ring_fence_signal)
      return false;
   if (!thr->poke->get_hw_render_interface(thr->driver_data, &real) || !real)
      return false;

   switch (real->interface_type)
   {
#ifdef HAVE_VULKAN
      case RETRO_HW_RENDER_INTERFACE_VULKAN:
         if (!thr->poke->hw_ring_install)
            return false;
         if (!hw_ring_setup(thr, &ring))
            return false;
         ring->api                           = HW_API_VULKAN;
         ring->real                          = real;
         /* The driver's interface, with the bookkeeping redirected
          * here. The handle the core sees is the wrapper. */
         ring->iface.vk                      = *(const struct retro_hw_render_interface_vulkan*)real;
         ring->iface.vk.handle               = thr;
         ring->iface.vk.set_image            = hw_set_image;
         ring->iface.vk.get_sync_index       = hw_get_sync_index;
         ring->iface.vk.get_sync_index_mask  = hw_get_sync_index_mask;
         ring->iface.vk.wait_sync_index      = hw_wait_sync_index;
         ring->iface.vk.set_command_buffers  = hw_set_command_buffers;
         ring->iface.vk.lock_queue           = hw_lock_queue;
         ring->iface.vk.unlock_queue         = hw_unlock_queue;
         ring->iface.vk.set_signal_semaphore = hw_set_signal_semaphore;
         *iface = (const struct retro_hw_render_interface*)&ring->iface.vk;
         return true;
#endif
#ifdef HAVE_D3D12
      case RETRO_HW_RENDER_INTERFACE_D3D12:
         if (!thr->poke->hw_ring_capture || !thr->poke->hw_ring_present_slot)
            return false;
         if (!hw_ring_setup(thr, &ring))
            return false;
         ring->api                     = HW_API_D3D12;
         ring->real                    = real;
         /* Device, queue and compiler pass straight through; only the
          * texture handoff is redirected. */
         ring->iface.d3d12             = *(const struct retro_hw_render_interface_d3d12*)real;
         ring->iface.d3d12.handle      = thr;
         ring->iface.d3d12.set_texture = hw_d3d12_set_texture;
         /* Version 2 for a core that asked for it, and only if the
          * driver can take the core's texture as it is. Everything the
          * struct copy above brought along from the driver's own version
          * 2 is replaced: the ring answers for the sync index. */
         ring->iface.d3d12.interface_version   = RETRO_HW_RENDER_INTERFACE_D3D12_VERSION;
         ring->iface.d3d12.get_sync_index      = NULL;
         ring->iface.d3d12.get_sync_index_mask = NULL;
         ring->iface.d3d12.wait_sync_index     = NULL;
         ring->iface.d3d12.set_texture_fenced  = NULL;
         if (     thr->poke->hw_ring_install
               && d3d12_hw_interface_negotiated_version()
                  >= RETRO_HW_RENDER_INTERFACE_D3D12_VERSION_2)
         {
            ring->iface.d3d12.interface_version   = RETRO_HW_RENDER_INTERFACE_D3D12_VERSION_2;
            ring->iface.d3d12.get_sync_index      = hw_d3d12_get_sync_index;
            ring->iface.d3d12.get_sync_index_mask = hw_d3d12_get_sync_index_mask;
            ring->iface.d3d12.wait_sync_index     = hw_d3d12_wait_sync_index;
            ring->iface.d3d12.set_texture_fenced  = hw_d3d12_set_texture_fenced;
            if (ring->logged_version != RETRO_HW_RENDER_INTERFACE_D3D12_VERSION_2)
            {
               ring->logged_version = RETRO_HW_RENDER_INTERFACE_D3D12_VERSION_2;
               RARCH_LOG("[Video] Threaded video: D3D12 hardware render interface version 2, no frame copy.\n");
            }
         }
         *iface = (const struct retro_hw_render_interface*)&ring->iface.d3d12;
         return true;
#endif
#ifdef HAVE_D3D11
      case RETRO_HW_RENDER_INTERFACE_D3D11:
         if (!thr->poke->hw_ring_capture || !thr->poke->hw_ring_present_slot
               || !thr->poke->hw_ring_context_new)
            return false;
         if (!hw_ring_setup(thr, &ring))
            return false;
         /* hw_ring_install is how the texture is handed over without a
          * copy, which is all version 2 is; without it the core gets
          * version 1. */
         if (     thr->poke->hw_ring_install
               && ((const struct retro_hw_render_interface_d3d11*)real)->interface_version
                  >= RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2
               && d3d11_hw_interface_negotiated_version()
                  >= RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2)
         {
            /* Version 2: the core gets the immediate context itself and
             * takes turns on it with the driver. No deferred context, no
             * proxy, no command list to replay - and everything a
             * deferred context cannot do, reading back above all, works. */
            ring->api                          = HW_API_D3D11;
            ring->real                         = real;
            ring->d3d11_v2                     = true;
            ring->iface.d3d11                  = *(const struct retro_hw_render_interface_d3d11*)real;
            ring->iface.d3d11.handle           = thr;
            ring->iface.d3d11.lock_context     = hw_d3d11_lock_context;
            ring->iface.d3d11.unlock_context   = hw_d3d11_unlock_context;
            ring->iface.d3d11.set_texture      = hw_d3d11_set_texture;
            ring->iface.d3d11.interface_version   = RETRO_HW_RENDER_INTERFACE_D3D11_VERSION_2;
            ring->iface.d3d11.get_sync_index      = hw_d3d11_get_sync_index;
            ring->iface.d3d11.get_sync_index_mask = hw_d3d11_get_sync_index_mask;
            ring->iface.d3d11.wait_sync_index     = hw_d3d11_wait_sync_index;
            *iface = (const struct retro_hw_render_interface*)&ring->iface.d3d11;
            if (ring->logged_version != ring->iface.d3d11.interface_version)
            {
               ring->logged_version = ring->iface.d3d11.interface_version;
               RARCH_LOG("[Video] Threaded video: D3D11 hardware render interface version %u, the core keeps the immediate context%s.\n",
                     ring->iface.d3d11.interface_version,
                     ", no frame copy");
            }
            return true;
         }
         if (!ring->core_ctx
               && !thr->poke->hw_ring_context_new(thr->driver_data, &ring->core_ctx))
            return false;
         ring->api                 = HW_API_D3D11;
         ring->real                = real;
         /* Device and compiler pass through; the context is the proxy
          * over the core's own deferred one, never the video thread's
          * immediate one. */
         ring->iface.d3d11         = *(const struct retro_hw_render_interface_d3d11*)real;
         ring->iface.d3d11.handle  = thr;
         ring->iface.d3d11.context = (ID3D11DeviceContext*)ring->core_ctx;
         /* Version 1, whatever the driver's own interface says. */
         ring->iface.d3d11.interface_version = RETRO_HW_RENDER_INTERFACE_D3D11_VERSION;
         ring->iface.d3d11.lock_context      = NULL;
         ring->iface.d3d11.unlock_context    = NULL;
         ring->iface.d3d11.set_texture       = NULL;
         *iface = (const struct retro_hw_render_interface*)&ring->iface.d3d11;
         return true;
#endif
      default:
         return false;
   }
}

/* OpenGL: there is no interface to interpose - the core renders into a
 * framebuffer the driver hands it, on the context that is current. The
 * ring is set up when the main thread takes the core's context, which
 * must happen before the core's context_reset runs there. */
bool video_thread_hw_bind_core_context(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;
   hw_ring_t *ring;
   video_driver_state_t *video_st = video_state_get_ptr();

   if (!thr || !thr->poke)
      return false;
   switch (video_st->hw_render.context_type)
   {
      case RETRO_HW_CONTEXT_OPENGL:
      case RETRO_HW_CONTEXT_OPENGL_CORE:
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
         break;
      default:
         return false;
   }
   if (!thr->poke->hw_ring_context_new || !thr->poke->hw_ring_framebuffer
         || !thr->poke->hw_ring_capture || !thr->poke->hw_ring_present_slot
         || !thr->poke->hw_ring_fence_new)
      return false;
   if (!hw_ring_setup(thr, &ring))
      return false;
   if (!ring->core_ctx
         && !thr->poke->hw_ring_context_new(thr->driver_data, &ring->core_ctx))
   {
      RARCH_ERR("[Video] Threaded video: the core's GL context could not be "
            "taken on the main thread; the core will have no context.\n");
      return false;
   }
   ring->api = HW_API_GL;
   return true;
}

/* thread_poke.get_current_framebuffer: the framebuffer for the slot the
 * core is filling. Main thread. */
uintptr_t video_thread_hw_get_current_framebuffer(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;
   hw_ring_t *ring     = hw_ring_of(data);
   if (!ring || ring->api != HW_API_GL || !thr->poke->hw_ring_framebuffer)
      return 0;
   return thr->poke->hw_ring_framebuffer(thr->driver_data, ring->index);
}

int video_thread_hw_publish(thread_video_t *thr)
{
   hw_ring_t *ring = (hw_ring_t*)thr->frame.hw_ring;
   unsigned published;
   if (!ring)
      return -1;
   published = ring->index;
#ifdef HAVE_D3D11
   if (ring->api == HW_API_D3D11)
   {
      /* Close the core's recording into the slot; the slot must not be
       * mid-replay on the video thread. */
      hw_wait_slot(ring, published);
      if (ring->d3d11_v2)
      {
         /* Version 2: the driver copies the texture the core named into
          * the slot, now, on the immediate context the core's thread
          * holds the lock for. */
         hw_slot_t *s            = &ring->slot[published];
         ID3D11Texture2D *texture = s->d3d11_texture;
         s->d3d11_texture        = NULL;
         if (!texture)
            return -1;
         /* Nothing to do with the texture on this thread but keep it
          * alive. The core waited for this slot before it drew into the
          * texture, so what the slot held is free. */
         hw_d3d11_slot_release_direct(s);
         texture->lpVtbl->AddRef(texture);
         s->d3d11_direct = texture;
      }
      else if (!thr->poke->hw_ring_capture(thr->driver_data, published,
               ring->core_ctx, 0))
         return -1;
   }
#endif
   if (ring->api == HW_API_GL)
   {
      /* Fence the core's rendering into the slot, on its context. */
      if (!thr->poke->hw_ring_capture(thr->driver_data, published, NULL, 0))
         return -1;
   }
#ifdef HAVE_D3D12
   if (ring->api == HW_API_D3D12)
   {
      /* The slot's copy is taken now, on this thread, so the copy is
       * queued before the core's next frame: the slot must be free of
       * the video thread first. */
      hw_slot_t *s = &ring->slot[published];
      hw_wait_slot(ring, published);
      if (s->v2)
      {
         /* Version 2: nothing to do on this thread. The video thread
          * reads the core's texture itself, behind the core's fence. */
         if (!s->v2_texture)
            return -1;
      }
      else if (!s->texture || !thr->poke->hw_ring_capture(thr->driver_data,
               published, s->texture, s->format))
         return -1;
   }
#endif
   ring->index = (ring->index + 1) % VIDEO_THREAD_HW_RING;
   /* The slot the core fills next was last driven two frames ago;
    * normally long done, and if not this is where the core waits. */
#ifdef HAVE_D3D12
   /* Called from video_thread_frame() with thr->lock held. */
   if (ring->api == HW_API_D3D12)
      hw_d3d12_wait_queued(ring, ring->index, true);
#endif
#ifdef HAVE_D3D11
   if (ring->api == HW_API_D3D11 && ring->d3d11_v2)
      hw_wait_queued(ring, ring->index, true);
#endif
   hw_wait_slot(ring, ring->index);
   return (int)published;
}

void video_thread_hw_before_frame(thread_video_t *thr, int hw_slot)
{
   hw_ring_t *ring = (hw_ring_t*)thr->frame.hw_ring;
   if (!ring || hw_slot < 0 || hw_slot >= VIDEO_THREAD_HW_RING)
      return;
   switch (ring->api)
   {
#ifdef HAVE_VULKAN
      case HW_API_VULKAN:
      {
         hw_slot_t *s = &ring->slot[hw_slot];
         thr->poke->hw_ring_install(thr->driver_data,
               s->has_image ? &s->image : NULL,
               s->semaphores, s->num_semaphores, s->src_queue_family,
               s->cmd, s->num_cmd);
         break;
      }
#endif
#ifdef HAVE_D3D12
      case HW_API_D3D12:
      {
         hw_slot_t *s = &ring->slot[hw_slot];
         if (s->v2)
         {
            d3d12_hw_ring_frame_t f;
            f.texture = s->v2_texture;
            f.format  = (DXGI_FORMAT)s->format;
            f.fence   = s->v2_fence;
            f.value   = s->v2_value;
            thr->poke->hw_ring_install(thr->driver_data, &f, NULL, 0, 0, NULL, 0);
         }
         else
            thr->poke->hw_ring_present_slot(thr->driver_data, (unsigned)hw_slot);
         break;
      }
#endif
#ifdef HAVE_D3D11
      case HW_API_D3D11:
         thr->poke->hw_ring_install(thr->driver_data,
               ring->slot[hw_slot].d3d11_direct, NULL, 0, 0, NULL, 0);
         break;
#endif
      case HW_API_GL:
         thr->poke->hw_ring_present_slot(thr->driver_data, (unsigned)hw_slot);
         break;
      default:
         break;
   }
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

static void hw_context_free_cb(void *data)
{
   thread_video_t *thr = (thread_video_t*)data;
   hw_ring_t *ring     = (hw_ring_t*)thr->frame.hw_ring;
   thr->poke->hw_ring_context_free(thr->driver_data, ring->core_ctx);
   ring->core_ctx = NULL;
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
#ifdef HAVE_VULKAN
      free(s->semaphores);
      free(s->cmd);
#endif
#ifdef HAVE_D3D12
      /* The references a version 2 handoff took. */
      if (ring->api == HW_API_D3D12)
         hw_d3d12_slot_release_v2(s);
#endif
#ifdef HAVE_D3D11
      if (ring->api == HW_API_D3D11)
         hw_d3d11_slot_release_direct(s);
#endif
   }
   if (ring->core_ctx && thr->poke && thr->poke->hw_ring_context_free)
   {
      /* The core's context is the main thread's, and giving it up
       * must happen there: this runs on the video thread, inside the
       * free command the main thread is waiting on. */
      video_thread_call_on_waiter(hw_context_free_cb, thr);
   }
   free(ring);
   thr->frame.hw_ring = NULL;
}

/* Hardware cores run under the wrapper whenever the wrapper runs, where
 * a ring exists for their API: the ring gives them what the swapchain
 * gave them unthreaded, and a core that honoured the interface there
 * honours it here. Other contexts have no ring yet and stay unthreaded. */
bool video_thread_hw_allowed(void)
{
   settings_t *settings           = config_get_ptr();
   video_driver_state_t *video_st = video_state_get_ptr();
   if (!settings)
      return false;
   switch (video_st->hw_render.context_type)
   {
#ifdef HAVE_VULKAN
      case RETRO_HW_CONTEXT_VULKAN:
         return string_is_equal(settings->arrays.video_driver, "vulkan");
#endif
#ifdef HAVE_D3D12
      case RETRO_HW_CONTEXT_D3D12:
         return string_is_equal(settings->arrays.video_driver, "d3d12");
#endif
#if defined(HAVE_OPENGL) || defined(HAVE_OPENGLES)
      case RETRO_HW_CONTEXT_OPENGL:
      case RETRO_HW_CONTEXT_OPENGL_CORE:
      case RETRO_HW_CONTEXT_OPENGLES2:
      case RETRO_HW_CONTEXT_OPENGLES3:
      case RETRO_HW_CONTEXT_OPENGLES_VERSION:
#ifdef __APPLE__
         /* The ring needs the core's context to share objects with the
          * driver's - textures, framebuffers' attachments, sync objects.
          * The Cocoa GL context driver creates its hardware context with
          * no share group, so on the ring a core there rendered into
          * objects the video thread could not see, and locked up. Until
          * that driver shares the two, OpenGL cores stay unthreaded on
          * Apple, and threaded video is declined for them rather than
          * installed with a ring that cannot work. */
         return false;
#else
         return string_is_equal(settings->arrays.video_driver, "gl")
             || string_is_equal(settings->arrays.video_driver, "glcore");
#endif
#endif
#ifdef HAVE_D3D11
      case RETRO_HW_CONTEXT_D3D11:
         return string_is_equal(settings->arrays.video_driver, "d3d11");
#endif
      default:
         return false;
   }
}

#else /* !VIDEO_THREAD_HW_ANY */

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
bool video_thread_hw_bind_core_context(void *data) { (void)data; return false; }
uintptr_t video_thread_hw_get_current_framebuffer(void *data) { (void)data; return 0; }

#endif /* VIDEO_THREAD_HW_ANY */

#endif /* HAVE_THREADS */

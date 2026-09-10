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
#endif
#ifdef HAVE_D3D11
#include <libretro_d3d11.h>
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
#ifdef HAVE_D3D12
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
}
#endif /* HAVE_D3D12 */

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
      if (!thr->poke->hw_ring_capture(thr->driver_data, published,
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
      if (!s->texture || !thr->poke->hw_ring_capture(thr->driver_data,
               published, s->texture, s->format))
         return -1;
   }
#endif
   ring->index = (ring->index + 1) % VIDEO_THREAD_HW_RING;
   /* The slot the core fills next was last driven two frames ago;
    * normally long done, and if not this is where the core waits. */
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
         thr->poke->hw_ring_present_slot(thr->driver_data, (unsigned)hw_slot);
         break;
#endif
#ifdef HAVE_D3D11
      case HW_API_D3D11:
         thr->poke->hw_ring_present_slot(thr->driver_data, (unsigned)hw_slot);
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

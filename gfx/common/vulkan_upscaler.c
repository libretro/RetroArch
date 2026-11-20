/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - Libretro team
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
#include <retro_atomic.h>
#include <dynamic/dylib.h>
#include <gfx/nvidia/ngx_loader.h>

#include <vulkan/vulkan_symbol_wrapper.h>

#include "../../verbosity.h"
#include "../../configuration.h"
#include "vulkan_upscaler.h"
#include "vulkan_ngx.h"
#include "vulkan_ffx.h"
#include "vulkan_xess.h"

typedef struct vulkan_upscaler_setup
{
   struct vulkan_upscaler_setup *next;
   VkCommandPool pool;
   VkCommandBuffer cmd;
   unsigned frames;
} vulkan_upscaler_setup_t;

struct vulkan_upscaler
{
   enum video_temporal_upscaler kind;
   vulkan_ngx_t *ngx;
   vulkan_ffx_t *ffx;
   vulkan_xess_t *xess;
   video_upscaler_async_t *async;
   vulkan_upscaler_device_t *owner;
   VkInstance instance;
   VkPhysicalDevice gpu;
   VkDevice device;
   PFN_vkGetInstanceProcAddr gipa;
   PFN_vkGetDeviceProcAddr gdpa;
   VkPhysicalDeviceMemoryProperties memory_properties;
   char *dir;
   char *sr_dir;
   VkFence reload_fence;

   vulkan_upscaler_setup_t *setup_ready;
   vulkan_upscaler_setup_t *setup_submit;
   vulkan_upscaler_setup_t *setup_retired;
   unsigned cache_mode, cache_out, cache_render, cache_min, cache_max;
   bool cache_valid;
   retro_atomic_int_t frame_started;
   bool nr_loaded, nr_reload, nr_signalled, nr_queued;
   unsigned queue_family;
};

struct vulkan_upscaler_instance
{
   VkInstance instance;
   PFN_vkDestroyInstance destroy;
   dylib_t loader;
   retro_atomic_int_t refs;
};

struct vulkan_upscaler_device
{
   VkDevice device;
   PFN_vkDestroyDevice destroy;
   vulkan_upscaler_instance_t *instance;
   retro_atomic_int_t refs;
};

vulkan_upscaler_instance_t *vulkan_upscaler_instance_new(VkInstance instance,
      PFN_vkGetInstanceProcAddr gipa)
{
   vulkan_upscaler_instance_t *owner = (vulkan_upscaler_instance_t*)
      calloc(1, sizeof(*owner));
   if (!owner)
      return NULL;
#ifdef _WIN32
   owner->loader = dylib_load("vulkan-1.dll");
#else
   owner->loader = dylib_load("libvulkan.so.1");
#endif
   owner->destroy = (PFN_vkDestroyInstance)gipa(instance, "vkDestroyInstance");
   if (!owner->loader || !owner->destroy)
   {
      if (owner->loader)
         dylib_close(owner->loader);
      free(owner);
      return NULL;
   }
   owner->instance = instance;
   retro_atomic_store_release_int(&owner->refs, 1);
   return owner;
}

void vulkan_upscaler_instance_release(vulkan_upscaler_instance_t *owner)
{
   if (owner && retro_atomic_fetch_sub_int(&owner->refs, 1) == 1)
   {
      owner->destroy(owner->instance, NULL);
      dylib_close(owner->loader);
      free(owner);
   }
}

vulkan_upscaler_device_t *vulkan_upscaler_device_new(
      vulkan_upscaler_instance_t *instance, VkDevice device,
      PFN_vkGetDeviceProcAddr gdpa)
{
   vulkan_upscaler_device_t *owner = (vulkan_upscaler_device_t*)
      calloc(1, sizeof(*owner));
   if (!owner)
      return NULL;
   owner->destroy = (PFN_vkDestroyDevice)gdpa(device, "vkDestroyDevice");
   if (!owner->destroy)
   {
      free(owner);
      return NULL;
   }
   owner->device   = device;
   owner->instance = instance;
   retro_atomic_fetch_add_int(&instance->refs, 1);
   retro_atomic_store_release_int(&owner->refs, 1);
   return owner;
}

void vulkan_upscaler_device_release(vulkan_upscaler_device_t *owner)
{
   if (owner && retro_atomic_fetch_sub_int(&owner->refs, 1) == 1)
   {
      owner->destroy(owner->device, NULL);
      vulkan_upscaler_instance_release(owner->instance);
      free(owner);
   }
}

static void vulkan_upscaler_setup_free(vulkan_upscaler_t *up, vulkan_upscaler_setup_t *item)
{
   PFN_vkDestroyCommandPool destroy = (PFN_vkDestroyCommandPool)
      up->gdpa(up->device, "vkDestroyCommandPool");
   if (item->pool)
      destroy(up->device, item->pool, NULL);
   free(item);
}

static void vulkan_upscaler_setup_clear(vulkan_upscaler_t *up)
{
   vulkan_upscaler_setup_t *item;
   if (up->setup_ready)
      vulkan_upscaler_setup_free(up, up->setup_ready);
   if (up->setup_submit)
      vulkan_upscaler_setup_free(up, up->setup_submit);
   up->setup_ready = up->setup_submit = NULL;
   while ((item = up->setup_retired))
   {
      up->setup_retired = item->next;
      vulkan_upscaler_setup_free(up, item);
   }
}

static void vulkan_upscaler_async_prepare(void *data)
{
   vulkan_upscaler_t *up = (vulkan_upscaler_t*)data;
   vulkan_upscaler_setup_t *item = (vulkan_upscaler_setup_t*)calloc(1, sizeof(*item));
   VkCommandPoolCreateInfo pool_info;
   VkCommandBufferAllocateInfo alloc;
   VkCommandBufferBeginInfo begin;
   PFN_vkCreateCommandPool create_pool = (PFN_vkCreateCommandPool)
      up->gdpa(up->device, "vkCreateCommandPool");
   PFN_vkAllocateCommandBuffers allocate = (PFN_vkAllocateCommandBuffers)
      up->gdpa(up->device, "vkAllocateCommandBuffers");
   PFN_vkBeginCommandBuffer begin_cmd = (PFN_vkBeginCommandBuffer)
      up->gdpa(up->device, "vkBeginCommandBuffer");
   PFN_vkEndCommandBuffer end_cmd = (PFN_vkEndCommandBuffer)
      up->gdpa(up->device, "vkEndCommandBuffer");
   if (!item)
      goto fail;
   pool_info.sType = VK_STRUCTURE_TYPE_COMMAND_POOL_CREATE_INFO;
   pool_info.pNext = NULL;
   pool_info.flags = VK_COMMAND_POOL_CREATE_TRANSIENT_BIT;
   pool_info.queueFamilyIndex = up->queue_family;
   if (create_pool(up->device, &pool_info, NULL, &item->pool) != VK_SUCCESS)
      goto fail;
   alloc.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_ALLOCATE_INFO;
   alloc.pNext = NULL;
   alloc.commandPool = item->pool;
   alloc.level = VK_COMMAND_BUFFER_LEVEL_PRIMARY;
   alloc.commandBufferCount = 1;
   if (allocate(up->device, &alloc, &item->cmd) != VK_SUCCESS)
      goto fail;
   begin.sType = VK_STRUCTURE_TYPE_COMMAND_BUFFER_BEGIN_INFO;
   begin.pNext = NULL;
   begin.flags = VK_COMMAND_BUFFER_USAGE_ONE_TIME_SUBMIT_BIT;
   begin.pInheritanceInfo = NULL;
   if (begin_cmd(item->cmd, &begin) != VK_SUCCESS)
      goto fail;
   vulkan_ngx_prepare(up->ngx, item->cmd);
   if (end_cmd(item->cmd) != VK_SUCCESS)
      goto fail;
   up->setup_ready = item;
   return;
fail:
   video_upscaler_async_fail(up->async);
   vulkan_ngx_cancel_prepare(up->ngx);
   if (item)
      vulkan_upscaler_setup_free(up, item);
}

static bool vulkan_upscaler_async_init(void *data)
{
   vulkan_upscaler_t *up = (vulkan_upscaler_t*)data;
   const char *path = up->dir;
   PFN_vkCreateFence create = (PFN_vkCreateFence)up->gdpa(up->device, "vkCreateFence");
   VkFenceCreateInfo info;
   info.sType = VK_STRUCTURE_TYPE_FENCE_CREATE_INFO;
   info.pNext = NULL;
   info.flags = 0;
   if (!up->reload_fence
         && create(up->device, &info, NULL, &up->reload_fence) != VK_SUCCESS)
      return false;
   if (!up->nr_loaded)
   {
      if (!up->sr_dir)
         up->sr_dir = ngx_lib_sr_runtime(up->dir);
      if (!up->sr_dir)
         return false;
      path = up->sr_dir;
   }
   up->ngx = vulkan_ngx_init(up->instance, up->gpu, up->device,
         up->gipa, up->gdpa, &up->memory_properties, path);
   vulkan_ngx_defer(up->ngx);
   if (up->ngx && up->nr_loaded)
      ngx_lib_keep_feature(up->dir, NGX_FEATURE_DLSS_NR);
   return up->ngx != NULL;
}

static void vulkan_upscaler_async_reload_nr(void *data)
{
   vulkan_upscaler_t *up = (vulkan_upscaler_t*)data;
   const char *path = up->dir;
   PFN_vkWaitForFences wait = (PFN_vkWaitForFences)up->gdpa(up->device, "vkWaitForFences");
   if (wait(up->device, 1, &up->reload_fence, VK_TRUE, UINT64_MAX) != VK_SUCCESS)
   {
      video_upscaler_async_fail(up->async);
      return;
   }
   vulkan_ngx_free(up->ngx);
   vulkan_upscaler_setup_clear(up);
   up->ngx = vulkan_ngx_init(up->instance, up->gpu, up->device,
         up->gipa, up->gdpa, &up->memory_properties, path);
   vulkan_ngx_defer(up->ngx);
   if (!up->ngx)
      video_upscaler_async_fail(up->async);
   else
      ngx_lib_keep_feature(up->dir, NGX_FEATURE_DLSS_NR);
   up->nr_loaded = true;
}

static void vulkan_upscaler_async_reset(void *data)
{
   vulkan_upscaler_t *up = (vulkan_upscaler_t*)data;
   vulkan_ngx_reset(up->ngx);
   vulkan_upscaler_setup_clear(up);
}

static void vulkan_upscaler_async_destroy(void *data)
{
   vulkan_upscaler_t *up = (vulkan_upscaler_t*)data;
   vulkan_ngx_free(up->ngx);
   vulkan_upscaler_setup_clear(up);
   if (up->reload_fence)
   {
      PFN_vkDestroyFence destroy = (PFN_vkDestroyFence)up->gdpa(up->device, "vkDestroyFence");
      destroy(up->device, up->reload_fence, NULL);
   }
   vulkan_upscaler_device_release(up->owner);
   free(up->sr_dir);
   free(up->dir);
   free(up);
}

vulkan_upscaler_t *vulkan_upscaler_init_async(VkInstance instance,
      VkPhysicalDevice gpu, VkDevice device,
      PFN_vkGetInstanceProcAddr gipa, PFN_vkGetDeviceProcAddr gdpa,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      const char *dir, vulkan_upscaler_device_t *owner, unsigned queue_family)
{
   vulkan_upscaler_t *up;
   if (!owner || !ngx_lib_get())
      return NULL;
   up = (vulkan_upscaler_t*)calloc(1, sizeof(*up));
   if (!up)
      return NULL;
   up->kind     = VIDEO_TEMPORAL_UPSCALER_DLSS;
   up->instance = instance;
   up->gpu      = gpu;
   up->device   = device;
   up->gipa     = gipa;
   up->gdpa     = gdpa;
   up->memory_properties = *memory_properties;
   up->dir      = strdup(dir ? dir : "");
   up->owner    = owner;
   up->queue_family = queue_family;
   up->nr_loaded = config_get_ptr()->bools.video_dlss_nr_enable || ngx_lib_is_optiscaler();
   if (up->nr_loaded)
      video_upscaler_retire_cached();
   if (up->dir)
      up->async = video_upscaler_async_new(up, vulkan_upscaler_async_init,
            vulkan_upscaler_async_reset, vulkan_upscaler_async_destroy);
   if (!up->async)
   {
      free(up->dir);
      free(up);
      return NULL;
   }
   retro_atomic_fetch_add_int(&owner->refs, 1);
   return up;
}

unsigned vulkan_upscaler_instance_extensions(enum video_temporal_upscaler kind,
      const char *dir, const char **list, unsigned count, unsigned cap)
{
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         return vulkan_ngx_instance_extensions(list, count, cap);
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         return vulkan_xess_instance_extensions(dir, list, count, cap);
      default:
         break;
   }
   return count;
}

unsigned vulkan_upscaler_device_extensions(enum video_temporal_upscaler kind,
      const char *dir, VkPhysicalDevice gpu, PFN_vkGetInstanceProcAddr gipa,
      VkInstance instance, const char **list, unsigned count, unsigned cap)
{
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         return vulkan_ngx_device_extensions(gpu, gipa, instance, list, count, cap);
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         return vulkan_ffx_device_extensions(dir, gpu, gipa, instance, list, count, cap);
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         return vulkan_xess_device_extensions(dir, gpu, gipa, instance, list, count, cap);
      default:
         break;
   }
   return count;
}

const void *vulkan_upscaler_device_features(enum video_temporal_upscaler kind,
      const char *dir, VkPhysicalDevice gpu, PFN_vkGetInstanceProcAddr gipa,
      VkInstance instance, const void *pnext)
{
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         return vulkan_ffx_device_features(dir, gpu, gipa, instance, pnext);
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         return vulkan_xess_device_features(dir, gpu, gipa, instance, pnext);
      default:
         break;
   }
   return pnext;
}

vulkan_upscaler_t *vulkan_upscaler_init(enum video_temporal_upscaler kind,
      VkInstance instance, VkPhysicalDevice gpu, VkDevice device,
      PFN_vkGetInstanceProcAddr gipa, PFN_vkGetDeviceProcAddr gdpa,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      const char *dir)
{
   vulkan_upscaler_t *up = (vulkan_upscaler_t*)calloc(1, sizeof(*up));
   if (!up)
      return NULL;
   up->kind = kind;
   switch (kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         if (video_upscaler_async_busy())
         {
            free(up);
            return NULL;
         }
         up->ngx = vulkan_ngx_init(instance, gpu, device, gipa, gdpa,
               memory_properties, dir);
         break;
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         up->ffx = vulkan_ffx_init(gpu, device, gdpa, memory_properties, dir);
         break;
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         up->xess = vulkan_xess_init(instance, gpu, device, memory_properties, dir);
         break;
      default:
         break;
   }
   if (!up->ngx && !up->ffx && !up->xess)
   {
      free(up);
      return NULL;
   }
   return up;
}

static bool vulkan_upscaler_optimal_impl(vulkan_upscaler_t *up, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims)
{
   if (!up)
      return false;
   switch (up->kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
         return vulkan_ngx_optimal(up->ngx, video_upscaler_mode_ngx(mode),
               out_dims, render_dims, min_dims, max_dims);
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         return vulkan_ffx_optimal(up->ffx, video_upscaler_mode_ffx(mode),
               out_dims, render_dims, min_dims, max_dims);
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         return vulkan_xess_optimal(up->xess, video_upscaler_mode_xess(mode),
               out_dims, render_dims, min_dims, max_dims);
      default:
         break;
   }
   return false;
}

bool vulkan_upscaler_optimal(vulkan_upscaler_t *up, unsigned mode,
      unsigned out_dims, unsigned *render_dims,
      unsigned *min_dims, unsigned *max_dims)
{
   bool ok;
   if (!up)
      return false;
   /* A reused session also lets the next core submit its native frame
    * before adopting the DLSS render policy. */
   if (up->async && !retro_atomic_load_acquire_int(&up->frame_started))
      return false;
   /* This snapshot belongs to the core caller and stays valid while
    * the worker prepares the feature for its requested mode and size. */
   if (up->async && up->cache_valid && up->cache_mode == mode && up->cache_out == out_dims)
   {
      *render_dims = up->cache_render;
      *min_dims    = up->cache_min;
      *max_dims    = up->cache_max;
      return true;
   }
   if (up->async && !video_upscaler_async_enter(up->async))
      return false;

   ok = vulkan_upscaler_optimal_impl(up, mode, out_dims, render_dims, min_dims, max_dims);
   if (up->async)
   {
      if (ok)
      {
         up->cache_mode   = mode;
         up->cache_out    = out_dims;
         up->cache_render = *render_dims;
         up->cache_min    = *min_dims;
         up->cache_max    = *max_dims;
         up->cache_valid  = true;
      }
      video_upscaler_async_leave();
   }
   return ok;
}

static bool vulkan_upscaler_upscale_impl(vulkan_upscaler_t *up, VkCommandBuffer cmd,
      const vulkan_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, vulkan_upscaler_image_t *out)
{
   if (!up)
      return false;
   switch (up->kind)
   {
      case VIDEO_TEMPORAL_UPSCALER_DLSS:
      {
         vulkan_ngx_image_t in, res;
         in.image  = color->image;
         in.view   = color->view;
         in.layout = color->layout;
         in.format = color->format;
         in.dims   = color->dims;
         if (!vulkan_ngx_upscale(up->ngx, cmd, &in, inputs,
                  video_upscaler_mode_ngx(mode), video_upscaler_nr_mode(),
                  out_dims, &res))
            return false;
         out->image  = res.image;
         out->view   = res.view;
         out->layout = res.layout;
         out->format = res.format;
         out->dims   = res.dims;
         return true;
      }
      case VIDEO_TEMPORAL_UPSCALER_FSR:
         return vulkan_ffx_upscale(up->ffx, cmd, color, inputs,
               video_upscaler_mode_ffx(mode), out_dims, out);
      case VIDEO_TEMPORAL_UPSCALER_XESS:
         return vulkan_xess_upscale(up->xess, cmd, color, inputs,
               video_upscaler_mode_xess(mode), out_dims, out);
      default:
         break;
   }
   return false;
}

bool vulkan_upscaler_upscale(vulkan_upscaler_t *up, VkCommandBuffer cmd,
      const vulkan_upscaler_image_t *color,
      const struct retro_hw_upscaler_inputs *inputs,
      unsigned mode, unsigned out_dims, vulkan_upscaler_image_t *out)
{
   bool ok;
   if (!up || (up->async && (!retro_atomic_load_acquire_int(&up->frame_started)
               || !video_upscaler_async_enter(up->async))))
      return false;
   if (up->async && (!up->nr_loaded && video_upscaler_nr_mode()))
   {
      if (!up->nr_reload)
         video_upscaler_retire_cached();
      up->nr_reload = true;
   }
   if (up->async && up->nr_reload && !up->nr_loaded)
   {
      video_upscaler_async_leave();
      return false;
   }
   if (up->async && up->setup_ready)
   {
      up->setup_submit = up->setup_ready;
      up->setup_ready  = NULL;
   }
   ok = vulkan_upscaler_upscale_impl(up, cmd, color, inputs, mode, out_dims, out);
   if (up->async)
   {
      if (vulkan_ngx_prepare_pending(up->ngx))
         video_upscaler_async_work(up->async, vulkan_upscaler_async_prepare);
      video_upscaler_async_leave();
   }
   return ok;
}

VkCommandBuffer vulkan_upscaler_take_commands(vulkan_upscaler_t *up)
{
   vulkan_upscaler_setup_t *item;
   if (!up || !(item = up->setup_submit))
      return VK_NULL_HANDLE;
   up->setup_submit  = NULL;
   item->frames      = VULKAN_UPSCALER_RETIRE_FRAMES;
   item->next        = up->setup_retired;
   up->setup_retired = item;
   return item->cmd;
}

bool vulkan_upscaler_pending(vulkan_upscaler_t *up)
{
   return up && up->async && video_upscaler_async_pending(up->async);
}

VkResult vulkan_upscaler_signal_reload(vulkan_upscaler_t *up, VkQueue queue)
{
   VkResult res;
   VkSubmitInfo submit;
   if (!up || !up->nr_reload || up->nr_signalled)
      return VK_SUCCESS;
   /* An empty submit: the fence signals once the queue's work so far is
    * done. Filled here, as vulkan_queue_signal_fence() does, so this file
    * keeps linking without vulkan_common.c. */
   memset(&submit, 0, sizeof(submit));
   submit.sType = VK_STRUCTURE_TYPE_SUBMIT_INFO;
   res = vkQueueSubmit(queue, 1, &submit, up->reload_fence);
   if (res == VK_SUCCESS)
      up->nr_signalled = true;
   else
      video_upscaler_async_fail(up->async);
   return res;
}

void vulkan_upscaler_frame_done(vulkan_upscaler_t *up)
{
   if (!up)
      return;
   if (up->async)
   {
      vulkan_upscaler_setup_t **link;
      retro_atomic_store_release_int(&up->frame_started, 1);
      if (up->nr_signalled && !up->nr_queued)
      {
         up->nr_queued = video_upscaler_async_work(up->async, vulkan_upscaler_async_reload_nr);
      }
      video_upscaler_async_start(up->async);
      if (!video_upscaler_async_enter(up->async))
         return;
      for (link = &up->setup_retired; *link;)
      {
         vulkan_upscaler_setup_t *item = *link;
         if (--item->frames)
            link = &item->next;
         else
         {
            *link = item->next;
            vulkan_upscaler_setup_free(up, item);
         }
      }
   }
   vulkan_ngx_frame_done(up->ngx);
   vulkan_ffx_frame_done(up->ffx);
   vulkan_xess_frame_done(up->xess);
   if (up->async)
      video_upscaler_async_leave();
}

void vulkan_upscaler_free(vulkan_upscaler_t *up)
{
   if (!up)
      return;
   if (up->async)
   {
      video_upscaler_async_free(up->async);
      return;
   }
   vulkan_ngx_free(up->ngx);
   vulkan_ffx_free(up->ffx);
   vulkan_xess_free(up->xess);
   free(up);
}

void vulkan_upscaler_reset(vulkan_upscaler_t *up)
{
   if (up && up->async)
   {
      retro_atomic_store_release_int(&up->frame_started, 0);
      video_upscaler_async_reset(up->async);
   }
   else if (up)
      vulkan_ngx_reset(up->ngx);
}

/* ---- What the backends share ---- */

VkImageAspectFlags vulkan_upscaler_aspect(VkFormat format)
{
   switch (format)
   {
      case VK_FORMAT_D16_UNORM:
      case VK_FORMAT_X8_D24_UNORM_PACK32:
      case VK_FORMAT_D32_SFLOAT:
         return VK_IMAGE_ASPECT_DEPTH_BIT;
      case VK_FORMAT_D16_UNORM_S8_UINT:
      case VK_FORMAT_D24_UNORM_S8_UINT:
      case VK_FORMAT_D32_SFLOAT_S8_UINT:
         return VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT;
      case VK_FORMAT_S8_UINT:
         return VK_IMAGE_ASPECT_STENCIL_BIT;
      default:
         return VK_IMAGE_ASPECT_COLOR_BIT;
   }
}

void vulkan_upscaler_barrier(VkCommandBuffer cmd, VkImage image,
      VkImageAspectFlags aspect, VkImageLayout old_layout,
      VkImageLayout new_layout, VkAccessFlags src_access,
      VkAccessFlags dst_access, VkPipelineStageFlags src_stages,
      VkPipelineStageFlags dst_stages)
{
   VkImageMemoryBarrier barrier;
   barrier.sType                           = VK_STRUCTURE_TYPE_IMAGE_MEMORY_BARRIER;
   barrier.pNext                           = NULL;
   barrier.srcAccessMask                   = src_access;
   barrier.dstAccessMask                   = dst_access;
   barrier.oldLayout                       = old_layout;
   barrier.newLayout                       = new_layout;
   barrier.srcQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   barrier.dstQueueFamilyIndex             = VK_QUEUE_FAMILY_IGNORED;
   barrier.image                           = image;
   barrier.subresourceRange.aspectMask     = aspect;
   barrier.subresourceRange.baseMipLevel   = 0;
   barrier.subresourceRange.levelCount     = 1;
   barrier.subresourceRange.baseArrayLayer = 0;
   barrier.subresourceRange.layerCount     = 1;
   vkCmdPipelineBarrier(cmd, src_stages, dst_stages, 0, 0, NULL, 0, NULL,
         1, &barrier);
}

bool vulkan_upscaler_input_needs_move(const struct retro_hw_upscaler_image *img)
{
   return img->image
      && img->state != (unsigned)VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL
      && img->state != (unsigned)VK_IMAGE_LAYOUT_GENERAL;
}

void vulkan_upscaler_move_input(VkCommandBuffer cmd,
      const struct retro_hw_upscaler_image *img, bool to_read)
{
   VkImageLayout layout = (VkImageLayout)img->state;
   vulkan_upscaler_barrier(cmd, (VkImage)(uintptr_t)img->image,
         vulkan_upscaler_aspect((VkFormat)img->format),
         to_read ? layout : VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL,
         to_read ? VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL : layout,
         to_read ? VK_ACCESS_MEMORY_WRITE_BIT : VK_ACCESS_MEMORY_READ_BIT,
         to_read ? VK_ACCESS_MEMORY_READ_BIT : VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT);
}

void vulkan_upscaler_full_barrier(VkCommandBuffer cmd)
{
   VkMemoryBarrier barrier;
   barrier.sType         = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
   barrier.pNext         = NULL;
   barrier.srcAccessMask = VK_ACCESS_MEMORY_WRITE_BIT;
   barrier.dstAccessMask = VK_ACCESS_MEMORY_READ_BIT | VK_ACCESS_MEMORY_WRITE_BIT;
   vkCmdPipelineBarrier(cmd, VK_PIPELINE_STAGE_ALL_COMMANDS_BIT,
         VK_PIPELINE_STAGE_ALL_COMMANDS_BIT, 0, 1, &barrier, 0, NULL, 0, NULL);
}

bool vulkan_upscaler_output_create(VkDevice device,
      const VkPhysicalDeviceMemoryProperties *memory_properties,
      VkFormat format, unsigned dims, vulkan_upscaler_output_t *out)
{
   VkImageCreateInfo image;
   VkImageViewCreateInfo view;
   VkMemoryRequirements reqs;
   VkMemoryAllocateInfo alloc;
   uint32_t type;
   unsigned i;

   memset(out, 0, sizeof(*out));
   image.sType                 = VK_STRUCTURE_TYPE_IMAGE_CREATE_INFO;
   image.pNext                 = NULL;
   image.flags                 = 0;
   image.imageType             = VK_IMAGE_TYPE_2D;
   image.format                = format;
   image.extent.width          = VIDEO_SCALE_W(dims);
   image.extent.height         = VIDEO_SCALE_H(dims);
   image.extent.depth          = 1;
   image.mipLevels             = 1;
   image.arrayLayers           = 1;
   image.samples               = VK_SAMPLE_COUNT_1_BIT;
   image.tiling                = VK_IMAGE_TILING_OPTIMAL;
   /* (written by the library however it likes to: a transfer too) */
   image.usage                 = VK_IMAGE_USAGE_STORAGE_BIT
                               | VK_IMAGE_USAGE_SAMPLED_BIT
                               | VK_IMAGE_USAGE_TRANSFER_SRC_BIT
                               | VK_IMAGE_USAGE_TRANSFER_DST_BIT;
   image.sharingMode           = VK_SHARING_MODE_EXCLUSIVE;
   image.queueFamilyIndexCount = 0;
   image.pQueueFamilyIndices   = NULL;
   image.initialLayout         = VK_IMAGE_LAYOUT_UNDEFINED;
   if (vkCreateImage(device, &image, NULL, &out->image) != VK_SUCCESS)
   {
      out->image = VK_NULL_HANDLE;
      return false;
   }

   vkGetImageMemoryRequirements(device, out->image, &reqs);
   type = memory_properties->memoryTypeCount;
   for (i = 0; i < memory_properties->memoryTypeCount; i++)
      if (   (reqs.memoryTypeBits & (1u << i))
          && (memory_properties->memoryTypes[i].propertyFlags
             & VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT))
      {
         type = i;
         break;
      }
   if (type == memory_properties->memoryTypeCount)
      for (i = 0; i < memory_properties->memoryTypeCount; i++)
         if (reqs.memoryTypeBits & (1u << i))
         {
            type = i;
            break;
         }
   alloc.sType           = VK_STRUCTURE_TYPE_MEMORY_ALLOCATE_INFO;
   alloc.pNext           = NULL;
   alloc.allocationSize  = reqs.size;
   alloc.memoryTypeIndex = type;
   if (   type == memory_properties->memoryTypeCount
       || vkAllocateMemory(device, &alloc, NULL, &out->memory) != VK_SUCCESS
       || vkBindImageMemory(device, out->image, out->memory, 0) != VK_SUCCESS)
      goto fail;

   view.sType                           = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
   view.pNext                           = NULL;
   view.flags                           = 0;
   view.image                           = out->image;
   view.viewType                        = VK_IMAGE_VIEW_TYPE_2D;
   view.format                          = format;
   view.components.r                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.components.g                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.components.b                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.components.a                    = VK_COMPONENT_SWIZZLE_IDENTITY;
   view.subresourceRange.aspectMask     = VK_IMAGE_ASPECT_COLOR_BIT;
   view.subresourceRange.baseMipLevel   = 0;
   view.subresourceRange.levelCount     = 1;
   view.subresourceRange.baseArrayLayer = 0;
   view.subresourceRange.layerCount     = 1;
   if (vkCreateImageView(device, &view, NULL, &out->view) != VK_SUCCESS)
      goto fail;
   out->format = format;
   out->dims   = dims;
   return true;

fail:
   vulkan_upscaler_output_destroy(device, out);
   return false;
}

void vulkan_upscaler_output_destroy(VkDevice device, vulkan_upscaler_output_t *out)
{
   if (out->view)
      vkDestroyImageView(device, out->view, NULL);
   if (out->image)
      vkDestroyImage(device, out->image, NULL);
   if (out->memory)
      vkFreeMemory(device, out->memory, NULL);
   memset(out, 0, sizeof(*out));
}

static void vulkan_upscaler_ring_free_slot(vulkan_upscaler_ring_t *ring,
      vulkan_upscaler_retired_t *slot)
{
   if (slot->feature && ring->release)
      ring->release(ring->release_data, slot->feature);
   vulkan_upscaler_output_destroy(ring->device, &slot->output);
   memset(slot, 0, sizeof(*slot));
}

void vulkan_upscaler_ring_retire(vulkan_upscaler_ring_t *ring,
      void *feature, vulkan_upscaler_output_t *output)
{
   unsigned i;
   vulkan_upscaler_retired_t *slot = NULL;
   if (!feature && !output->image)
      return;
   for (i = 0; i < VULKAN_UPSCALER_RETIRED_MAX; i++)
      if (!ring->slot[i].frames)
      {
         slot = &ring->slot[i];
         break;
      }
   if (!slot)
   {
      slot = &ring->slot[0];
      for (i = 1; i < VULKAN_UPSCALER_RETIRED_MAX; i++)
         if (ring->slot[i].frames < slot->frames)
            slot = &ring->slot[i];
      vulkan_upscaler_ring_free_slot(ring, slot);
   }
   slot->feature = feature;
   slot->output  = *output;
   slot->frames  = VULKAN_UPSCALER_RETIRE_FRAMES;
   memset(output, 0, sizeof(*output));
}

void vulkan_upscaler_ring_frame_done(vulkan_upscaler_ring_t *ring)
{
   unsigned i;
   for (i = 0; i < VULKAN_UPSCALER_RETIRED_MAX; i++)
   {
      vulkan_upscaler_retired_t *r = &ring->slot[i];
      if (!r->frames || --r->frames)
         continue;
      vulkan_upscaler_ring_free_slot(ring, r);
   }
}

void vulkan_upscaler_ring_flush(vulkan_upscaler_ring_t *ring)
{
   unsigned i;
   for (i = 0; i < VULKAN_UPSCALER_RETIRED_MAX; i++)
      if (ring->slot[i].frames)
         vulkan_upscaler_ring_free_slot(ring, &ring->slot[i]);
}

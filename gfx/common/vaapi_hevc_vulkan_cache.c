/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_hevc_vulkan_cache.h"
#include <stdlib.h>
#include <string.h>

typedef struct vaapi_hevc_vulkan_cache_entry
{
   vaapi_hevc_vulkan_frame_t frame;
   vaapi_hevc_vulkan_submission_t submission;
   vaapi_hevc_vulkan_lease_t lease;
   bool occupied;
} vaapi_hevc_vulkan_cache_entry_t;

struct vaapi_hevc_vulkan_cache
{
   vaapi_hevc_vulkan_cache_config_t config;
   VkPhysicalDeviceMemoryProperties memory;
   vaapi_hevc_vulkan_resources_t resources;
   vaapi_hevc_vulkan_cache_entry_t entries[32];
   video_hw_frame_t stranded;
   struct vaapi_hevc_vulkan_cache *retired_next;
   unsigned count, next;
   bool fp16, closed, deferred, children_destroyed;
};

vaapi_hevc_vulkan_cache_t *vaapi_hevc_vulkan_cache_new(
      const vaapi_hevc_vulkan_cache_config_t *config, unsigned count, bool fp16)
{
   vaapi_hevc_vulkan_cache_t *cache;
   if (!config || !config->memory || !config->gpu || !config->device
         || !config->queue || !config->prime_device || !config->get_format
         || !config->get_image || !config->submit.fence_status || !count
         || count > 32)
      return NULL;
   cache = (vaapi_hevc_vulkan_cache_t *)calloc(1, sizeof(*cache));
   if (!cache)
      return NULL;
   cache->config        = *config;
   cache->memory        = *config->memory;
   cache->config.memory = &cache->memory;
   cache->count         = count;
   cache->fp16          = fp16;
   if (!vaapi_hevc_vulkan_resources_create(&cache->resources, config->device,
             config->family, count, fp16, &config->resources))
   {
      free(cache);
      return NULL;
   }
   return cache;
}

int vaapi_hevc_vulkan_cache_poll(vaapi_hevc_vulkan_cache_t *cache)
{
   unsigned i;
   int result = 1;
   if (!cache || cache->children_destroyed)
      return -1;
   if (cache->stranded.owner)
      result = -1;
   for (i = 0; i < cache->count; i++)
      if (cache->entries[i].occupied)
      {
         int status = vaapi_hevc_vulkan_lease_poll(&cache->entries[i].lease);
         if (status < 0)
         {
            cache->closed = true;
            result        = -1;
         }
         else if (!status && result > 0)
            result = 0;
      }
   return result;
}

static bool vaapi_hevc_vulkan_cache_claim(
      const video_hw_frame_t *frame, vaapi_frame_view_t *view)
{
   return frame && vaapi_frame_callback(frame->abandon)
          && vaapi_frame_claim(frame->owner, frame->done, frame->done_value,
                frame->image, view);
}

static bool vaapi_hevc_vulkan_cache_unclaim(
      vaapi_hevc_vulkan_cache_t *cache, const video_hw_frame_t *frame)
{
   if (vaapi_frame_unclaim(frame->owner, frame->done, frame->done_value, NULL))
      return true;
   cache->stranded = *frame;
   cache->closed   = true;
   return false;
}

static bool vaapi_hevc_vulkan_cache_populate(vaapi_hevc_vulkan_cache_t *cache,
      unsigned index, const vaapi_frame_view_t *view)
{
   vaapi_hevc_vulkan_cache_entry_t *entry         = &cache->entries[index];
   const vaapi_hevc_vulkan_cache_config_t *config = &cache->config;
   vaapi_hevc_vulkan_submission_t *submission     = &entry->submission;
   if (!vaapi_hevc_vulkan_frame_prepare(&entry->frame, view->source,
             config->gpu, config->device, config->prime_device, config->memory,
             &config->image, config->get_format, config->get_image))
      return false;
   submission->frame    = &entry->frame;
   submission->ops      = config->submit;
   submission->device   = config->device;
   submission->queue    = config->queue;
   submission->family   = config->family;
   submission->command  = cache->resources.commands[index];
   submission->fence    = cache->resources.fences[index];
   submission->set      = cache->resources.sets[index];
   submission->sampler  = cache->resources.sampler;
   submission->pipeline = cache->resources.pipeline;
   submission->layout   = cache->resources.layout;
   if (!vaapi_hevc_vulkan_lease_bind(&entry->lease, submission, cache->fp16))
   {
      vaapi_hevc_export_t empty;
      memset(&empty, 0, sizeof(empty));
      vaapi_hevc_vulkan_frame_retire(&entry->frame, &empty);
      memset(entry, 0, sizeof(*entry));
      return false;
   }
   entry->occupied = true;
   return true;
}

int vaapi_hevc_vulkan_cache_prepare(
      vaapi_hevc_vulkan_cache_t *cache, const video_hw_frame_t *frame)
{
   vaapi_frame_view_t view;
   vaapi_hevc_export_t empty;
   unsigned i, index;
   int result = 0;
   if (!cache || cache->closed || vaapi_hevc_vulkan_cache_poll(cache) < 0
         || !vaapi_hevc_vulkan_cache_claim(frame, &view))
      return -1;
   for (i = 0; i < cache->count; i++)
      if (cache->entries[i].occupied
            && vaapi_hevc_vulkan_frame_matches(
                  &cache->entries[i].frame, view.source))
      {
         result = cache->entries[i].lease.owner ? 0 : 1;
         goto done;
      }
   memset(&empty, 0, sizeof(empty));
   for (i = 0; i < cache->count; i++)
   {
      vaapi_hevc_vulkan_cache_entry_t *entry;
      index = (cache->next + i) % cache->count;
      entry = &cache->entries[index];
      if (entry->occupied)
      {
         if (entry->lease.owner || entry->lease.returned.active)
            continue;
         if (!vaapi_hevc_vulkan_frame_retire(&entry->frame, &empty))
            continue;
         memset(entry, 0, sizeof(*entry));
      }
      result = vaapi_hevc_vulkan_cache_populate(cache, index, &view) ? 1 : -1;
      cache->next = (index + 1) % cache->count;
      break;
   }
done:
   if (!vaapi_hevc_vulkan_cache_unclaim(cache, frame))
      return -2;
   return result;
}

int vaapi_hevc_vulkan_cache_present(vaapi_hevc_vulkan_cache_t *cache,
      const video_hw_frame_t *frame, VkImage destination, VkImageView view,
      VkFormat format, unsigned width, unsigned height, VkImageLayout layout,
      bool *prepare_needed)
{
   vaapi_frame_view_t source;
   unsigned i;
   int result;
   if (prepare_needed)
      *prepare_needed = false;
   if (!cache || cache->closed || vaapi_hevc_vulkan_cache_poll(cache) < 0
         || !vaapi_hevc_vulkan_cache_claim(frame, &source))
      return -1;
   for (i = 0; i < cache->count; i++)
      if (cache->entries[i].occupied
            && vaapi_hevc_vulkan_frame_matches(
                  &cache->entries[i].frame, source.source))
         break;
   if (!vaapi_hevc_vulkan_cache_unclaim(cache, frame))
      return -2;
   if (i == cache->count)
   {
      if (prepare_needed)
         *prepare_needed = true;
      return 0;
   }
   result = vaapi_hevc_vulkan_lease_present(&cache->entries[i].lease, frame,
         destination, view, format, width, height, layout);
   if (result == -2)
      cache->closed = true;
   return result;
}

void vaapi_hevc_vulkan_cache_close(vaapi_hevc_vulkan_cache_t *cache)
{
   if (cache)
      cache->closed = true;
}

bool vaapi_hevc_vulkan_cache_free(vaapi_hevc_vulkan_cache_t *cache)
{
   vaapi_hevc_export_t empty;
   unsigned i;
   if (!cache)
      return true;
   if (!cache->closed || cache->deferred
         || vaapi_hevc_vulkan_cache_poll(cache) != 1)
      return false;
   vaapi_hevc_vulkan_resources_destroy(&cache->resources);
   memset(&empty, 0, sizeof(empty));
   for (i = 0; i < cache->count; i++)
      if (cache->entries[i].occupied)
      {
         if (!vaapi_hevc_vulkan_frame_retire(&cache->entries[i].frame, &empty))
            return false;
         cache->entries[i].occupied = false;
      }
   free(cache);
   return true;
}

static vaapi_hevc_vulkan_cache_t *vaapi_hevc_vulkan_retired_caches;

void vaapi_hevc_vulkan_cache_defer(vaapi_hevc_vulkan_cache_t *cache)
{
   if (!cache || cache->deferred)
      return;
   cache->closed                    = true;
   cache->deferred                  = true;
   cache->retired_next              = vaapi_hevc_vulkan_retired_caches;
   vaapi_hevc_vulkan_retired_caches = cache;
}

bool vaapi_hevc_vulkan_cache_device_pending(VkDevice device)
{
   vaapi_hevc_vulkan_cache_t *cache;
   if (!device)
      return false;
   for (cache  = vaapi_hevc_vulkan_retired_caches; cache;
         cache = cache->retired_next)
      if (cache->config.device == device)
         return true;
   return false;
}

void vaapi_hevc_vulkan_cache_device_destroying(VkDevice device)
{
   vaapi_hevc_vulkan_cache_t *cache;
   if (!device)
      return;
   for (cache  = vaapi_hevc_vulkan_retired_caches; cache;
         cache = cache->retired_next)
      if (cache->config.device == device && !cache->children_destroyed)
      {
         unsigned i, j;
         vaapi_hevc_vulkan_resources_destroy(&cache->resources);
         for (i = 0; i < cache->count; i++)
            if (cache->entries[i].occupied)
            {
               vaapi_hevc_vulkan_frame_t *frame = &cache->entries[i].frame;
               for (j = 0; j < 2; j++)
                  vulkan_prime_image_destroy(
                        device, &frame->ops, &frame->layers[j]);
            }
         if (cache->config.destroy_deferred_destination)
            cache->config.destroy_deferred_destination(
                  device, cache->config.deferred_destination);
         cache->children_destroyed = true;
      }
}

void vaapi_hevc_vulkan_cache_device_destroyed(VkDevice device)
{
   vaapi_hevc_vulkan_cache_t **link = &vaapi_hevc_vulkan_retired_caches;
   if (!device)
      return;
   while (*link)
   {
      vaapi_hevc_vulkan_cache_t *cache = *link;
      unsigned i;
      bool complete = true;
      if (cache->config.device != device || !cache->children_destroyed)
      {
         link = &cache->retired_next;
         continue;
      }
      for (i = 0; i < cache->count; i++)
         if (cache->entries[i].occupied)
         {
            if (vaapi_hevc_vulkan_lease_after_device_destroy(
                      &cache->entries[i].lease, device))
               cache->entries[i].occupied = false;
            else
               complete = false;
         }
      if (cache->stranded.owner)
      {
         video_hw_frame_t *frame = &cache->stranded;
         if (vaapi_frame_finish(
                   frame->owner, frame->done, frame->done_value, NULL))
            memset(frame, 0, sizeof(*frame));
         else
            complete = false;
      }
      if (!complete)
      {
         link = &cache->retired_next;
         continue;
      }
      *link = cache->retired_next;
      free(cache);
   }
}

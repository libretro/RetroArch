/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier:
 * GPL-3.0-or-later */
#include "vulkan_android_cache.h"
#include <stdlib.h>
#include <string.h>
struct vulkan_android_cache
{
   vulkan_android_entry_config_t config;
   VkPhysicalDeviceMemoryProperties memory;
   struct vulkan_android_cache *retired_next;
   unsigned count, next;
   bool fp16, closed, deferred, children_destroyed;
   bool occupied[32];
   vulkan_android_cache_entry_t entry[32];
};
vulkan_android_cache_t *vulkan_android_cache_new(
      const vulkan_android_entry_config_t *c, unsigned count, bool fp16)
{
   vulkan_android_cache_t *cache;
   if (!c || !c->memory || !count || count > 32 || !c->device || !c->queue
         || !c->command_pool)
      return NULL;
   cache = (vulkan_android_cache_t *)calloc(1, sizeof(*cache));
   if (!cache)
      return NULL;
   cache->config = *c;
   cache->memory = *c->memory;
   cache->config.memory = &cache->memory;
   cache->count = count;
   cache->fp16 = fp16;
   return cache;
}
static bool vulkan_android_cache_matches(
      const vulkan_android_submission_t *e, const android_frame_view_t *v)
{
   const android_image_view_t *a = &e->source.image, *b = &v->image;
   return a->buffer == b->buffer && a->buffer_width == b->buffer_width
         && a->buffer_height == b->buffer_height && a->left == b->left
         && a->top == b->top && a->right == b->right && a->bottom == b->bottom
         && e->source.pixfmt == v->pixfmt
         && !memcmp(&e->source.color, &v->color, sizeof(v->color));
}
static bool vulkan_android_cache_claim(
      const video_hw_frame_t *frame, android_frame_view_t *view)
{
   if (!frame || !android_frame_callback(frame->abandon)
         || !android_frame_claim(frame->owner, frame->done, frame->done_value,
               frame->image, view))
      return false;
   if (android_frame_resolve_color(view, &frame->color))
      return true;
   android_frame_unclaim(frame->owner, frame->done, frame->done_value);
   return false;
}
int vulkan_android_cache_prepare(
      vulkan_android_cache_t *cache, const video_hw_frame_t *frame)
{
   android_frame_view_t view;
   unsigned i, index;
   int result = 0;
   if (!cache || cache->closed || !vulkan_android_cache_claim(frame, &view))
      return -1;
   for (i = 0; i < cache->count; i++)
      if (cache->occupied[i]
            && vulkan_android_cache_matches(&cache->entry[i].submission, &view))
      {
         result = vulkan_android_submission_poll(&cache->entry[i].submission);
         if (result < 0 && vulkan_android_cache_entry_destroy(&cache->entry[i]))
         {
            cache->occupied[i] = false;
            if (vulkan_android_cache_entry_create(
                      &cache->config, &view, cache->fp16, &cache->entry[i]))
            {
               cache->occupied[i] = true;
               result = 1;
            }
         }
         goto done;
      }
   /* Round-robin eviction only on this cold path, and only after completion. */
   for (i = 0; i < cache->count; i++)
   {
      index = (cache->next + i) % cache->count;
      if (cache->occupied[index]
            && !vulkan_android_cache_entry_destroy(&cache->entry[index]))
         continue;
      cache->occupied[index] = false;
      if (!vulkan_android_cache_entry_create(
                &cache->config, &view, cache->fp16, &cache->entry[index]))
      {
         result = -1;
         goto done;
      }
      cache->occupied[index] = true;
      cache->next = (index + 1) % cache->count;
      result = 1;
      break;
   }
done:
   if (!android_frame_unclaim(frame->owner, frame->done, frame->done_value))
      return -2;
   return result;
}
int vulkan_android_cache_present(vulkan_android_cache_t *cache,
      const video_hw_frame_t *frame, VkImage destination,
      VkImageView image_view, VkImageLayout layout, bool *prepare_needed)
{
   android_frame_view_t view;
   unsigned i;
   if (prepare_needed)
      *prepare_needed = false;
   if (!cache || cache->closed || !vulkan_android_cache_claim(frame, &view))
      return -1;
   for (i = 0; i < cache->count; i++)
      if (cache->occupied[i]
            && vulkan_android_cache_matches(&cache->entry[i].submission, &view))
         break;
   if (!android_frame_unclaim(frame->owner, frame->done, frame->done_value))
      return -2;
   if (i == cache->count)
   {
      if (prepare_needed)
         *prepare_needed = true;
      return 0;
   }
   return vulkan_android_submission_present(
         &cache->entry[i].submission, frame, destination, image_view, layout);
}
int vulkan_android_cache_poll(vulkan_android_cache_t *cache)
{
   unsigned i;
   int result = 1;
   if (!cache || cache->closed)
      return -1;
   for (i = 0; i < cache->count; i++)
      if (cache->occupied[i])
      {
         int status = vulkan_android_submission_poll(&cache->entry[i].submission);
         if (status < result)
            result = status;
      }
   return result;
}
void vulkan_android_cache_close(vulkan_android_cache_t *cache)
{
   if (cache)
      cache->closed = true;
}
bool vulkan_android_cache_free(vulkan_android_cache_t *cache)
{
   unsigned i;
   bool pending = false;
   if (!cache)
      return true;
   if (!cache->closed || cache->deferred)
      return false;
   for (i = 0; i < cache->count; i++)
      if (cache->occupied[i])
      {
         if (vulkan_android_cache_entry_destroy(&cache->entry[i]))
            cache->occupied[i] = false;
         else
            pending = true;
      }
   if (pending)
      return false;
   if (cache->config.destroy_owned_pool)
      cache->config.destroy_owned_pool(
            cache->config.device, cache->config.command_pool, NULL);
   free(cache);
   return true;
}
bool vulkan_android_cache_after_device_destroy(vulkan_android_cache_t *cache)
{
   unsigned i;
   bool complete = true;
   if (!cache)
      return true;
   if (!cache->children_destroyed)
      return false;
   cache->closed = true;
   for (i = 0; i < cache->count; i++)
      if (cache->occupied[i]
            && !vulkan_android_submission_after_device_destroy(
                  &cache->entry[i].submission))
         complete = false;
   return complete;
}

/* Lifecycle-owner list, never accessed by frame or decoder producer threads. */
static vulkan_android_cache_t *vulkan_android_retired_caches;
void vulkan_android_cache_defer(vulkan_android_cache_t *cache)
{
   if (!cache || cache->deferred)
      return;
   cache->closed = true;
   cache->deferred = true;
   cache->retired_next = vulkan_android_retired_caches;
   vulkan_android_retired_caches = cache;
}
bool vulkan_android_cache_device_pending(VkDevice device)
{
   vulkan_android_cache_t *cache = vulkan_android_retired_caches;
   if (!device)
      return false;
   for (; cache; cache = cache->retired_next)
      if (cache->config.device == device)
         return true;
   return false;
}
void vulkan_android_cache_device_collect(VkDevice device)
{
   vulkan_android_cache_t **link = &vulkan_android_retired_caches;
   if (!device)
      return;
   while (*link)
   {
      vulkan_android_cache_t *cache = *link;
      vulkan_android_cache_t *next = cache->retired_next;
      void (*destroy_destination)(VkDevice, void *);
      void *destination;
      if (cache->config.device != device || cache->children_destroyed)
      {
         link = &cache->retired_next;
         continue;
      }
      destroy_destination = cache->config.destroy_deferred_destination;
      destination = cache->config.deferred_destination;
      cache->deferred = false;
      if (!vulkan_android_cache_free(cache))
      {
         cache->deferred = true;
         link = &cache->retired_next;
         continue;
      }
      *link = next;
      if (destroy_destination)
         destroy_destination(device, destination);
   }
}
void vulkan_android_cache_device_destroying(VkDevice device)
{
   vulkan_android_cache_t *cache = vulkan_android_retired_caches;
   if (!device)
      return;
   for (; cache; cache = cache->retired_next)
      if (cache->config.device == device && !cache->children_destroyed)
      {
         unsigned i;
         for (i = 0; i < cache->count; i++)
            if (cache->occupied[i])
            {
               vulkan_android_submission_t *s = &cache->entry[i].submission;
               void *owner = s->held_owner;
               uint64_t slot = s->held_slot, generation = s->held_generation;
               /* Device idle or loss permits child teardown, but external
                * producer memory remains leased until actual device
                * destruction. Do not poll/return it. */
               s->inflight = 0;
               s->held_owner = NULL;
               vulkan_android_cache_entry_destroy(&cache->entry[i]);
               s->held_owner = owner;
               s->held_slot = slot;
               s->held_generation = generation;
            }
         if (cache->config.destroy_owned_pool)
            cache->config.destroy_owned_pool(
                  device, cache->config.command_pool, NULL);
         if (cache->config.destroy_deferred_destination)
            cache->config.destroy_deferred_destination(
                  device, cache->config.deferred_destination);
         cache->children_destroyed = true;
      }
}
void vulkan_android_cache_device_destroyed(VkDevice device)
{
   vulkan_android_cache_t **link = &vulkan_android_retired_caches;
   if (!device)
      return;
   while (*link)
   {
      vulkan_android_cache_t *cache = *link;
      if (cache->config.device != device || !cache->children_destroyed)
      {
         link = &cache->retired_next;
         continue;
      }
      if (!vulkan_android_cache_after_device_destroy(cache))
      {
         link = &cache->retired_next;
         continue;
      }
      *link = cache->retired_next;
      free(cache);
   }
}

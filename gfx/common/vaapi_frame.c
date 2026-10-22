/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include "vaapi_frame.h"
#include "vaapi_hevc_export.h"
#include <retro_atomic.h>
#include <formats/image.h>
#include <stdlib.h>
#include <string.h>

typedef struct vaapi_frame_slot
{
   retro_atomic_int_t state; /* free, pending, claimed, mutation, returned */
   vaapi_hevc_export_t source;
   vaapi_frame_view_t view;
   vaapi_hevc_pool_t *pool;
   uint64_t generation, ticket;
   unsigned slot;
} vaapi_frame_slot_t;

struct vaapi_frame_pool
{
   vaapi_frame_slot_t slots[32];
   unsigned count;
   bool closed;
};

vaapi_frame_pool_t *vaapi_frame_pool_new(unsigned count)
{
   vaapi_frame_pool_t *pool;
   unsigned i;
   if (!count || count > 32)
      return NULL;
#ifndef RETRO_ATOMIC_LOCK_FREE
   return NULL;
#endif
   pool = (vaapi_frame_pool_t *)calloc(1, sizeof(*pool));
   if (!pool)
      return NULL;
   pool->count = count;
   for (i = 0; i < count; i++)
      retro_atomic_int_init(&pool->slots[i].state, 0);
   return pool;
}

void vaapi_frame_pool_close(vaapi_frame_pool_t *pool)
{
   if (pool)
      pool->closed = true;
}

bool vaapi_frame_pool_free(vaapi_frame_pool_t *pool)
{
   unsigned i;
   if (!pool)
      return true;
   if (!pool->closed)
      return false;
   for (i = 0; i < pool->count; i++)
      if (retro_atomic_load_acquire_int(&pool->slots[i].state))
         return false;
   free(pool);
   return true;
}

static bool vaapi_frame_put(void *owner, uint64_t index, uint64_t generation,
      vaapi_hevc_export_t *source, int next)
{
   vaapi_frame_pool_t *pool = (vaapi_frame_pool_t *)owner;
   vaapi_frame_slot_t *slot;
   if (!pool || index >= pool->count)
      return false;
   slot = &pool->slots[(unsigned)index];
   if (!retro_atomic_cas_int(&slot->state, 2, 3))
      return false;
   if (slot->generation != generation
         || (source
               && (slot->source.active || source == &slot->source
                     || !source->active || !source->ready
                     || source->pool != slot->pool || source->slot != slot->slot
                     || source->ticket != slot->ticket))
         || (!source && !slot->source.active))
   {
      retro_atomic_store_release_int(&slot->state, 2);
      return false;
   }
   if (source)
   {
      slot->source = *source;
      memset(source, 0, sizeof(*source));
   }
   retro_atomic_store_release_int(&slot->state, next);
   return true;
}

bool vaapi_frame_unclaim(void *owner, uint64_t index, uint64_t generation,
      vaapi_hevc_export_t *source)
{
   return vaapi_frame_put(owner, index, generation, source, 1);
}

bool vaapi_frame_finish(void *owner, uint64_t index, uint64_t generation,
      vaapi_hevc_export_t *source)
{
   return vaapi_frame_put(owner, index, generation, source, 4);
}

bool vaapi_frame_take(void *owner, uint64_t index, uint64_t generation,
      vaapi_hevc_export_t *out)
{
   vaapi_frame_pool_t *pool = (vaapi_frame_pool_t *)owner;
   vaapi_frame_slot_t *slot;
   if (!pool || index >= pool->count || !out || out->active)
      return false;
   slot = &pool->slots[(unsigned)index];
   if (!retro_atomic_cas_int(&slot->state, 2, 3))
      return false;
   if (slot->generation != generation || !slot->source.active
         || out == &slot->source)
   {
      retro_atomic_store_release_int(&slot->state, 2);
      return false;
   }
   *out = slot->source;
   memset(&slot->source, 0, sizeof(slot->source));
   retro_atomic_store_release_int(&slot->state, 2);
   return true;
}

bool vaapi_frame_claim(void *owner, uint64_t index, uint64_t generation,
      uint64_t image, vaapi_frame_view_t *view)
{
   vaapi_frame_pool_t *pool = (vaapi_frame_pool_t *)owner;
   vaapi_frame_slot_t *slot;
   if (!pool || index >= pool->count || !view)
      return false;
   slot = &pool->slots[(unsigned)index];
   if (image != (uint64_t)(uintptr_t)slot
         || !retro_atomic_cas_int(&slot->state, 1, 2))
      return false;
   if (slot->generation != generation)
   {
      retro_atomic_store_release_int(&slot->state, 1);
      return false;
   }
   *view = slot->view;
   return true;
}

static void vaapi_frame_abandon(
      void *owner, uint64_t index, uint64_t generation)
{
   vaapi_frame_pool_t *pool = (vaapi_frame_pool_t *)owner;
   vaapi_frame_slot_t *slot;
   if (!pool || index >= pool->count)
      return;
   slot = &pool->slots[(unsigned)index];
   if (!retro_atomic_cas_int(&slot->state, 1, 3))
      return;
   if (slot->generation != generation)
   {
      retro_atomic_store_release_int(&slot->state, 1);
      return;
   }
   retro_atomic_store_release_int(&slot->state, 4);
}

bool vaapi_frame_callback(void (*abandon)(void *, uint64_t, uint64_t))
{
   return abandon == vaapi_frame_abandon;
}

int vaapi_frame_export(vaapi_frame_pool_t *pool, vaapi_hevc_export_t *source,
      unsigned x, unsigned y, unsigned width, unsigned height, unsigned yuv,
      const video_color_metadata_t *color, video_hw_frame_t *out)
{
   vaapi_frame_slot_t *slot;
   video_hw_frame_t frame;
   unsigned i;
   if (!pool || pool->closed || !source || !source->active || !source->ready
         || !source->pool || !source->ticket || !out || !width || !height
         || ((x | y) & 1) || x >= source->descriptor.width
         || y >= source->descriptor.height
         || width > source->descriptor.width - x
         || height > source->descriptor.height - y
         || (source->descriptor.fourcc != VA_FOURCC_NV12
               && source->descriptor.fourcc != VA_FOURCC_P010))
      return -1;
   if (!source->pool->active || source->pool->broken || source->slot >= 32
         || !(source->pool->exports & ((uint32_t)1 << source->slot))
         || source->pool->generation[source->slot] != source->ticket
         || !source->descriptor.num_objects
         || source->descriptor.num_objects > 4)
      return -1;
   for (i = 0; i < source->descriptor.num_objects; i++)
      if (source->descriptor.objects[i].fd < 0)
         return -1;
   for (i = 0; i < pool->count; i++)
      if (!retro_atomic_load_acquire_int(&pool->slots[i].state))
         break;
   if (i == pool->count)
      return 1;
   slot = &pool->slots[i];
   if (slot->generation == ~(uint64_t)0 || source == &slot->source)
      return -1;
   slot->generation++;
   slot->pool   = source->pool;
   slot->slot   = source->slot;
   slot->ticket = source->ticket;
   slot->source = *source;
   memset(&slot->view, 0, sizeof(slot->view));
   slot->view.source = &slot->source;
   slot->view.x      = x;
   slot->view.y      = y;
   slot->view.width  = width;
   slot->view.height = height;
   slot->view.yuv    = yuv;
   slot->view.pixfmt = source->descriptor.fourcc == VA_FOURCC_P010
                             ? IMAGE_PIXFMT_P010
                             : IMAGE_PIXFMT_NV12;
   if (color)
      slot->view.color = *color;
   memset(&frame, 0, sizeof(frame));
   frame.image      = (uint64_t)(uintptr_t)slot;
   frame.api        = VIDEO_HW_API_VULKAN;
   frame.owner      = pool;
   frame.done       = i;
   frame.done_value = slot->generation;
   frame.abandon    = vaapi_frame_abandon;
   frame.width      = width;
   frame.height     = height;
   frame.crop_x     = x;
   frame.crop_y     = y;
   frame.pixfmt     = slot->view.pixfmt;
   frame.color      = slot->view.color;
   frame.yuv        = yuv;
   memset(source, 0, sizeof(*source));
   retro_atomic_store_release_int(&slot->state, 1);
   *out = frame;
   return 0;
}

unsigned vaapi_frame_collect(vaapi_frame_pool_t *pool)
{
   unsigned i, collected = 0;
   if (!pool)
      return 0;
   for (i = 0; i < pool->count; i++)
   {
      vaapi_frame_slot_t *slot = &pool->slots[i];
      if (retro_atomic_load_acquire_int(&slot->state) != 4)
         continue;
      if (!vaapi_hevc_export_close(&slot->source))
         continue;
      memset(&slot->view, 0, sizeof(slot->view));
      slot->pool   = NULL;
      slot->ticket = 0;
      retro_atomic_store_release_int(&slot->state, 0);
      collected++;
   }
   return collected;
}

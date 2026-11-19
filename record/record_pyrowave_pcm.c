/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_pyrowave_pcm.h"
#include <stdlib.h>
#include <string.h>
#include <formats/rpyrowave_stream.h>

struct record_pyrowave_pcm
{
   uint64_t last_end;
   record_gpu_queue_t *queue;
   uint8_t *storage;
   size_t slot_bytes;
   unsigned maximum_frames;
   unsigned reserved;
};

bool record_pyrowave_pcm_compatible(const record_pyrowave_pcm_t *p,
      unsigned channels, unsigned maximum_frames)
{
   return p && channels && channels <= 8 && p->maximum_frames == maximum_frames
      && p->slot_bytes == (size_t)maximum_frames * channels * 2;
}

record_pyrowave_pcm_t *record_pyrowave_pcm_new(unsigned channels,
      unsigned maximum_frames, unsigned slots)
{
   record_pyrowave_pcm_t *p;
   if (!channels || channels > 8 || !maximum_frames || maximum_frames > 4096
         || maximum_frames * channels * 2 > RPYROWAVE_STREAM_MAX_PAYLOAD
         || !slots || slots > RECORD_GPU_QUEUE_SLOTS) return NULL;
   p = (record_pyrowave_pcm_t*)calloc(1, sizeof(*p));
   if (!p) return NULL;
   p->slot_bytes = (size_t)maximum_frames * channels * 2;
   p->maximum_frames = maximum_frames;
   p->reserved = RECORD_GPU_QUEUE_SLOTS;
   p->storage = (uint8_t*)malloc(p->slot_bytes * slots);
   p->queue = record_gpu_queue_new(slots);
   if (!p->storage || !p->queue)
   {
      record_pyrowave_pcm_free(p);
      return NULL;
   }
   return p;
}

int record_pyrowave_pcm_acquire(record_pyrowave_pcm_t *p, void **data, size_t *capacity)
{
   int result;
   if (data) *data = NULL;
   if (capacity) *capacity = 0;
   if (!p || !data || !capacity) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_acquire(p->queue, &p->reserved);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   *data = p->storage + p->reserved * p->slot_bytes;
   *capacity = p->slot_bytes;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_pcm_submit(record_pyrowave_pcm_t *p,
      uint64_t first_sample, size_t frames)
{
   record_gpu_frame_t descriptor;
   if (!p || p->reserved == RECORD_GPU_QUEUE_SLOTS || !frames || frames > p->maximum_frames
         || first_sample < p->last_end || first_sample > (uint64_t)-1 - frames) return false;
   memset(&descriptor, 0, sizeof(descriptor));
   descriptor.timestamp = first_sample;
   descriptor.duration = frames;
   descriptor.slot = p->reserved;
   if (!record_gpu_queue_submit(p->queue, &descriptor)) return false;
   p->last_end = first_sample + frames;
   p->reserved = RECORD_GPU_QUEUE_SLOTS;
   return true;
}

bool record_pyrowave_pcm_cancel(record_pyrowave_pcm_t *p)
{
   if (!p || !record_gpu_queue_cancel(p->queue)) return false;
   p->reserved = RECORD_GPU_QUEUE_SLOTS;
   return true;
}

bool record_pyrowave_pcm_finish(record_pyrowave_pcm_t *p)
{
   return p && record_gpu_queue_finish(p->queue);
}

int record_pyrowave_pcm_take(record_pyrowave_pcm_t *p, record_pyrowave_pcm_view_t *view)
{
   record_gpu_frame_t descriptor;
   int result;
   if (view) memset(view, 0, sizeof(*view));
   if (!p || !view) return RECORD_GPU_QUEUE_ERROR;
   result = record_gpu_queue_take(p->queue, &descriptor);
   if (result != RECORD_GPU_QUEUE_OK) return result;
   view->first_sample = descriptor.timestamp;
   view->frames = (size_t)descriptor.duration;
   view->slot = descriptor.slot;
   view->data = p->storage + descriptor.slot * p->slot_bytes;
   return RECORD_GPU_QUEUE_OK;
}

bool record_pyrowave_pcm_release(record_pyrowave_pcm_t *p, unsigned slot)
{
   return p && record_gpu_queue_release(p->queue, slot);
}

bool record_pyrowave_pcm_free(record_pyrowave_pcm_t *p)
{
   if (!p) return true;
   if (!record_gpu_queue_free(p->queue)) return false;
   free(p->storage);
   free(p);
   return true;
}

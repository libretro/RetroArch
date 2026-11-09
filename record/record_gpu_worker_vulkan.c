/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_gpu_worker_vulkan.h"
#include <stdlib.h>

struct record_gpu_worker_vulkan
{
   record_gpu_queue_t *queue;
   vulkan_encode_h264_t *encoder;
   record_gpu_frame_t frame;
   vulkan_encode_packet_t packet;
   uint32_t owned;
   bool pending, packet_ready, unlocked, input_end, eos, ended, started, failed;
};

record_gpu_worker_vulkan_t *record_gpu_worker_vulkan_new(
      record_gpu_queue_t *queue, vulkan_encode_h264_t *encoder)
{
   record_gpu_worker_vulkan_t *w;
   if (!queue || !encoder)
      return NULL;
   w = (record_gpu_worker_vulkan_t*)calloc(1, sizeof(*w));
   if (w)
   {
      w->queue = queue;
      w->encoder = encoder;
   }
   return w;
}

static int record_gpu_worker_vulkan_fail(record_gpu_worker_vulkan_t *w)
{
   w->failed = true;
   return RECORD_GPU_VULKAN_ERROR;
}

int record_gpu_worker_vulkan_step(record_gpu_worker_vulkan_t *w,
      vulkan_encode_packet_t *packet)
{
   int result;
   bool progress = false;
   if (!w || !packet || !w->encoder || w->failed)
      return RECORD_GPU_VULKAN_ERROR;
   if (w->ended)
      return RECORD_GPU_VULKAN_END;
   if (w->packet_ready)
   {
      if (w->unlocked)
         return record_gpu_worker_vulkan_fail(w);
      *packet = w->packet;
      return RECORD_GPU_VULKAN_PACKET;
   }
   result = vulkan_encode_h264_poll(w->encoder, &w->packet);
   if (result == VULKAN_ENCODE_ERROR)
      return record_gpu_worker_vulkan_fail(w);
   if (result == VULKAN_ENCODE_OK)
   {
      if (w->packet.slot >= RECORD_GPU_QUEUE_SLOTS
            || !(w->owned & ((uint32_t)1 << w->packet.slot)))
         return record_gpu_worker_vulkan_fail(w);
      w->packet_ready = true;
      *packet = w->packet;
      return RECORD_GPU_VULKAN_PACKET;
   }
   if (result == VULKAN_ENCODE_END)
   {
      if (!w->eos || !w->input_end || w->owned || w->pending)
         return record_gpu_worker_vulkan_fail(w);
      w->ended = true;
      return RECORD_GPU_VULKAN_END;
   }
   if (!w->pending && !w->input_end)
   {
      result = record_gpu_queue_take(w->queue, &w->frame);
      if (result == RECORD_GPU_QUEUE_ERROR)
         return record_gpu_worker_vulkan_fail(w);
      if (result == RECORD_GPU_QUEUE_OK)
      {
         w->started = w->pending = true;
         w->owned |= (uint32_t)1 << w->frame.slot;
         progress = true;
      }
      else if (result == RECORD_GPU_QUEUE_END)
      {
         w->input_end = true;
         progress = true;
      }
   }
   if (w->pending)
   {
      result = vulkan_encode_h264_submit(w->encoder, w->frame.slot,
            w->frame.timestamp, w->frame.duration,
            w->frame.ready_value, w->frame.keyframe);
      if (result == VULKAN_ENCODE_ERROR)
         return record_gpu_worker_vulkan_fail(w);
      if (result == VULKAN_ENCODE_OK)
      {
         w->pending = false;
         progress = true;
      }
   }
   if (w->input_end && !w->eos)
   {
      result = vulkan_encode_h264_finish(w->encoder);
      if (result == VULKAN_ENCODE_ERROR)
         return record_gpu_worker_vulkan_fail(w);
      if (result == VULKAN_ENCODE_OK)
      {
         w->eos = true;
         progress = true;
      }
   }
   return progress ? RECORD_GPU_VULKAN_PROGRESS : RECORD_GPU_VULKAN_BUSY;
}

bool record_gpu_worker_vulkan_retry_needed(const record_gpu_worker_vulkan_t *w)
{
   return w && w->encoder && !w->ended
      && (w->pending || w->owned || w->input_end);
}

bool record_gpu_worker_vulkan_release(record_gpu_worker_vulkan_t *w)
{
   if (!w || !w->encoder || !w->packet_ready)
      return false;
   if (!w->unlocked)
   {
      if (!vulkan_encode_h264_release(w->encoder))
         return false;
      w->unlocked = true;
   }
   if (!record_gpu_queue_release(w->queue, w->packet.slot))
      return false;
   w->owned &= ~((uint32_t)1 << w->packet.slot);
   w->packet_ready = w->unlocked = false;
   return true;
}

bool record_gpu_worker_vulkan_free(record_gpu_worker_vulkan_t *w)
{
   if (!w)
      return true;
   if (w->owned || w->pending || w->packet_ready
         || ((!w->ended) && (w->started || w->eos)))
      return false;
   free(w);
   return true;
}

bool record_gpu_worker_vulkan_abandon(record_gpu_worker_vulkan_t *w)
{
   unsigned slot;
   int result;
   record_gpu_frame_t frame;
   if (!w)
      return true;
   w->encoder = NULL;
   for (slot = 0; slot < RECORD_GPU_QUEUE_SLOTS; slot++)
      if (w->owned & ((uint32_t)1 << slot))
      {
         if (!record_gpu_queue_release(w->queue, slot))
            return false;
         w->owned &= ~((uint32_t)1 << slot);
      }
   for (slot = 0; slot <= RECORD_GPU_QUEUE_SLOTS; slot++)
   {
      result = record_gpu_queue_take(w->queue, &frame);
      if (result == RECORD_GPU_QUEUE_END)
      {
         free(w);
         return true;
      }
      if (result != RECORD_GPU_QUEUE_OK)
         return false;
      if (!record_gpu_queue_release(w->queue, frame.slot))
      {
         w->owned |= (uint32_t)1 << frame.slot;
         return false;
      }
   }
   return false;
}

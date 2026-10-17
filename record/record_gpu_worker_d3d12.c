/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_gpu_worker_d3d12.h"
#include <stdlib.h>

struct record_gpu_worker_d3d12
{
   record_gpu_queue_t *queue;
   nvenc_d3d12_t *encoder;
   void *fence;
   record_gpu_frame_t frame;
   nvenc_d3d12_packet_t packet;
   uint32_t owned;
   bool pending, packet_ready, unlocked, input_end, eos, ended, started;
};

record_gpu_worker_d3d12_t *record_gpu_worker_d3d12_new(
      record_gpu_queue_t *queue, nvenc_d3d12_t *encoder, void *ready_fence)
{
   record_gpu_worker_d3d12_t *w;
   if (!queue || !encoder)
      return NULL;
   w = (record_gpu_worker_d3d12_t*)calloc(1, sizeof(*w));
   if (w)
   {
      w->queue = queue;
      w->encoder = encoder;
      w->fence = ready_fence;
   }
   return w;
}

int record_gpu_worker_d3d12_step(record_gpu_worker_d3d12_t *w,
      nvenc_d3d12_packet_t *packet)
{
   int result;
   bool progress = false;
   if (!w || !packet || !w->encoder)
      return RECORD_GPU_WORKER_ERROR;
   if (w->ended)
      return RECORD_GPU_WORKER_END;
   if (w->packet_ready)
   {
      if (w->unlocked)
         return RECORD_GPU_WORKER_ERROR;
      *packet = w->packet;
      return RECORD_GPU_WORKER_PACKET;
   }
   result = nvenc_d3d12_poll(w->encoder, &w->packet);
   if (result == NVENC_D3D12_ERROR)
      return RECORD_GPU_WORKER_ERROR;
   if (result == NVENC_D3D12_OK)
   {
      if (w->packet.slot >= RECORD_GPU_QUEUE_SLOTS
            || !(w->owned & ((uint32_t)1 << w->packet.slot)))
         return RECORD_GPU_WORKER_ERROR;
      w->packet_ready = true;
      *packet = w->packet;
      return RECORD_GPU_WORKER_PACKET;
   }
   if (result == NVENC_D3D12_END)
   {
      if (!w->eos || !w->input_end || w->owned || w->pending)
         return RECORD_GPU_WORKER_ERROR;
      w->ended = true;
      return RECORD_GPU_WORKER_END;
   }
   if (!w->pending && !w->input_end)
   {
      result = record_gpu_queue_take(w->queue, &w->frame);
      if (result == RECORD_GPU_QUEUE_ERROR)
         return RECORD_GPU_WORKER_ERROR;
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
      result = nvenc_d3d12_submit(w->encoder, w->frame.slot,
            w->frame.timestamp, w->frame.duration, w->fence,
            w->frame.ready_value, w->frame.keyframe);
      if (result == NVENC_D3D12_ERROR)
         return RECORD_GPU_WORKER_ERROR;
      if (result == NVENC_D3D12_OK)
      {
         w->pending = false;
         progress = true;
      }
   }
   if (w->input_end && !w->eos)
   {
      result = nvenc_d3d12_finish(w->encoder);
      if (result == NVENC_D3D12_ERROR)
         return RECORD_GPU_WORKER_ERROR;
      if (result == NVENC_D3D12_OK)
      {
         w->eos = true;
         progress = true;
      }
   }
   return progress ? RECORD_GPU_WORKER_PROGRESS : RECORD_GPU_WORKER_BUSY;
}

void *record_gpu_worker_d3d12_wait_handle(const record_gpu_worker_d3d12_t *w)
{
   return w && w->encoder && !w->packet_ready && !w->ended
      ? nvenc_d3d12_wait_handle(w->encoder) : NULL;
}

bool record_gpu_worker_d3d12_retry_needed(const record_gpu_worker_d3d12_t *w)
{
   return w && w->encoder && !w->ended
      && (w->pending || w->owned || w->input_end);
}

bool record_gpu_worker_d3d12_release(record_gpu_worker_d3d12_t *w)
{
   if (!w || !w->encoder || !w->packet_ready)
      return false;
   if (!w->unlocked)
   {
      if (nvenc_d3d12_release(w->encoder) != NVENC_D3D12_OK)
         return false;
      w->unlocked = true;
   }
   if (!record_gpu_queue_release(w->queue, w->packet.slot))
      return false;
   w->owned &= ~((uint32_t)1 << w->packet.slot);
   w->packet_ready = w->unlocked = false;
   return true;
}

bool record_gpu_worker_d3d12_free(record_gpu_worker_d3d12_t *w)
{
   if (!w)
      return true;
   if (w->owned || w->pending || w->packet_ready
         || ((!w->ended) && (w->started || w->eos)))
      return false;
   free(w);
   return true;
}

bool record_gpu_worker_d3d12_abandon(record_gpu_worker_d3d12_t *w)
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

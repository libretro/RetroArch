/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "vulkan_mock_worker.h"
static unsigned read32(FILE *f)
{
   uint8_t p[4];
   if (fread(p, 1, 4, f) != 4)
      return 0;
   return p[0] | (unsigned)p[1] << 8 | (unsigned)p[2] << 16 | (unsigned)p[3] << 24;
}
int record_gpu_worker_vulkan_step(record_gpu_worker_vulkan_t *w,
                                  vulkan_encode_packet_t *packet)
{
   static const uint8_t idr[] = {0, 0, 0, 1, 0x65, 0x88};
   w->calls++;
   if (w->fail)
      return RECORD_GPU_VULKAN_ERROR;
   if (w->busy)
      return RECORD_GPU_VULKAN_BUSY;
   if (w->held)
   {
      *packet = w->packet;
      return RECORD_GPU_VULKAN_PACKET;
   }
   if (w->made == w->frames)
      return RECORD_GPU_VULKAN_END;
   w->packet.keyframe = !w->made;
   w->packet.size = sizeof(idr);
   w->packet.data = idr;
   if (w->in)
   {
      w->packet.keyframe = read32(w->in) != 0;
      w->packet.size = read32(w->in);
      if (!w->packet.size || w->packet.size > w->size ||
          fread(w->data, 1, w->packet.size, w->in) != w->packet.size)
         return RECORD_GPU_VULKAN_ERROR;
      w->packet.data = w->data;
   }
   w->packet.timestamp = w->made * w->duration;
   w->packet.duration = w->duration;
   w->held = true;
   *packet = w->packet;
   return RECORD_GPU_VULKAN_PACKET;
}
bool record_gpu_worker_vulkan_release(record_gpu_worker_vulkan_t *w)
{
   if (!w->held || w->release_fail)
      return false;
   w->held = false;
   w->made++;
   w->releases++;
   return true;
}
bool record_gpu_worker_vulkan_retry_needed(const record_gpu_worker_vulkan_t *w)
{
   return w && w->retry;
}

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include "record_gpu_worker_vulkan_hevc.h"

static int rvgw_hevc_submit(void *encoder, unsigned slot, uint64_t timestamp,
      uint64_t duration, uint64_t ready, bool keyframe)
{
   return vulkan_encode_hevc_submit((vulkan_encode_hevc_t*)encoder, slot,
         timestamp, duration, ready, keyframe);
}

static int rvgw_hevc_poll(void *encoder, vulkan_encode_packet_t *packet)
{
   return vulkan_encode_hevc_poll((vulkan_encode_hevc_t*)encoder, packet);
}

static bool rvgw_hevc_release(void *encoder)
{
   return vulkan_encode_hevc_release((vulkan_encode_hevc_t*)encoder);
}

static int rvgw_hevc_finish(void *encoder)
{
   return vulkan_encode_hevc_finish((vulkan_encode_hevc_t*)encoder);
}

record_gpu_worker_vulkan_t *record_gpu_worker_vulkan_new_hevc(
      record_gpu_queue_t *queue, vulkan_encode_hevc_t *encoder)
{
   static const record_gpu_worker_vulkan_encoder_ops_t ops = {
      rvgw_hevc_submit, rvgw_hevc_poll, rvgw_hevc_release, rvgw_hevc_finish
   };
   return record_gpu_worker_vulkan_new_encoder(queue, encoder, &ops);
}

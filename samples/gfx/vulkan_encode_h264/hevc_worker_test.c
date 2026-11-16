/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#define record_gpu_worker_vulkan_new hevc_worker_under_test
#define main hevc_worker_contracts
#include "worker_test.c"
#undef main
#undef record_gpu_worker_vulkan_new
#include "../../../record/record_gpu_worker_vulkan_hevc.h"
static unsigned hevc_submits, hevc_polls, hevc_releases, hevc_finishes;

record_gpu_worker_vulkan_t *hevc_worker_under_test(
      record_gpu_queue_t *queue, vulkan_encode_h264_t *encoder)
{
   return record_gpu_worker_vulkan_new_hevc(queue, (vulkan_encode_hevc_t*)encoder);
}

int vulkan_encode_hevc_submit(vulkan_encode_hevc_t *encoder, unsigned slot,
      uint64_t timestamp, uint64_t duration, uint64_t ready, bool keyframe)
{
   hevc_submits++;
   return vulkan_encode_h264_submit((vulkan_encode_h264_t*)encoder, slot,
         timestamp, duration, ready, keyframe);
}

int vulkan_encode_hevc_poll(vulkan_encode_hevc_t *encoder, vulkan_encode_packet_t *packet)
{
   hevc_polls++;
   return vulkan_encode_h264_poll((vulkan_encode_h264_t*)encoder, packet);
}

bool vulkan_encode_hevc_release(vulkan_encode_hevc_t *encoder)
{
   hevc_releases++;
   return vulkan_encode_h264_release((vulkan_encode_h264_t*)encoder);
}

int vulkan_encode_hevc_finish(vulkan_encode_hevc_t *encoder)
{
   hevc_finishes++;
   return vulkan_encode_h264_finish((vulkan_encode_h264_t*)encoder);
}

int main(void)
{
   static const record_gpu_worker_vulkan_encoder_ops_t empty;
   vulkan_encode_h264_t encoder;
   record_gpu_queue_t *queue = record_gpu_queue_new(2);
   assert(queue && !record_gpu_worker_vulkan_new_encoder(queue, &encoder, NULL));
   assert(!record_gpu_worker_vulkan_new_encoder(queue, &encoder, &empty));
   assert(record_gpu_queue_finish(queue) && record_gpu_queue_free(queue));
   assert(hevc_worker_contracts() == 0);
   assert(hevc_submits && hevc_polls && hevc_releases && hevc_finishes);
   puts("HEVC factory preserves shared worker backpressure, EOS and failure ownership PASS");
   return 0;
}

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "../../../record/record_gpu_worker_vulkan.h"
struct record_gpu_worker_vulkan
{
   FILE *in;
   uint8_t *data;
   size_t size;
   uint64_t duration;
   unsigned made, frames, calls, releases;
   bool held, fail, release_fail, busy, retry;
   vulkan_encode_packet_t packet;
};

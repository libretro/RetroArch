/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <stdio.h>
#include "../../../record/record_gpu_worker_d3d12.h"
struct record_gpu_worker_d3d12
{
   FILE *in;
   uint8_t *data;
   size_t size;
   uint64_t duration;
   unsigned made, frames, calls, releases;
   bool held, fail, release_fail, busy, retry;
   nvenc_d3d12_packet_t packet;
};

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/video_hdr_metadata.h"
#include "../../../gfx/common/nvenc_d3d12.h"
#include "color_fragment.c"

int main(void)
{
   video_hdr_metadata_snapshot_t snapshot;
   struct retro_hdr_metadata hdr;
   nvenc_d3d12_config_t encoder, before;
   unsigned mode, codec;
   memset(&snapshot, 0, sizeof(snapshot));
   snapshot.max_nits = 4000.0f;
   snapshot.paper_white_nits = 200.0f;
   for (mode = 0; mode < 3; mode++)
      for (codec = 0; codec < 3; codec++)
      {
         memset(&hdr, 0, sizeof(hdr));
         hdr.version = 1; hdr.size = sizeof(hdr);
         snapshot.output_mode = mode;
         assert(video_hdr_metadata_query(RETRO_ENVIRONMENT_GET_HDR_METADATA,
                  &hdr, &snapshot));
         memset(&encoder, 0, sizeof(encoder));
         encoder.codec = (enum nvenc_d3d12_codec)codec;
         encoder.width = 1920; encoder.height = 1080;
         before = encoder;
         if (mode && !codec)
         {
            assert(!record_nvenc_color(&encoder, &hdr));
            assert(!memcmp(&before, &encoder, sizeof(encoder)));
            continue;
         }
         assert(record_nvenc_color(&encoder, &hdr));
         assert(encoder.format == (mode ? NVENC_D3D12_P010 : NVENC_D3D12_BGRA8));
         assert(encoder.color == (mode ? NVENC_D3D12_COLOR_BT2020_PQ : NVENC_D3D12_COLOR_BT709));
         assert(!encoder.full_range && !encoder.max_cll && !encoder.max_fall
               && !encoder.mastering.present);
         assert(encoder.width == 1920 && encoder.height == 1080
               && (unsigned)encoder.codec == codec);
      }
   encoder.codec = NVENC_D3D12_HEVC;
   hdr.output_mode = 1; hdr.color_primaries = 9; hdr.transfer_function = 18;
   assert(!record_nvenc_color(&encoder, &hdr));
   hdr.transfer_function = 16; hdr.color_primaries = 1;
   assert(!record_nvenc_color(&encoder, &hdr));
   hdr.color_primaries = 9; hdr.output_mode = 3;
   assert(!record_nvenc_color(&encoder, &hdr));
   hdr.output_mode = 1; hdr.version = 2;
   assert(!record_nvenc_color(&encoder, &hdr));
   hdr.version = 1; hdr.size--;
   assert(!record_nvenc_color(&encoder, &hdr));
   assert(!record_nvenc_color(NULL, &hdr) && !record_nvenc_color(&encoder, NULL));
   puts("PASS: negotiated SDR/PQ/scRGB recording admission and unknown HDR metadata");
   return 0;
}

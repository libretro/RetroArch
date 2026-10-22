/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include "../../../gfx/common/videotoolbox_vp9_config.h"

static rvp9_hdr header;
static videotoolbox_vp9_config_t config, empty;
static video_color_metadata_t color;

static void reset(void)
{
   memset(&header, 0, sizeof(header));
   memset(&color, 0, sizeof(color));
   header.w             = 1920;
   header.h             = 1080;
   header.bit_depth     = 8;
   header.subsampling_x = header.subsampling_y = 1;
}

int main(void)
{
   static const uint8_t sdr[] = {1, 0, 0, 0, 0, 0, 0x82, 1, 1, 1, 0, 0};
   static const uint8_t pq[]  = {1, 0, 0, 0, 2, 0, 0xa1, 9, 16, 9, 0, 0};
   unsigned i;
   reset();
   header.color_space            = 2;
   color.transfer                = 1;
   color.max_cll                 = 1000;
   color.max_fall                = 400;
   color.mastering.present       = VIDEO_MASTERING_DISPLAY_COMPLETE;
   color.mastering.luminance_max = 10000000;
   assert(videotoolbox_vp9_configure(&header, &color, 1, &config));
   assert(!memcmp(config.vpcc, sdr, sizeof(sdr)));
   assert(config.width == 1920 && config.height == 1080);
   assert(config.pixfmt == IMAGE_PIXFMT_NV12 && config.color.max_cll == 1000);
   assert(config.color.max_fall == 400
          && config.color.mastering.luminance_max == 10000000);
   assert(videotoolbox_vp9_configure(&header, &config.color, 1, &config));
   assert(!memcmp(config.vpcc, sdr, sizeof(sdr))
          && config.color.max_cll == 1000);
   color.primaries = 9;
   assert(!videotoolbox_vp9_configure(&header, &color, 1, &config));
   assert(!memcmp(&config, &empty, sizeof(config)));

   reset();
   header.profile     = 2;
   header.bit_depth   = 10;
   header.color_space = 5;
   header.color_range = 1;
   color.transfer     = 16;
   assert(videotoolbox_vp9_configure(&header, &color, 0, &config));
   assert(!memcmp(config.vpcc, pq, sizeof(pq)));
   assert(config.pixfmt == IMAGE_PIXFMT_P010 && config.color.range == 2);
   color.transfer = 18;
   assert(videotoolbox_vp9_configure(&header, &color, 0, &config));
   assert(config.vpcc[8] == 18 && config.color.transfer == 18);
   header.bit_depth = 12;
   assert(!videotoolbox_vp9_configure(&header, &color, 0, &config));
   reset();
   color.transfer = 16;
   assert(!videotoolbox_vp9_configure(&header, &color, 0, &config));

   reset();
   assert(videotoolbox_vp9_configure(&header, NULL, 0, &config));
   assert(config.vpcc[7] == 2 && config.vpcc[8] == 2 && config.vpcc[9] == 2);
   assert(!config.color.primaries && !config.color.transfer
          && !config.color.matrix);
   for (i = 0; i < 8; i++)
   {
      header.color_space = (int)i;
      assert(videotoolbox_vp9_configure(&header, NULL, 0, &config) == (i < 6));
   }
   reset();
   header.color_space = 1;
   color.primaries = color.matrix = 5;
   assert(videotoolbox_vp9_configure(&header, &color, 0, &config));
   assert(config.vpcc[7] == 5 && config.vpcc[9] == 5);
   color.matrix = 6;
   assert(videotoolbox_vp9_configure(&header, &color, 0, &config));
   assert(config.vpcc[7] == 5 && config.vpcc[9] == 6);
   color.range = 2;
   assert(!videotoolbox_vp9_configure(&header, &color, 0, &config));

   reset();
   header.w = 0;
   assert(!videotoolbox_vp9_configure(&header, NULL, 0, &config));
   header.w = 16385;
   assert(!videotoolbox_vp9_configure(&header, NULL, 0, &config));
   header.w = 16384;
   assert(videotoolbox_vp9_configure(&header, NULL, 0, &config));
   header.h = -1;
   assert(!videotoolbox_vp9_configure(&header, NULL, 0, &config));
   reset();
   assert(!videotoolbox_vp9_configure(&header, NULL, 2, &config));
   header.profile = 1;
   assert(!videotoolbox_vp9_configure(&header, NULL, 0, &config));
   header.profile = 3;
   assert(!videotoolbox_vp9_configure(&header, NULL, 0, &config));
   reset();
   header.subsampling_x = 0;
   assert(!videotoolbox_vp9_configure(&header, NULL, 0, &config));
   reset();
   color.primaries = 3;
   assert(!videotoolbox_vp9_configure(&header, &color, 0, &config));
   color.primaries = 22;
   assert(videotoolbox_vp9_configure(&header, &color, 0, &config));
   color.transfer = 19;
   assert(!videotoolbox_vp9_configure(&header, &color, 0, &config));
   color.transfer = 2;
   color.matrix   = 256;
   assert(!videotoolbox_vp9_configure(&header, &color, 0, &config));
   assert(!videotoolbox_vp9_configure(NULL, NULL, 0, &config));
   assert(!videotoolbox_vp9_configure(&header, NULL, 0, NULL));
   puts("PASS: VP9 vpcC, explicit chroma position, shared color, SDR/PQ/HLG, conflicts and bounds; no native device claim");
   return 0;
}

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include <formats/rvp9_config.h>
#include <formats/rvp9_color.h>

static const uint8_t original[12] = {1, 0, 0, 0, 0, 0, 0x82, 1, 13, 1, 0, 0};
static rvp9_config_t config, empty;

static void refuse(uint8_t *bytes, size_t size)
{
   memset(&config, 0xff, sizeof(config));
   assert(!rvp9_config_parse(bytes, size, &config));
   assert(!memcmp(&config, &empty, sizeof(config)));
}

int main(void)
{
   static const unsigned levels[] =
         {0, 10, 11, 20, 21, 30, 31, 40, 41, 50, 51, 52, 60, 61, 62};
   static const unsigned depths[] = {8, 10, 12};
   static const unsigned depth_masks[] = {1, 1, 6, 6};
   static const unsigned chroma_masks[] = {3, 12, 3, 12};
   uint8_t bytes[13];
   video_color_metadata_t color, merged, no_color;
   rvp9_hdr header;
   unsigned i, j, profile, depth, chroma;
   bool allowed;
   memcpy(bytes, original, sizeof(original));
   assert(rvp9_config_parse(bytes, 12, &config));
   assert(config.profile == 0 && config.level == 0 && config.bit_depth == 8
         && config.chroma == 1 && !config.full_range && config.primaries == 1
         && config.transfer == 13 && config.matrix == 1);
   assert(!rvp9_config_parse(bytes, 12, NULL));
   refuse(NULL, 12);
   for (i = 0; i < 12; i++)
      refuse(bytes, i);
   refuse(bytes, 13);
   for (i = 0; i < 12; i++)
   {
      if (i == 0 || (i > 0 && i < 4) || i > 9)
      {
         memcpy(bytes, original, sizeof(original));
         bytes[i] = i ? 1 : 0;
         refuse(bytes, 12);
      }
   }
   memcpy(bytes, original, sizeof(original));
   for (i = 0; i < 256; i++)
   {
      allowed = false;
      for (j = 0; j < sizeof(levels) / sizeof(levels[0]); j++)
         if (i == levels[j])
            allowed = true;
      bytes[5] = (uint8_t)i;
      assert(rvp9_config_parse(bytes, 12, &config) == allowed);
   }
   for (profile = 0; profile < 5; profile++)
      for (depth = 0; depth < 3; depth++)
         for (chroma = 0; chroma < 8; chroma++)
         {
            memcpy(bytes, original, sizeof(original));
            bytes[4] = (uint8_t)profile;
            bytes[6] = (uint8_t)((depths[depth] << 4) | (chroma << 1) | 1);
            allowed = profile < 4
                  && (depth_masks[profile] & (1u << depth)) != 0
                  && (chroma_masks[profile] & (1u << chroma)) != 0;
            assert(rvp9_config_parse(bytes, 12, &config) == allowed);
            if (allowed)
               assert(config.chroma == chroma && config.full_range == 1
                     && config.bit_depth == depths[depth]);
         }
   memcpy(bytes, original, sizeof(original));
   bytes[4] = 1;
   bytes[6] = 0x86;
   for (i = 0; i < 256; i++)
   {
      bytes[7] = bytes[8] = bytes[9] = (uint8_t)i;
      assert(rvp9_config_parse(bytes, 12, &config));
      assert(config.primaries == i && config.transfer == i && config.matrix == i);
   }
   bytes[9] = 0;
   bytes[6] = 0x84;
   refuse(bytes, 12);
   memset(&color, 0, sizeof(color));
   memset(&no_color, 0, sizeof(no_color));
   color.max_cll = 1000;
   color.mastering.present = VIDEO_MASTERING_DISPLAY_COMPLETE;
   color.mastering.luminance_max = 10000000;
   assert(rvp9_config_parse(original, sizeof(original), &config));
   assert(rvp9_config_apply_color(&config, &color, &merged));
   assert(merged.transfer == 13 && merged.primaries == 1 && merged.matrix == 1
         && merged.range == 1 && merged.max_cll == 1000
         && merged.mastering.luminance_max == 10000000);
   color.transfer = 16;
   assert(!rvp9_config_apply_color(&config, &color, &merged));
   assert(!memcmp(&merged, &no_color, sizeof(merged)));
   config.transfer = 2;
   assert(rvp9_config_apply_color(&config, &color, &color));
   assert(color.transfer == 16 && color.max_cll == 1000);
   config.primaries = 0;
   assert(!rvp9_config_apply_color(&config, &color, &merged));
   assert(!memcmp(&merged, &no_color, sizeof(merged)));
   memset(&header, 0, sizeof(header));
   memset(&color, 0, sizeof(color));
   header.profile = 2;
   header.bit_depth = 10;
   header.subsampling_x = header.subsampling_y = 1;
   header.color_space = 5;
   header.color_range = 1;
   assert(rvp9_color_resolve(&header, &color, &merged));
   assert(merged.primaries == 9 && merged.matrix == 9 && merged.range == 2
         && !merged.transfer);
   color.transfer = 16;
   color.max_cll = 1200;
   assert(rvp9_color_resolve(&header, &color, &color));
   assert(color.transfer == 16 && color.max_cll == 1200 && color.primaries == 9);
   color.primaries = 1;
   assert(!rvp9_color_resolve(&header, &color, &merged));
   assert(!memcmp(&merged, &no_color, sizeof(merged)));
   puts("PASS: VP9 vpcC FullBox bounds, profiles/depths/chroma, levels and raw CICP preservation");
   return 0;
}

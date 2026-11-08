/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: MIT */
#include <assert.h>
#include <limits.h>
#include <stdio.h>
#include <formats/rvp9_hw.h>

static rvp9_hdr header;
static rvp9_hw_segment_t segments[8], unchanged[8];

static void reset(void)
{
   memset(&header, 0, sizeof(header));
   header.bit_depth = 8;
   header.base_qindex = 100;
   header.lf_level = 32;
   header.lf_ref_deltas[0] = 1;
   header.lf_ref_deltas[2] = header.lf_ref_deltas[3] = -1;
   header.lf_mode_deltas[1] = 1;
}

static void refuse(void)
{
   memset(segments, 0xa5, sizeof(segments));
   memcpy(unchanged, segments, sizeof(segments));
   assert(!rvp9_hw_segments(&header, segments));
   assert(!memcmp(unchanged, segments, sizeof(segments)));
}

static void quantizers(void)
{
   static const int depths[3] = {8, 10, 12};
   static const int expected[3][4] = {
      {78, 112, 116, 108},
      {304, 441, 460, 425},
      {1206, 1758, 1838, 1693}
   };
   unsigned d, s;
   reset();
   header.y_dc_delta_q = -15;
   header.uv_dc_delta_q = 15;
   header.uv_ac_delta_q = -2;
   for (d = 0; d < 3; d++)
   {
      header.bit_depth = depths[d];
      assert(rvp9_hw_segments(&header, segments));
      for (s = 0; s < 8; s++)
      {
         assert(segments[s].y_quant[0] == expected[d][0]);
         assert(segments[s].y_quant[1] == expected[d][1]);
         assert(segments[s].uv_quant[0] == expected[d][2]);
         assert(segments[s].uv_quant[1] == expected[d][3]);
      }
   }
   reset();
   header.bit_depth = 10;
   header.seg_enabled = 1;
   header.seg_feature_enabled[0][0] = 1;
   header.seg_feature_data[0][0] = -255;
   header.seg_feature_enabled[1][0] = 1;
   header.seg_feature_data[1][0] = 255;
   header.seg_feature_enabled[2][0] = 1;
   header.seg_feature_data[2][0] = -15;
   assert(rvp9_hw_segments(&header, segments));
   assert(segments[0].y_quant[0] == 4 && segments[0].uv_quant[1] == 4);
   assert(segments[1].y_quant[0] == 5347 && segments[1].uv_quant[1] == 7312);
   assert(segments[2].y_quant[0] == 304 && segments[2].uv_quant[1] == 358);
   assert(segments[3].y_quant[0] == 369 && segments[3].uv_quant[1] == 441);
   header.seg_abs_delta = 1;
   assert(rvp9_hw_segments(&header, segments));
   assert(segments[0].y_quant[0] == 4 && segments[2].uv_quant[1] == 4);
   assert(segments[1].y_quant[0] == 5347 && segments[1].uv_quant[1] == 7312);
   header.seg_enabled = 0;
   assert(rvp9_hw_segments(&header, segments));
   assert(segments[0].y_quant[0] == 369 && segments[2].uv_quant[1] == 441);
}

static void filters(void)
{
   unsigned s, ref, mode;
   reset();
   assert(rvp9_hw_segments(&header, segments));
   for (s = 0; s < 8; s++)
      for (ref = 0; ref < 4; ref++)
         for (mode = 0; mode < 2; mode++)
            assert(segments[s].filter_level[ref][mode] == 32);
   header.lf_delta_enabled = 1;
   header.seg_enabled = 1;
   header.seg_feature_enabled[0][1] = header.seg_feature_enabled[1][1] = 1;
   header.seg_feature_data[0][1] = -63;
   header.seg_feature_data[1][1] = 63;
   assert(rvp9_hw_segments(&header, segments));
   assert(segments[0].filter_level[0][0] == 2);
   assert(segments[0].filter_level[0][1] == 2);
   assert(segments[0].filter_level[1][0] == 0);
   assert(segments[0].filter_level[1][1] == 2);
   assert(segments[1].filter_level[0][0] == 63);
   assert(segments[1].filter_level[2][0] == 61);
   assert(segments[1].filter_level[2][1] == 63);
   header.seg_abs_delta = 1;
   header.seg_feature_data[0][1] = 10;
   assert(rvp9_hw_segments(&header, segments));
   assert(segments[0].filter_level[0][0] == 12);
   assert(segments[0].filter_level[2][0] == 8);
   assert(segments[0].filter_level[2][1] == 10);
   header.lf_level = 31;
   assert(rvp9_hw_segments(&header, segments));
   assert(segments[0].filter_level[0][0] == 11);
   assert(segments[0].filter_level[2][0] == 9);
   assert(segments[0].filter_level[2][1] == 10);
}

static void invalid(void)
{
   unsigned s, f;
   static const int limits[4] = {255, 63, 3, 0};
   reset();
   assert(!rvp9_hw_segments(&header, NULL));
   memset(segments, 0xa5, sizeof(segments));
   memcpy(unchanged, segments, sizeof(segments));
   assert(!rvp9_hw_segments(NULL, segments));
   assert(!memcmp(unchanged, segments, sizeof(segments)));
   header.bit_depth = 9; refuse();
   reset(); header.base_qindex = INT_MAX; refuse();
   reset(); header.base_qindex = -1; refuse();
   reset(); header.y_dc_delta_q = -16; refuse();
   reset(); header.uv_dc_delta_q = 16; refuse();
   reset(); header.uv_ac_delta_q = INT_MIN; refuse();
   reset(); header.seg_enabled = 2; refuse();
   reset(); header.seg_abs_delta = -1; refuse();
   reset(); header.lf_level = 64; refuse();
   reset(); header.lf_delta_enabled = -1; refuse();
   for (s = 0; s < 4; s++)
   {
      reset(); header.lf_ref_deltas[s] = 64; refuse();
      reset(); header.lf_ref_deltas[s] = -64; refuse();
   }
   for (s = 0; s < 2; s++)
   {
      reset(); header.lf_mode_deltas[s] = 64; refuse();
      reset(); header.lf_mode_deltas[s] = -64; refuse();
   }
   for (s = 0; s < 8; s++)
      for (f = 0; f < 4; f++)
      {
         reset(); header.seg_feature_enabled[s][f] = 2; refuse();
         reset(); header.seg_feature_data[s][f] = limits[f] + 1; refuse();
         reset(); header.seg_feature_data[s][f] = -limits[f] - 1; refuse();
      }
}

int main(void)
{
   quantizers();
   filters();
   invalid();
   puts("VP9 hardware segment preparation passed");
   return 0;
}

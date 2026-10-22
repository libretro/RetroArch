/* SPDX-License-Identifier: MIT */
#include <assert.h>
#include <stdio.h>
#include "../../../formats/vp9/rvp9.c"
static rvp9_hdr header;
static rvp9_lf_info reference;
static void quantization(void)
{
   static const int depths[3] = {8, 10, 12};
   static const int dc_max[3] = {1336, 5347, 21387};
   static const int ac_max[3] = {1828, 7312, 29247};
   rvp9_hw_segment_t value, unchanged;
   unsigned depth;
   for (depth = 0; depth < 3; depth++)
   {
      memset(&header, 0, sizeof(header));
      header.bit_depth = depths[depth];
      assert(rvp9_hw_segment(&header, 0, &value));
      assert(value.y_dc == 4 && value.y_ac == 4
            && value.uv_dc == 4 && value.uv_ac == 4);
      header.base_qindex = 255;
      header.y_dc_delta_q = header.uv_dc_delta_q = header.uv_ac_delta_q = 15;
      assert(rvp9_hw_segment(&header, 0, &value));
      assert(value.y_dc == dc_max[depth] && value.y_ac == ac_max[depth]
            && value.uv_dc == dc_max[depth] && value.uv_ac == ac_max[depth]);
      header.seg_enabled = 1;
      header.seg_feature_enabled[3][0] = 1;
      header.seg_feature_data[3][0] = -255;
      header.y_dc_delta_q = header.uv_dc_delta_q = header.uv_ac_delta_q = -15;
      assert(rvp9_hw_segment(&header, 3, &value));
      assert(value.y_dc == 4 && value.y_ac == 4
            && value.uv_dc == 4 && value.uv_ac == 4);
      header.seg_abs_delta = 1;
      header.seg_feature_data[3][0] = 0;
      assert(rvp9_hw_segment(&header, 3, &value) && value.y_ac == 4);
      unchanged = value;
      header.y_dc_delta_q = -16;
      assert(!rvp9_hw_segment(&header, 3, &value));
      assert(!memcmp(&value, &unchanged, sizeof(value)));
   }
}
static void filters(void)
{
   rvp9_hw_segment_t value;
   unsigned level, segment, mode;
   memset(&header, 0, sizeof(header));
   header.bit_depth = 8;
   header.seg_enabled = 1;
   header.lf_ref_deltas[0] = 2;
   header.lf_ref_deltas[1] = -10;
   header.lf_ref_deltas[2] = 63;
   header.lf_ref_deltas[3] = -63;
   header.lf_mode_deltas[0] = -3;
   header.lf_mode_deltas[1] = 4;
   for (segment = 0; segment < 8; segment++)
   {
      header.seg_feature_enabled[segment][1] = segment & 1;
      header.seg_feature_data[segment][1] = (int)segment * 18 - 63;
   }
   for (level = 0; level < 64; level++)
      for (mode = 0; mode < 4; mode++)
      {
         header.lf_level = (int)level;
         header.seg_abs_delta = mode & 1;
         header.lf_delta_enabled = mode >> 1;
         rvp9_lf_init(&reference, &header, header.lf_ref_deltas,
               header.lf_mode_deltas);
         for (segment = 0; segment < 8; segment++)
         {
            assert(rvp9_hw_segment(&header, segment, &value));
            assert(!memcmp(value.filter, reference.lvl[segment],
                  sizeof(value.filter)));
         }
      }
   assert(!rvp9_hw_segment(&header, 8, &value));
   assert(!rvp9_hw_segment(NULL, 0, &value));
   assert(!rvp9_hw_segment(&header, 0, NULL));
}
int main(void)
{
   quantization();
   filters();
   puts("PASS: native VP9 segment quantizers and software loop-filter parity");
   return 0;
}

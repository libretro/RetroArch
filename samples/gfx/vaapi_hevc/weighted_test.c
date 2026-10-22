/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error WPP conformance requires assertions
#endif
#define main packet_reference_main
#include "test.c"
#undef main
#include "weighted_data.h"
#include "native_pixels.h"
static unsigned wpp_slices, weighted_l1;

static void software_picture(
      rh265_video *source, const sequence_case_t *fixture, unsigned display)
{
   unsigned p, row, col, index;
   assert(display < 8);
   for (index = 0; index < 8; index++)
      if (fixture->packets[index].display_index == display)
         break;
   assert(index < 8);
   for (p = 0; p < 3; p++)
   {
      int stride, width, height;
      const uint8_t *plane =
            rh265_video_plane(source, (int)p, &stride, &width, &height);
      uint32_t crc = (uint32_t)-1;
      assert(plane && width == (p ? 88 : 176) && height == (p ? 72 : 144));
      for (row = 0; row < (unsigned)height; row++)
         for (col = 0; col < (unsigned)width; col++)
         {
            unsigned value =
                  fixture->picture.depth == 10
                        ? ((const uint16_t *)plane)[row * stride + col]
                        : plane[row * stride + col];
            crc = native_crc_byte(crc, value & 255u);
            if (fixture->picture.depth == 10)
               crc = native_crc_byte(crc, value >> 8);
         }
      if (~crc != fixture->packets[index].crc[p])
         fprintf(stderr, "%s display %u plane %u actual=%08x expected=%08x\n",
               fixture->picture.name, display, p, (unsigned)~crc,
               (unsigned)fixture->packets[index].crc[p]);
      assert(~crc == fixture->packets[index].crc[p]);
   }
}

static void check_case(const sequence_case_t *fixture)
{
   rh265_video *source = rh265_video_open();
   unsigned i, display = 0, before_weights = weighted_l1;
   int result;
   assert(source
          && !rh265_video_set_extradata(
                source, fixture->picture.hvcc, fixture->picture.hvcc_size));
   for (i = 0; i < 8; i++)
   {
      unsigned j;
      assert(!rh265_video_get_access_unit(source, fixture->packets[i].data,
            fixture->packets[i].size, 4, many_slices, 256, &au));
      for (j = 0; j < au.num_slices; j++)
      {
         unsigned r;
         assert(!rh265_video_get_slice_decode(source, many_slices[j].data,
               many_slices[j].size, &long_info, long_entries,
               RH265_SLICE_ENTRY_POINTS_MAX));
         assert(long_info.num_entry_points > 0);
         wpp_slices++;
         if (long_info.picture.slice_type == 0)
            for (r = 0; r < (unsigned)long_info.num_refs[1]; r++)
               if (long_info.luma_weight[1][r]
                           != (1 << long_info.luma_log2_denom)
                     || long_info.luma_offset[1][r])
                  weighted_l1++;
      }
      result = rh265_video_decode(
            source, fixture->packets[i].data, fixture->packets[i].size);
      assert(result == 0 || result == 1);
      if (result)
         software_picture(source, fixture, display++);
   }
   while (!rh265_video_drain(source))
      software_picture(source, fixture, display++);
   assert(display == 8);
   assert(weighted_l1 > before_weights);
   rh265_video_close(source);
   sequence(fixture);
   assert(long_work.num_entries > 0);
   assert(packet_creates == 4u + !!params.scaling_list_enabled);
}

int main(void)
{
   unsigned i;
   for (i = 0; i < 32; i++)
      surfaces[i] = 100 + i;
   for (i = 0; i < sizeof(weighted_cases) / sizeof(weighted_cases[0]); i++)
      check_case(&weighted_cases[i]);
   assert(wpp_slices == 32 && weighted_l1 > 0 && parsed == 32);
   printf(
         "PASS: 32 real Main8/Main10 WPP weighted-B pictures, %u nondefault L1 weights, byte-exact FFmpeg CRCs and NORMAL subset submissions\n",
         weighted_l1);
   return 0;
}

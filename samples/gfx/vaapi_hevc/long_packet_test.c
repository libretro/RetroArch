/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#define main slice_query_main
#include "../../../libretro-common/samples/formats/h265/rh265_slice_info_test.c"
#undef main
#include "../../../gfx/common/vaapi_hevc_long_packet.h"

static vaapi_hevc_long_work_t packet_work;
static rh265_video_params_t packet_params;
static rh265_access_unit_t packet_au;
static rh265_hw_picture_t packet_picture;
static rh265_nal_t packet_nals[2];
static uint8_t input[1024], output[1024], output_saved[1024];
static const VASurfaceID surfaces[3] = {90, 21, 72};

static void run_packet(unsigned type, unsigned depth)
{
   rh265_video *v = make_decoder(true);
   size_t i, header;
   v->sps[0].bit_depth_luma = v->sps[0].bit_depth_chroma = (int)depth;
   v->sps[0].log2_min_cb                                 = 3;
   v->sps[0].log2_ctb                                    = 5;
   v->sps[0].log2_min_tb                                 = 2;
   v->sps[0].log2_max_tb                                 = 5;
   v->sps[0].height                                      = 96;
   v->sps[0].pic_size_ctbs                               = 6;
   v->sps[0].max_dec_pic_buffering                       = 3;
   v->sps[0].max_num_reorder_pics                        = 1;
   v->pps[0].log2_parallel_merge_level                   = 2;
   chroma_bias                                           = -1;
   make_slice(type, true, true, true);
   input[0] = input[1] = 0;
   input[2]            = (uint8_t)(nal_size >> 8);
   input[3]            = (uint8_t)nal_size;
   memcpy(input + 4, nal, nal_size);
   assert(!rh265_video_get_access_unit(
         v, input, nal_size + 4, 4, packet_nals, 2, &packet_au));
   assert(!rh265_video_get_params(v, 0, &packet_params));
   memset(&packet_picture, 0, sizeof(packet_picture));
   packet_picture.width     = 64;
   packet_picture.height    = 96;
   packet_picture.bit_depth = depth;
   packet_picture.poc_log2  = 4;
   packet_picture.poc       = type == 2 ? 0 : 3;
   if (type != 2)
   {
      packet_picture.num_refs     = 2;
      packet_picture.refs[0].poc  = 2;
      packet_picture.refs[0].slot = 1;
      packet_picture.refs[1].poc  = 4;
      packet_picture.refs[1].slot = 2;
   }
   packet_work.data     = output;
   packet_work.capacity = sizeof(output);
   assert(vaapi_hevc_long_packet(v, &packet_params, &packet_au, &packet_picture,
         surfaces, 3, packet_nals, &packet_work));
   assert(packet_work.num_entries == 2 && packet_work.entries[0] == 3
         && packet_work.entries[1] == 5);
   assert(packet_work.slices[0].num_entry_point_offsets == 2
         && !packet_work.slices[0].entry_offset_to_subset_array);
   assert(packet_work.used == 128 && !memcmp(output, nal, nal_size));
   header = packet_work.parsed.picture.header_bytes;
   for (i = nal_size; i < packet_work.used; i++)
      assert(!output[i]);
   memset(output, 0xa5, sizeof(output));
   memcpy(output_saved, output, sizeof(output));
#define BAD_PACKET(change, restore)                                            \
   do                                                                          \
   {                                                                           \
      change;                                                                  \
      assert(!vaapi_hevc_long_packet(v, &packet_params, &packet_au,            \
            &packet_picture, surfaces, 3, packet_nals, &packet_work));         \
      assert(!packet_work.used && !packet_work.num_entries                     \
            && !memcmp(output, output_saved, sizeof(output)));                \
      restore;                                                                 \
   } while (0)
   BAD_PACKET(
         packet_work.capacity = 127, packet_work.capacity = sizeof(output));
   BAD_PACKET(packet_au.coded_bytes++, packet_au.coded_bytes--);
   BAD_PACKET(packet_nals[0].size = 2, packet_nals[0].size = nal_size);
   BAD_PACKET(packet_nals[0].data = output, packet_nals[0].data = input + 4);
   BAD_PACKET(input[5] = 0, input[5] = 1);
   /* Leave only nine payload bytes: 4+6 entry lengths cannot fit. */
   i                   = header + 9;
   packet_nals[0].size = packet_au.coded_bytes = i;
   assert(!vaapi_hevc_long_packet(v, &packet_params, &packet_au,
         &packet_picture, surfaces, 3, packet_nals, &packet_work));
   assert(!packet_work.used && !packet_work.num_entries
         && !memcmp(output, output_saved, sizeof(output)));
#undef BAD_PACKET
   rh265_video_close(v);
}

int main(void)
{
   unsigned type, depth;
   for (depth = 8; depth <= 10; depth += 2)
      for (type = 0; type < 3; type++)
         run_packet(type, depth);
   puts("PASS: Main8/Main10 I/P/B NORMAL packets, WPP coded offsets, padding and refusal before payload copy; no native execution");
   return 0;
}

/* Copyright (C) 2026 The RetroArch team. SPDX-License-Identifier: GPL-3.0-or-later */
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <formats/rh265.h>
int main(int argc, char **argv)
{
   rh265_video *decoder;
   rh265_video_params_t *params = (rh265_video_params_t*)calloc(1, sizeof(*params));
   rh265_sequence_info_t sequence;
   rh265_vps_info_t vps;
   rh265_video_color_t color;
   unsigned char *data = (unsigned char*)malloc(65536);
   size_t size;
   unsigned depth;
   FILE *file;
   assert(argc == 3 && data && params);
   depth = (unsigned)atoi(argv[1]);
   file = fopen(argv[2], "rb");
   assert(file);
   size = fread(data, 1, 65536, file);
   assert(size && size < 65536 && !ferror(file));
   assert(fclose(file) == 0);
   decoder = rh265_video_open();
   assert(decoder && rh265_video_decode(decoder, data, size) == 0);
   assert(rh265_video_get_params(decoder, 0, params) == 0);
   assert(params->width == 192 && params->height == 128 && params->bit_depth == (int)depth);
   assert(rh265_video_get_sequence(decoder, 0, &sequence) == 0);
   assert(sequence.profile_idc == (depth == 10 ? 2u : 1u));
   assert(rh265_video_get_vps(decoder, 0, &vps) == 0);
   assert(vps.timing_present && vps.num_units_in_tick == 1001 && vps.time_scale == 60000);
   assert(rh265_video_get_color(decoder, 0, &color) == 0);
   assert(color.primaries == (depth == 10 ? 9u : 1u));
   assert(color.transfer == (depth == 10 ? 16u : 1u));
   assert(color.matrix == (depth == 10 ? 9u : 1u));
   assert(!color.full_range);
   rh265_video_close(decoder);
   free(data);
   free(params);
   puts("Independent HEVC decoder parsed native VPS/SPS/PPS geometry, profile, color and clock PASS");
   return 0;
}

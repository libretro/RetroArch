/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error VA AV1 tile conformance requires assertions
#endif
#include <assert.h>
#include "../../../gfx/common/vulkan_av1_params.h"
#include "../../../gfx/common/vaapi_av1_tiles.h"
static VASliceParameterBufferAV1 native_tiles[4096];
static bool compare_native_tiles(vulkan_av1_picture_t *out,
      const uint8_t *packet, size_t size, const rav1_sequence_t *sequence,
      const rav1_hw_picture_t *picture)
{
   unsigned count = 0, i;
   if (!vulkan_av1_picture(out, packet, size, sequence, picture))
      return false;
   assert(vaapi_av1_tiles(native_tiles, 4096, &count, packet, size, picture));
   assert(count == out->info.tileCount);
   for (i = 0; i < count; i++)
   {
      assert(native_tiles[i].slice_data_offset == out->offsets[i]);
      assert(native_tiles[i].slice_data_size == out->sizes[i]);
      assert(native_tiles[i].tile_column == i % picture->tiles.layout.columns);
      assert(native_tiles[i].tile_row == i / picture->tiles.layout.columns);
      assert(native_tiles[i].slice_data_flag == VA_SLICE_DATA_FLAG_ALL);
   }
   return true;
}
#define vulkan_av1_picture compare_native_tiles
#define AV1_PARAMS_NO_MAIN
#include "../vulkan_video_caps/av1_params_test.c"
#undef vulkan_av1_picture
int main(int argc, char **argv)
{
   if (argc != 2)
      return 1;
   return inspect_params(argv[1]);
}

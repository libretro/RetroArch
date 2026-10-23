/* Copyright (C) 2026 The RetroArch team.
 * SPDX-License-Identifier: GPL-3.0-or-later */
#ifdef NDEBUG
#error VA AV1 tile conformance requires assertions
#endif
#include <assert.h>
#include "../../../gfx/common/vulkan_av1_params.h"
#include "../../../gfx/common/vaapi_av1_picture.h"
static VADecPictureParameterBufferAV1 native_picture;
static bool (*volatile build_native_picture)(VADecPictureParameterBufferAV1 *,
      const rav1_sequence_t *, const rav1_hw_picture_t *, const VASurfaceID *,
      unsigned, VASurfaceID) = vaapi_av1_picture;
static VASliceParameterBufferAV1 native_tiles[4096];
static bool compare_native_tiles(vulkan_av1_picture_t *out,
      const uint8_t *packet, size_t size, const rav1_sequence_t *sequence,
      const rav1_hw_picture_t *picture)
{
   unsigned count = 0, i;
   VASurfaceID surfaces[32];
   if (!vulkan_av1_picture(out, packet, size, sequence, picture))
      return false;
   for (i = 0; i < 32; i++)
      surfaces[i] = 100 + i;
   assert(build_native_picture(&native_picture, sequence, picture, surfaces,
         32, 1000));
   assert(native_picture.current_frame == surfaces[picture->slot]);
   assert(native_picture.frame_width_minus1 + 1u
         == picture->setup.size.upscaled_width);
   assert(native_picture.frame_height_minus1 + 1u == picture->setup.size.height);
   assert(native_picture.base_qindex == picture->picture.base_q_idx);
   assert(native_picture.bit_depth_idx == (sequence->bit_depth == 10));
   assert(native_picture.cdef_bits == out->cdef.cdef_bits);
   assert(native_picture.cdef_damping_minus_3 == out->cdef.cdef_damping_minus_3);
   assert(native_picture.film_grain_info.grain_seed == out->grain.grain_seed);
   for (i = 0; i < 8; i++)
   {
      assert(native_picture.cdef_y_strengths[i]
            == (out->cdef.cdef_y_pri_strength[i] << 2
                  | out->cdef.cdef_y_sec_strength[i]));
      assert(native_picture.cdef_uv_strengths[i]
            == (out->cdef.cdef_uv_pri_strength[i] << 2
                  | out->cdef.cdef_uv_sec_strength[i]));
   }
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

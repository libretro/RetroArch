#include <assert.h>
#include <limits.h>
#ifdef __clang__
#include <stdbool.h>
#undef bool
/* Keep the boolean ABI without silencing other C99 diagnostics. */
__extension__ typedef _Bool bool;
#endif
#include "../../../gfx/common/metal_frame_convert.h"
#include <string.h>

static void test_sdr_params(void)
{
   metal_frame_view_t view;
   metal_frame_sdr_params_t p, saved;
   unsigned ten, full;
   memset(&view, 0, sizeof(view));
   view.width = 7; view.height = 5; view.crop_x = 2; view.crop_y = 2;
   view.color.primaries = 1; view.color.matrix = 1; view.color.transfer = 1;
   assert(sizeof(p) == 64);
   for (ten = 0; ten < 2; ten++) for (full = 0; full < 2; full++)
   {
      float black, white, center, value;
      view.pixfmt = ten ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
      view.color.range = full ? 2 : 1;
      assert(metal_frame_sdr_params(&view, &p));
      assert(p.dims[0] == 7 && p.dims[1] == 5 && p.dims[2] == 2 && p.dims[3] == 2);
      black = full ? 0.0f : ten ? 4096.0f / 65535.0f : 16.0f / 255.0f;
      white = full ? (ten ? 65472.0f / 65535.0f : 1.0f)
         : ten ? 60160.0f / 65535.0f : 235.0f / 255.0f;
      center = ten ? 32768.0f / 65535.0f : 128.0f / 255.0f;
      value = black * p.levels[0] + p.levels[1];
      assert(value > -0.000001f && value < 0.000001f);
      value = white * p.levels[0] + p.levels[1];
      assert(value > 0.999999f && value < 1.000001f);
      value = center * p.levels[2] + p.levels[3];
      assert(value > -0.000001f && value < 0.000001f);
   }
   saved = p; view.color.transfer = 16;
   assert(!metal_frame_sdr_params(&view, &p) && !memcmp(&p, &saved, sizeof(p)));
   view.color.transfer = 18;
   assert(!metal_frame_sdr_params(&view, &p));
   view.color.transfer = 13; view.color.range = 0;
   assert(!metal_frame_sdr_params(&view, &p));
   view.color.range = 2; view.color.primaries = 9;
   assert(!metal_frame_sdr_params(&view, &p));
}

int main(void)
{
   test_sdr_params();
   assert(VIDEO_HW_API_VULKAN == 1 && VIDEO_HW_API_D3D11 == 2
         && VIDEO_HW_API_METAL == 3);
   assert(metal_frame_geometry_valid(IMAGE_PIXFMT_8888, 8, 6, 0, 0, 8, 6, 0, 0));
   assert(metal_frame_geometry_valid(IMAGE_PIXFMT_FP16, 4, 2, 4, 4, 8, 6, 0, 0));
   assert(metal_frame_geometry_valid(IMAGE_PIXFMT_NV12, 7, 5, 0, 0, 7, 5, 4, 3));
   assert(metal_frame_geometry_valid(IMAGE_PIXFMT_P010, 5, 3, 2, 2, 7, 5, 4, 3));
   assert(!metal_frame_geometry_valid(IMAGE_PIXFMT_NV12, 4, 2, 1, 0, 8, 6, 4, 3));
   assert(!metal_frame_geometry_valid(IMAGE_PIXFMT_P010, 4, 2, 0, 1, 8, 6, 4, 3));
   assert(!metal_frame_geometry_valid(IMAGE_PIXFMT_NV12, 7, 5, 0, 0, 7, 5, 3, 2));
   assert(!metal_frame_geometry_valid(IMAGE_PIXFMT_8888, 8, 6, 0, 0, 8, 6, 4, 3));
   assert(!metal_frame_geometry_valid(IMAGE_PIXFMT_I420, 8, 6, 0, 0, 8, 6, 4, 3));
   assert(!metal_frame_geometry_valid(IMAGE_PIXFMT_8888, 0, 6, 0, 0, 8, 6, 0, 0));
   assert(!metal_frame_geometry_valid(IMAGE_PIXFMT_8888, 65536u, 1, 0, 0, 65536u, 1, 0, 0));
   assert(!metal_frame_geometry_valid(IMAGE_PIXFMT_8888, 8, 6, UINT_MAX, 0, 8, 6, 0, 0));
   assert(!metal_frame_geometry_valid(IMAGE_PIXFMT_8888, 8, 6, 1, 0, 8, 6, 0, 0));
   return 0;
}

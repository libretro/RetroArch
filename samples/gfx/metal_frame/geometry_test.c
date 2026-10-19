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

static void test_hdr_params(void)
{
   metal_frame_view_t view;
   metal_frame_sdr_params_t p, unchanged;
   unsigned ten, full, hlg, bad;
   memset(&view, 0, sizeof(view));
   memset(&p, 0x5a, sizeof(p)); unchanged = p;
   assert(!metal_frame_hdr_params(NULL, &p));
   assert(!metal_frame_hdr_params(&view, NULL));
   for (ten = 0; ten < 2; ten++) for (full = 0; full < 2; full++)
      for (hlg = 0; hlg < 2; hlg++)
      {
         double sample = ten ? 65535.0 / 64.0 : 255.0;
         double black = full ? 0.0 : ten ? 64.0 : 16.0;
         double white = full ? (ten ? 1023.0 : 255.0) : ten ? 940.0 : 235.0;
         double center = ten ? 512.0 : 128.0;
         double at_black, at_white, neutral;
         view.pixfmt = ten ? IMAGE_PIXFMT_P010 : IMAGE_PIXFMT_NV12;
         view.width = 7; view.height = 5; view.crop_x = view.crop_y = 2;
         view.color.primaries = view.color.matrix = 9;
         view.color.transfer = hlg ? 18 : 16; view.color.range = full ? 2 : 1;
         assert(!metal_frame_sdr_params(&view, &p));
         assert(metal_frame_hdr_params(&view, &p));
         assert(p.mode[0] == view.color.transfer && p.dims[0] == 7 && p.dims[2] == 2);
         assert(p.coefficients[0] > 1.4745f && p.coefficients[0] < 1.4747f);
         assert(p.coefficients[1] < 0 && p.coefficients[2] < 0 && p.coefficients[3] > 1.8813f);
         at_black = black / sample * p.levels[0] + p.levels[1];
         at_white = white / sample * p.levels[0] + p.levels[1];
         neutral = center / sample * p.levels[2] + p.levels[3];
         assert(at_black > -0.000001 && at_black < 0.000001);
         assert(at_white > 0.999999 && at_white < 1.000001);
         assert(neutral > -0.000001 && neutral < 0.000001);
         for (bad = 0; bad < 8; bad++)
         {
            metal_frame_view_t invalid = view;
            if (bad == 0) invalid.color.primaries = 1;
            if (bad == 1) invalid.color.matrix = 10;
            if (bad == 2) invalid.color.transfer = 1;
            if (bad == 3) invalid.color.range = 0;
            if (bad == 4) invalid.pixfmt = IMAGE_PIXFMT_FP16;
            if (bad == 5) invalid.crop_x = 1;
            if (bad == 6) invalid.width = 0;
            if (bad == 7) invalid.height = 65536u;
            p = unchanged;
            assert(!metal_frame_hdr_params(&invalid, &p));
            assert(!memcmp(&p, &unchanged, sizeof(p)));
         }
      }
}

int main(void)
{
   test_hdr_params();
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

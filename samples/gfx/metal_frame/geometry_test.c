#include <assert.h>
#include <limits.h>
#ifdef __clang__
#include <stdbool.h>
#undef bool
/* Keep the boolean ABI without silencing other C99 diagnostics. */
__extension__ typedef _Bool bool;
#endif
#include "../../../gfx/common/metal_frame.h"

int main(void)
{
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

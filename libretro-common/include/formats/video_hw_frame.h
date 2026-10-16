/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (video_hw_frame.h).
 * ---------------------------------------------------------------------------------------
 *
 * Permission is hereby granted, free of charge,
 * to any person obtaining a copy of this software and associated documentation files (the "Software"),
 * to deal in the Software without restriction, including without limitation the rights to
 * use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR IMPLIED,
 * INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER LIABILITY,
 * WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#ifndef __LIBRETRO_SDK_FORMAT_VIDEO_HW_FRAME_H__
#define __LIBRETRO_SDK_FORMAT_VIDEO_HW_FRAME_H__

/* A decoded picture that stays on the GPU: what a hardware decoder
 * hands out, and what a video driver that imports such frames reads.
 * Handles are given as their bytes (a Vulkan non-dispatchable handle
 * is 64 bits wide on every target).
 *
 * Vulkan: an NV12/P010 VkImage in @layout,
 * and two timeline semaphores. A reader waits for @wait to reach
 * @wait_value before it reads, and signals @done with @done_value once
 * it reads the frame no more: until then the decoder does not write
 * the image again. A frame that will never be read is given back with
 * video_hw_frame_abandon, which signals @done in the reader's place.
 * Optional @plane_views are Y/UV VkImageViews in R8/R8G8 or R16/R16G16
 * UNORM format. Both are nonzero or both zero and share the image's lease.
 * A view selects @array_slice; @crop_x/@crop_y locate the visible window.
 *
 * D3D11: NT handles of a shared DXGI_FORMAT_NV12 or P010 texture and of two
 * shared ID3D11Fences, used as above; @layout names the decoder's set
 * of handles, so that a reader keeping what it opened of them opens
 * them again when it changes. D3D12 readers may receive a decoder array:
 * @array_slice selects its element and @crop_x/@crop_y its visible origin.
 * These fields are zero for standalone output textures. */

#include <stdint.h>
#include <stddef.h>

#include <retro_common_api.h>
#include <retro_inline.h>
#include <formats/video_color.h>

RETRO_BEGIN_DECLS

enum video_hw_api
{
   VIDEO_HW_API_UNKNOWN = 0,
   VIDEO_HW_API_VULKAN = 1,
   VIDEO_HW_API_D3D11 = 2
};

typedef struct video_hw_frame
{
   uint64_t image;
   uint64_t plane_views[2]; /* optional Vulkan Y/UV views; same lease */
   uint64_t wait;
   uint64_t wait_value;
   uint64_t done;
   uint64_t done_value;
   /* The decoder's: signals @done as if read. NULL: nothing to do. */
   void (*abandon)(void *owner, uint64_t done, uint64_t done_value);
   void *owner;
   unsigned width, height;
   unsigned layout;
   unsigned yuv;             /* IMAGE_YUV_FLAG_* (image_yuv_blit.h) */
   unsigned pixfmt;          /* image_pixfmt; zero means legacy NV12 */
   unsigned array_slice;    /* D3D12: shared decoder array element */
   unsigned crop_x, crop_y; /* visible origin within that element */
   video_color_metadata_t color;
   unsigned api;             /* video_hw_api; zero for older producers */
} video_hw_frame_t;

static INLINE void video_hw_frame_abandon(const video_hw_frame_t *f)
{
   if (f && f->abandon)
      f->abandon(f->owner, f->done, f->done_value);
}

RETRO_END_DECLS

#endif

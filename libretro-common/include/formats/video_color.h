/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (video_color.h).
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

#ifndef __LIBRETRO_SDK_FORMAT_VIDEO_COLOR_H__
#define __LIBRETRO_SDK_FORMAT_VIDEO_COLOR_H__

#include <stdint.h>

/* RGB and white xy coordinates in 1/50000 units; luminance in 1/10000
 * cd/m2. Present bits 0..7 name xy values, 8 max, 9 min; zero is valid. */
typedef struct video_mastering_display
{
   uint32_t luminance_max, luminance_min;
   unsigned present;
   uint16_t chromaticity[8];
} video_mastering_display_t;

#define VIDEO_MASTERING_DISPLAY_COMPLETE 0x3ffu

/* Source ISO/IEC 23001-8 code points, not the converted output space.
 * Zero means absent; code point 2 means unspecified. Range is 0 absent,
 * 1 limited, 2 full. MaxCLL/MaxFALL are cd/m2; zero means absent. */
typedef struct video_color_metadata
{
   video_mastering_display_t mastering;
   unsigned primaries, transfer, matrix, range;
   unsigned max_cll, max_fall;
} video_color_metadata_t;

#endif

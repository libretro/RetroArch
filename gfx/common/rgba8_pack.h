/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef RARCH_RGBA8_PACK_H__
#define RARCH_RGBA8_PACK_H__

#include <stdint.h>

#include <retro_inline.h>
#include <retro_endianness.h>

/* One 0..1 RGBA colour as four UNORM8 bytes, red at the lowest
 * address - what R8G8B8A8_UNORM (Vulkan), RGBA8Unorm (Metal) and a
 * normalised GL_UNSIGNED_BYTE attribute read. Each channel is
 * c * 255 + 0.5, truncated and saturated to 0..255; a NaN is 0. Every
 * path below gives the same bytes.
 *
 * RGBA8_PACK_NO_SIMD takes the scalar path whatever the target, which
 * is how samples/gfx/rgba8_pack holds the vector paths to it. */

#if !defined(RGBA8_PACK_NO_SIMD) && (defined(__SSE2__) \
      || defined(_M_X64) || defined(_M_AMD64) \
      || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define RGBA8_PACK_SSE2
#include <emmintrin.h>
#elif !defined(RGBA8_PACK_NO_SIMD) && !defined(__ARM_BIG_ENDIAN) \
      && (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64))
#define RGBA8_PACK_NEON
#include <arm_neon.h>
#endif

#define RGBA8_WHITE 0xFFFFFFFFu

#if !defined(RGBA8_PACK_SSE2) && !defined(RGBA8_PACK_NEON)
static INLINE uint32_t rgba8_unorm(float c)
{
   c = c * 255.0f + 0.5f;
   if (!(c > 0.0f))
      return 0;
   if (c >= 255.0f)
      return 255;
   return (uint32_t)c;
}
#endif

static INLINE uint32_t rgba8_pack(const float *rgba)
{
#if defined(RGBA8_PACK_SSE2)
   /* min with 255 first: the convert gives INT_MIN for anything past
    * int's range, which would saturate to 0. A NaN passes through
    * (minps returns its second operand) and does become 0. */
   __m128i v = _mm_cvttps_epi32(_mm_min_ps(_mm_set1_ps(255.0f),
            _mm_add_ps(_mm_mul_ps(_mm_loadu_ps(rgba),
                  _mm_set1_ps(255.0f)), _mm_set1_ps(0.5f))));
   v = _mm_packs_epi32(v, v);
   v = _mm_packus_epi16(v, v);
   return (uint32_t)_mm_cvtsi128_si32(v);
#elif defined(RGBA8_PACK_NEON)
   int32x4_t v = vcvtq_s32_f32(vmlaq_n_f32(vdupq_n_f32(0.5f),
            vld1q_f32(rgba), 255.0f));
   int16x4_t h = vqmovn_s32(v);
   return vget_lane_u32(vreinterpret_u32_u8(
            vqmovun_s16(vcombine_s16(h, h))), 0);
#else
   uint32_t r = rgba8_unorm(rgba[0]);
   uint32_t g = rgba8_unorm(rgba[1]);
   uint32_t b = rgba8_unorm(rgba[2]);
   uint32_t a = rgba8_unorm(rgba[3]);
#if RETRO_IS_BIG_ENDIAN
   return (r << 24) | (g << 16) | (b << 8) | a;
#else
   return r | (g << 8) | (b << 16) | (a << 24);
#endif
#endif
}

#endif

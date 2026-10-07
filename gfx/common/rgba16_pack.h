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

#ifndef RARCH_RGBA16_PACK_H__
#define RARCH_RGBA16_PACK_H__

#include <stdint.h>

#include <retro_inline.h>
#include <retro_endianness.h>

/* One 0..1 RGBA colour as four UNORM16 channels, red at the lowest
 * address, each in native byte order - what R16G16B16A16_UNORM
 * (Vulkan), UShort4Normalized (Metal) and a normalised
 * GL_UNSIGNED_SHORT attribute read. Sixteen bits keep a colour finer
 * than any swapchain a menu draws to, so a fade that is smooth on a
 * 10-bit or scRGB target stays smooth; half the size of four floats.
 * Each channel is c * 65535 + 0.5, truncated and saturated to
 * 0..65535; a NaN is 0. Every path below gives the same bits.
 *
 * RGBA16_PACK_NO_SIMD takes the scalar path whatever the target, which
 * is how samples/gfx/rgba16_pack holds the vector paths to it. */

#if !defined(RGBA16_PACK_NO_SIMD) && (defined(__SSE2__) \
      || defined(_M_X64) || defined(_M_AMD64) \
      || (defined(_M_IX86_FP) && _M_IX86_FP >= 2))
#define RGBA16_PACK_SSE2
#include <emmintrin.h>
#elif !defined(RGBA16_PACK_NO_SIMD) && !defined(__ARM_BIG_ENDIAN) \
      && (defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(_M_ARM64))
#define RGBA16_PACK_NEON
#include <arm_neon.h>
#endif

#define RGBA16_WHITE (~(uint64_t)0)

/* One channel, as every path below packs it. */
static INLINE uint64_t rgba16_unorm(float c)
{
   /* Written as selects rather than early returns, which compile to a
    * branchless max/min; a NaN fails the first test and becomes 0. */
   c = c * 65535.0f + 0.5f;
   c = (c > 0.0f)     ? c : 0.0f;
   c = (c < 65535.0f) ? c : 65535.0f;
   return (uint32_t)(int32_t)c;
}

/* White at alpha a: rgba16_pack of 1, 1, 1, a in one scalar convert. */
static INLINE uint64_t rgba16_white(float a)
{
#if RETRO_IS_BIG_ENDIAN
   return (RGBA16_WHITE << 16) | rgba16_unorm(a);
#else
   return (RGBA16_WHITE >> 16) | (rgba16_unorm(a) << 48);
#endif
}

static INLINE uint64_t rgba16_pack(const float *rgba)
{
#if defined(RGBA16_PACK_SSE2)
   /* max with 0 first, so a NaN (maxps returns its second operand)
    * becomes 0; min with 65535 keeps the convert inside int's range.
    * SSE2 has no unsigned 32 -> 16 saturating pack: bias to signed,
    * pack, and flip the sign bit back. */
   uint64_t r;
   __m128i  v = _mm_cvttps_epi32(_mm_min_ps(_mm_set1_ps(65535.0f),
            _mm_max_ps(_mm_add_ps(_mm_mul_ps(_mm_loadu_ps(rgba),
                  _mm_set1_ps(65535.0f)), _mm_set1_ps(0.5f)),
               _mm_setzero_ps())));
   v = _mm_sub_epi32(v, _mm_set1_epi32(0x8000));
   v = _mm_packs_epi32(v, v);
   v = _mm_xor_si128(v, _mm_set1_epi16((short)0x8000));
   _mm_storel_epi64((__m128i*)&r, v);
   return r;
#elif defined(RGBA16_PACK_NEON)
   /* The unsigned convert saturates at both ends and takes a NaN to 0. */
   uint32x4_t v = vcvtq_u32_f32(vmlaq_n_f32(vdupq_n_f32(0.5f),
            vld1q_f32(rgba), 65535.0f));
   return vget_lane_u64(vreinterpret_u64_u16(vqmovn_u32(v)), 0);
#else
   uint64_t r = rgba16_unorm(rgba[0]);
   uint64_t g = rgba16_unorm(rgba[1]);
   uint64_t b = rgba16_unorm(rgba[2]);
   uint64_t a = rgba16_unorm(rgba[3]);
#if RETRO_IS_BIG_ENDIAN
   return (r << 48) | (g << 32) | (b << 16) | a;
#else
   return r | (g << 16) | (b << 32) | (a << 48);
#endif
#endif
}

#endif

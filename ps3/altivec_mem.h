/*********************************************************************************
 *   Copyright (C) 2008-2010 by Konstantinos Margaritis <markos@codex.gr>        *
 *   All rights reserved.                                                        *
 *                                                                               *
 * Redistribution and use in source and binary forms, with or without            *
 * modification, are permitted provided that the following conditions are met:   *
 *  1. Redistributions of source code must retain the above copyright            *
 *     notice, this list of conditions and the following disclaimer.             *
 *  2. Redistributions in binary form must reproduce the above copyright         *
 *     notice, this list of conditions and the following disclaimer in the       *
 *     documentation and/or other materials provided with the distribution.      *
 *  3. Neither the name of the Codex nor the                                     *
 *     names of its contributors may be used to endorse or promote products      *
 *     derived from this software without specific prior written permission.     *
 *                                                                               *
 * THIS SOFTWARE IS PROVIDED BY CODEX ''AS IS'' AND ANY                          *
 * EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED     *
 * WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE        *
 * DISCLAIMED. IN NO EVENT SHALL CODEX BE LIABLE FOR ANY                         *
 * DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES    *
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;  *
 * LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND   *
 * ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT    *
 * (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS *
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.                  *
 *********************************************************************************/

/* $Id$ */

#ifndef ALTIVEC_MEM_H
#define ALTIVEC_MEM_H

#include <sys/types.h>
#include <stdint.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

#if defined(__SNC__)
#include <ppu_altivec_internals.h>
#else
#include <altivec.h>
#endif

#define himagic         0x80808080L
#define lomagic         0x01010101L
#define magic_bits32    0x07efefeff
#define magic_bits64    (((unsigned long int) 0x7efefefe << 32) | 0xfefefeff)

#define charmask8(c)    ((uint8_t)(c & 0xff))
#define charmask16(c)   (uint16_t)((charmask8(c)) | (charmask8(c) << 8))
#define charmask32(c)   (uint32_t)((charmask16(c)) | (charmask16(c) << 16))
#define charmask64(c)   (uint64_t)((charmask32(c)) | (charmask32(c) << 32))

#define QMAKESTR(x) #x
#define MAKESTR(x) QMAKESTR(x)
#define SMASH(x,y) x/y
#define MAKEINC(x) SMASH(x,MACROFILE)

#define LIBFREEVEC_SIMD_MACROS_INC MAKEINC(LIBFREEVEC_SIMD_ENGINE)

#define ptrdiff_t(a, b)     ((word_t)(a)-(word_t)(b))
#define CMP_LT_OR_GT(a, b) ((a) - (b))
#define MIN(a,b)    ((a) <= (b) ? (a) : (b))
#define DIFF(a, b) ((a)-(b))

#ifdef LSB_FIRST
#define MERGE_SHIFTED_WORDS(a, b, sl, sr)      ((a) >> sl) | ((b) << sr)
#else
#define MERGE_SHIFTED_WORDS(a, b, sl, sr)      ((a) << sl) | ((b) >> sr)
#endif

#define DST_CHAN_1 1
#define DST_CHAN_2 2

#define DST_CTRL(size, count, stride) (((size) << 24) | ((count) << 16) | (stride))

//#define READ_PREFETCH_START1(addr)    vec_dstt(addr, DST_CTRL(2,2,32), DST_CHAN_1)
//#define READ_PREFETCH_START2(addr)    vec_dstt(addr, DST_CTRL(2,2,32), DST_CHAN_2)
//#define WRITE_PREFETCH_START1(addr)   vec_dststt(addr, DST_CTRL(2,2,32), DST_CHAN_1)
//#define WRITE_PREFETCH_START2(addr)   vec_dststt(addr, DST_CTRL(2,2,32), DST_CHAN_2)
//#define PREFETCH_STOP1                vec_dss(DST_CHAN_1)
//#define PREFETCH_STOP2                vec_dss(DST_CHAN_2)
#define READ_PREFETCH_START1(addr)
#define READ_PREFETCH_START2(addr)
#define WRITE_PREFETCH_START1(addr)
#define WRITE_PREFETCH_START2(addr)
#define PREFETCH_STOP1
#define PREFETCH_STOP2 

//#define LINUX64

#ifdef LINUX64

#define word_t  uint64_t
#define LIBFREEVEC_SCALAR_MACROS_INC MAKEINC(scalar64)
#define charmask(c) charmask64(c)

#define SIMD_PACKETSIZE     16
#define WORDS_IN_PACKET     2

static inline int copy_fwd_until_dst_word_aligned(uint8_t *d, const uint8_t *s)
{
   int dstal = ((word_t)d) % sizeof(word_t);

   switch (dstal)
   {
      case 1:
         *d++ = *s++;

      case 2:
         *d++ = *s++;

      case 3:
         *d++ = *s++;

      case 4:
         *d++ = *s++;

      case 5:
         *d++ = *s++;

      case 6:
         *d++ = *s++;

      case 7:
         *d = *s;
   }

   return dstal;
}

static inline void copy_fwd_rest_bytes(uint8_t *d, const uint8_t *s, size_t len)
{
    switch (len)
    {
        case 7:
            *d++ = *s++;

        case 6:
            *d++ = *s++;

        case 5:
            *d++ = *s++;

        case 4:
            *d++ = *s++;

        case 3:
            *d++ = *s++;

        case 2:
            *d++ = *s++;

        case 1:
            *d = *s;
    }
}

static inline void copy_fwd_rest_words_aligned(word_t *d, const word_t *s, size_t l)
{
    while (l > 0)
    {
        *d++ = *s++;
        l--;
    }
}

static inline void copy_fwd_rest_words_unaligned(word_t *d, const word_t *s, int sl, int sr, size_t l)
{
    while (l > 0)
    {
        *d++ = MERGE_SHIFTED_WORDS(*(s), *(s + 1), sl, sr); s++;
        l--;
    }
}

static inline void copy_fwd_until_dst_simd_aligned(word_t *d, const word_t *s, 
                                                int srcoffset4, size_t dstal, int sl, int sr)
{
    if (srcoffset4 == 0)
    {
        if (dstal == 8)
            *d++ = *s++;
    }
    else
    {
        if (dstal == 8)
        {
            *d = MERGE_SHIFTED_WORDS(*(s), *(s + 1), sl, sr);
            d++; s++;
        }
    }
}

static inline void memset_fwd_until_dst_word_aligned(uint8_t *ptr, uint8_t c, size_t al)
{
   switch(al)
   {
      case 1:
         *ptr++ = c;
      case 2:
         *ptr++ = c;
      case 3:
         *ptr++ = c;
   }
}

static inline void memset_fwd_until_simd_aligned(word_t *ptr_w, word_t w, size_t al)
{
  switch (al)
  {
     case 4:
        *ptr_w++ = w;
     case 8:
        *ptr_w++ = w;
     case 12:
        *ptr_w++ = w;
  }
}

static inline void memset_rest_words(word_t *ptr_w, word_t w, size_t l)
{
  while (l--)
      *ptr_w++ = w;
}

static inline int memset_rest_bytes(uint8_t *ptr, uint8_t c, size_t len)
{
   switch(len)
   {
      case 3:
         *ptr++ = c;
      case 2:
         *ptr++ = c;
      case 1:
         *ptr++ = c;
   }
}
#else

#define word_t  uint32_t
#define LIBFREEVEC_SCALAR_MACROS_INC MAKEINC(scalar32)
#define charmask(c) charmask32(c)

#define SIMD_PACKETSIZE     16
#define WORDS_IN_PACKET     4

static inline int copy_fwd_until_dst_word_aligned(uint8_t *d, const uint8_t *s)
{
   size_t dstal = ((size_t)d) % sizeof(word_t);

   switch (dstal)
   {
      case 1:
         *d++ = *s++;
      case 2:
         *d++ = *s++;
      case 3:
         *d = *s;
   }

   return dstal;
}

static inline void copy_fwd_rest_bytes(uint8_t *d, const uint8_t *s, size_t len)
{
    switch (len)
    {
        case 3:
            *d++ = *s++;
        case 2:
            *d++ = *s++;
        case 1:
            *d++ = *s++;
    }
}

static inline void copy_fwd_rest_words_aligned(word_t *d, const word_t *s, size_t l)
{
    while (l > 0)
    {
        *d++ = *s++;
        l--;
    }
}

static inline void copy_fwd_rest_words_unaligned(word_t *d, const word_t *s, int sl, int sr, size_t l)
{
    while (l > 0)
    {
        *d++ = MERGE_SHIFTED_WORDS(*(s), *(s + 1), sl, sr); s++;
        l--;
    }
}

static inline void copy_fwd_until_dst_simd_aligned(word_t *d, const word_t *s, 
      int srcoffset4, size_t dstal, int sl, int sr)
{
   if (srcoffset4 == 0)
   {
      switch (dstal)
      {
         case 4:
            *d++ = *s++;
         case 8:
            *d++ = *s++;
         case 12:
            *d++ = *s++;
      }
   }
   else
   {
      switch (dstal)
      {
         case 4:
            *d = MERGE_SHIFTED_WORDS(*(s), *(s + 1), sl, sr);
            d++; s++;
         case 8:
            *d = MERGE_SHIFTED_WORDS(*(s), *(s + 1), sl, sr);
            d++; s++;
         case 12:
            *d = MERGE_SHIFTED_WORDS(*(s), *(s + 1), sl, sr);
            d++; s++;
      }
   }
}

static inline void memset_fwd_until_dst_word_aligned(uint8_t *ptr, uint8_t c, size_t al)
{
   switch(al)
   {
      case 1:
         *ptr++ = c;
      case 2:
         *ptr++ = c;
      case 3:
         *ptr++ = c;
   }
}

static inline void memset_fwd_until_simd_aligned(word_t *ptr_w, word_t w, size_t al)
{
   switch (al)
   {
      case 4:
         *ptr_w++ = w;
      case 8:
         *ptr_w++ = w;
      case 12:
         *ptr_w++ = w;
   }
}

static inline void memset_rest_words(word_t *ptr_w, word_t w, size_t l)
{
   while (l--)
   {
      *ptr_w++ = w;
   }
}

static inline int memset_rest_bytes(uint8_t *ptr, uint8_t c, size_t len)
{
   switch(len)
   {
      case 3:
         *ptr++ = c;
      case 2:
         *ptr++ = c;
      case 1:
         *ptr++ = c;
   }
}
#endif

static inline void copy_fwd_rest_blocks_aligned(word_t *d, const uint8_t *s, size_t blocks)
{
   __vector unsigned char v1, v2, v3, v4;
   // Unroll blocks of 4 words
   while (blocks > 4)
   {
      v1 = vec_ld(0,  s);
      v2 = vec_ld(16, s);
      v3 = vec_ld(32, s);
      v4 = vec_ld(48, s);
      vec_st(v1, 0,  (uint8_t *)d);
      vec_st(v2, 16, (uint8_t *)d);
      vec_st(v3, 32, (uint8_t *)d);
      vec_st(v4, 48, (uint8_t *)d);
      d += 16; s += 4 * SIMD_PACKETSIZE;
      blocks -= 4;
   }

   while (blocks > 0)
   {
      v1 = vec_ld(0, s);
      vec_st(v1, 0, (uint8_t *)d);
      d += 4; s += SIMD_PACKETSIZE;
      blocks--;
   }
}

static inline void copy_fwd_rest_blocks_unaligned(word_t *d, const uint8_t *s, int srcoffset, int sl, int sr, size_t blocks)
{
   __vector unsigned char mask, MSQ1, LSQ1, LSQ2, LSQ3, LSQ4;
   mask = vec_lvsl(0, s);

   // Unroll blocks of 4 words
   while (blocks > 4)
   {
      MSQ1 = vec_ld(0, s);
      LSQ1 = vec_ld(15, s);
      LSQ2 = vec_ld(31, s);
      LSQ3 = vec_ld(47, s);
      LSQ4 = vec_ld(63, s);
      vec_st(vec_perm(MSQ1, LSQ1, mask), 0, (uint8_t *)d);
      vec_st(vec_perm(LSQ1, LSQ2, mask), 16, (uint8_t *)d);
      vec_st(vec_perm(LSQ2, LSQ3, mask), 32, (uint8_t *)d);
      vec_st(vec_perm(LSQ3, LSQ4, mask), 48, (uint8_t *)d);
      d += 16; s += 4 * SIMD_PACKETSIZE;
      blocks -= 4;
   }

   while (blocks > 0)
   {
      MSQ1 = vec_ld(0, s);
      LSQ1 = vec_ld(15, s);
      vec_st(vec_perm(MSQ1, LSQ1, mask), 0, (uint8_t *)d);
      d += 4; s += SIMD_PACKETSIZE;
      blocks--;
   }
}

static inline __vector unsigned char simdpacket_set_from_byte(const uint8_t c)
{
   __vector unsigned char v = vec_lde(0, &c);
   return vec_splat(v, 0);
}

static inline void memset_set_blocks(word_t *ptr_w, word_t pw, uint8_t c, size_t blocks)
{
   __vector unsigned char vc = simdpacket_set_from_byte(c);
   while (blocks > 4)
   { 
      vec_st(vc, 0, (uint8_t *)ptr_w); 
      vec_st(vc, 16, (uint8_t *)ptr_w);
      vec_st(vc, 32, (uint8_t *)ptr_w);
      vec_st(vc, 48, (uint8_t *)ptr_w);
      ptr_w += 4 * WORDS_IN_PACKET;
      blocks -= 4;
   }

   while (blocks--)
   {
      vec_st(vc, 0, (uint8_t *)ptr_w); 
      ptr_w += WORDS_IN_PACKET;
   }
}

extern void *vec_memcpy(void *dstpp, const void *srcpp, size_t len);
extern void *vec_memcpy_aligned(void *dstpp, const void *srcpp, size_t len);
extern void *vec_memset(void *s, int p, size_t len);

#ifdef __cplusplus
}
#endif

#define memcpy(dest, src, n) vec_memcpy(dest, src, n)
#define memset(s, c, n) vec_memset(s, c, n)

#endif

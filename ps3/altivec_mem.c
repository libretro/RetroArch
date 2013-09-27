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

#include <stdio.h>
#include <limits.h>
#include "altivec_mem.h"

void *vec_memcpy(void *dstpp, const void *srcpp, size_t len)
{
   const uint8_t *src = srcpp;
   uint8_t *dst = dstpp;

   if (len >= sizeof(word_t))
   {
      // Prefetch some stuff
      READ_PREFETCH_START1(src);
      WRITE_PREFETCH_START2(dst);

      // Copy until dst is word aligned
      int al = copy_fwd_until_dst_word_aligned(dst, src), l;

      if (al)
      {
         src += sizeof(word_t) - al;
         dst += sizeof(word_t) - al;
         len -= sizeof(word_t) - al;
      }

      // Now dst is word aligned. We'll continue by word copying, but
      // for this we have to know the word-alignment of src also.
      int srcoffset = ((word_t)(src) % sizeof(word_t)), sh_l, sh_r;
      sh_l = srcoffset * CHAR_BIT;
      sh_r = CHAR_BIT * sizeof(word_t) - sh_l;

      // Take the word-aligned long pointers of src and dest.
      word_t *dstl = (word_t *)(dst);
      const word_t *srcl = (word_t *)(src - srcoffset);

      if (len >= SIMD_PACKETSIZE)
      { 
         // While we're not 16-byte aligned, move in 4-byte long steps.

         al = (word_t)dstl % SIMD_PACKETSIZE;
         if (al)
         {
            copy_fwd_until_dst_simd_aligned(dstl, srcl, srcoffset, al, sh_l, sh_r);
            srcl += (SIMD_PACKETSIZE - al)/WORDS_IN_PACKET;
            src = (uint8_t *) srcl + srcoffset;
            dstl += (SIMD_PACKETSIZE - al)/WORDS_IN_PACKET;
            len -= SIMD_PACKETSIZE - al;
         }

         // Now, dst is 16byte aligned. We can use SIMD if len >= 16
         l = len / SIMD_PACKETSIZE;
         len -= l * SIMD_PACKETSIZE;
         if (((word_t)(src) % SIMD_PACKETSIZE) == 0)
            copy_fwd_rest_blocks_aligned(dstl, src, l);
         else
            copy_fwd_rest_blocks_unaligned(dstl, src, srcoffset, sh_l, sh_r, l);
         src += l*SIMD_PACKETSIZE;
         dstl += l * WORDS_IN_PACKET;
         srcl = (word_t *)(src - srcoffset);
      }
      // Stop the prefetching
      PREFETCH_STOP1;
      PREFETCH_STOP2;
      //#endif

      // Copy the remaining bytes using word-copying
      // Handle alignment as appropriate
      l = len / sizeof(word_t);
      len -= l * sizeof(word_t);

      if (srcoffset == 0)
      {
         copy_fwd_rest_words_aligned(dstl, srcl, l);
         srcl += l;
         src = (uint8_t *) srcl;
      }
      else
      {
         copy_fwd_rest_words_unaligned(dstl, srcl, sh_l, sh_r, l);
         srcl += l;
         src = (uint8_t *) srcl + srcoffset;
      }

      dstl += l;

      // For the end copy we have to use char * pointers.
      dst = (uint8_t *) dstl;
   }

   // Copy the remaining bytes
   copy_fwd_rest_bytes(dst, src, len);

   return dstpp;
}

void *vec_memcpy_aligned(void *dstpp, const void *srcpp, size_t len)
{
   const uint8_t *src = srcpp;
   uint8_t *dst = dstpp;

   if (len >= sizeof(word_t))
   {
      // Prefetch some stuff
      READ_PREFETCH_START1(src);
      WRITE_PREFETCH_START2(dst);

      // Take the word-aligned long pointers of src and dest.
      word_t *dstl = (word_t *)(dst);
      const word_t *srcl = (word_t *)(src);
      int l;

#ifdef LIBFREEVEC_SIMD_ENGINE
      if (len >= SIMD_PACKETSIZE)
      { 
         l = len / SIMD_PACKETSIZE;
         len -= l * SIMD_PACKETSIZE;
         // Now, dst is 16byte aligned. We can use SIMD if len >= 16
         copy_fwd_rest_blocks_aligned(dstl, src, l);
      }
#endif

      // Copy the remaining bytes using word-copying
      // Handle alignment as appropriate
      l = len / sizeof(word_t);
      copy_fwd_rest_words_aligned(dstl, srcl, l);
      srcl += l;
      dstl += l;
      len -= l * sizeof(word_t);
      // For the end copy we have to use char * pointers.
      src = (uint8_t *) srcl;
      dst = (uint8_t *) dstl;
   }

   // Stop the prefetching
   PREFETCH_STOP1;

   PREFETCH_STOP2;

   // Copy the remaining bytes
   copy_fwd_rest_bytes(dst, src, len);

   return dstpp;
}

void *vec_memset(void *s, int p, size_t len)
{
   uint8_t* ptr = s;
   uint8_t __attribute__ ((aligned(16))) P = p;
   if (len >= sizeof(word_t))
   {
      word_t pw = charmask(P);

      size_t al = ((size_t)ptr) % sizeof(word_t);
      if (al)
      {
         memset_fwd_until_dst_word_aligned(ptr, P, al);
         ptr += sizeof(word_t) - al;
         len -= sizeof(word_t) - al;
      }

      int l;
      word_t *ptr_w = (word_t *)(ptr);
      if (len >= SIMD_PACKETSIZE)
      {
         // ptr is now word (32/64bit) aligned, memset until ptr is SIMD aligned
         al = (word_t) ptr_w % SIMD_PACKETSIZE;
         if (al)
         {
            memset_fwd_until_simd_aligned(ptr_w, pw, al);
            ptr_w += (SIMD_PACKETSIZE - al)/WORDS_IN_PACKET;
            len -= SIMD_PACKETSIZE - al;
         }
         // ptr is now 128-bit aligned
         // perform set using SIMD

         l = len / SIMD_PACKETSIZE;
         len -= l * SIMD_PACKETSIZE; 
         memset_set_blocks(ptr_w, pw, P, l);
         ptr_w += l * WORDS_IN_PACKET;
      }
      // memset the remaining words
      l = len / sizeof(word_t);
      len -= l * sizeof(word_t); 
      memset_rest_words(ptr_w, pw, l);
      ptr_w += l;
      ptr = (uint8_t *)ptr_w;
   }
   // Handle the remaining bytes
   switch(len)
   {
      case 3:
         *ptr++ = P;
      case 2:
         *ptr++ = P;
      case 1:
         *ptr++ = P;
   }
   return s;
}

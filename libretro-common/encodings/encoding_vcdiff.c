/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_vcdiff.c).
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

#include <stdlib.h>
#include <string.h>

#include <encodings/encoding_vcdiff.h>

/* --------------------------------------------------------------------
 * VCDIFF (RFC 3284), decode side only.
 *
 * Written from the specification.  Section references below are to
 * RFC 3284.
 * -------------------------------------------------------------------- */

/* Instruction types (s5.4). */
#define VCD_NOOP 0
#define VCD_ADD  1
#define VCD_RUN  2
#define VCD_COPY 3

/* Hdr_Indicator bits (s4.1). */
#define VCD_DECOMPRESS 0x01
#define VCD_CODETABLE  0x02
#define VCD_APPHEADER  0x04

/* Win_Indicator bits (s4.2). */
#define VCD_SOURCE     0x01
#define VCD_TARGET     0x02

/* The address cache (s5.1): four "near" slots and three pages of 256
 * "same" slots. */
#define VCD_NEAR 4
#define VCD_SAME 3

struct vcd_inst
{
   uint8_t type1, size1, mode1;
   uint8_t type2, size2, mode2;
};

/* The default code table (s5.5), generated from the rules that define
 * it rather than transcribed, then checked against a reference decoder
 * over a corpus of real patches. */
static const struct vcd_inst vcd_table[256] = {
   {2, 0,0,0,0,0}, {1, 0,0,0,0,0}, {1, 1,0,0,0,0}, {1, 2,0,0,0,0},
   {1, 3,0,0,0,0}, {1, 4,0,0,0,0}, {1, 5,0,0,0,0}, {1, 6,0,0,0,0},
   {1, 7,0,0,0,0}, {1, 8,0,0,0,0}, {1, 9,0,0,0,0}, {1,10,0,0,0,0},
   {1,11,0,0,0,0}, {1,12,0,0,0,0}, {1,13,0,0,0,0}, {1,14,0,0,0,0},
   {1,15,0,0,0,0}, {1,16,0,0,0,0}, {1,17,0,0,0,0}, {3, 0,0,0,0,0},
   {3, 4,0,0,0,0}, {3, 5,0,0,0,0}, {3, 6,0,0,0,0}, {3, 7,0,0,0,0},
   {3, 8,0,0,0,0}, {3, 9,0,0,0,0}, {3,10,0,0,0,0}, {3,11,0,0,0,0},
   {3,12,0,0,0,0}, {3,13,0,0,0,0}, {3,14,0,0,0,0}, {3,15,0,0,0,0},
   {3,16,0,0,0,0}, {3,17,0,0,0,0}, {3,18,0,0,0,0}, {3, 0,1,0,0,0},
   {3, 4,1,0,0,0}, {3, 5,1,0,0,0}, {3, 6,1,0,0,0}, {3, 7,1,0,0,0},
   {3, 8,1,0,0,0}, {3, 9,1,0,0,0}, {3,10,1,0,0,0}, {3,11,1,0,0,0},
   {3,12,1,0,0,0}, {3,13,1,0,0,0}, {3,14,1,0,0,0}, {3,15,1,0,0,0},
   {3,16,1,0,0,0}, {3,17,1,0,0,0}, {3,18,1,0,0,0}, {3, 0,2,0,0,0},
   {3, 4,2,0,0,0}, {3, 5,2,0,0,0}, {3, 6,2,0,0,0}, {3, 7,2,0,0,0},
   {3, 8,2,0,0,0}, {3, 9,2,0,0,0}, {3,10,2,0,0,0}, {3,11,2,0,0,0},
   {3,12,2,0,0,0}, {3,13,2,0,0,0}, {3,14,2,0,0,0}, {3,15,2,0,0,0},
   {3,16,2,0,0,0}, {3,17,2,0,0,0}, {3,18,2,0,0,0}, {3, 0,3,0,0,0},
   {3, 4,3,0,0,0}, {3, 5,3,0,0,0}, {3, 6,3,0,0,0}, {3, 7,3,0,0,0},
   {3, 8,3,0,0,0}, {3, 9,3,0,0,0}, {3,10,3,0,0,0}, {3,11,3,0,0,0},
   {3,12,3,0,0,0}, {3,13,3,0,0,0}, {3,14,3,0,0,0}, {3,15,3,0,0,0},
   {3,16,3,0,0,0}, {3,17,3,0,0,0}, {3,18,3,0,0,0}, {3, 0,4,0,0,0},
   {3, 4,4,0,0,0}, {3, 5,4,0,0,0}, {3, 6,4,0,0,0}, {3, 7,4,0,0,0},
   {3, 8,4,0,0,0}, {3, 9,4,0,0,0}, {3,10,4,0,0,0}, {3,11,4,0,0,0},
   {3,12,4,0,0,0}, {3,13,4,0,0,0}, {3,14,4,0,0,0}, {3,15,4,0,0,0},
   {3,16,4,0,0,0}, {3,17,4,0,0,0}, {3,18,4,0,0,0}, {3, 0,5,0,0,0},
   {3, 4,5,0,0,0}, {3, 5,5,0,0,0}, {3, 6,5,0,0,0}, {3, 7,5,0,0,0},
   {3, 8,5,0,0,0}, {3, 9,5,0,0,0}, {3,10,5,0,0,0}, {3,11,5,0,0,0},
   {3,12,5,0,0,0}, {3,13,5,0,0,0}, {3,14,5,0,0,0}, {3,15,5,0,0,0},
   {3,16,5,0,0,0}, {3,17,5,0,0,0}, {3,18,5,0,0,0}, {3, 0,6,0,0,0},
   {3, 4,6,0,0,0}, {3, 5,6,0,0,0}, {3, 6,6,0,0,0}, {3, 7,6,0,0,0},
   {3, 8,6,0,0,0}, {3, 9,6,0,0,0}, {3,10,6,0,0,0}, {3,11,6,0,0,0},
   {3,12,6,0,0,0}, {3,13,6,0,0,0}, {3,14,6,0,0,0}, {3,15,6,0,0,0},
   {3,16,6,0,0,0}, {3,17,6,0,0,0}, {3,18,6,0,0,0}, {3, 0,7,0,0,0},
   {3, 4,7,0,0,0}, {3, 5,7,0,0,0}, {3, 6,7,0,0,0}, {3, 7,7,0,0,0},
   {3, 8,7,0,0,0}, {3, 9,7,0,0,0}, {3,10,7,0,0,0}, {3,11,7,0,0,0},
   {3,12,7,0,0,0}, {3,13,7,0,0,0}, {3,14,7,0,0,0}, {3,15,7,0,0,0},
   {3,16,7,0,0,0}, {3,17,7,0,0,0}, {3,18,7,0,0,0}, {3, 0,8,0,0,0},
   {3, 4,8,0,0,0}, {3, 5,8,0,0,0}, {3, 6,8,0,0,0}, {3, 7,8,0,0,0},
   {3, 8,8,0,0,0}, {3, 9,8,0,0,0}, {3,10,8,0,0,0}, {3,11,8,0,0,0},
   {3,12,8,0,0,0}, {3,13,8,0,0,0}, {3,14,8,0,0,0}, {3,15,8,0,0,0},
   {3,16,8,0,0,0}, {3,17,8,0,0,0}, {3,18,8,0,0,0}, {1, 1,0,3,4,0},
   {1, 1,0,3,5,0}, {1, 1,0,3,6,0}, {1, 2,0,3,4,0}, {1, 2,0,3,5,0},
   {1, 2,0,3,6,0}, {1, 3,0,3,4,0}, {1, 3,0,3,5,0}, {1, 3,0,3,6,0},
   {1, 4,0,3,4,0}, {1, 4,0,3,5,0}, {1, 4,0,3,6,0}, {1, 1,0,3,4,1},
   {1, 1,0,3,5,1}, {1, 1,0,3,6,1}, {1, 2,0,3,4,1}, {1, 2,0,3,5,1},
   {1, 2,0,3,6,1}, {1, 3,0,3,4,1}, {1, 3,0,3,5,1}, {1, 3,0,3,6,1},
   {1, 4,0,3,4,1}, {1, 4,0,3,5,1}, {1, 4,0,3,6,1}, {1, 1,0,3,4,2},
   {1, 1,0,3,5,2}, {1, 1,0,3,6,2}, {1, 2,0,3,4,2}, {1, 2,0,3,5,2},
   {1, 2,0,3,6,2}, {1, 3,0,3,4,2}, {1, 3,0,3,5,2}, {1, 3,0,3,6,2},
   {1, 4,0,3,4,2}, {1, 4,0,3,5,2}, {1, 4,0,3,6,2}, {1, 1,0,3,4,3},
   {1, 1,0,3,5,3}, {1, 1,0,3,6,3}, {1, 2,0,3,4,3}, {1, 2,0,3,5,3},
   {1, 2,0,3,6,3}, {1, 3,0,3,4,3}, {1, 3,0,3,5,3}, {1, 3,0,3,6,3},
   {1, 4,0,3,4,3}, {1, 4,0,3,5,3}, {1, 4,0,3,6,3}, {1, 1,0,3,4,4},
   {1, 1,0,3,5,4}, {1, 1,0,3,6,4}, {1, 2,0,3,4,4}, {1, 2,0,3,5,4},
   {1, 2,0,3,6,4}, {1, 3,0,3,4,4}, {1, 3,0,3,5,4}, {1, 3,0,3,6,4},
   {1, 4,0,3,4,4}, {1, 4,0,3,5,4}, {1, 4,0,3,6,4}, {1, 1,0,3,4,5},
   {1, 1,0,3,5,5}, {1, 1,0,3,6,5}, {1, 2,0,3,4,5}, {1, 2,0,3,5,5},
   {1, 2,0,3,6,5}, {1, 3,0,3,4,5}, {1, 3,0,3,5,5}, {1, 3,0,3,6,5},
   {1, 4,0,3,4,5}, {1, 4,0,3,5,5}, {1, 4,0,3,6,5}, {1, 1,0,3,4,6},
   {1, 2,0,3,4,6}, {1, 3,0,3,4,6}, {1, 4,0,3,4,6}, {1, 1,0,3,4,7},
   {1, 2,0,3,4,7}, {1, 3,0,3,4,7}, {1, 4,0,3,4,7}, {1, 1,0,3,4,8},
   {1, 2,0,3,4,8}, {1, 3,0,3,4,8}, {1, 4,0,3,4,8}, {3, 4,0,1,1,0},
   {3, 4,1,1,1,0}, {3, 4,2,1,1,0}, {3, 4,3,1,1,0}, {3, 4,4,1,1,0},
   {3, 4,5,1,1,0}, {3, 4,6,1,1,0}, {3, 4,7,1,1,0}, {3, 4,8,1,1,0}
};

struct vcd_dec
{
   const uint8_t *src;        /* source file, may be NULL when empty  */
   size_t         src_len;

   uint8_t       *out;        /* target being built                   */
   size_t         out_len;    /* bytes produced so far                */
   size_t         out_cap;

   uint32_t       near[VCD_NEAR];
   uint32_t       same[VCD_SAME * 256];
   unsigned       next_slot;
};

/* --------------------------------------------------------------------
 * primitives
 * -------------------------------------------------------------------- */

/* Big-endian base-128, high bit continues (s2).  Refuses anything that
 * will not fit in 32 bits: every length and address in a patch this
 * decoder will accept is bounded by the buffers it addresses, and a
 * wider value can only be corrupt. */
static bool vcd_varint(const uint8_t **p, const uint8_t *end,
      uint32_t *out)
{
   uint32_t v = 0;
   unsigned i;

   for (i = 0; i < 5; i++)
   {
      uint8_t b;
      if (*p >= end)
         return false;
      b = *(*p)++;
      if (v > (uint32_t)0x01FFFFFF)   /* the shift below would drop bits */
         return false;
      v = (v << 7) | (uint8_t)(b & 0x7F);
      if (!(b & 0x80))
      {
         *out = v;
         return true;
      }
   }
   return false;
}

static bool vcd_reserve(struct vcd_dec *d, size_t need)
{
   size_t   cap;
   uint8_t *buf;

   if (need <= d->out_cap)
      return true;
   /* The first reservation is the pre-pass's exact total, so take it
    * verbatim rather than rounding it up a doubling ladder; only
    * growth beyond a hint that turned out short doubles. */
   if (!d->out_cap)
      cap = need;
   else
   {
      cap = d->out_cap;
      while (cap < need)
      {
         if (cap > ((size_t)-1) / 2)
            return false;
         cap <<= 1;
      }
   }
   if (!(buf = (uint8_t*)realloc(d->out, cap)))
      return false;
   d->out     = buf;
   d->out_cap = cap;
   return true;
}

/* --------------------------------------------------------------------
 * address cache (s5.1)
 * -------------------------------------------------------------------- */

static void vcd_cache_reset(struct vcd_dec *d)
{
   memset(d->near, 0, sizeof(d->near));
   memset(d->same, 0, sizeof(d->same));
   d->next_slot = 0;
}

static void vcd_cache_update(struct vcd_dec *d, uint32_t addr)
{
   d->same[addr % (VCD_SAME * 256)] = addr;
   d->near[d->next_slot]            = addr;
   d->next_slot                     = (d->next_slot + 1) % VCD_NEAR;
}

/* Decode one address in @mode, for a COPY whose target position is
 * @here.  Advances the address section cursor. */
static bool vcd_addr(struct vcd_dec *d, unsigned mode, uint32_t here,
      const uint8_t **ap, const uint8_t *aend, uint32_t *out)
{
   uint32_t v = 0;

   if (mode == 0)                       /* SELF: absolute            */
   {
      if (!vcd_varint(ap, aend, &v))
         return false;
   }
   else if (mode == 1)                  /* HERE: back from here      */
   {
      if (!vcd_varint(ap, aend, &v) || v > here)
         return false;
      v = here - v;
   }
   else if (mode < 2 + VCD_NEAR)        /* near cache                */
   {
      uint32_t base = d->near[mode - 2];
      uint32_t off  = 0;
      if (!vcd_varint(ap, aend, &off) || off > (uint32_t)0xFFFFFFFFu - base)
         return false;
      v = base + off;
   }
   else                                 /* same cache: a byte index  */
   {
      unsigned page = mode - (2 + VCD_NEAR);
      if (*ap >= aend)
         return false;
      v = d->same[page * 256 + *(*ap)++];
   }

   vcd_cache_update(d, v);
   *out = v;
   return true;
}

/* --------------------------------------------------------------------
 * window decode (s4.3)
 * -------------------------------------------------------------------- */

/* Copy @len bytes to the target at @at from address @addr in the
 * window's address space: [0, seg_len) is the source segment, and
 * everything above it is the target window itself.  A COPY may overlap
 * its own output - that is how the format expresses runs - so the
 * target-referencing case must copy byte by byte. */
static bool vcd_copy(struct vcd_dec *d, const uint8_t *seg, size_t seg_len,
      size_t win_start, uint32_t addr, uint32_t len, size_t at)
{
   if (addr < seg_len)
   {
      /* wholly inside the source segment, or it would run past its end */
      if ((size_t)len > seg_len - addr)
         return false;
      memcpy(d->out + at, seg + addr, len);
      return true;
   }
   {
      size_t from = win_start + (size_t)(addr - seg_len);
      uint32_t i;
      if (from >= at + len)
      {
         /* cannot happen for a well-formed patch: the source of a
          * target COPY is always behind the cursor */
         return false;
      }
      if (from + len <= at)
      {
         memcpy(d->out + at, d->out + from, len);
         return true;
      }
      /* Overlapping, which is how the format spells a repeat: the
       * source is behind the cursor, so the first (at - from) bytes
       * are already final and each chunk of that width can be copied
       * whole before the next one needs it.  Byte-at-a-time is
       * correct but needlessly slow for the wide distances that
       * dominate real patches. */
      {
         size_t dist = at - from;
         size_t done = 0;
         if (dist == 1)
         {
            memset(d->out + at, d->out[from], len);
            return true;
         }
         while (done < (size_t)len)
         {
            size_t chunk = (size_t)len - done;
            if (chunk > dist)
               chunk = dist;
            memcpy(d->out + at + done, d->out + from + done, chunk);
            done += chunk;
         }
      }
      (void)i;
   }
   return true;
}

/* Sum the windows' target lengths without decoding them, so the output
 * is one allocation of the right size.
 *
 * The format carries no total length, only a length per window, so a
 * decoder that simply grows as it goes pays a copy every time it
 * doubles - on a 32 MiB target that is a dozen reallocations and twice
 * the target in memmove traffic, which measured as the single largest
 * cost in the decode.  Walking the headers is cheap by comparison:
 * every section length is stated, so each window is a handful of
 * varints and a skip.  A malformed patch is left to the real decode to
 * reject; this pass only ever produces a hint, and returns 0 to mean
 * "cannot tell", which falls back to growing. */
static size_t vcd_total_target(const uint8_t *p, const uint8_t *pend,
      size_t src_len)
{
   size_t total = 0;

   while (p < pend)
   {
      uint32_t seg_len = 0, seg_pos = 0, enc_len = 0, tgt_len = 0;
      uint32_t data_len = 0, inst_len = 0, addr_len = 0;
      uint8_t  win_ind;

      if (p >= pend)
         break;
      win_ind = *p++;
      if (win_ind & (VCD_SOURCE | VCD_TARGET))
      {
         if (     !vcd_varint(&p, pend, &seg_len)
               || !vcd_varint(&p, pend, &seg_pos))
            return 0;
      }
      if (     !vcd_varint(&p, pend, &enc_len)
            || !vcd_varint(&p, pend, &tgt_len))
         return 0;
      if (p >= pend)
         return 0;
      p++;                                   /* delta indicator */
      if (     !vcd_varint(&p, pend, &data_len)
            || !vcd_varint(&p, pend, &inst_len)
            || !vcd_varint(&p, pend, &addr_len))
         return 0;
      if (      (size_t)(pend - p) < (size_t)data_len
            || (size_t)(pend - p) - data_len < (size_t)inst_len
            || (size_t)(pend - p) - data_len - inst_len < (size_t)addr_len)
         return 0;
      p    += (size_t)data_len + inst_len + addr_len;
      if (total > ((size_t)-1) - tgt_len)
         return 0;
      total += tgt_len;
   }
   (void)src_len;
   return total;
}

static bool vcd_window(struct vcd_dec *d, const uint8_t **pp,
      const uint8_t *pend)
{
   const uint8_t *p = *pp;
   const uint8_t *dp, *ip, *ap, *dend, *iend, *aend;
   const uint8_t *seg = NULL;
   uint32_t seg_len = 0, seg_pos = 0;
   uint32_t enc_len = 0, tgt_len = 0;
   uint32_t data_len = 0, inst_len = 0, addr_len = 0;
   size_t   win_start = d->out_len;
   uint8_t  win_ind, delta_ind;

   if (p >= pend)
      return false;
   win_ind = *p++;

   if (win_ind & (VCD_SOURCE | VCD_TARGET))
   {
      if (     !vcd_varint(&p, pend, &seg_len)
            || !vcd_varint(&p, pend, &seg_pos))
         return false;
      if (win_ind & VCD_SOURCE)
      {
         if (      (size_t)seg_pos > d->src_len
               || (size_t)seg_len > d->src_len - seg_pos)
            return false;
         seg = d->src + seg_pos;
      }
      else
      {
         /* the segment comes from the target already produced */
         if (      (size_t)seg_pos > d->out_len
               || (size_t)seg_len > d->out_len - seg_pos)
            return false;
         seg = d->out + seg_pos;
      }
   }

   if (     !vcd_varint(&p, pend, &enc_len)
         || !vcd_varint(&p, pend, &tgt_len))
      return false;
   if (p >= pend)
      return false;
   delta_ind = *p++;

   /* Any of the three sections being compressed means a secondary
    * compressor, which this decoder does not implement and must not
    * pretend to. */
   if (delta_ind != 0)
      return false;

   if (     !vcd_varint(&p, pend, &data_len)
         || !vcd_varint(&p, pend, &inst_len)
         || !vcd_varint(&p, pend, &addr_len))
      return false;

   /* the three sections follow, back to back */
   if (      (size_t)(pend - p) < (size_t)data_len
         || (size_t)(pend - p) - data_len < (size_t)inst_len
         || (size_t)(pend - p) - data_len - inst_len < (size_t)addr_len)
      return false;

   dp = p;             dend = dp + data_len;
   ip = dend;          iend = ip + inst_len;
   ap = iend;          aend = ap + addr_len;

   if (!vcd_reserve(d, win_start + tgt_len))
      return false;

   vcd_cache_reset(d);

   while (d->out_len < win_start + tgt_len)
   {
      const struct vcd_inst *e;
      unsigned half;

      if (ip >= iend)
         return false;
      e = &vcd_table[*ip++];

      for (half = 0; half < 2; half++)
      {
         unsigned type = half ? e->type2 : e->type1;
         uint32_t size = half ? e->size2 : e->size1;
         unsigned mode = half ? e->mode2 : e->mode1;
         size_t   at   = d->out_len;

         if (type == VCD_NOOP)
            continue;
         if (size == 0 && !vcd_varint(&ip, iend, &size))
            return false;
         if (      size == 0
               || (size_t)size > win_start + tgt_len - at)
            return false;

         switch (type)
         {
            case VCD_ADD:
               if ((size_t)(dend - dp) < (size_t)size)
                  return false;
               memcpy(d->out + at, dp, size);
               dp += size;
               break;

            case VCD_RUN:
               if (dp >= dend)
                  return false;
               memset(d->out + at, *dp++, size);
               break;

            case VCD_COPY:
            {
               uint32_t addr = 0;
               uint32_t here = (uint32_t)(seg_len + (at - win_start));
               if (!vcd_addr(d, mode, here, &ap, aend, &addr))
                  return false;
               if (!vcd_copy(d, seg, seg_len, win_start, addr, size, at))
                  return false;
               break;
            }

            default:
               return false;
         }
         d->out_len = at + size;
      }
   }

   if (d->out_len != win_start + tgt_len)
      return false;

   *pp = aend;
   return true;
}

/* --------------------------------------------------------------------
 * entry point
 * -------------------------------------------------------------------- */

bool vcdiff_decode(const uint8_t *patch, size_t patch_len,
      const uint8_t *src, size_t src_len,
      uint8_t **out, size_t *out_len)
{
   struct vcd_dec d;
   const uint8_t *p, *pend;
   uint8_t hdr;

   if (!patch || !out || !out_len || patch_len < 5)
      return false;
   if (!src && src_len)
      return false;

   /* header (s4.1): magic, version, indicator */
   if (      patch[0] != 0xD6 || patch[1] != 0xC3
         ||  patch[2] != 0xC4 || patch[3] != 0x00)
      return false;

   p    = patch + 4;
   pend = patch + patch_len;
   hdr  = *p++;

   /* A secondary compressor or a replacement code table would change
    * how everything below is read.  Refuse rather than misread. */
   if (hdr & (VCD_DECOMPRESS | VCD_CODETABLE))
      return false;

   if (hdr & VCD_APPHEADER)
   {
      uint32_t alen = 0;
      if (!vcd_varint(&p, pend, &alen) || (size_t)(pend - p) < (size_t)alen)
         return false;
      p += alen;                       /* informational only */
   }

   memset(&d, 0, sizeof(d));
   d.src     = src;
   d.src_len = src_len;

   {
      size_t hint = vcd_total_target(p, pend, src_len);
      if (hint && !vcd_reserve(&d, hint))
         return false;
   }

   while (p < pend)
   {
      if (!vcd_window(&d, &p, pend))
      {
         free(d.out);
         return false;
      }
   }

   *out     = d.out;
   *out_len = d.out_len;
   return true;
}

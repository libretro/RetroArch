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

struct vcdiff_stream
{
   const uint8_t *patch;      /* borrowed, must outlive the stream    */
   size_t         patch_len;
   size_t         p_off;      /* first window not yet decoded         */

   const uint8_t *src;        /* caller's buffer, or own_src below    */
   size_t         src_len;    /* declared total                       */
   size_t         src_seen;   /* how much of it has arrived           */
   uint8_t       *own_src;    /* retained copy, streaming only        */
   size_t         own_cap;
   uint8_t        owns_src;   /* 1 when own_src is in use             */
   uint8_t        failed;

   uint8_t       *out;        /* target being built                   */
   size_t         out_len;    /* bytes produced so far                */
   size_t         out_cap;

   uint32_t       near[VCD_NEAR];
   uint32_t       same[VCD_SAME * 256];
   unsigned       next_slot;
};

/* kept for the internal functions, which predate the streaming form */
#define vcd_dec vcdiff_stream

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
      /* A COPY that reads the target must read bytes already produced.
       * Anything at or past the cursor names output that does not
       * exist yet - and the distance arithmetic below would underflow,
       * handing memcpy overlapping ranges and copying whatever the
       * allocation happened to contain into the decoded content.
       *
       * Compare inside the window's own address space rather than on
       * the derived pointer offsets: an address near the top of the
       * 32-bit range added to win_start wraps on a 32-bit host, and a
       * wrapped value can land below the cursor and pass a check made
       * after the addition.  Both terms here are bounded by the window
       * itself, so neither can wrap. */
      size_t produced = at - win_start;      /* target bytes so far    */
      size_t off      = (size_t)(addr - seg_len);
      size_t from;
      uint32_t i;

      if (off >= produced)
         return false;
      from = win_start + off;
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

/* A window either decodes, or waits for source it has not been given
 * yet, or is malformed.  The middle case is not an error: the caller
 * feeds more and the same window is attempted again. */
enum vcd_win
{
   VCD_WIN_OK = 0,
   VCD_WIN_WAIT,
   VCD_WIN_ERROR
};

static enum vcd_win vcd_window(struct vcd_dec *d, const uint8_t **pp,
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
      return VCD_WIN_ERROR;
   win_ind = *p++;

   if (win_ind & (VCD_SOURCE | VCD_TARGET))
   {
      if (     !vcd_varint(&p, pend, &seg_len)
            || !vcd_varint(&p, pend, &seg_pos))
         return VCD_WIN_ERROR;
      if (win_ind & VCD_SOURCE)
      {
         if (      (size_t)seg_pos > d->src_len
               || (size_t)seg_len > d->src_len - seg_pos)
            return VCD_WIN_ERROR;
         /* The segment this window needs may not have arrived yet.
          * Nothing has been consumed at this point - the cursor is
          * still at the window header - so the caller can simply try
          * again once more source has been fed. */
         if ((size_t)seg_pos + seg_len > d->src_seen)
            return VCD_WIN_WAIT;
         seg = d->src + seg_pos;
      }
      else
      {
         /* the segment comes from the target already produced */
         if (      (size_t)seg_pos > d->out_len
               || (size_t)seg_len > d->out_len - seg_pos)
            return VCD_WIN_ERROR;
         seg = d->out + seg_pos;
      }
   }

   if (     !vcd_varint(&p, pend, &enc_len)
         || !vcd_varint(&p, pend, &tgt_len))
      return VCD_WIN_ERROR;
   if (p >= pend)
      return VCD_WIN_ERROR;
   delta_ind = *p++;

   /* Any of the three sections being compressed means a secondary
    * compressor, which this decoder does not implement and must not
    * pretend to. */
   if (delta_ind != 0)
      return VCD_WIN_ERROR;

   if (     !vcd_varint(&p, pend, &data_len)
         || !vcd_varint(&p, pend, &inst_len)
         || !vcd_varint(&p, pend, &addr_len))
      return VCD_WIN_ERROR;

   /* the three sections follow, back to back */
   if (      (size_t)(pend - p) < (size_t)data_len
         || (size_t)(pend - p) - data_len < (size_t)inst_len
         || (size_t)(pend - p) - data_len - inst_len < (size_t)addr_len)
      return VCD_WIN_ERROR;

   dp = p;             dend = dp + data_len;
   ip = dend;          iend = ip + inst_len;
   ap = iend;          aend = ap + addr_len;

   /* On a 32-bit host a large enough window could carry the end past
    * SIZE_MAX; every bound below is expressed against that sum, so it
    * has to be real before any of them mean anything. */
   if ((size_t)tgt_len > ((size_t)-1) - win_start)
      return VCD_WIN_ERROR;
   if (!vcd_reserve(d, win_start + tgt_len))
      return VCD_WIN_ERROR;

   vcd_cache_reset(d);

   while (d->out_len < win_start + tgt_len)
   {
      const struct vcd_inst *e;
      unsigned half;

      if (ip >= iend)
         return VCD_WIN_ERROR;
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
            return VCD_WIN_ERROR;
         if (      size == 0
               || (size_t)size > win_start + tgt_len - at)
            return VCD_WIN_ERROR;

         switch (type)
         {
            case VCD_ADD:
               if ((size_t)(dend - dp) < (size_t)size)
                  return VCD_WIN_ERROR;
               memcpy(d->out + at, dp, size);
               dp += size;
               break;

            case VCD_RUN:
               if (dp >= dend)
                  return VCD_WIN_ERROR;
               memset(d->out + at, *dp++, size);
               break;

            case VCD_COPY:
            {
               uint32_t addr = 0;
               uint32_t here = (uint32_t)(seg_len + (at - win_start));
               if (!vcd_addr(d, mode, here, &ap, aend, &addr))
                  return VCD_WIN_ERROR;
               if (!vcd_copy(d, seg, seg_len, win_start, addr, size, at))
                  return VCD_WIN_ERROR;
               break;
            }

            default:
               return VCD_WIN_ERROR;
         }
         d->out_len = at + size;
      }
   }

   if (d->out_len != win_start + tgt_len)
      return VCD_WIN_ERROR;

   *pp = aend;
   return VCD_WIN_OK;
}

/* --------------------------------------------------------------------
 * public API
 * -------------------------------------------------------------------- */

/* Decode every window whose source has arrived.  Stops at the first one
 * that must wait, leaving p_off on its header so the next feed retries
 * it from a clean cursor. */
static bool vcd_drain(struct vcdiff_stream *s)
{
   const uint8_t *pend = s->patch + s->patch_len;
   const uint8_t *p    = s->patch + s->p_off;

   while (p < pend)
   {
      const uint8_t *before = p;
      switch (vcd_window(s, &p, pend))
      {
         case VCD_WIN_OK:
            s->p_off = (size_t)(p - s->patch);
            break;
         case VCD_WIN_WAIT:
            p        = before;
            s->p_off = (size_t)(before - s->patch);
            return true;
         default:
            s->failed = 1;
            return false;
      }
   }
   s->p_off = s->patch_len;
   return true;
}

vcdiff_stream_t *vcdiff_stream_open(const uint8_t *patch, size_t patch_len,
      size_t src_len)
{
   struct vcdiff_stream *s;
   const uint8_t        *p;
   uint8_t               hdr;

   if (!patch || patch_len < 5)
      return NULL;
   if (      patch[0] != 0xD6 || patch[1] != 0xC3
         ||  patch[2] != 0xC4 || patch[3] != 0x00)
      return NULL;

   p   = patch + 4;
   hdr = *p++;

   /* A secondary compressor or a replacement code table would change
    * how everything below is read.  Refuse rather than misread. */
   if (hdr & (VCD_DECOMPRESS | VCD_CODETABLE))
      return NULL;

   if (hdr & VCD_APPHEADER)
   {
      uint32_t alen = 0;
      if (     !vcd_varint(&p, patch + patch_len, &alen)
            || (size_t)(patch + patch_len - p) < (size_t)alen)
         return NULL;
      p += alen;                       /* informational only */
   }

   if (!(s = (struct vcdiff_stream*)calloc(1, sizeof(*s))))
      return NULL;

   s->patch     = patch;
   s->patch_len = patch_len;
   s->p_off     = (size_t)(p - patch);
   s->src_len   = src_len;

   {
      size_t hint = vcd_total_target(p, patch + patch_len, src_len);
      if (hint && !vcd_reserve(s, hint))
      {
         free(s);
         return NULL;
      }
   }
   return s;
}

size_t vcdiff_stream_feed(vcdiff_stream_t *s, const uint8_t *chunk,
      size_t len)
{
   if (!s || !chunk || !len || s->failed)
      return 0;

   /* Retain the source: a later window may name any part of it. */
   if (s->src_seen + len > s->own_cap)
   {
      size_t   cap = s->own_cap ? s->own_cap : 65536;
      uint8_t *tmp;
      while (s->src_seen + len > cap)
      {
         if (cap > ((size_t)-1) / 2)
         {
            s->failed = 1;
            return 0;
         }
         cap <<= 1;
      }
      if (!(tmp = (uint8_t*)realloc(s->own_src, cap)))
      {
         s->failed = 1;
         return 0;
      }
      s->own_src = tmp;
      s->own_cap = cap;
   }
   memcpy(s->own_src + s->src_seen, chunk, len);
   s->owns_src = 1;
   s->src      = s->own_src;
   s->src_seen += len;

   if (!vcd_drain(s))
      return 0;
   return len;
}

bool vcdiff_stream_finish(vcdiff_stream_t *s, uint8_t **out,
      size_t *out_len)
{
   if (!s || !out || !out_len || s->failed)
      return false;

   /* A feed that stopped short is indistinguishable from a source that
    * is genuinely shorter, and the windows would decode against
    * whatever did arrive.  Refuse instead. */
   if (s->src_seen < s->src_len)
      return false;

   if (!vcd_drain(s) || s->p_off != s->patch_len)
      return false;

   *out     = s->out;
   *out_len = s->out_len;
   s->out   = NULL;
   return true;
}

void vcdiff_stream_free(vcdiff_stream_t *s)
{
   if (!s)
      return;
   free(s->out);
   free(s->own_src);
   free(s);
}

bool vcdiff_decode(const uint8_t *patch, size_t patch_len,
      const uint8_t *src, size_t src_len,
      uint8_t **out, size_t *out_len)
{
   struct vcdiff_stream *s;
   bool ok;

   if (!src && src_len)
      return false;
   if (!(s = vcdiff_stream_open(patch, patch_len, src_len)))
      return false;

   /* The whole source is already here, so point at the caller's buffer
    * rather than taking a copy of it - this path decodes patches
    * against multi-megabyte ROMs and the copy would cost more than the
    * decode. */
   s->src      = src;
   s->src_seen = src_len;

   ok = vcd_drain(s) && s->p_off == patch_len;
   if (ok)
   {
      *out     = s->out;
      *out_len = s->out_len;
      s->out   = NULL;
   }
   vcdiff_stream_free(s);
   return ok;
}

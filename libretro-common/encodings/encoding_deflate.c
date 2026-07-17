/* Copyright  (C) 2010-2024 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (encoding_deflate.c).
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


#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>

#include <retro_inline.h>
/* Byte-order source of truth for the word-compare first-difference logic
 * in rd_longest_match().  Do NOT sniff platform macros locally: newlib
 * and bionic define _BIG_ENDIAN as a byte-order *constant* on every
 * target, so testing defined(_BIG_ENDIAN) misfires on little-endian
 * platforms (and a bare MSB_FIRST define leaks into every later file in
 * single-TU griffin builds, tripping the LSB_FIRST/MSB_FIRST
 * consistency check in retro_endianness.h). */
#include <retro_endianness.h>
#include <encodings/deflate.h>

/* ===================== inflate (RFC 1951 / RFC 1950) ===================== */
/* Clean-room RFC 1951 (DEFLATE) / RFC 1950 (zlib) inflate.
 * Non-blocking, resumable: suspends when input is exhausted or output is
 * full and resumes on the next call, in the style of image_transfer. */

/* Decoder state-machine phases. */
enum rinf_phase
{
   RINF_ZHEADER = 0, /* consume 2-byte zlib header (wrapped mode)          */
   RINF_BLOCK_HDR,   /* read BFINAL + BTYPE                                */
   RINF_STORED_LEN,  /* stored block: read LEN/NLEN                        */
   RINF_STORED_DATA, /* stored block: copy literal bytes                  */
   RINF_DYN_TABLE,   /* dynamic block: read/build the huffman tables      */
   RINF_BLOCK_DATA,  /* huffman block: decode symbols                     */
   RINF_ADLER,       /* consume/verify adler32 trailer (wrapped mode)     */
   RINF_DONE
};

/* A canonical-huffman decode table.  We use a two-level scheme: a direct
 * lookup on the low FAST_BITS bits, and for codes longer than FAST_BITS a
 * small linear/step search via the canonical first-code arrays. */
#define RINF_FAST_BITS 9
#define RINF_MAX_BITS  15

struct rinf_huff
{
   /* fast[b] packs: (symbol<<4)|len for codes whose first FAST_BITS bits
    * (bit-reversed reading order) equal b and len<=FAST_BITS; len==0 means
    * "not a complete code, use the slow path". */
   uint16_t fast[1 << RINF_FAST_BITS];
   /* Canonical decode for the slow path. */
   uint16_t firstcode[RINF_MAX_BITS + 1];  /* first canonical code of len  */
   int      firstsym[RINF_MAX_BITS + 1];   /* symbol index of that code    */
   uint16_t maxcode[RINF_MAX_BITS + 2];    /* first code >= this is too big */
   uint8_t  size[288];                     /* code length per symbol       */
   uint16_t value[288];                    /* symbols in canonical order   */
   int      num_symbols;
};

struct rinflate
{
   enum rinf_phase phase;

   /* current input window */
   const uint8_t *in;
   size_t         in_size;
   size_t         in_pos;

   /* current output window */
   uint8_t       *out;
   size_t         out_size;
   size_t         out_pos;

   /* bit buffer (LSB-first per DEFLATE) */
   uint32_t       bitbuf;
   int            bitcnt;

   /* 32KB sliding window for back-references */
   uint8_t        window[32768];
   uint32_t       whave;   /* how many bytes are valid in the ring       */
   uint32_t       wnext;   /* next write position in the ring            */

   int            wrapped; /* zlib wrapper present?                      */
   int            bfinal;  /* current block is the last                  */
   int            btype;

   /* stored-block bookkeeping */
   uint32_t       stored_len;

   /* huffman tables for the current dynamic/fixed block */
   struct rinf_huff lencode;
   struct rinf_huff distcode;
   int              have_tables;

   /* dynamic-table construction scratch, persisted across suspends */
   int            hlit, hdist, hclen;
   struct rinf_huff clcode;
   uint8_t        cl_lengths[19];   /* code-length-code lengths            */
   uint8_t        lengths[288 + 32];
   int            lengths_have;
   int            clcodes_read;
   int            clcode_built;
   int            cl_pending_sym;  /* clcode sym awaiting its extra bits (-1 none) */

   /* symbol decode scratch (persist a pending copy across suspends) */
   uint32_t       copy_len;
   uint32_t       copy_dist;
   int            copy_active;

   /* explicit sub-state for the length/distance decode so we can suspend
    * between any two steps and resume without re-consuming bits */
   int            ld_step;     /* 0=decode len sym,1=len extra,2=dist sym,3=dist extra */
   int            ld_lensym;   /* decoded length symbol (257..285) - 257     */
   uint32_t       ld_length;   /* assembled length                          */
   int            ld_distsym;  /* decoded distance symbol                   */
   uint8_t        pending_lit; /* a literal awaiting output room            */
   int            have_pending_lit;

   uint32_t       adler;      /* running adler32 of the output           */
   uint32_t       adler_read; /* trailer value read so far               */
   int            adler_have;

   int            error;
};

/* --- adler32 (RFC 1950) --- */
#define ADLER_MOD 65521u
static uint32_t rinf_adler32_update(uint32_t adler,
      const uint8_t *buf, size_t len)
{
   uint32_t a = adler & 0xffff;
   uint32_t b = (adler >> 16) & 0xffff;
   while (len)
   {
      /* process in chunks so the sums never overflow before the modulo */
      size_t n = len > 5552 ? 5552 : len;
      len -= n;
      do {
         a += *buf++; b += a;
      } while (--n);
      a %= ADLER_MOD;
      b %= ADLER_MOD;
   }
   return (b << 16) | a;
}

/* --- bit reader helpers (LSB-first) --- */
/* Ensure at least n bits are available; returns 0 if input ran out. */
static int rinf_need(struct rinflate *s, int n)
{
   while (s->bitcnt < n)
   {
      if (s->in_pos >= s->in_size)
         return 0;
      s->bitbuf |= (uint32_t)s->in[s->in_pos++] << s->bitcnt;
      s->bitcnt += 8;
   }
   return 1;
}
static uint32_t rinf_getbits(struct rinflate *s, int n)
{
   uint32_t v = s->bitbuf & ((1u << n) - 1);
   s->bitbuf >>= n;
   s->bitcnt  -= n;
   return v;
}

/* Build a huffman decode table from an array of code lengths. */
static int rinf_build(struct rinf_huff *h, const uint8_t *lengths, int num)
{
   int i, k;
   int code, next_code[RINF_MAX_BITS + 1];
   int sizes[RINF_MAX_BITS + 1];

   memset(h->fast, 0, sizeof(h->fast));
   for (i = 0; i <= RINF_MAX_BITS; i++)
      sizes[i] = 0;
   for (i = 0; i < num; i++)
   {
      if (lengths[i] > RINF_MAX_BITS)
         return 0;
      sizes[lengths[i]]++;
   }
   sizes[0] = 0;

   /* over-subscribed / incomplete check per RFC (allow single-symbol) */
   {
      int left = 1;
      for (i = 1; i <= RINF_MAX_BITS; i++)
      {
         left <<= 1;
         left -= sizes[i];
         if (left < 0)
            return 0;
      }
      /* left > 0 means incomplete; permitted only in degenerate cases,
       * zlib accepts them for the distance table, so we allow it. */
   }

   code = 0;
   k    = 0;
   for (i = 1; i <= RINF_MAX_BITS; i++)
   {
      next_code[i]     = code;
      h->firstcode[i]  = (uint16_t)code;
      h->firstsym[i]   = k;
      code            += sizes[i];
      /* maxcode[i]: codes of length i are in [firstcode, firstcode+sizes) */
      h->maxcode[i]    = (uint16_t)code;
      code           <<= 1;
      k               += sizes[i];
   }
   h->maxcode[RINF_MAX_BITS + 1] = 0xffff;
   h->num_symbols = num;

   /* assign symbols in canonical order and populate the fast table */
   for (i = 0; i < num; i++)
   {
      int len = lengths[i];
      if (!len)
         continue;
      {
         int c         = next_code[len]++;
         int sym       = h->firstsym[len] + (c - h->firstcode[len]);
         h->size[sym]  = (uint8_t)len;
         h->value[sym] = (uint16_t)i;

         if (len <= RINF_FAST_BITS)
         {
            /* reverse the 'len' bits of c into reading order and splat
             * across all high-bit combinations */
            int j, rev = 0;
            for (j = 0; j < len; j++)
               rev |= ((c >> j) & 1) << (len - 1 - j);
            for (j = rev; j < (1 << RINF_FAST_BITS); j += (1 << len))
               h->fast[j] = (uint16_t)((i << 4) | len);
         }
      }
   }
   return 1;
}

/* Decode one symbol; returns -1 if not enough bits are buffered yet
 * (caller should suspend), otherwise the symbol. */
static int rinf_decode(struct rinflate *s, struct rinf_huff *h)
{
   int len, sym;
   uint32_t rev, cur;
   uint16_t f;

   /* Refill the bit buffer as full as it will go (up to 24 bits kept) in a
    * single pass, so the common case needs no further input reads. */
   while (s->bitcnt <= 24 && s->in_pos < s->in_size)
   {
      s->bitbuf |= (uint32_t)s->in[s->in_pos++] << s->bitcnt;
      s->bitcnt += 8;
   }

   f = h->fast[s->bitbuf & ((1 << RINF_FAST_BITS) - 1)];
   if (f)
   {
      len = f & 15;
      if (len <= s->bitcnt)
      {
         s->bitbuf >>= len;
         s->bitcnt  -= len;
         return f >> 4;
      }
      return -1; /* short code but not enough bits buffered yet */
   }

   /* slow path: codes longer than RINF_FAST_BITS */
   rev = 0;
   for (len = 1; len <= RINF_MAX_BITS; len++)
   {
      if (s->bitcnt < len)
      {
         if (!rinf_need(s, len))
            return -1;
      }
      rev = (rev << 1) | ((s->bitbuf >> (len - 1)) & 1);
      cur = rev;
      if (cur < h->maxcode[len])
      {
         sym = h->firstsym[len] + (cur - h->firstcode[len]);
         s->bitbuf >>= len;
         s->bitcnt  -= len;
         return h->value[sym];
      }
   }
   s->error = 1;
   return -2;
}

/* fixed huffman tables (RFC 1951 3.2.6) */
static void rinf_fixed_tables(struct rinflate *s)
{
   uint8_t ll[288], dd[30];
   int i;
   for (i = 0;   i < 144; i++)
      ll[i] = 8;
   for (i = 144; i < 256; i++)
      ll[i] = 9;
   for (i = 256; i < 280; i++)
      ll[i] = 7;
   for (i = 280; i < 288; i++)
      ll[i] = 8;
   for (i = 0;   i < 30;  i++)
      dd[i] = 5;
   rinf_build(&s->lencode, ll, 288);
   rinf_build(&s->distcode, dd, 30);
   s->have_tables = 1;
}

/* length/distance base + extra-bit tables */
static const uint16_t rinf_len_base[29] = {
   3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,
   67,83,99,115,131,163,195,227,258 };
static const uint8_t rinf_len_extra[29] = {
   0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
static const uint16_t rinf_dist_base[30] = {
   1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,
   1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
static const uint8_t rinf_dist_extra[30] = {
   0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };

/* order of code-length code lengths (RFC 1951 3.2.7) */
static const uint8_t rinf_clc_order[19] = {
   16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };

/* emit a byte to the output + window, updating adler */
static int rinf_emit(struct rinflate *s, uint8_t b)
{
   if (s->out_pos >= s->out_size)
      return 0; /* output full - suspend */
   s->out[s->out_pos++] = b;
   return 1;
}

/* Fetch one already-produced byte that lies `dist` bytes behind the current
 * output position, consulting the current output buffer first and then the
 * ring window of previously-flushed output. */
static uint8_t rinf_back(struct rinflate *s, uint32_t dist)
{
   if (dist > s->out_pos)
   {
      uint32_t back = dist - (uint32_t)s->out_pos; /* into prior output */
      uint32_t idx  = (s->wnext + 32768 - back) & 32767;
      return s->window[idx];
   }
   return s->out[s->out_pos - dist];
}


/* Snapshot the tail of the just-produced output into the ring window so the
 * next call's back-references into prior output resolve correctly.  Called
 * once per process() invocation rather than per byte. */
static void rinf_window_commit(struct rinflate *s)
{
   size_t n = s->out_pos;
   const uint8_t *src;
   if (n == 0)
      return;
   if (n > 32768)
   {
      src = s->out + (n - 32768);
      n   = 32768;
   }
   else
      src = s->out;
   /* append n bytes into the ring at wnext */
   {
      size_t i;
      for (i = 0; i < n; i++)
      {
         s->window[s->wnext++] = src[i];
         if (s->wnext == 32768) s->wnext = 0;
      }
   }
   s->whave += (uint32_t)n;
   if (s->whave > 32768) s->whave = 32768;
}

void *rinflate_new(int window_bits)
{
   struct rinflate *s = (struct rinflate*)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->wrapped = (window_bits >= 0);
   s->phase   = s->wrapped ? RINF_ZHEADER : RINF_BLOCK_HDR;
   s->adler   = 1;
   return s;
}

void rinflate_free(void *data) { free(data); }

void rinflate_set_in(void *data, const uint8_t *in, size_t size)
{
   struct rinflate *s = (struct rinflate*)data;
   s->in = in; s->in_size = size; s->in_pos = 0;
}
void rinflate_set_out(void *data, uint8_t *out, size_t size)
{
   struct rinflate *s = (struct rinflate*)data;
   s->out = out; s->out_size = size; s->out_pos = 0;
}

int rinflate_process(void *data, size_t *read, size_t *wrote)
{
   struct rinflate *s = (struct rinflate*)data;
   size_t in_start  = s->in_pos;
   size_t out_start = s->out_pos;
   size_t fold_start = s->out_pos;   /* adler fold cursor (wrapped mode)   */
   int status = RDEFLATE_PROCESS_NEXT;

   for (;;)
   {
      switch (s->phase)
      {
         case RINF_ZHEADER:
            if (!rinf_need(s, 16)) goto suspend;
            {
               uint32_t cmf = rinf_getbits(s, 8);
               uint32_t flg = rinf_getbits(s, 8);
               uint32_t hdr = (cmf << 8) | flg;
               if ((cmf & 0x0f) != 8) { s->error = 1; goto error; }
               if (hdr % 31 != 0)     { s->error = 1; goto error; }
               if (flg & 0x20)        { s->error = 1; goto error; } /* preset dict unsupported */
            }
            s->phase = RINF_BLOCK_HDR;
            break;

         case RINF_BLOCK_HDR:
            if (!rinf_need(s, 3)) goto suspend;
            s->bfinal = rinf_getbits(s, 1);
            s->btype  = rinf_getbits(s, 2);
            if (s->btype == 0)
            {
               /* stored: skip to byte boundary */
               s->bitbuf >>= (s->bitcnt & 7);
               s->bitcnt  -= (s->bitcnt & 7);
               s->phase = RINF_STORED_LEN;
            }
            else if (s->btype == 1)
            {
               rinf_fixed_tables(s);
               s->copy_active = 0;
               s->phase = RINF_BLOCK_DATA;
            }
            else if (s->btype == 2)
            {
               s->lengths_have = 0;
               s->clcodes_read = 0;
               s->hlit = s->hdist = s->hclen = -1;
               s->phase = RINF_DYN_TABLE;
            }
            else { s->error = 1; goto error; }
            break;

         case RINF_STORED_LEN:
            if (!rinf_need(s, 32)) goto suspend;
            {
               uint32_t len  = rinf_getbits(s, 16);
               uint32_t nlen = rinf_getbits(s, 16);
               if ((len ^ 0xffff) != nlen) { s->error = 1; goto error; }
               s->stored_len = len;
            }
            s->phase = RINF_STORED_DATA;
            break;

         case RINF_STORED_DATA:
            if (s->have_pending_lit)
            {
               if (!rinf_emit(s, s->pending_lit)) goto suspend;
               s->have_pending_lit = 0;
               s->stored_len--;
            }
            while (s->stored_len > 0)
            {
               uint8_t b;
               if (!rinf_need(s, 8)) goto suspend;
               b = (uint8_t)rinf_getbits(s, 8);
               if (!rinf_emit(s, b))
               {
                  /* output full: stash the already-decoded byte so we do
                   * not re-read it, and resume here next call */
                  s->pending_lit = b;
                  s->have_pending_lit = 1;
                  goto suspend;
               }
               s->stored_len--;
            }
            s->phase = s->bfinal ? (s->wrapped ? RINF_ADLER : RINF_DONE)
                                 : RINF_BLOCK_HDR;
            break;

         case RINF_DYN_TABLE:
            if (s->hlit < 0)
            {
               if (!rinf_need(s, 14)) goto suspend;
               s->hlit  = rinf_getbits(s, 5) + 257;
               s->hdist = rinf_getbits(s, 5) + 1;
               s->hclen = rinf_getbits(s, 4) + 4;
               s->clcodes_read = 0;
               s->clcode_built = 0;
               s->cl_pending_sym = -1;
               memset(s->lengths, 0, sizeof(s->lengths));
               memset(s->cl_lengths, 0, sizeof(s->cl_lengths));
            }
            /* read hclen code-length-code lengths */
            {
               while (s->clcodes_read < s->hclen)
               {
                  if (!rinf_need(s, 3)) goto suspend;
                  s->cl_lengths[rinf_clc_order[s->clcodes_read]] =
                     (uint8_t)rinf_getbits(s, 3);
                  s->clcodes_read++;
               }
               if (!s->clcode_built)
               {
                  if (!rinf_build(&s->clcode, s->cl_lengths, 19))
                     { s->error = 1; goto error; }
                  s->clcode_built = 1;
               }
            }
            /* read hlit+hdist code lengths using clcode */
            while (s->lengths_have < s->hlit + s->hdist)
            {
               int sym = s->cl_pending_sym;
               if (sym < 0)
               {
                  sym = rinf_decode(s, &s->clcode);
                  if (sym == -1) goto suspend;
                  if (sym < 0) { goto error; }
               }
               if (sym < 16)
               {
                  s->cl_pending_sym = -1;
                  s->lengths[s->lengths_have++] = (uint8_t)sym;
               }
               else
               {
                  int repeat, val = 0;
                  /* Remember the symbol so that, if we suspend waiting for
                   * its extra bits, we do not re-decode (and re-consume)
                   * on resume. */
                  s->cl_pending_sym = sym;
                  if (sym == 16)
                  {
                     if (s->lengths_have == 0) { s->error = 1; goto error; }
                     if (!rinf_need(s, 2)) goto suspend;
                     repeat = 3 + rinf_getbits(s, 2);
                     val = s->lengths[s->lengths_have - 1];
                  }
                  else if (sym == 17)
                  {
                     if (!rinf_need(s, 3)) goto suspend;
                     repeat = 3 + rinf_getbits(s, 3);
                  }
                  else /* 18 */
                  {
                     if (!rinf_need(s, 7)) goto suspend;
                     repeat = 11 + rinf_getbits(s, 7);
                  }
                  s->cl_pending_sym = -1;
                  if (s->lengths_have + repeat > s->hlit + s->hdist)
                     { s->error = 1; goto error; }
                  while (repeat--)
                     s->lengths[s->lengths_have++] = (uint8_t)val;
               }
            }
            /* build lit/len and dist tables */
            if (!rinf_build(&s->lencode, s->lengths, s->hlit))
               { s->error = 1; goto error; }
            if (!rinf_build(&s->distcode, s->lengths + s->hlit, s->hdist))
               { s->error = 1; goto error; }
            s->have_tables = 1;
            s->copy_active = 0;
            s->phase = RINF_BLOCK_DATA;
            break;

         case RINF_BLOCK_DATA:
            if (s->have_pending_lit)
            {
               if (!rinf_emit(s, s->pending_lit)) goto suspend;
               s->have_pending_lit = 0;
            }

            /* -------- fast inner loop --------
             * Runs while both input and output have comfortable margins so
             * that no single symbol can straddle a buffer boundary, letting
             * us skip the per-symbol suspend bookkeeping.  We keep at least
             * a few input bytes (max symbol: len code + 5 extra + dist code
             * + 13 extra ~ 48 bits) and enough output room for the longest
             * match (258 bytes) plus a literal.  We bail to the careful path
             * as soon as either margin gets tight. */
            while (!s->copy_active
                   && s->ld_step == 0
                   && s->in_pos + 8 <= s->in_size
                   && s->out_pos + 258 + 1 <= s->out_size)
            {
               int sym;
               /* refill: guaranteed input available */
               while (s->bitcnt <= 24)
               {
                  s->bitbuf |= (uint32_t)s->in[s->in_pos++] << s->bitcnt;
                  s->bitcnt += 8;
               }
               {
                  uint16_t f = s->lencode.fast[s->bitbuf & ((1 << RINF_FAST_BITS) - 1)];
                  if (f)
                  {
                     int l = f & 15;
                     s->bitbuf >>= l;
                     s->bitcnt  -= l;
                     sym = f >> 4;
                  }
                  else
                  {
                     sym = rinf_decode(s, &s->lencode);
                     if (sym < -1)
                        goto error;
                     /* Shouldn't happen given margin */
                     if (sym == -1)
                        break; 
                  }
               }
               if (sym < 256)
               {
                  s->out[s->out_pos++] = (uint8_t)sym;
                  continue;
               }
               if (sym == 256)
               {
                  s->phase = s->bfinal
                     ? (s->wrapped ? RINF_ADLER : RINF_DONE)
                     : RINF_BLOCK_HDR;
                  goto block_done;
               }
               {
                  int li = sym - 257, dsym, ei;
                  uint32_t length, dist;
                  if (li >= 29) { s->error = 1; goto error; }
                  ei = rinf_len_extra[li];
                  if (s->bitcnt < ei)
                  {
                     while (s->bitcnt <= 24 && s->in_pos < s->in_size)
                     { s->bitbuf |= (uint32_t)s->in[s->in_pos++] << s->bitcnt; s->bitcnt += 8; }
                  }
                  length = rinf_len_base[li] + (ei ? rinf_getbits(s, ei) : 0);
                  dsym = rinf_decode(s, &s->distcode);
                  if (dsym < 0)
                  {
                     if (dsym == -1)
                        break;
                     goto error;
                  }
                  if (dsym >= 30)
                  {
                     s->error = 1;
                     goto error;
                  }
                  ei = rinf_dist_extra[dsym];
                  if (s->bitcnt < ei)
                  {
                     while (s->bitcnt <= 24 && s->in_pos < s->in_size)
                     {
                        s->bitbuf |= (uint32_t)s->in[s->in_pos++] << s->bitcnt;
                        s->bitcnt += 8;
                     }
                  }
                  dist = rinf_dist_base[dsym] + (ei ? rinf_getbits(s, ei) : 0);
                  if (dist > s->out_pos + s->whave)
                  {
                     s->error = 1;
                     goto error;
                  }
                  /* copy the match */
                  if (dist <= s->out_pos)
                  {
                     uint8_t       *dst  = s->out + s->out_pos;
                     const uint8_t *srcp = dst - dist;
                     if (dist >= length)
                        memcpy(dst, srcp, length);
                     else if (dist == 1) /* run of a single byte */
                        memset(dst, srcp[0], length);
                     else
                     {
                        /* overlapping run: grow the copied region by
                         * doubling so we memcpy progressively larger blocks
                         * instead of one byte at a time */
                        uint32_t done = dist;
                        memcpy(dst, srcp, dist);
                        while (done < length)
                        {
                           uint32_t chunk = done;
                           if (chunk > length - done)
                              chunk = length - done;
                           memcpy(dst + done, dst, chunk);
                           done += chunk;
                        }
                     }
                     s->out_pos += length;
                  }
                  else
                  {
                     /* rare: reaches into prior-call output; use slow path */
                     s->copy_len   = length;
                     s->copy_dist  = dist;
                     s->copy_active = 1;
                     break;
                  }
               }
            }

            for (;;)
            {
               /* finish any pending back-reference copy first */
               if (s->copy_active)
               {
                  if (s->copy_dist <= s->out_pos)
                  {
                     /* Source lies within the current output buffer. */
                     while (s->copy_len > 0 && s->out_pos < s->out_size)
                     {
                        size_t room  = s->out_size - s->out_pos;
                        size_t chunk = s->copy_len < room ? s->copy_len : room;
                        uint8_t       *dst  = s->out + s->out_pos;
                        const uint8_t *srcp = s->out + s->out_pos - s->copy_dist;
                        if (s->copy_dist >= chunk)
                           memcpy(dst, srcp, chunk);
                        else
                        {
                           size_t k;
                           for (k = 0; k < chunk; k++)
                              dst[k] = srcp[k];
                        }
                        s->out_pos  += chunk;
                        s->copy_len -= (uint32_t)chunk;
                     }
                     if (s->copy_len > 0) goto suspend;
                  }
                  else
                  {
                     /* Source starts in previously-flushed output (ring). */
                     while (s->copy_len > 0)
                     {
                        uint8_t b;
                        if (s->copy_dist > s->out_pos + s->whave)
                           { s->error = 1; goto error; }
                        b = rinf_back(s, s->copy_dist);
                        if (!rinf_emit(s, b)) goto suspend;
                        s->copy_len--;
                     }
                  }
                  s->copy_active = 0;
                  s->ld_step = 0;
               }
               if (s->ld_step == 0)
               {
                  int sym = rinf_decode(s, &s->lencode);
                  if (sym == -1)
                     goto suspend;
                  if (sym < 0)
                     goto error;
                  if (sym < 256)
                  {
                     if (!rinf_emit(s, (uint8_t)sym))
                     {
                        s->pending_lit      = (uint8_t)sym;
                        s->have_pending_lit = 1;
                        goto suspend;
                     }
                     continue;
                  }
                  if (sym == 256)
                  {
                     s->phase = s->bfinal
                        ? (s->wrapped ? RINF_ADLER : RINF_DONE)
                        : RINF_BLOCK_HDR;
                     break;
                  }
                  s->ld_lensym = sym - 257;
                  if (s->ld_lensym >= 29) { s->error = 1; goto error; }
                  s->ld_step = 1;
               }

               if (s->ld_step == 1)
               {
                  int ei = rinf_len_extra[s->ld_lensym];
                  if (ei && !rinf_need(s, ei)) goto suspend;
                  s->ld_length = rinf_len_base[s->ld_lensym]
                     + (ei ? rinf_getbits(s, ei) : 0);
                  s->ld_step = 2;
               }

               if (s->ld_step == 2)
               {
                  int dsym = rinf_decode(s, &s->distcode);
                  if (dsym == -1) goto suspend;
                  if (dsym < 0 || dsym >= 30) { s->error = 1; goto error; }
                  s->ld_distsym = dsym;
                  s->ld_step = 3;
               }

               if (s->ld_step == 3)
               {
                  int ei = rinf_dist_extra[s->ld_distsym];
                  if (ei && !rinf_need(s, ei)) goto suspend;
                  s->copy_dist  = rinf_dist_base[s->ld_distsym]
                     + (ei ? rinf_getbits(s, ei) : 0);
                  s->copy_len   = s->ld_length;
                  s->copy_active = 1;
                  s->ld_step = 0;
               }
            }
block_done:
            break;

         case RINF_ADLER:
            /* fold any output produced in this call before comparing */
            if (s->out_pos > fold_start)
            {
               s->adler = rinf_adler32_update(s->adler,
                     s->out + fold_start, s->out_pos - fold_start);
               fold_start = s->out_pos; /* don't re-fold at the done label */
            }
            /* align to byte, then read 4 bytes big-endian */
            s->bitbuf >>= (s->bitcnt & 7);
            s->bitcnt  -= (s->bitcnt & 7);
            while (s->adler_have < 4)
            {
               if (!rinf_need(s, 8)) goto suspend;
               s->adler_read = (s->adler_read << 8) | rinf_getbits(s, 8);
               s->adler_have++;
            }
            if (s->adler_read != s->adler) { s->error = 1; goto error; }
            s->phase = RINF_DONE;
            break;

         case RINF_DONE:
            status = RDEFLATE_PROCESS_END;
            goto done;
      }
   }

suspend:
   status = RDEFLATE_PROCESS_NEXT;
done:
   rinf_window_commit(s);
   if (s->wrapped && s->out_pos > fold_start)
      s->adler = rinf_adler32_update(s->adler,
            s->out + fold_start, s->out_pos - fold_start);
   if (read)  *read  = s->in_pos  - in_start;
   if (wrote) *wrote = s->out_pos - out_start;
   return status;

error:
   if (s->wrapped && s->out_pos > fold_start)
      s->adler = rinf_adler32_update(s->adler,
            s->out + fold_start, s->out_pos - fold_start);
   if (read)
      *read  = s->in_pos  - in_start;
   if (wrote)
      *wrote = s->out_pos - out_start;
   return RDEFLATE_PROCESS_ERROR;
}


/* ===================== deflate (RFC 1951 / RFC 1950) ===================== */
/* Clean-room RFC 1951 (DEFLATE) / RFC 1950 (zlib) deflate.
 * Non-blocking, resumable, in the style of image_transfer.
 *
 * Strategy: this is a single-pass streaming compressor.  It buffers input
 * into a 32 KiB-window-plus-lookahead history, finds matches with hash
 * chains + lazy evaluation, accumulates a block's worth of symbols, then
 * emits the block with whichever of stored / fixed / dynamic Huffman is
 * smallest.  Output is produced through a bit writer that can suspend when
 * the caller's output buffer fills. */



#define RD_WINDOW      32768
#define RD_WSIZE       RD_WINDOW
#define RD_WMASK       (RD_WINDOW - 1)
#define RD_MIN_MATCH   3
#define RD_MAX_MATCH   258
#define RD_HASH_BITS   15
#define RD_HASH_SIZE   (1 << RD_HASH_BITS)
#define RD_HASH_MASK   (RD_HASH_SIZE - 1)
/* symbols accumulated per block before we flush */
#define RD_BLOCK_SYMS  16384

/* one deferred symbol: either a literal (dist==0) or a match */
struct rd_sym
{
   uint8_t  lit;    /* literal byte, or length-256 code base for matches  */
   uint8_t  extra;  /* unused padding                                     */
   uint16_t dist;   /* 0 = literal; else match distance                   */
   uint16_t len;    /* match length (only if dist != 0)                   */
};

struct rdeflate
{
   int      level;
   int      wrapped;
   int      good, lazy, nice, chain;  /* match-finder tuning per level     */

   /* input history: a linear buffer holding the sliding window + lookahead.
    * We keep it simple: accumulate into `win`, slide when it fills. */
   uint8_t  win[RD_WINDOW * 2 + 8];  /* +8: over-read margin for word cmp */
   uint32_t win_len;    /* valid bytes in win                              */
   uint32_t pos;        /* current parse position within win               */
   uint32_t block_start;/* start of the not-yet-emitted region             */
   uint32_t strstart;   /* == pos, kept for clarity                        */

   /* hash chains */
   int32_t  head[RD_HASH_SIZE];
   int32_t  prev[RD_WINDOW];
   uint32_t hash;

   /* deferred symbol buffer for the current block */
   struct rd_sym syms[RD_BLOCK_SYMS];
   uint32_t nsyms;
   uint32_t fixed_bits_acc;   /* running fixed-Huffman bit count for block */
   uint32_t freq_lit[286];
   uint32_t freq_dist[30];

   /* lazy-match state */
   int      have_prev;
   uint32_t prev_len;
   uint32_t prev_dist;
   uint32_t prev_lit;

   /* bit writer */
   uint64_t bitbuf;
   int      bitcnt;

   /* current output window */
   uint8_t *out;
   size_t   out_size;
   size_t   out_pos;

   /* current input window */
   const uint8_t *in;
   size_t   in_size;
   size_t   in_pos;

   uint32_t adler;
   int      final_in;    /* caller signalled end of input                  */
   int      done;
   int      error;

   /* emit state machine (for suspendable block output) */
   int      emit_phase;
   uint32_t sym_cursor;   /* next symbol index to emit within the block    */
   int      block_final;  /* is the block being emitted the last one       */
   int      trailer_cursor;

   /* cached fixed-Huffman codes */
   uint16_t fix_lit_code[288];
   uint8_t  fix_lit_len[288];
   uint16_t fix_dist_code[30];
   uint8_t  fix_dist_len[30];
   int      fixed_ready;
   int      use_stored;
   int      use_dynamic;
   int      emitting;
   int      header_done;

   /* dynamic Huffman tables for the current block */
   uint8_t  dyn_lit_len[288];
   uint16_t dyn_lit_code[288];
   uint8_t  dyn_dist_len[30];
   uint16_t dyn_dist_code[30];
   int      dyn_hlit, dyn_hdist, dyn_hclen;
   uint8_t  dyn_cl_len[19];       /* code-length-code lengths               */
   uint16_t dyn_cl_code[19];
   uint8_t  dyn_rle[288 + 30];    /* RLE'd (lit+dist) code lengths           */
   uint8_t  dyn_rle_extra[288 + 30];
   int      dyn_rle_n;
};

/* ------- adler32 ------- */
#define RD_ADLER_MOD 65521u
static uint32_t rd_adler32(uint32_t adler, const uint8_t *buf, size_t len)
{
   uint32_t a = adler & 0xffff;
   uint32_t b = (adler >> 16) & 0xffff;
   while (len)
   {
      size_t n = len > 5552 ? 5552 : len;
      len -= n;
      do { a += *buf++; b += a; } while (--n);
      a %= RD_ADLER_MOD;
      b %= RD_ADLER_MOD;
   }
   return (b << 16) | a;
}

/* ------- bit writer (LSB-first) -------
 * Bits accumulate in bitbuf; whole bytes are flushed to the output window.
 * If the output window fills mid-flush, the caller must provide more room
 * and call process() again; bitbuf/bitcnt persist across calls. */
/* Drain whole bytes from the 64-bit bit buffer into the output.  Returns 0
 * if the output filled before the buffer was drained below 8 bits (i.e. the
 * caller should suspend and resume with a fresh output buffer).  The bits
 * that remain in bitbuf/bitcnt persist across calls. */
static int rd_flush_bytes(struct rdeflate *s)
{
   int nbytes = s->bitcnt >> 3;
   if (s->out_pos + (size_t)nbytes <= s->out_size)
   {
      uint8_t *dst = s->out + s->out_pos;
      while (nbytes-- > 0)
      {
         *dst++ = (uint8_t)(s->bitbuf & 0xff);
         s->bitbuf >>= 8;
         s->bitcnt  -= 8;
      }
      s->out_pos = dst - s->out;
      return 1;
   }
   while (s->bitcnt >= 8)
   {
      if (s->out_pos >= s->out_size)
         return 0;
      s->out[s->out_pos++] = (uint8_t)(s->bitbuf & 0xff);
      s->bitbuf >>= 8;
      s->bitcnt  -= 8;
   }
   return 1;
}

/* Accumulate n bits (n <= 32) into the bit buffer.  This never suspends: the
 * 64-bit buffer always has room for one symbol's worth of bits between
 * flushes.  We opportunistically drain whole bytes when the buffer gets
 * full enough that another putbits could overflow, but ignore a full output
 * here -- the emit loop flushes explicitly at symbol boundaries where a
 * suspend can be cleanly resumed. */
/* Accumulate up to 32 bits.  Pure buffer write, no draining -- the 64-bit
 * buffer holds a full symbol.  The caller drains between symbols. */
static INLINE void rd_putbits(struct rdeflate *s, uint32_t val, int n)
{
   s->bitbuf |= ((uint64_t)(val & ((n >= 32) ? 0xffffffffu
                                             : ((1u << n) - 1)))) << s->bitcnt;
   s->bitcnt += n;
}

/* Drain whole bytes from the buffer, assuming output has room (caller has
 * checked the margin).  bitcnt drops below 8. */
static INLINE void rd_drain_fast(struct rdeflate *s)
{
   uint8_t *dst = s->out + s->out_pos;
   while (s->bitcnt >= 8)
   {
      *dst++ = (uint8_t)(s->bitbuf & 0xff);
      s->bitbuf >>= 8;
      s->bitcnt  -= 8;
   }
   s->out_pos = (size_t)(dst - s->out);
}
/* align to a byte boundary (pad with zero bits) */
static int rd_align(struct rdeflate *s)
{
   if (s->bitcnt & 7)
      rd_putbits(s, 0, 8 - (s->bitcnt & 7));
   return rd_flush_bytes(s);
}

/* ------- length/distance coding tables (RFC 1951) ------- */
static const uint16_t rd_len_base[29] = {
   3,4,5,6,7,8,9,10,11,13,15,17,19,23,27,31,35,43,51,59,
   67,83,99,115,131,163,195,227,258 };
static const uint8_t rd_len_extra[29] = {
   0,0,0,0,0,0,0,0,1,1,1,1,2,2,2,2,3,3,3,3,4,4,4,4,5,5,5,5,0 };
static const uint16_t rd_dist_base[30] = {
   1,2,3,4,5,7,9,13,17,25,33,49,65,97,129,193,257,385,513,769,
   1025,1537,2049,3073,4097,6145,8193,12289,16385,24577 };
static const uint8_t rd_dist_extra[30] = {
   0,0,0,0,1,1,2,2,3,3,4,4,5,5,6,6,7,7,8,8,9,9,10,10,11,11,12,12,13,13 };

/* O(1) symbol lookups via precomputed tables (built at first use). */
static uint8_t  rd_length_code[259];      /* len 0..258 -> length symbol   */
static uint8_t  rd_dist_code_lo[256];     /* dist 1..256 -> dist symbol     */
static uint8_t  rd_dist_code_hi[256];     /* (dist-1)>>7 for 257..32768     */
static int      rd_code_tables_ready = 0;

static void rd_init_code_tables(void)
{
   int i, code, dist, len;
   if (rd_code_tables_ready)
      return;
   /* length codes */
   code = 0;
   for (len = 3; len <= 258; len++)
   {
      while (code < 28 && len >= rd_len_base[code + 1])
         code++;
      rd_length_code[len] = (uint8_t)code;
   }
   rd_length_code[0] = 0; rd_length_code[1] = 0; rd_length_code[2] = 0;
   /* distance codes: low half (1..256) direct */
   code = 0;
   for (dist = 1; dist <= 256; dist++)
   {
      while (code < 29 && dist >= rd_dist_base[code + 1])
         code++;
      rd_dist_code_lo[dist - 1] = (uint8_t)code;
   }
   /* distance codes: high half indexed by (dist-1)>>7 for 257..32768 */
   code = 0;
   for (i = 0; i < 256; i++)
   {
      dist = (i << 7) + 1;   /* representative distance for this bucket     */
      while (code < 29 && dist >= rd_dist_base[code + 1])
         code++;
      rd_dist_code_hi[i] = (uint8_t)code;
   }
   rd_code_tables_ready = 1;
}

static int rd_len_sym(uint32_t len)
{
   return rd_length_code[len];
}

static int rd_dist_sym(uint32_t dist)
{
   if (dist <= 256)
      return rd_dist_code_lo[dist - 1];
   return rd_dist_code_hi[(dist - 1) >> 7];
}

/* ------- canonical Huffman code construction (encoder side) ------- */
/* Given code lengths, produce the canonical codes (bit-reversed for output,
 * since DEFLATE writes Huffman codes MSB-first within the LSB-first stream). */
static uint16_t rd_bitrev(uint16_t code, int len)
{
   int i;
   uint16_t r = 0;
   for (i = 0; i < len; i++)
   {
      r = (uint16_t)((r << 1) | (code & 1));
      code = (uint16_t)(code >> 1);
   }
   return r;
}
static void rd_codes_from_lengths(const uint8_t *lens, int n,
      uint16_t *codes)
{
   int   bl_count[16];
   int   next_code[16];
   int   bits, i;
   int   code = 0;
   for (i = 0; i < 16; i++)
      bl_count[i] = 0;
   for (i = 0; i < n; i++)
      bl_count[lens[i]]++;
   bl_count[0] = 0;
   for (bits = 1; bits < 16; bits++)
   {
      code = (code + bl_count[bits - 1]) << 1;
      next_code[bits] = code;
   }
   for (i = 0; i < n; i++)
   {
      int l = lens[i];
      if (l)
      {
         uint16_t c = (uint16_t)next_code[l]++;
         codes[i] = rd_bitrev(c, l);   /* store reversed, ready to emit */
      }
      else
         codes[i] = 0;
   }
}

/* fixed Huffman code lengths (RFC 1951 3.2.6) */
static void rd_fixed_lit_lengths(uint8_t *ll)
{
   int i;
   for (i = 0;   i < 144; i++)
      ll[i] = 8;
   for (i = 144; i < 256; i++)
      ll[i] = 9;
   for (i = 256; i < 280; i++)
      ll[i] = 7;
   for (i = 280; i < 288; i++)
      ll[i] = 8;
}

/* ------- match finder (hash chains + lazy) ------- */
#define RD_HASH_SHIFT ((RD_HASH_BITS + RD_MIN_MATCH - 1) / RD_MIN_MATCH)
/* Hardware CRC32 hash where available: it spreads 3-byte keys across the
 * table far more evenly than the multiplicative hash, which shortens the
 * per-bucket chains the match finder has to walk.  Falls back to the
 * multiplicative hash on targets without a CRC32 instruction. */
#if defined(__SSE4_2__)
#include <nmmintrin.h>
#define RD_HAVE_CRC32_HASH 1
static uint32_t rd_hash(const uint8_t *p)
{
   uint32_t k = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
   return _mm_crc32_u32(0, k) & RD_HASH_MASK;
}
#elif defined(__ARM_FEATURE_CRC32)
#include <arm_acle.h>
#define RD_HAVE_CRC32_HASH 1
static uint32_t rd_hash(const uint8_t *p)
{
   uint32_t k = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
   return __crc32w(0, k) & RD_HASH_MASK;
}
#else
static uint32_t rd_hash(const uint8_t *p)
{
   return (uint32_t)(((p[0] << (2*RD_HASH_SHIFT)) ^
                      (p[1] <<   RD_HASH_SHIFT)  ^
                       p[2]) & RD_HASH_MASK);
}
#endif

/* insert position `pos` into the hash chain; return previous head */
static int32_t rd_insert(struct rdeflate *s, uint32_t pos)
{
   uint32_t h = rd_hash(s->win + pos);
   int32_t  prev = s->head[h];
   s->prev[pos & RD_WMASK] = prev;
   s->head[h] = (int32_t)pos;
   return prev;
}

/* find the longest match for the string at `pos`, searching the chain.
 * Returns match length (>=MIN_MATCH) and sets *dist, or 0 if none. */
/* count-trailing / leading zero bits on a 64-bit word, for the word-at-a-time
 * match compare.  Uses compiler builtins where available. */
#if defined(__GNUC__) || defined(__clang__)
static INLINE int rd_ctz64(uint64_t x) { return __builtin_ctzll(x); }
static INLINE int rd_clz64(uint64_t x) { return __builtin_clzll(x); }
#else
static INLINE int rd_ctz64(uint64_t x)
{
   int n = 0;
   while (!(x & 1)) { x >>= 1; n++; }
   return n;
}
static INLINE int rd_clz64(uint64_t x)
{
   int n = 0;
   while (!(x & ((uint64_t)1 << 63))) { x <<= 1; n++; }
   return n;
}
#endif

static INLINE uint32_t rd_longest_match(struct rdeflate *s, uint32_t pos,
      uint32_t max_len, uint32_t best_start, uint32_t *dist_out)
{
   int      chain;
   uint32_t limit;
   uint16_t scan_end;   /* the two bytes scan[best_len-1..best_len]         */
   const uint8_t *win  = s->win;
   const uint8_t *scan = win + pos;
   uint32_t best_len   = best_start;   /* only a longer match is interesting */
   uint32_t best_dist  = 0;
   /* The caller has already inserted `pos`; begin at the previous
    * occurrence recorded in prev[].  Empty chain -> no match. */
   int32_t cur         = s->prev[pos & RD_WMASK];
   if (cur < 0)
      return 0;

   {
      const int32_t *prev = s->prev;   /* hoisted: avoids repeated struct load */
      chain = s->chain;
      /* good_match heuristic (zlib): if we already carry a decent match into
       * this search, spend proportionally less effort looking for a better one.
       * Evaluated once here, never in the inner loop. */
      if (best_len >= (uint32_t)s->good)
         chain >>= 2;
      limit = pos > RD_WINDOW ? pos - RD_WINDOW : 0;
      /* Load the two bytes bracketing the current best as a single 16-bit value
       * so the per-candidate quick reject is one 16-bit load + compare instead
       * of two byte loads + compares.  Endianness does not matter: we only test
       * equality of the same two bytes on both sides. */
      memcpy(&scan_end, scan + best_len - 1, 2);

      while (cur >= 0 && (uint32_t)cur >= limit && chain-- > 0)
      {
         const uint8_t *m = win + cur;
         uint16_t m_end;
         /* Quick reject: a candidate can only beat best_len if the two bytes
          * bracketing the current best both match.  Compared as one 16-bit
          * load, this rejects the vast majority of candidates cheaply. */
         memcpy(&m_end, m + best_len - 1, 2);
         if (m_end != scan_end)
         {
            cur = prev[cur & RD_WMASK];
            continue;
         }
         {
            /* Compare 8 bytes at a time: load a word from each side, XOR, and
             * on a mismatch locate the first differing byte from the low end of
             * the XOR (which corresponds to the first byte in memory order on
             * little-endian; on big-endian we take the high end).  This turns a
             * long match compare into ~max_len/8 iterations instead of max_len.
             * We already know scan[0]==m[0]. */
            const uint8_t *sc  = scan;
            const uint8_t *mp  = m;
            uint32_t lim8 = max_len & ~7u;
            uint32_t l = 0;
            while (l < lim8)
            {
               uint64_t a, b, x;
               memcpy(&a, sc + l, 8);
               memcpy(&b, mp + l, 8);
               x = a ^ b;
               if (x != 0)
               {
#if RETRO_IS_BIG_ENDIAN
                  /* first differing byte is the most-significant nonzero byte */
                  l += (uint32_t)(rd_clz64(x) >> 3);
#else
                  /* first differing byte is the least-significant nonzero byte */
                  l += (uint32_t)(rd_ctz64(x) >> 3);
#endif
                  goto have_len;
               }
               l += 8;
            }
            while (l < max_len && sc[l] == mp[l])
               l++;
have_len:
            if (l > max_len)
               l = max_len;
            if (l > best_len)
            {
               best_len  = l;
               best_dist = pos - (uint32_t)cur;
               if (l >= max_len || l >= (uint32_t)s->nice)
                  break;
               memcpy(&scan_end, scan + best_len - 1, 2);
            }
         }
         cur = prev[cur & RD_WMASK];
      }
   }
   /* best_dist stays 0 until an actual candidate beats best_len; when the
    * caller seeded best_len with a pending match, a zero dist means nothing
    * longer was found, so report no (new) match. */
   if (best_dist != 0 && best_len >= RD_MIN_MATCH)
   {
      *dist_out = best_dist;
      return best_len;
   }
   return 0;
}

/* ------- emit accumulated symbols as one fixed-Huffman block ------- */
/* returns 0 if suspended (output full), 1 when the whole block is written.
 * emit_phase and an index cursor let us resume. */

static void rd_build_fixed(struct rdeflate *s)
{
   uint8_t dl[30];
   int i;
   if (s->fixed_ready)
      return;
   rd_fixed_lit_lengths(s->fix_lit_len);
   rd_codes_from_lengths(s->fix_lit_len, 288, s->fix_lit_code);
   for (i = 0; i < 30; i++) dl[i] = 5;
   memcpy(s->fix_dist_len, dl, 30);
   rd_codes_from_lengths(dl, 30, s->fix_dist_code);
   s->fixed_ready = 1;
}

/* Emit one symbol (literal or match) with the fixed Huffman codes.
 * Returns 0 if the output filled (suspend), 1 on success. */
static void rd_emit_sym_fixed(struct rdeflate *s, const struct rd_sym *y)
{
   if (y->dist == 0)
      rd_putbits(s, s->fix_lit_code[y->lit], s->fix_lit_len[y->lit]);
   else
   {
      int ls = rd_len_sym(y->len);
      int lc = 257 + ls;
      int ds = rd_dist_sym(y->dist);
      rd_putbits(s, s->fix_lit_code[lc], s->fix_lit_len[lc]);
      if (rd_len_extra[ls])
         rd_putbits(s, y->len - rd_len_base[ls], rd_len_extra[ls]);
      rd_putbits(s, s->fix_dist_code[ds], s->fix_dist_len[ds]);
      if (rd_dist_extra[ds])
         rd_putbits(s, y->dist - rd_dist_base[ds], rd_dist_extra[ds]);
   }
}

/* Emit the accumulated block using fixed Huffman.  Suspendable via
 * emit_phase / sym_cursor.  Phases:
 *   0 = write block header (BFINAL + BTYPE=01)
 *   1 = write symbols[sym_cursor..]
 *   2 = write end-of-block (256)
 *   3 = done
 */
static int rd_emit_block_fixed(struct rdeflate *s)
{
   rd_build_fixed(s);
   if (s->emit_phase == 0)
   {
      rd_putbits(s, (uint32_t)(s->block_final ? 1 : 0), 1);
      rd_putbits(s, 1, 2);   /* BTYPE = 01 fixed */
      s->sym_cursor = 0;
      s->emit_phase = 1;
   }
   if (s->emit_phase == 1)
   {
      if (!rd_flush_bytes(s))
         return 0;
      /* Each symbol emits at most ~48 bits (6 bytes); keeping an 8-byte
       * output margin lets us emit and drain without per-byte bounds checks.
       * When the margin is gone, fall back to the careful path so we can
       * suspend cleanly at a symbol boundary. */
      while (s->sym_cursor < s->nsyms)
      {
         if (s->out_pos + 8 <= s->out_size)
         {
            rd_emit_sym_fixed(s, &s->syms[s->sym_cursor]);
            s->sym_cursor++;
            rd_drain_fast(s);
         }
         else
         {
            rd_emit_sym_fixed(s, &s->syms[s->sym_cursor]);
            s->sym_cursor++;
            if (!rd_flush_bytes(s))
               return 0;
         }
      }
      s->emit_phase = 2;
   }
   if (s->emit_phase == 2)
   {
      if (!rd_flush_bytes(s)) /* Drain last symbol */
         return 0;   
      rd_putbits(s, s->fix_lit_code[256], s->fix_lit_len[256]);
      s->emit_phase = 3;
   }
   if (s->emit_phase == 3)
   {
      if (!rd_flush_bytes(s)) /* drain EOB */
         return 0;
      s->emit_phase = 4;                  /* done sentinel */
   }
   return 1;
}

/* per-level match-finder tuning: {good, lazy, nice, chain}.  Mirrors the
 * shape of zlib's table (not the exact values). level 0 = store only. */
static void rd_set_level(struct rdeflate *s)
{
   switch (s->level <= 0 ? 0 : (s->level > 9 ? 9 : s->level))
   {
      case 0: s->good=0;  s->lazy=0;   s->nice=0;   s->chain=0;    break;
      case 1: s->good=4;  s->lazy=0;   s->nice=8;   s->chain=4;    break;
      case 2: s->good=4;  s->lazy=0;   s->nice=16;  s->chain=8;    break;
      case 3: s->good=4;  s->lazy=0;   s->nice=24;  s->chain=16;   break;
      case 4: s->good=4;  s->lazy=4;   s->nice=16;  s->chain=16;   break;
      case 5: s->good=8;  s->lazy=16;  s->nice=32;  s->chain=32;   break;
      case 6: s->good=8;  s->lazy=16;  s->nice=128; s->chain=128;  break;
      case 7: s->good=8;  s->lazy=32;  s->nice=128; s->chain=256;  break;
      case 8: s->good=32; s->lazy=128; s->nice=258; s->chain=512; break;
      default:s->good=32; s->lazy=258; s->nice=258; s->chain=4096; break;
   }
}

/* record a literal / match into the current block's symbol buffer */
/* fixed-Huffman code length for a literal/length symbol index (RFC 1951). */
static INLINE uint32_t rd_fixed_litlen_bits(int sym)
{
   if (sym < 144)
      return 8;
   if (sym < 256)
      return 9;
   if (sym < 280)
      return 7;
   return 8;
}
static void rd_record_lit(struct rdeflate *s, uint8_t c)
{
   struct rd_sym *y = &s->syms[s->nsyms++];
   y->lit = c; y->dist = 0; y->len = 0;
   s->freq_lit[c]++;
   s->fixed_bits_acc += (c < 144) ? 8u : 9u;
}
static void rd_record_match(struct rdeflate *s, uint32_t len, uint32_t dist)
{
   int ls = rd_len_sym(len);
   int ds = rd_dist_sym(dist);
   struct rd_sym *y = &s->syms[s->nsyms++];
   y->dist = (uint16_t)dist; y->len = (uint16_t)len;
   s->freq_lit[257 + ls]++;
   s->freq_dist[ds]++;
   /* fixed table: length codes 257..279 are 7 bits, 280..287 are 8 bits;
    * distance codes are always 5 bits.  Add the extra bits too. */
   s->fixed_bits_acc += rd_fixed_litlen_bits(257 + ls) + rd_len_extra[ls]
                      + 5u + rd_dist_extra[ds];
}

void rdeflate_set_in(void *data, const uint8_t *in, size_t size)
{
   struct rdeflate *s = (struct rdeflate*)data;
   s->in = in; s->in_size = size; s->in_pos = 0;
}
void rdeflate_set_out(void *data, uint8_t *out, size_t size)
{
   struct rdeflate *s = (struct rdeflate*)data;
   s->out = out; s->out_size = size; s->out_pos = 0;
}
/* tell the encoder no more input will follow after the current buffer */
void rdeflate_finish(void *data)
{
   struct rdeflate *s = (struct rdeflate*)data;
   s->final_in = 1;
}

/* ------- the parser: consume win[block_start..win_len) into symbols ------- */
/* Fills the symbol buffer using greedy/lazy matching.  Leaves s->pos at the
 * first unprocessed byte.  Stops early if the symbol buffer is near full.
 * Requires that all input is already in win (single-shot model for now). */
static void rd_parse(struct rdeflate *s)
{
   uint32_t end = s->win_len;
   if (s->level == 0)
   {
      /* store: no matching, everything is a literal (block chooser will
       * pick a stored block for these) */
      while (s->pos < end && s->nsyms < RD_BLOCK_SYMS - 2)
         rd_record_lit(s, s->win[s->pos++]);
      return;
   }

   /* Fast greedy path (no lazy) for low levels: mirrors zlib deflate_fast.
    * Lower per-byte overhead since there is no deferred-match state. */
   if (s->lazy == 0)
   {
      while (s->pos < end)
      {
         uint32_t max_len;
         uint32_t mlen = 0, mdist = 0;
         if (s->nsyms >= RD_BLOCK_SYMS - 2)
            break;
         max_len = end - s->pos;
         if (max_len > RD_MAX_MATCH) max_len = RD_MAX_MATCH;
         if (max_len >= RD_MIN_MATCH)
         {
            /* insert returns the previous head of this hash chain; only
             * bother searching when the chain is non-empty. */
            if (rd_insert(s, s->pos) >= 0)
               mlen = rd_longest_match(s, s->pos, max_len,
                     (uint32_t)(RD_MIN_MATCH - 1), &mdist);
         }
         if (mlen >= RD_MIN_MATCH)
         {
            uint32_t stop = s->pos + mlen, q;
            rd_record_match(s, mlen, mdist);
            /* Insert interior positions so future matches can reference them,
             * but for long matches skip most interiors to save time (a small
             * ratio cost that pays off in speed at fast levels). */
            if (mlen <= 32)
            {
               for (q = s->pos + 1; q < stop; q++)
                  if (q + RD_MIN_MATCH <= end)
                     rd_insert(s, q);
            }
            else
            {
               /* hash only a few positions near the start of the match */
               uint32_t lim = s->pos + 8;
               for (q = s->pos + 1; q < lim && q < stop; q++)
                  if (q + RD_MIN_MATCH <= end)
                     rd_insert(s, q);
            }
            s->pos = stop;
         }
         else
         {
            rd_record_lit(s, s->win[s->pos]);
            s->pos++;
         }
      }
      return;
   }

   /* Lazy matching, modeled on the classic zlib deflate_slow structure.
    * match_available means we have a pending literal (prev byte) whose fate
    * depends on whether the next position yields a longer match. */
   while (s->pos < end)
   {
      uint32_t max_len;
      uint32_t mlen = 0, mdist = 0;

      if (s->nsyms >= RD_BLOCK_SYMS - 2)
         break;

      max_len = end - s->pos;
      if (max_len > RD_MAX_MATCH) max_len = RD_MAX_MATCH;

      /* insert the current position into the hash chain and find a match */
      if (max_len >= RD_MIN_MATCH)
      {
         rd_insert(s, s->pos);
         /* When a match is pending from the previous byte, only a strictly
          * longer match here matters -- start the search bar at prev_len. */
         mlen = rd_longest_match(s, s->pos, max_len,
               s->have_prev ? s->prev_len : (uint32_t)(RD_MIN_MATCH - 1),
               &mdist);
      }

      if (s->have_prev)
      {
         /* a match (prev_len/prev_dist) was found at pos-1 and deferred */
         if (mlen > s->prev_len)
         {
            /* longer match here: emit pos-1 as literal, defer this one */
            rd_record_lit(s, (uint8_t)s->prev_lit);
            s->prev_len  = mlen;
            s->prev_dist = mdist;
            s->prev_lit  = s->win[s->pos];
            s->pos++;
         }
         else
         {
            /* take the deferred match at pos-1 */
            uint32_t plen = s->prev_len;
            uint32_t pdist = s->prev_dist;
            uint32_t stop, q;
            s->have_prev = 0;
            rd_record_match(s, plen, pdist);
            /* insert the interior positions the match covers.  pos-1 is
             * already inserted; pos is already inserted (this iteration).
             * Insert pos+1 .. (pos-1)+plen-1. */
            stop = (s->pos - 1) + plen;
            for (q = s->pos + 1; q < stop; q++)
               if (q + RD_MIN_MATCH <= end)
                  rd_insert(s, q);
            s->pos = stop;
         }
      }
      else if (mlen >= RD_MIN_MATCH)
      {
         if (mlen >= (uint32_t)s->nice || (uint32_t)s->lazy == 0
             || mlen > (uint32_t)s->lazy)
         {
            /* strong enough: take immediately */
            uint32_t stop, q;
            rd_record_match(s, mlen, mdist);
            stop = s->pos + mlen;
            for (q = s->pos + 1; q < stop; q++)
               if (q + RD_MIN_MATCH <= end)
                  rd_insert(s, q);
            s->pos = stop;
         }
         else
         {
            /* defer one byte for lazy evaluation */
            s->have_prev = 1;
            s->prev_len  = mlen;
            s->prev_dist = mdist;
            s->prev_lit  = s->win[s->pos];
            s->pos++;
         }
      }
      else
      {
         rd_record_lit(s, s->win[s->pos]);
         s->pos++;
      }
   }

   /* flush a deferred match/literal at true end of input */
   if (s->final_in && s->pos >= end && s->have_prev
       && s->nsyms < RD_BLOCK_SYMS - 2)
   {
      s->have_prev = 0;
      rd_record_match(s, s->prev_len, s->prev_dist);
   }
}

/* ------- stored (uncompressed) block emitter ------- */
/* Emits win[block_start .. pos) as one or more stored blocks. Suspendable. */
static int rd_emit_block_stored(struct rdeflate *s)
{
   /* phase 0: header+LEN/NLEN, phase 1: raw bytes */
   uint32_t total = s->pos - s->block_start;
   if (s->emit_phase == 0)
   {
      uint32_t len = total;
      if (len > 65535) len = 65535;
      rd_putbits(s, (uint32_t)(s->block_final && len == total ? 1 : 0), 1);
      rd_putbits(s, 0, 2);       /* BTYPE=00 */
      /* pad to a byte boundary (bits, no flush needed for correctness) */
      if (s->bitcnt & 7)
         rd_putbits(s, 0, 8 - (s->bitcnt & 7));
      rd_putbits(s, len & 0xff, 8);
      rd_putbits(s, (len >> 8) & 0xff, 8);
      rd_putbits(s, (~len) & 0xff, 8);
      rd_putbits(s, ((~len) >> 8) & 0xff, 8);
      s->sym_cursor = s->block_start;           /* reuse cursor as byte idx */
      s->trailer_cursor = (int)len;             /* bytes left in this block  */
      s->emit_phase = 1;
   }
   if (s->emit_phase == 1)
   {
      /* drain the buffered header bytes before copying raw data */
      if (!rd_flush_bytes(s))
         return 0;
      s->emit_phase = 2;
   }
   if (s->emit_phase == 2)
   {
      while (s->trailer_cursor > 0)
      {
         if (s->out_pos >= s->out_size)
            return 0;
         s->out[s->out_pos++] = s->win[s->sym_cursor++];
         s->trailer_cursor--;
      }
      s->emit_phase = 3;
   }
   return 1;
}

/* ------- length-limited Huffman code lengths -------
 * Optimal prefix-code lengths for the given frequencies, capped at max_bits,
 * always yielding a complete (Kraft-exact) code.  Builds a Huffman tree by
 * repeated lowest-weight sibling merges, reads off depths, then repairs any
 * over-long codes with a Kraft-sum redistribution. */
static void rd_gen_lengths(const uint32_t *freq, int n, int max_bits,
      uint8_t *lengths_out)
{
   int      idx[288];
   uint32_t fr[288];
   int      lc[288];
   int      m = 0;
   int      i;

   for (i = 0; i < n; i++)
      lengths_out[i] = 0;
   for (i = 0; i < n; i++)
      if (freq[i]) { idx[m] = i; fr[m] = freq[i]; m++; }

   if (m == 0)
      return;
   if (m == 1)
   {
      lengths_out[idx[0]] = 1;
      return;
   }

   /* sort symbols by frequency ascending (insertion sort; m <= 288) */
   for (i = 1; i < m; i++)
   {
      int      ti = idx[i];
      uint32_t tf = fr[i];
      int      j  = i - 1;
      while (j >= 0 && fr[j] > tf)
      {
         fr[j + 1]  = fr[j];
         idx[j + 1] = idx[j];
         j--;
      }
      fr[j + 1]  = tf;
      idx[j + 1] = ti;
   }

   /* build the Huffman tree via a sorted-array min-heap of sibling merges */
   {
      uint32_t wt[2 * 288];
      int      dad[2 * 288];
      int      heap[288 + 1];
      int      hn = 0;
      int      node_used;

      for (i = 0; i < m; i++)
      {
         wt[i]      = fr[i];
         dad[i]     = -1;
         heap[hn++] = i;
      }
      node_used = m;
      while (hn > 1)
      {
         int x = heap[0], y = heap[1];
         int nd = node_used++;
         int ins, t;
         wt[nd]  = wt[x] + wt[y];
         dad[x]  = nd; dad[y] = nd; dad[nd] = -1;
         for (i = 2; i < hn; i++)
            heap[i - 2] = heap[i];
         hn -= 2;
         ins = hn;
         for (i = 0; i < hn; i++)
            if (wt[heap[i]] > wt[nd])
            {
               ins = i;
               break;
            }
         for (t = hn; t > ins; t--)
            heap[t] = heap[t - 1];
         heap[ins] = nd; hn++;
      }
      for (i = 0; i < m; i++)
      {
         int d = 0, c = i;
         while (dad[c] >= 0)
         {
            c = dad[c];
            d++;
         }
         lc[i] = d ? d : 1;
      }
   }

   /* enforce max_bits via Kraft-sum redistribution */
   {
      int      bl[64];
      int      over = 0;
      for (i = 0; i <= 63; i++)
         bl[i] = 0;
      for (i = 0; i < m; i++)
         if (lc[i] > max_bits) over = 1;
      if (over)
      {
         uint32_t one   = 1u << max_bits;
         uint32_t kraft = 0;
         for (i = 0; i < m; i++)
         {
            if (lc[i] > max_bits)
               lc[i] = max_bits;
            bl[lc[i]]++;
         }
         for (i = 0; i < m; i++)
            kraft += one >> lc[i];
         while (kraft > one)
         {
            int l2 = max_bits - 1;
            while (l2 > 0 && bl[l2] == 0)
               l2--;
            if (l2 == 0)
               break;
            bl[l2]--; bl[l2 + 1]++;
            for (i = 0; i < m; i++)
               if (lc[i] == l2) { lc[i] = l2 + 1; break; }
            kraft -= one >> (l2 + 1);
         }
         while (kraft < one)
         {
            int l2 = max_bits;
            while (l2 > 0 && bl[l2] == 0) l2--;
            if (l2 == 0)
               break;
            for (i = 0; i < m; i++)
               if (lc[i] == l2) { lc[i] = l2 - 1; break; }
            bl[l2]--; bl[l2 - 1]++;
            kraft += one >> l2;
         }
      }
   }

   for (i = 0; i < m; i++)
      lengths_out[idx[i]] = (uint8_t)lc[i];
}

/* order in which CL code lengths are written (RFC 1951 3.2.7) */
static const uint8_t rd_clc_order[19] = {
   16,17,18,0,8,7,9,6,10,5,11,4,12,3,13,2,14,1,15 };

/* RLE-encode the concatenated lit(hlit) + dist(hdist) code lengths into the
 * code-length alphabet (0-15 literal, 16=copy prev 3-6, 17=zero 3-10,
 * 18=zero 11-138).  Fills dyn_rle / dyn_rle_extra, sets dyn_rle_n, and
 * accumulates CL-symbol frequencies. */
static void rd_rle_lengths(struct rdeflate *s, uint32_t *cl_freq)
{
   uint8_t all[288 + 30];
   int total = s->dyn_hlit + s->dyn_hdist;
   int i = 0, n = 0;
   int k;
   for (k = 0; k < s->dyn_hlit; k++)  all[k] = s->dyn_lit_len[k];
   for (k = 0; k < s->dyn_hdist; k++) all[s->dyn_hlit + k] = s->dyn_dist_len[k];
   for (k = 0; k < 19; k++) cl_freq[k] = 0;

   while (i < total)
   {
      int len = all[i];
      int run = 1;
      while (i + run < total && all[i + run] == len)
         run++;
      if (len == 0)
      {
         while (run >= 11)
         {
            int rep = run > 138 ? 138 : run;
            s->dyn_rle[n] = 18; s->dyn_rle_extra[n] = (uint8_t)(rep - 11); n++;
            cl_freq[18]++;
            run -= rep; i += rep;
         }
         while (run >= 3)
         {
            int rep = run > 10 ? 10 : run;
            s->dyn_rle[n] = 17; s->dyn_rle_extra[n] = (uint8_t)(rep - 3); n++;
            cl_freq[17]++;
            run -= rep; i += rep;
         }
         while (run > 0)
         {
            s->dyn_rle[n] = 0; s->dyn_rle_extra[n] = 0; n++;
            cl_freq[0]++;
            run--; i++;
         }
      }
      else
      {
         /* emit the literal length once, then 16-repeats for the rest */
         s->dyn_rle[n] = (uint8_t)len; s->dyn_rle_extra[n] = 0; n++;
         cl_freq[len]++;
         run--; i++;
         while (run >= 3)
         {
            int rep = run > 6 ? 6 : run;
            s->dyn_rle[n] = 16; s->dyn_rle_extra[n] = (uint8_t)(rep - 3); n++;
            cl_freq[16]++;
            run -= rep; i += rep;
         }
         while (run > 0)
         {
            s->dyn_rle[n] = (uint8_t)len; s->dyn_rle_extra[n] = 0; n++;
            cl_freq[len]++;
            run--; i++;
         }
      }
   }
   s->dyn_rle_n = n;
}

/* Build the dynamic Huffman tables from the block's symbol frequencies.
 * Returns the estimated size in bits of a dynamic block (for the chooser). */
static uint32_t rd_build_dynamic(struct rdeflate *s)
{
   uint32_t cl_freq[19];
   int i, maxlit, maxdist;
   uint32_t bits = 0;

   /* the end-of-block symbol (256) always occurs once */
   s->freq_lit[256]++;

   rd_gen_lengths(s->freq_lit, 286, 15, s->dyn_lit_len);
   rd_gen_lengths(s->freq_dist, 30, 15, s->dyn_dist_len);

   /* hlit: number of lit/len codes (257..286); hdist: dist codes (1..30) */
   maxlit = 285;
   while (maxlit > 256 && s->dyn_lit_len[maxlit] == 0) maxlit--;
   s->dyn_hlit = maxlit + 1;
   if (s->dyn_hlit < 257) s->dyn_hlit = 257;

   maxdist = 29;
   while (maxdist > 0 && s->dyn_dist_len[maxdist] == 0) maxdist--;
   /* at least one distance code must be present */
   s->dyn_hdist = maxdist + 1;
   if (s->dyn_hdist < 1) s->dyn_hdist = 1;

   rd_codes_from_lengths(s->dyn_lit_len, 288, s->dyn_lit_code);
   rd_codes_from_lengths(s->dyn_dist_len, 30, s->dyn_dist_code);

   rd_rle_lengths(s, cl_freq);
   rd_gen_lengths(cl_freq, 19, 7, s->dyn_cl_len);
   rd_codes_from_lengths(s->dyn_cl_len, 19, s->dyn_cl_code);

   /* hclen: number of CL code lengths present (in clc_order), min 4 */
   s->dyn_hclen = 19;
   while (s->dyn_hclen > 4 && s->dyn_cl_len[rd_clc_order[s->dyn_hclen - 1]] == 0)
      s->dyn_hclen--;

   /* estimate size in bits: header + CL lengths + RLE stream + data */
   bits = 3 + 5 + 5 + 4;             /* BFINAL,BTYPE,HLIT,HDIST,HCLEN(=14) */
   bits += 3 * s->dyn_hclen;
   for (i = 0; i < s->dyn_rle_n; i++)
   {
      int sym = s->dyn_rle[i];
      bits += s->dyn_cl_len[sym];
      if (sym == 16) bits += 2;
      else if (sym == 17) bits += 3;
      else if (sym == 18) bits += 7;
   }
   /* data payload */
   for (i = 0; i < 286; i++)
      if (s->freq_lit[i])
      {
         bits += s->freq_lit[i] * s->dyn_lit_len[i];
         if (i >= 257) bits += s->freq_lit[i] * rd_len_extra[i - 257];
      }
   for (i = 0; i < 30; i++)
      if (s->freq_dist[i])
         bits += s->freq_dist[i] * (s->dyn_dist_len[i] + rd_dist_extra[i]);
   return bits;
}

/* emit one symbol using the dynamic tables */
static void rd_emit_sym_dynamic(struct rdeflate *s, const struct rd_sym *y)
{
   if (y->dist == 0)
   {
      rd_putbits(s, s->dyn_lit_code[y->lit], s->dyn_lit_len[y->lit]);
      return;
   }
   {
      int ls = rd_len_sym(y->len);
      int lc = 257 + ls;
      int ds = rd_dist_sym(y->dist);
      rd_putbits(s, s->dyn_lit_code[lc], s->dyn_lit_len[lc]);
      if (rd_len_extra[ls])
         rd_putbits(s, y->len - rd_len_base[ls], rd_len_extra[ls]);
      rd_putbits(s, s->dyn_dist_code[ds], s->dyn_dist_len[ds]);
      if (rd_dist_extra[ds])
         rd_putbits(s, y->dist - rd_dist_base[ds], rd_dist_extra[ds]);
   }
}

/* Emit the dynamic block. Phases:
 * 0 hdr, 1 HLIT/HDIST/HCLEN, 2 CL lengths, 3 RLE stream, 4 symbols, 5 EOB */
static int rd_emit_block_dynamic(struct rdeflate *s)
{
   if (s->emit_phase == 0)
   {
      rd_putbits(s, (uint32_t)(s->block_final ? 1 : 0), 1);
      rd_putbits(s, 2, 2);    /* BTYPE=10 dynamic */
      rd_putbits(s, (uint32_t)(s->dyn_hlit - 257), 5);
      rd_putbits(s, (uint32_t)(s->dyn_hdist - 1), 5);
      rd_putbits(s, (uint32_t)(s->dyn_hclen - 4), 4);
      s->sym_cursor = 0;
      s->emit_phase = 2;
   }
   if (s->emit_phase == 2)
   {
      if (!rd_flush_bytes(s))
         return 0;
      while (s->sym_cursor < (uint32_t)s->dyn_hclen)
      {
         rd_putbits(s, s->dyn_cl_len[rd_clc_order[s->sym_cursor]], 3);
         s->sym_cursor++;
         if (!rd_flush_bytes(s))
            return 0;
      }
      s->sym_cursor = 0;
      s->emit_phase = 3;
   }
   if (s->emit_phase == 3)
   {
      if (!rd_flush_bytes(s))
         return 0;
      while (s->sym_cursor < (uint32_t)s->dyn_rle_n)
      {
         int sym = s->dyn_rle[s->sym_cursor];
         rd_putbits(s, s->dyn_cl_code[sym], s->dyn_cl_len[sym]);
         if (sym == 16)
            rd_putbits(s, s->dyn_rle_extra[s->sym_cursor], 2);
         else if (sym == 17)
            rd_putbits(s, s->dyn_rle_extra[s->sym_cursor], 3);
         else if (sym == 18)
            rd_putbits(s, s->dyn_rle_extra[s->sym_cursor], 7);
         s->sym_cursor++;
         if (!rd_flush_bytes(s))
            return 0;
      }
      s->sym_cursor = 0;
      s->emit_phase = 4;
   }
   if (s->emit_phase == 4)
   {
      if (!rd_flush_bytes(s))
         return 0;
      while (s->sym_cursor < s->nsyms)
      {
         if (s->out_pos + 8 <= s->out_size)
         {
            rd_emit_sym_dynamic(s, &s->syms[s->sym_cursor]);
            s->sym_cursor++;
            rd_drain_fast(s);
         }
         else
         {
            rd_emit_sym_dynamic(s, &s->syms[s->sym_cursor]);
            s->sym_cursor++;
            if (!rd_flush_bytes(s))
               return 0;
         }
      }
      rd_putbits(s, s->dyn_lit_code[256], s->dyn_lit_len[256]);
      s->emit_phase = 6;
   }
   if (s->emit_phase == 6)
   {
      if (!rd_flush_bytes(s))
         return 0;   /* drain EOB */
      s->emit_phase = 7;
   }
   return 1;
}

/* placeholder: implemented incrementally */
void *rdeflate_new(int level, int window_bits)
{
   struct rdeflate *s = (struct rdeflate*)calloc(1, sizeof(*s));
   if (!s)
      return NULL;
   s->level   = level;
   s->wrapped = (window_bits >= 0);
   s->adler   = 1;
   rd_init_code_tables();
   rd_set_level(s);
   {
      int i;
      for (i = 0; i < RD_HASH_SIZE; i++)
         s->head[i] = -1;
   }
   return s;
}

void rdeflate_free(void *p)
{
   free(p);
}

/* Choose whether the current block should be stored or fixed-Huffman, then
 * emit it.  Returns 0 if suspended, 1 when fully emitted. */
static int rd_emit_current_block(struct rdeflate *s)
{
   /* decide once, before we start emitting (emit_phase 0) */
   if (s->emit_phase == 0)
   {
      s->use_stored    = 0;
      s->use_dynamic   = 0;
      if (s->level == 0)
         s->use_stored = 1;
      else
      {
         uint32_t total       = s->pos - s->block_start;
         /* fixed-block size: accumulated during parse (no re-walk), plus the
          * 3-bit block header and the 7-bit end-of-block code. */
         uint32_t fixed_bits  = 3 + 7 + s->fixed_bits_acc;
         /* dynamic-block size in bits (also builds the tables) */
         uint32_t dyn_bits    = rd_build_dynamic(s);
         /* stored-block size in bits: header + align + 4 + payload */
         uint32_t stored_bits = 3 + 8 + 32 + total * 8;  /* upper bound */

         if (stored_bits <= fixed_bits && stored_bits <= dyn_bits)
            s->use_stored = 1;
         else if (dyn_bits <= fixed_bits)
            s->use_dynamic = 1;
      }
   }
   if (s->use_stored)
      return rd_emit_block_stored(s);
   if (s->use_dynamic)
      return rd_emit_block_dynamic(s);
   return rd_emit_block_fixed(s);
}

int rdeflate_process(void *data, size_t *read, size_t *wrote)
{
   struct rdeflate *s = (struct rdeflate*)data;
   size_t in_start    = s->in_pos;
   size_t out_start   = s->out_pos;

   /* 0) zlib wrapper header (CMF/FLG) once, at stream start */
   if (s->wrapped && !s->header_done)
   {
      /* CM=8 (deflate), CINFO=7 (32K window) -> CMF=0x78; FLG chosen so that
       * (CMF*256+FLG) % 31 == 0 and no preset dict. 0x78 0x9C is the common
       * "default compression" pair. */
      if (s->out_pos + 2 > s->out_size)
         goto suspend;
      s->out[s->out_pos++] = 0x78;
      s->out[s->out_pos++] = 0x9c;
      s->header_done       = 1;
   }

   /* 1) ingest available input into the window */
   if (s->in_pos < s->in_size)
   {
      size_t n = s->in_size - s->in_pos;
      /* window full: parse what we have */
      if (s->win_len + n > sizeof(s->win))
         n = sizeof(s->win) - s->win_len;   
      memcpy(s->win + s->win_len, s->in + s->in_pos, n);
      if (s->wrapped)
         s->adler = rd_adler32(s->adler, s->in + s->in_pos, n);
      s->win_len += (uint32_t)n;
      s->in_pos  += n;
   }

   /* We only emit once we know the block is complete: either the symbol
    * buffer is full, the window is full, or input has finished. */
   for (;;)
   {
      /* Resume emitting a block that was mid-output */
      if (s->emitting)
      {
         if (!rd_emit_current_block(s))
            goto suspend;

         /* Block fully emitted: reset for next block */
         s->emitting       = 0;
         s->emit_phase     = 0;
         s->block_start    = s->pos;
         s->nsyms          = 0;
         s->fixed_bits_acc = 0;
         {
            int i;
            for (i = 0; i < 286; i++)
               s->freq_lit[i] = 0;
            for (i = 0; i < 30; i++)
               s->freq_dist[i] = 0;
         }
         if (s->block_final)
         {
            s->emit_phase = 10;   /* move to trailer */
            break;
         }
         /* slide the window down by RD_WINDOW once we've moved past it, so
          * fresh input has room.  All absolute positions shift by RD_WINDOW. */
         if (s->pos >= RD_WINDOW)
         {
            uint32_t slide = RD_WINDOW;
            int i;
            memmove(s->win, s->win + slide, s->win_len - slide);
            s->win_len     -= slide;
            s->pos         -= slide;
            s->block_start -= slide;
            /* rebase hash chains: drop entries that fall below the window */
            for (i = 0; i < RD_HASH_SIZE; i++)
               s->head[i] = (s->head[i] >= (int32_t)slide)
                  ? s->head[i] - (int32_t)slide : -1;
            for (i = 0; i < RD_WINDOW; i++)
               s->prev[i] = (s->prev[i] >= (int32_t)slide)
                  ? s->prev[i] - (int32_t)slide : -1;
         }
         /* top the window back up from any remaining caller input so a
          * single process() call can consume an arbitrarily large input
          * (needed for the one-shot trans_stream_trans_full path). */
         if (s->in_pos < s->in_size && s->win_len < sizeof(s->win))
         {
            size_t n = s->in_size - s->in_pos;
            if (s->win_len + n > sizeof(s->win))
               n = sizeof(s->win) - s->win_len;
            memcpy(s->win + s->win_len, s->in + s->in_pos, n);
            if (s->wrapped)
               s->adler = rd_adler32(s->adler, s->in + s->in_pos, n);
            s->win_len += (uint32_t)n;
            s->in_pos  += n;
         }
         continue;
      }

      /* parse more input into symbols */
      rd_parse(s);

      /* decide if a block is ready to emit */
      {
         int block_ready = 0;
         int is_final    = 0;
         if (s->nsyms >= RD_BLOCK_SYMS - 4)
            block_ready  = 1;
         /* window full and fully parsed but more input remains: flush a
          * non-final block so we can slide the window and continue. */
         if (s->win_len >= sizeof(s->win) && s->pos >= s->win_len)
            block_ready  = 1;
         if (s->final_in && s->in_pos >= s->in_size && s->pos >= s->win_len)
         {
            block_ready  = 1;
            is_final     = 1;
         }
         if (!block_ready) /* need more input */
            goto suspend;   
         s->block_final  = is_final;
         s->emitting     = 1;
         s->emit_phase   = 0;
      }
   }

   /* trailer: flush bits + (wrapped) adler32, byte-aligned big-endian */
   if (s->emit_phase == 10)
   {
      if (!rd_align(s))
         goto suspend;
      s->emit_phase = 11;
   }
   if (s->emit_phase == 11)
   {
      if (s->wrapped)
      {
         while (s->trailer_cursor < 4)
         {
            uint32_t byte = (s->adler >> (24 - 8*s->trailer_cursor)) & 0xff;
            if (s->out_pos >= s->out_size)
               goto suspend;
            s->out[s->out_pos++] = (uint8_t)byte;
            s->trailer_cursor++;
         }
      }
      s->emit_phase = 12;
      s->done = 1;
   }

suspend:
   if (read)
      *read  = s->in_pos  - in_start;
   if (wrote)
      *wrote = s->out_pos - out_start;
   if (s->error)
      return RDEFLATE_PROCESS_ERROR;
   if (s->done)
      return RDEFLATE_PROCESS_END;
   return RDEFLATE_PROCESS_NEXT;
}

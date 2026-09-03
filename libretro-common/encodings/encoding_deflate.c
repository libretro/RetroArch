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
#include <compat/intrinsics.h>
/* Byte-order source of truth for the word-compare first-difference logic
 * in rd_longest_match().  Do NOT sniff platform macros locally: newlib
 * and bionic define _BIG_ENDIAN as a byte-order *constant* on every
 * target, so testing defined(_BIG_ENDIAN) misfires on little-endian
 * platforms (and a bare MSB_FIRST define leaks into every later file in
 * single-TU griffin builds, tripping the LSB_FIRST/MSB_FIRST
 * consistency check in retro_endianness.h). */
#include <retro_endianness.h>
#include <encodings/crc32.h>
#include <encodings/deflate.h>

/* ---------------- adler32 (RFC 1950), shared by both halves --------------
 *
 * Both the inflate and the deflate side of this file need adler32 over a
 * byte range, so the kernel lives here rather than being written twice.
 *
 * The textbook loop is
 *
 *    for each byte:  a += *buf++;  b += a;
 *
 * whose b += a carries a serial dependency across every single byte.  Over
 * a block of N bytes the same result is
 *
 *    a' = a + SUM(x[j])
 *    b' = b + N*a + SUM((N-j)*x[j])
 *
 * two independent reductions with constant weights, which is the form that
 * vectorises.  Accumulating across blocks rather than per block keeps even
 * the N*a term out of the loop: with vs1 the running SUM of bytes, vps the
 * running SUM of vs1 sampled before each block, and vs2 the running SUM of
 * the per-block weighted sums, after K blocks of 16
 *
 *    a' = a + SUM(vs1)
 *    b' = b + 16*K*a + 16*SUM(vps) + SUM(vs2)
 *
 * so the loop body is three vector adds and no cross-iteration scalar work
 * at all.
 *
 * This is written by hand rather than left to the auto-vectoriser, which
 * cannot be relied on for it: GCC takes the shape in one of the two
 * adler32 sites in this file and declines the byte-identical other one
 * ("loop nest containing two or more consecutive inner loops cannot be
 * vectorized"), and clang declines both at -O2 and -O3.  Doing it
 * explicitly also buys what the vectoriser will not reach for - psadbw
 * for the unweighted sum, where the generic form pays an unpack and a
 * pmaddwd instead.
 *
 * RD_ADLER_NMAX is the classic 5552 - the largest run for which b cannot
 * overflow 32 bits - and every intermediate above stays inside it too.
 * Worst case is 5552 bytes of 0xff seeded at the largest legal adler
 * (a = b = 65520), which is 347 blocks of 16:
 *
 *    16*SUM(vps)  0xE994_8100   (the binding term)
 *    16*K*a         363767040
 *    SUM(vs2)        12033960
 *    b                  65520
 *    -------------------------
 *    total        0xFFFB_C598   of 0xFFFF_FFFF, 277095 to spare
 *
 * so do not raise NMAX.  ktest covers exactly this input. */

#define RD_ADLER_MODULUS 65521u
#define RD_ADLER_NMAX    5552u   /* 347 blocks of 16 */

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) \
   || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#include <emmintrin.h>
#define RD_ADLER_SIMD 1

static INLINE uint32_t rd_adler_hsum(__m128i v)
{
   v = _mm_add_epi32(v, _mm_shuffle_epi32(v, _MM_SHUFFLE(1, 0, 3, 2)));
   v = _mm_add_epi32(v, _mm_shuffle_epi32(v, _MM_SHUFFLE(2, 3, 0, 1)));
   return (uint32_t)_mm_cvtsi128_si32(v);
}

/* Consume `blocks` whole 16-byte blocks. */
static void rd_adler_blocks(uint32_t *pa, uint32_t *pb,
      const uint8_t *buf, size_t blocks)
{
   const __m128i zero = _mm_setzero_si128();
   const __m128i wlo  = _mm_setr_epi16(16, 15, 14, 13, 12, 11, 10, 9);
   const __m128i whi  = _mm_setr_epi16( 8,  7,  6,  5,  4,  3,  2, 1);
   __m128i vs1        = zero;
   __m128i vps        = zero;
   __m128i vs2        = zero;
   uint32_t a         = *pa;
   uint32_t b         = *pb;
   size_t n           = blocks;

   while (n--)
   {
      __m128i v = _mm_loadu_si128((const __m128i*)buf);
      /* sampled before the update, so vps ends up holding
       * SUM over blocks i of (SUM of bytes in blocks < i) */
      vps = _mm_add_epi32(vps, vs1);
      /* psadbw: two 64-bit sums of 8 bytes each, no unpack needed */
      vs1 = _mm_add_epi32(vs1, _mm_sad_epu8(v, zero));
      vs2 = _mm_add_epi32(vs2,
            _mm_add_epi32(
               _mm_madd_epi16(_mm_unpacklo_epi8(v, zero), wlo),
               _mm_madd_epi16(_mm_unpackhi_epi8(v, zero), whi)));
      buf += 16;
   }

   *pb = b + (uint32_t)(blocks << 4) * a
           + (rd_adler_hsum(vps) << 4)
           +  rd_adler_hsum(vs2);
   *pa = a + rd_adler_hsum(vs1);
}

#elif defined(__ARM_NEON) || defined(__ARM_NEON__) || defined(HAVE_NEON)
#include <arm_neon.h>
#define RD_ADLER_SIMD 1

static INLINE uint32_t rd_adler_hsum(uint32x4_t v)
{
   uint32x2_t s = vadd_u32(vget_low_u32(v), vget_high_u32(v));
   s            = vpadd_u32(s, s);
   return vget_lane_u32(s, 0);
}

static void rd_adler_blocks(uint32_t *pa, uint32_t *pb,
      const uint8_t *buf, size_t blocks)
{
   static const uint8_t w[16] = {
      16, 15, 14, 13, 12, 11, 10, 9, 8, 7, 6, 5, 4, 3, 2, 1
   };
   const uint8x16_t vw = vld1q_u8(w);
   uint32x4_t vs1      = vdupq_n_u32(0);
   uint32x4_t vps      = vdupq_n_u32(0);
   uint32x4_t vs2      = vdupq_n_u32(0);
   uint32_t a          = *pa;
   uint32_t b          = *pb;
   size_t n            = blocks;

   while (n--)
   {
      uint8x16_t v = vld1q_u8(buf);
      uint16x8_t p;
      vps = vaddq_u32(vps, vs1);
      vs1 = vpadalq_u16(vs1, vpaddlq_u8(v));
      /* lane j holds w[j]*x[j] + w[j+8]*x[j+8] <= 6120, so the u16
       * product vector cannot overflow before it is widened */
      p   = vmull_u8(vget_low_u8(v),  vget_low_u8(vw));
      p   = vmlal_u8(p, vget_high_u8(v), vget_high_u8(vw));
      vs2 = vpadalq_u16(vs2, p);
      buf += 16;
   }

   *pb = b + (uint32_t)(blocks << 4) * a
           + (rd_adler_hsum(vps) << 4)
           +  rd_adler_hsum(vs2);
   *pa = a + rd_adler_hsum(vs1);
}

#else   /* no SIMD: same decomposition, 32 bytes at a time.
         *
         * In its own function rather than inline in rd_adler_run: inline,
         * the group loop and the byte tail read as two consecutive inner
         * loops, a nest GCC's vectoriser refuses outright, so it would be
         * emitted scalar on targets whose intrinsics are simply not
         * covered above.  Correct either way. */
#define RD_ADLER_BLOCK 32

static void rd_adler_blocks(uint32_t *pa, uint32_t *pb,
      const uint8_t *buf, size_t blocks)
{
   uint32_t a = *pa;
   uint32_t b = *pb;
   while (blocks--)
   {
      uint32_t sum  = 0;
      uint32_t wsum = 0;
      unsigned j;
      for (j = 0; j < 32; j++)
      {
         sum  += buf[j];
         wsum += (uint32_t)(32 - j) * buf[j];
      }
      b   += 32 * a + wsum;
      a   += sum;
      buf += 32;
   }
   *pa = a;
   *pb = b;
}
#endif

#ifndef RD_ADLER_BLOCK
#define RD_ADLER_BLOCK 16
#endif

/* Fold up to RD_ADLER_NMAX bytes into (a, b) with no reduction inside. */
static void rd_adler_run(uint32_t *pa, uint32_t *pb,
      const uint8_t *buf, size_t n)
{
   uint32_t a = *pa;
   uint32_t b = *pb;
   if (n >= RD_ADLER_BLOCK)
   {
      size_t blocks = n / RD_ADLER_BLOCK;
      size_t got    = blocks * RD_ADLER_BLOCK;
      rd_adler_blocks(&a, &b, buf, blocks);
      buf += got;
      n   -= got;
   }
   while (n--)
   {
      a += *buf++;
      b += a;
   }
   *pa = a;
   *pb = b;
}

static uint32_t rd_adler32_update(uint32_t adler,
      const uint8_t *buf, size_t len)
{
   uint32_t a = adler & 0xffff;
   uint32_t b = (adler >> 16) & 0xffff;
   while (len)
   {
      size_t n = len > RD_ADLER_NMAX ? RD_ADLER_NMAX : len;
      len -= n;
      rd_adler_run(&a, &b, buf, n);
      buf += n;
      a %= RD_ADLER_MODULUS;
      b %= RD_ADLER_MODULUS;
   }
   return (b << 16) | a;
}

/* Test entry point for tools/encodings/adler32_test.c: the kernel above is
 * static, and driving it only through rinflate/rdeflate would exercise
 * whatever lengths and alignments a stream happens to produce rather than
 * the ones that matter (block boundaries, the NMAX bound, the worst-case
 * seed).  Not declared in any header - the tool declares it itself. */
uint32_t rd_probe_adler32(uint32_t adler, const uint8_t *buf, size_t len)
{
   return rd_adler32_update(adler, buf, len);
}

/* ===================== inflate (RFC 1951 / RFC 1950) ===================== */
/* Clean-room RFC 1951 (DEFLATE) / RFC 1950 (zlib) inflate.
 * Non-blocking, resumable: suspends when input is exhausted or output is
 * full and resumes on the next call, in the style of image_transfer. */

/* Container selection, following zlib's window_bits convention:
 *   < 0        raw deflate, no header or checksum
 *   0 .. 15    zlib wrapper (2-byte header + adler32)
 *   16 .. 31   gzip wrapper (RFC 1952 header + crc32/isize)
 *   32 .. 47   auto-detect zlib or gzip from the first byte
 * The last two are what callers porting from zlib pass as 31 and 47. */
enum
{
   RINF_WRAP_RAW = 0,
   RINF_WRAP_ZLIB,
   RINF_WRAP_GZIP,
   RINF_WRAP_AUTO
};

static int rinf_wrap_from_bits(int window_bits)
{
   if (window_bits < 0)   return RINF_WRAP_RAW;
   if (window_bits <= 15) return RINF_WRAP_ZLIB;
   if (window_bits <= 31) return RINF_WRAP_GZIP;
   return RINF_WRAP_AUTO;
}

/* Decoder state-machine phases. */
enum rinf_phase
{
   RINF_HDR_SNIFF = -1, /* auto-detect: peek one byte, pick zlib or gzip   */
   RINF_ZHEADER = 0, /* consume 2-byte zlib header (wrapped mode)          */
   RINF_GZHEADER,    /* consume the 10-byte gzip fixed header              */
   RINF_GZ_EXTRA,    /* FEXTRA: XLEN then that many bytes                  */
   RINF_GZ_NAME,     /* FNAME: NUL-terminated                              */
   RINF_GZ_COMMENT,  /* FCOMMENT: NUL-terminated                           */
   RINF_GZ_HCRC,     /* FHCRC: 2 bytes over the header, skipped            */
   RINF_GZCRC,       /* consume/verify crc32 + isize trailer               */
   RINF_BLOCK_HDR,   /* read BFINAL + BTYPE                                */
   RINF_STORED_LEN,  /* stored block: read LEN/NLEN                        */
   RINF_STORED_DATA, /* stored block: copy literal bytes                  */
   RINF_DYN_TABLE,   /* dynamic block: read/build the huffman tables      */
   RINF_BLOCK_DATA,  /* huffman block: decode symbols                     */
   RINF_ADLER,       /* consume/verify adler32 trailer (wrapped mode)     */
   RINF_DONE
};

static int rinf_end_phase(int wrap)
{
   if (wrap == RINF_WRAP_ZLIB) return RINF_ADLER;
   if (wrap == RINF_WRAP_GZIP) return RINF_GZCRC;
   return RINF_DONE;
}

static int rinf_start_phase(int wrap)
{
   switch (wrap)
   {
      case RINF_WRAP_ZLIB: return RINF_ZHEADER;
      case RINF_WRAP_GZIP: return RINF_GZHEADER;
      case RINF_WRAP_AUTO: return RINF_HDR_SNIFF;
      default:             break;
   }
   return RINF_BLOCK_HDR;
}


/* A canonical-huffman decode table.  We use a two-level scheme: a direct
 * lookup on the low FAST_BITS bits, and for codes longer than FAST_BITS a
 * small linear/step search via the canonical first-code arrays. */
/* 11 bits: the dynamic litlen tables photographic PNG streams build
 * put most of their code mass at 8-11 bits, which a 9-bit table sent
 * to the canonical slow path.  The tables are per-stream heap state,
 * so the growth (4 KB -> 16 KB of packed entries per table) costs
 * nothing per decode; the splat loop in rinf_build touches each
 * table entry once per dynamic block, which is noise. */
#define RINF_FAST_BITS 11
#define RINF_MAX_BITS  15

struct rinf_huff
{
   /* fast[b] packs: (symbol<<4)|len for codes whose first FAST_BITS bits
    * (bit-reversed reading order) equal b and len<=FAST_BITS; len==0 means
    * "not a complete code, use the slow path". */
   uint16_t fast[1 << RINF_FAST_BITS];
   /* Packed fast entries for the litlen/dist tables, consumed only by
    * the fast inner loop.  One lookup yields everything a symbol needs:
    * bits 0-3 code length (0 = long code, take the fast[]/slow path),
    * bits 4-8 extra-bit count, bit 9 literal, bit 10 end-of-block, bit
    * 11 invalid symbol (litlen 286/287, dist 30/31 - real codes a
    * hostile dynamic header can define, which must fail exactly as the
    * scalar lane fails them), bits 16-31 base value (the literal byte,
    * the length base or the distance base).  Code length and extra
    * bits are consumed with one fused shift.  Left all-zero for the
    * code-length table, which the fast loop never touches. */
   uint32_t pfast[1 << RINF_FAST_BITS];
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
   uint64_t       bitbuf;   /* 64-bit: one refill covers a whole
                             * length/distance symbol group           */
   int            bitcnt;

   /* 32KB sliding window for back-references */
   uint8_t        window[32768];
   uint32_t       whave;   /* how many bytes are valid in the ring       */
   uint32_t       wnext;   /* next write position in the ring            */

   /* Container: RINF_WRAP_RAW / RINF_WRAP_ZLIB / RINF_WRAP_GZIP.  Kept as
    * `wrapped' meaning "has a checksummed wrapper" wherever the value
    * only decides trailer-or-not. */
   int            wrap;
   int            wrapped; /* wrap != RINF_WRAP_RAW                      */
   int            stop_at_block; /* report each block boundary            */
   int            block_ready;   /* boundary reached, not yet reported    */
   int            first_block_reported;
   int            skip_bits;     /* bits to discard at stream start       */
   int            bfinal;  /* current block is the last                  */
   int            btype;

   /* stored-block bookkeeping */
   uint32_t       stored_len;

   /* huffman tables for the current dynamic/fixed block */
   struct rinf_huff lencode;
   struct rinf_huff distcode;
   /* lencode/distcode currently hold the RFC 1951 fixed tables. */
   int              fixed_loaded;
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

   /* gzip container (RINF_WRAP_GZIP) */
   uint32_t       crc;        /* running crc32 of the output             */
   uint32_t       total_out;  /* mod 2^32, for the ISIZE check           */
   uint32_t       gz_flg;     /* FLG byte from the header                */
   uint32_t       gz_count;   /* bytes still to skip in the current field*/
   int            gz_step;    /* sub-step within a header field          */

   int            error;
};

/* --- bit reader helpers (LSB-first) --- */
/* Ensure at least n bits are available; returns 0 if input ran out. */
static int rinf_need(struct rinflate *s, int n)
{
   while (s->bitcnt < n)
   {
      if (s->in_pos >= s->in_size)
         return 0;
      s->bitbuf |= (uint64_t)s->in[s->in_pos++] << s->bitcnt;
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

   /* Refill the bit buffer as full as it will go in a single pass, so
    * the common case needs no further input reads. */
   while (s->bitcnt <= 56 && s->in_pos < s->in_size)
   {
      s->bitbuf |= (uint64_t)s->in[s->in_pos++] << s->bitcnt;
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

/* Populate pfast[] from a freshly built fast[]; layout in the struct
 * note.  Two variants because the litlen and distance base tables
 * differ. */
#define RINF_PF_LIT  (1u << 9)
#define RINF_PF_EOB  (1u << 10)
#define RINF_PF_BAD  (1u << 11)

static void rinf_pack_len(struct rinf_huff *h)
{
   int j;
   for (j = 0; j < (1 << RINF_FAST_BITS); j++)
   {
      uint16_t f = h->fast[j];
      uint32_t e = 0;
      if (f)
      {
         int sym = f >> 4;
         int len = f & 15;
         if (sym < 256)
            e = (uint32_t)len | RINF_PF_LIT | ((uint32_t)sym << 16);
         else if (sym == 256)
            e = (uint32_t)len | RINF_PF_EOB;
         else if (sym - 257 < 29)
            /* Bits [4:8] hold the code-plus-extra TOTAL, not the extra
             * count: the critical-path shift that advances to the next
             * code then uses the field directly, and the extra count
             * for the value mask falls off the path as total - len. */
            e = (uint32_t)len
              | ((uint32_t)(len + rinf_len_extra[sym - 257]) << 4)
              | ((uint32_t)rinf_len_base[sym - 257] << 16);
         else
            e = (uint32_t)len | RINF_PF_BAD;
      }
      h->pfast[j] = e;
   }
}

static void rinf_pack_dist(struct rinf_huff *h)
{
   int j;
   for (j = 0; j < (1 << RINF_FAST_BITS); j++)
   {
      uint16_t f = h->fast[j];
      uint32_t e = 0;
      if (f)
      {
         int sym = f >> 4;
         int len = f & 15;
         if (sym < 30)
            e = (uint32_t)len
              | ((uint32_t)(len + rinf_dist_extra[sym]) << 4)
              | ((uint32_t)rinf_dist_base[sym] << 16);
         else
            e = (uint32_t)len | RINF_PF_BAD;
      }
      h->pfast[j] = e;
   }
}

/* fixed huffman tables (RFC 1951 3.2.6) */
/* The fixed tables are the same two tables every time - RFC 1951 states
 * their code lengths outright - so a stream that emits a run of fixed
 * blocks was rebuilding identical content per block.  One PNG in a
 * libretro core's asset set drove 28 of them, and each rebuild is two
 * rinf_build calls, the most expensive thing in the decoder outside the
 * symbol loop.  Skip it when lencode/distcode already hold them; a
 * dynamic block clears the flag when it builds over them. */
static void rinf_fixed_tables(struct rinflate *s)
{
   uint8_t ll[288], dd[30];
   int i;

   if (s->fixed_loaded)
   {
      s->have_tables = 1;
      return;
   }
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
   rinf_pack_len(&s->lencode);
   rinf_pack_dist(&s->distcode);
   s->have_tables  = 1;
   s->fixed_loaded = 1;
}

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
   /* Append n bytes into the ring at wnext, in at most two memcpys.
    * n is bounded by the ring size above, so the copy wraps at most
    * once.  This runs once per process() call over the whole slice -
    * a byte loop here costs as much as the inflate that produced the
    * data when the caller feeds small slices (rpng uses 32 KB). */
   {
      size_t first = 32768 - s->wnext;
      if (first > n)
         first = n;
      memcpy(s->window + s->wnext, src, first);
      s->wnext += (uint32_t)first;
      if (s->wnext == 32768)
         s->wnext = 0;
      if (first < n)
      {
         memcpy(s->window, src + first, n - first);
         s->wnext = (uint32_t)(n - first);
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
   s->wrap    = rinf_wrap_from_bits(window_bits);
   s->wrapped = (s->wrap != RINF_WRAP_RAW);
   s->crc     = encoding_crc32(0, NULL, 0);
   s->phase   = rinf_start_phase(s->wrap);
   s->adler   = 1;
   return s;
}

void rinflate_free(void *data) { free(data); }

void rinflate_reset(void *data, int window_bits)
{
   struct rinflate *s = (struct rinflate*)data;
   if (!s)
      return;

   /* Restore the state rinflate_new hands back, without re-zeroing the
    * 32 KiB back-reference window or the ~9 KiB of huffman tables.
    *
    * The window is safe to leave dirty because whave is cleared here:
    * a back-reference is only resolved out of the ring after a bounds
    * check against out_pos + whave, so with whave 0 no stale byte is
    * reachable, exactly as for a fresh instance whose window happens
    * to be zeroed. The tables are safe because have_tables and
    * fixed_loaded are cleared, so any stream must rebuild them before
    * a symbol is decoded, and rinf_build clears each table's fast
    * lookup as it goes.
    *
    * Everything else is set to the same value calloc would have
    * produced. Fields are listed rather than memset in bulk so that
    * adding one to the struct without touching this function is a
    * compile-time-visible omission rather than a silent stale value. */
   s->wrap             = rinf_wrap_from_bits(window_bits);
   s->wrapped          = (s->wrap != RINF_WRAP_RAW);
   s->phase            = rinf_start_phase(s->wrap);
   s->crc              = encoding_crc32(0, NULL, 0);
   s->total_out        = 0;
   s->gz_flg           = 0;
   s->gz_count         = 0;
   s->gz_step          = 0;

   s->in               = NULL;
   s->in_size          = 0;
   s->in_pos           = 0;
   s->out              = NULL;
   s->out_size         = 0;
   s->out_pos          = 0;

   s->bitbuf           = 0;
   s->bitcnt           = 0;

   s->whave            = 0;
   s->wnext            = 0;

   s->bfinal           = 0;
   s->btype            = 0;
   s->block_ready      = 0;
   s->skip_bits        = 0;
   s->stored_len       = 0;

   s->fixed_loaded     = 0;
   s->have_tables      = 0;

   s->hlit             = 0;
   s->hdist            = 0;
   s->hclen            = 0;
   memset(s->cl_lengths, 0, sizeof(s->cl_lengths));
   memset(s->lengths,    0, sizeof(s->lengths));
   s->lengths_have     = 0;
   s->clcodes_read     = 0;
   s->clcode_built     = 0;
   s->cl_pending_sym   = 0;

   s->copy_len         = 0;
   s->copy_dist        = 0;
   s->copy_active      = 0;

   s->ld_step          = 0;
   s->ld_lensym        = 0;
   s->ld_length        = 0;
   s->ld_distsym       = 0;
   s->pending_lit      = 0;
   s->have_pending_lit = 0;

   s->adler            = 1;
   s->adler_read       = 0;
   s->adler_have       = 0;

   s->error            = 0;
}

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

/* Prime the back-reference window with the tail of @dict, for resuming
 * raw-deflate decode mid-stream (indexed random access into gzip
 * members: the index stores the 32KB of plaintext preceding each entry
 * point, and decode restarts at a block boundary with that history).
 * Matches zlib inflateSetDictionary() semantics for the raw case: only
 * the last 32768 bytes matter, and the call replaces any history. */
void rinflate_set_dictionary(void *data, const uint8_t *dict, size_t len)
{
   struct rinflate *s = (struct rinflate *)data;
   if (!s || !dict)
      return;
   if (len > 32768)
   {
      dict += len - 32768;
      len   = 32768;
   }
   memcpy(s->window, dict, len);
   s->whave = (uint32_t)len;
   s->wnext = (uint32_t)(len & 32767);
}

/* zran-style indexed access primitives: report deflate block
 * boundaries, tell the exact input bit position, and start a resumed
 * stream part-way into its first byte. Together with
 * rinflate_set_dictionary these are the whole toolkit an index
 * builder/extractor needs. */
void rinflate_set_stop_at_block(void *data, int stop)
{
   struct rinflate *s = (struct rinflate *)data;
   if (s)
      s->stop_at_block = stop;
}

uint64_t rinflate_tell_bits(const void *data)
{
   const struct rinflate *s = (const struct rinflate *)data;
   if (!s)
      return 0;
   return (uint64_t)s->in_pos * 8 - (uint64_t)s->bitcnt;
}

void rinflate_set_start_bit(void *data, int bits)
{
   struct rinflate *s = (struct rinflate *)data;
   if (s)
      s->skip_bits = bits & 7;
}

int rinflate_process(void *data, size_t *read, size_t *wrote)
{
   struct rinflate *s = (struct rinflate*)data;
   size_t in_start  = s->in_pos;
   size_t out_start = s->out_pos;
   size_t fold_start = s->out_pos;   /* adler fold cursor (wrapped mode)   */
   int status = RDEFLATE_PROCESS_NEXT;

   if (s->skip_bits)
   {
      if (!rinf_need(s, s->skip_bits))
         goto suspend;
      rinf_getbits(s, s->skip_bits);
      s->skip_bits = 0;
   }

   for (;;)
   {
      switch (s->phase)
      {
         case RINF_HDR_SNIFF:
            /* zlib's 47 means "work it out from the stream".  A gzip
             * member always starts 1f 8b; a zlib header never can,
             * because its low nibble must be 8 and 0x1f gives 15. */
            if (!rinf_need(s, 8)) goto suspend;
            s->wrap  = ((s->bitbuf & 0xff) == 0x1f)
                     ? RINF_WRAP_GZIP : RINF_WRAP_ZLIB;
            s->phase = rinf_start_phase(s->wrap);
            break;

         case RINF_GZHEADER:
            /* ID1 ID2 CM FLG MTIME[4] XFL OS */
            if (!rinf_need(s, 16)) goto suspend;
            {
               uint32_t id1 = rinf_getbits(s, 8);
               uint32_t id2 = rinf_getbits(s, 8);
               if (id1 != 0x1f || id2 != 0x8b) { s->error = 1; goto error; }
            }
            if (!rinf_need(s, 16)) goto suspend;
            {
               uint32_t cm = rinf_getbits(s, 8);
               s->gz_flg   = rinf_getbits(s, 8);
               if (cm != 8)              { s->error = 1; goto error; }
               /* bits 5-7 are reserved and must be zero */
               if (s->gz_flg & 0xe0)     { s->error = 1; goto error; }
            }
            {
               int i;
               for (i = 0; i < 6; i++)   /* MTIME[4], XFL, OS */
               {
                  if (!rinf_need(s, 8)) goto suspend;
                  rinf_getbits(s, 8);
               }
            }
            s->gz_step  = 0;
            s->gz_count = 0;
            s->phase    = RINF_GZ_EXTRA;
            break;

         case RINF_GZ_EXTRA:
            if (!(s->gz_flg & 0x04)) { s->phase = RINF_GZ_NAME; break; }
            if (s->gz_step == 0)
            {
               if (!rinf_need(s, 16)) goto suspend;
               s->gz_count = rinf_getbits(s, 8);
               s->gz_count |= rinf_getbits(s, 8) << 8;  /* XLEN, LE */
               s->gz_step  = 1;
            }
            while (s->gz_count)
            {
               if (!rinf_need(s, 8)) goto suspend;
               rinf_getbits(s, 8);
               s->gz_count--;
            }
            s->gz_step = 0;
            s->phase   = RINF_GZ_NAME;
            break;

         case RINF_GZ_NAME:
            if (!(s->gz_flg & 0x08)) { s->phase = RINF_GZ_COMMENT; break; }
            for (;;)
            {
               if (!rinf_need(s, 8)) goto suspend;
               if (rinf_getbits(s, 8) == 0) break;
            }
            s->phase = RINF_GZ_COMMENT;
            break;

         case RINF_GZ_COMMENT:
            if (!(s->gz_flg & 0x10)) { s->phase = RINF_GZ_HCRC; break; }
            for (;;)
            {
               if (!rinf_need(s, 8)) goto suspend;
               if (rinf_getbits(s, 8) == 0) break;
            }
            s->phase = RINF_GZ_HCRC;
            break;

         case RINF_GZ_HCRC:
            /* Header crc16, if present.  Consumed, not verified: a
             * mismatch here says nothing the crc32 over the data will
             * not say more reliably a moment later. */
            if (s->gz_flg & 0x02)
            {
               if (!rinf_need(s, 16)) goto suspend;
               rinf_getbits(s, 8);
               rinf_getbits(s, 8);
            }
            s->phase = RINF_BLOCK_HDR;
            break;

         case RINF_GZCRC:
            /* fold any output produced in this call before comparing */
            if (s->out_pos > fold_start)
            {
               s->crc = encoding_crc32(s->crc,
                     s->out + fold_start, s->out_pos - fold_start);
               s->total_out += (uint32_t)(s->out_pos - fold_start);
               fold_start = s->out_pos;
            }
            /* align to byte, then CRC32 and ISIZE, both little-endian */
            s->bitbuf >>= (s->bitcnt & 7);
            s->bitcnt  -= (s->bitcnt & 7);
            while (s->adler_have < 8)
            {
               if (!rinf_need(s, 8)) goto suspend;
               s->adler_read |= rinf_getbits(s, 8) << ((s->adler_have & 3) * 8);
               s->adler_have++;
               if (s->adler_have == 4)
               {
                  if (s->adler_read != s->crc) { s->error = 1; goto error; }
                  s->adler_read = 0;
               }
            }
            if (s->adler_read != s->total_out) { s->error = 1; goto error; }
            s->phase = RINF_DONE;
            break;

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
            if (s->stop_at_block && !s->first_block_reported)
            {
               s->first_block_reported = 1;
               status = RDEFLATE_PROCESS_BLOCK;
               goto done;
            }
            if (s->block_ready)
            {
               s->block_ready = 0;
               status = RDEFLATE_PROCESS_BLOCK;
               goto done;
            }
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
            /* Bulk copy: a stored block is byte-aligned by
             * construction, and the LEN/NLEN read that precedes this
             * phase consumes whole bytes from an aligned start, so
             * the bit buffer is empty on arrival and the payload can
             * go straight from input to output.  The bitcnt test is
             * an invariant check rather than a live branch - if a
             * future change leaves bits buffered, the byte-at-a-time
             * loop below still produces correct output, just slowly. */
            if (s->stored_len > 0 && s->bitcnt == 0)
            {
               size_t avail_in  = s->in_size  - s->in_pos;
               size_t avail_out = s->out_size - s->out_pos;
               size_t n         = s->stored_len;
               if (n > avail_in)  n = avail_in;
               if (n > avail_out) n = avail_out;
               if (n)
               {
                  memcpy(s->out + s->out_pos, s->in + s->in_pos, n);
                  s->in_pos     += n;
                  s->out_pos    += n;
                  s->stored_len -= (uint32_t)n;
               }
               if (s->stored_len > 0)
                  goto suspend;   /* input or output exhausted */
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
            s->phase = s->bfinal ? rinf_end_phase(s->wrap)
                                 : RINF_BLOCK_HDR;
            if (!s->bfinal && s->stop_at_block)
               s->block_ready = 1;
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
            s->fixed_loaded = 0;
            if (!rinf_build(&s->lencode, s->lengths, s->hlit))
               { s->error = 1; goto error; }
            if (!rinf_build(&s->distcode, s->lengths + s->hlit, s->hdist))
               { s->error = 1; goto error; }
            rinf_pack_len(&s->lencode);
            rinf_pack_dist(&s->distcode);
            s->have_tables = 1;
            s->copy_active = 0;
            s->phase = RINF_BLOCK_DATA;
            break;

         case RINF_BLOCK_DATA:
fast_again:
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
            /* Hoisted state.  Every s-> access in here would otherwise
             * be a reload: the compiler cannot prove that the store to
             * s->out[] does not alias the struct itself, so the bit
             * buffer, the cursors and the table pointers all round-trip
             * through memory once per symbol.  Keeping them in locals
             * and writing back on exit is worth more than any of the
             * decode changes. */
            if (!s->copy_active && s->ld_step == 0)
            {
               uint64_t bitbuf       = s->bitbuf;
               int      bitcnt       = s->bitcnt;
               size_t   in_pos       = s->in_pos;
               size_t   out_pos      = s->out_pos;
               const uint8_t *in     = s->in;
               uint8_t       *out    = s->out;
               const uint32_t *plen  = s->lencode.pfast;
               const uint32_t *pdist = s->distcode.pfast;
               int      done_fast    = 0;

               /* The output guard reserves the longest match plus the
                * copy over-run pad: the inlined copies below write in
                * whole 8- or 16-byte steps and may run up to 15 bytes
                * past the match end.  Those bytes lie beyond out_pos,
                * so they are scratch - either rewritten by later output
                * or beyond the produced length entirely.  The margin in
                * the careful path's hand-back check must stay equal to
                * this one: a larger value there would re-enter a loop
                * whose own guard immediately fails, bouncing control
                * between the two paths without progress. */
               {
               /* Preload carrier: at the end of a match copy the next
                * code's low table-index bits are already final - the
                * refill only ORs into bits at or above bitcnt - so the
                * next litlen entry can be fetched before the copy's
                * stores retire, hiding the table load under them.  The
                * entry is only trusted when at least 15 bits were
                * buffered at preload time (a full code's worth), which
                * mirrors the literal chain's own reuse condition. */
               uint32_t e_pre    = 0;
               int      have_pre = 0;
               while (in_pos + 8 <= s->in_size
                     && out_pos + 258 + 16 <= s->out_size)
               {
                  uint32_t e;
                  /* Refill only when the buffer cannot already cover a
                   * whole length/distance group - litlen (15) + length
                   * extra (5) + distance (15) + distance extra (13) = 48
                   * bits - so nothing below needs to touch the input
                   * again.  The loop guard has already established that
                   * 8 input bytes are available, and this consumes at
                   * most 7.
                   *
                   * Topping up to 56 on every iteration instead, which
                   * is what this did, paid the whole refill per symbol.
                   * A literal is about eight bits, so the buffer still
                   * holds a group's worth after one and the refill is
                   * simply skipped; on literal-dominated input that is
                   * half of them, and worth 30-40%.  It costs nothing
                   * on match-dominated input, where a group empties the
                   * buffer far enough to refill anyway. */
                  if (bitcnt < 48)
                  {
                     /* Exactly what the byte loop did, in one load: take
                      * the whole bytes that fit above bitcnt, mask off
                      * the rest so nothing lands above the new count,
                      * and advance by that many.  Keeping the mask means
                      * the invariant every other path relies on - bits
                      * at or above bitcnt are zero - still holds, which
                      * the maskless form used elsewhere would break for
                      * the byte-aligning stored-block path. */
                     static const uint64_t keep[8] = {
                        0x0000000000000000ull, 0x00000000000000ffull,
                        0x000000000000ffffull, 0x0000000000ffffffull,
                        0x00000000ffffffffull, 0x000000ffffffffffull,
                        0x0000ffffffffffffull, 0x00ffffffffffffffull
                     };
                     int      nb_ = (63 - bitcnt) >> 3;
                     uint64_t chunk_;
                     memcpy(&chunk_, in + in_pos, sizeof(chunk_));
#if RETRO_IS_BIG_ENDIAN
                     chunk_ = SWAP64(chunk_);
#endif
                     bitbuf |= (chunk_ & keep[nb_]) << bitcnt;
                     in_pos += (size_t)nb_;
                     bitcnt += nb_ * 8;
                  }
                  if (have_pre)
                  {
                     e        = e_pre;
                     have_pre = 0;
                  }
                  else
                     e = plen[bitbuf & ((1 << RINF_FAST_BITS) - 1)];
                  /* Literal chain: after one refill the buffer holds at
                   * least 48 bits, and every packed literal costs at
                   * most 15, so several literals can be emitted before
                   * the next refill.  bitcnt >= 15 before each lookup
                   * guarantees the entry's code length is covered; the
                   * output side is covered by the loop guard's 258+16
                   * reserve, which no chain can outrun (a literal code
                   * is at least one bit, so at most 47 emissions). */
                  while (e & RINF_PF_LIT)
                  {
                     bitbuf >>= (e & 15);
                     bitcnt  -= (int)(e & 15);
                     out[out_pos++] = (uint8_t)(e >> 16);
                     if (bitcnt < 15)
                        break;
                     e = plen[bitbuf & ((1 << RINF_FAST_BITS) - 1)];
                  }
                  if (e & RINF_PF_LIT)
                     continue;      /* broke for a refill; guards re-run */
                  /* A non-literal group needs up to 48 buffered bits
                   * (15 + 5 + 15 + 13); a chain can leave fewer.  Retry
                   * through the loop head, which refills. */
                  if (bitcnt < 48 && e)
                     continue;
                  if (!e)
                  {
                     /* Long code: hand back to the shared decoder. */
                     int sym;
                     s->bitbuf = bitbuf;
                     s->bitcnt = bitcnt;
                     s->in_pos = in_pos;
                     sym       = rinf_decode(s, &s->lencode);
                     bitbuf    = s->bitbuf;
                     bitcnt    = s->bitcnt;
                     in_pos    = s->in_pos;
                     if (sym < -1)
                     {
                        s->out_pos = out_pos;
                        goto error;
                     }
                     if (sym == -1)
                        break;
                     if (sym < 256)
                     {
                        out[out_pos++] = (uint8_t)sym;
                        continue;
                     }
                     if (sym == 256)
                     {
                        s->phase = s->bfinal
                           ? rinf_end_phase(s->wrap)
                           : RINF_BLOCK_HDR;
                        if (!s->bfinal && s->stop_at_block)
                           s->block_ready = 1;
                        done_fast = 1;
                        break;
                     }
                     if (sym - 257 >= 29)
                     {
                        s->error   = 1;
                        s->out_pos = out_pos;
                        goto error;
                     }
                     /* Re-enter the packed flow with a synthesised
                      * entry; the code bits are already consumed, so its
                      * code length is zero and the packed total in bits
                      * [4:8] degenerates to the extra count - the same
                      * encoding either way. */
                     e = ((uint32_t)rinf_len_extra[sym - 257] << 4)
                       | ((uint32_t)rinf_len_base[sym - 257] << 16);
                     if (bitcnt < 33)
                     {
                        /* Not enough buffered bits for the extra-bits
                         * and distance group without a refill the loop
                         * guard may no longer permit; park the decoded
                         * symbol in the careful path's resume state and
                         * let it finish this one group. */
                        s->ld_lensym = sym - 257;
                        s->ld_step   = 1;
                        break;
                     }
                  }
                  if (e & RINF_PF_EOB)
                  {
                     bitbuf >>= (e & 15);
                     bitcnt  -= (int)(e & 15);
                     s->phase = s->bfinal
                        ? rinf_end_phase(s->wrap)
                        : RINF_BLOCK_HDR;
                     if (!s->bfinal && s->stop_at_block)
                        s->block_ready = 1;
                     done_fast = 1;
                     break;
                  }
                  if (e & RINF_PF_BAD)
                  {
                     s->error   = 1;
                     s->out_pos = out_pos;
                     goto error;
                  }
                  {
                     int dsym, ei;
                     uint32_t length, dist;
                     {
                        int l  = (int)(e & 15);
                        int t  = (int)((e >> 4) & 31);
                        length = (e >> 16)
                              + (uint32_t)((bitbuf >> l)
                                    & (((uint64_t)1 << (t - l)) - 1));
                        bitbuf >>= t;
                        bitcnt  -= t;
                     }
                     {
                        uint32_t d = pdist[bitbuf
                           & ((1 << RINF_FAST_BITS) - 1)];
                        if (d & RINF_PF_BAD)
                        {
                           s->error   = 1;
                           s->out_pos = out_pos;
                           goto error;
                        }
                        if (d)
                        {
                           /* Length consumed at most 20 bits of the 48
                            * the head guaranteed, so the full distance
                            * group (15 + 13) is always covered here. */
                           int dl  = (int)(d & 15);
                           int dt  = (int)((d >> 4) & 31);
                           dist    = (d >> 16)
                                 + (uint32_t)((bitbuf >> dl)
                                       & (((uint64_t)1 << (dt - dl)) - 1));
                           bitbuf >>= dt;
                           bitcnt  -= dt;
                        }
                        else
                        {
                           s->bitbuf = bitbuf;
                           s->bitcnt = bitcnt;
                           s->in_pos = in_pos;
                           dsym      = rinf_decode(s, &s->distcode);
                           bitbuf    = s->bitbuf;
                           bitcnt    = s->bitcnt;
                           in_pos    = s->in_pos;
                           if (dsym < 0)
                           {
                              if (dsym == -1)
                              {
                                 /* Bits ran short mid-group; park the
                                  * assembled length for the careful
                                  * path to finish. */
                                 s->ld_length = length;
                                 s->ld_step   = 2;
                                 break;
                              }
                              s->out_pos = out_pos;
                              goto error;
                           }
                           if (dsym >= 30)
                           {
                              s->error   = 1;
                              s->out_pos = out_pos;
                              goto error;
                           }
                           ei   = rinf_dist_extra[dsym];
                           dist = rinf_dist_base[dsym];
                           if (ei)
                           {
                              if (bitcnt < ei)
                              {
                                 s->ld_length  = length;
                                 s->ld_distsym = dsym;
                                 s->ld_step    = 3;
                                 break;
                              }
                              dist  += (uint32_t)(bitbuf & ((1u << ei) - 1));
                              bitbuf >>= ei;
                              bitcnt  -= ei;
                           }
                        }
                     }
                     if (dist > out_pos + s->whave)
                     {
                        s->error   = 1;
                        s->out_pos = out_pos;
                        goto error;
                     }
                     if (dist <= out_pos)
                     {
                        /* All copies below run in whole 8-byte steps
                         * (constant-size memcpy compiles to one move,
                         * not a call) and may write up to 7 bytes past
                         * the match end - covered by the loop guard's
                         * pad.  Short distances replicate the pattern:
                         * 1/2/4 broadcast into a 64-bit word; 3/5/6/7
                         * prime 16 bytes serially, after which 8-byte
                         * chunks at distance dist*(16/dist) >= 9 both
                         * preserve the period and never load a byte
                         * stored in the same step. */
                        uint8_t       *dst  = out + out_pos;
                        const uint8_t *srcp = dst - dist;
                        uint8_t       *dend = dst + length;
                        if (dist >= 16)
                        {
                           do
                           {
                              memcpy(dst, srcp, 16);
                              dst  += 16;
                              srcp += 16;
                           } while (dst < dend);
                        }
                        else if (dist >= 8)
                        {
                           do
                           {
                              memcpy(dst, srcp, 8);
                              dst  += 8;
                              srcp += 8;
                           } while (dst < dend);
                        }
                        else if (dist == 1)
                        {
                           uint64_t pat = (uint64_t)srcp[0]
                                 * 0x0101010101010101ull;
                           do
                           {
                              memcpy(dst,     &pat, 8);
                              memcpy(dst + 8, &pat, 8);
                              dst += 16;
                           } while (dst < dend);
                        }
                        else if (dist == 2 || dist == 4)
                        {
                           uint64_t pat;
                           if (dist == 2)
                           {
                              uint16_t w;
                              memcpy(&w, srcp, 2);
                              pat = (uint64_t)w * 0x0001000100010001ull;
                           }
                           else
                           {
                              uint32_t w;
                              memcpy(&w, srcp, 4);
                              pat = (uint64_t)w | ((uint64_t)w << 32);
                           }
                           do
                           {
                              memcpy(dst,     &pat, 8);
                              memcpy(dst + 8, &pat, 8);
                              dst += 16;
                           } while (dst < dend);
                        }
                        else /* dist 3, 5, 6, 7 */
                        {
                           size_t k;
                           size_t prime = length < 16 ? (size_t)length : 16;
                           for (k = 0; k < prime; k++)
                              dst[k] = srcp[k];
                           if (length > 16)
                           {
                              uint32_t D        = dist * (16 / dist);
                              uint8_t *p        = dst + 16;
                              const uint8_t *q  = p - D;
                              do
                              {
                                 memcpy(p, q, 8);
                                 p += 8;
                                 q += 8;
                              } while (p < dend);
                           }
                        }
                        out_pos += length;
                        e_pre    = plen[bitbuf
                              & ((1 << RINF_FAST_BITS) - 1)];
                        have_pre = (bitcnt >= 15);
                     }
                     else
                     {
                        /* rare: reaches into prior-call output */
                        s->copy_len    = length;
                        s->copy_dist   = dist;
                        s->copy_active = 1;
                        break;
                     }
                  }
               }
               }

               s->bitbuf  = bitbuf;
               s->bitcnt  = bitcnt;
               s->in_pos  = in_pos;
               s->out_pos = out_pos;
               if (done_fast)
                  goto block_done;
            }

            for (;;)
            {
               /* The careful path exists for boundary states - a copy
                * or a length/distance group straddling a slice edge, a
                * long Huffman code, tight margins.  Once the boundary
                * state is resolved and the margins are comfortable
                * again, hand control straight back to the fast loop:
                * without this, one straddling match at a slice edge
                * left the whole next slice on the symbol-at-a-time
                * path (the fast block only ran on entry to the phase),
                * which was most of them, since a 32 KB output slice
                * almost always fills mid-match.  The first iteration
                * after a fast-loop exit fails these margin checks by
                * construction, so this cannot spin. */
               if (   !s->copy_active && s->ld_step == 0
                   && !s->have_pending_lit
                   && s->in_pos + 8 <= s->in_size
                   && s->out_pos + 258 + 16 <= s->out_size)
                  goto fast_again;
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
                        ? rinf_end_phase(s->wrap)
                        : RINF_BLOCK_HDR;
                     if (!s->bfinal && s->stop_at_block)
                        s->block_ready = 1;
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
               s->adler = rd_adler32_update(s->adler,
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
   if (s->out_pos > fold_start)
   {
      if (s->wrap == RINF_WRAP_ZLIB)
         s->adler = rd_adler32_update(s->adler,
               s->out + fold_start, s->out_pos - fold_start);
      else if (s->wrap == RINF_WRAP_GZIP)
      {
         s->crc = encoding_crc32(s->crc,
               s->out + fold_start, s->out_pos - fold_start);
         s->total_out += (uint32_t)(s->out_pos - fold_start);
      }
   }
   if (read)  *read  = s->in_pos  - in_start;
   if (wrote) *wrote = s->out_pos - out_start;
   return status;

error:
   if (s->wrapped && s->out_pos > fold_start)
      s->adler = rd_adler32_update(s->adler,
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
   int      wrap;      /* RINF_WRAP_RAW / _ZLIB / _GZIP                 */
   int      wrapped;   /* wrap != RINF_WRAP_RAW                         */
   int      use_crc_hash; /* settled once at construction; see rd_hash */
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
   uint32_t crc;       /* gzip: running crc32 of the input              */
   uint32_t total_in;  /* gzip: mod 2^32, for ISIZE                     */
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

   /* Huffman length-generation scratch.  These were locals in
    * rd_gen_lengths(), which made that a 12768-byte frame - the
    * two-queue tree needs 2*288 nodes of weight, left, right and
    * depth, on top of the 288-entry sort arrays.  A frame that size
    * does not belong on a stack whatever the target: it sits under
    * whatever called it, and the state struct here is calloc'd once
    * per stream, so the arrays cost nothing extra to keep. */
   int      gl_idx[288];
   uint32_t gl_fr[288];
   int      gl_lc[288];
   uint32_t gl_wt[2 * 288];
   int      gl_left[2 * 288];
   int      gl_right[2 * 288];
   int      gl_depth[2 * 288];
};

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

/* O(1) symbol lookups.  These used to be built at first use behind a
 * non-atomic flag, which is a data race when two streams are
 * constructed concurrently: the flag could be seen before the entries
 * on a weakly-ordered machine, and concurrent writers storing
 * identical values are a race even where it happens to be benign.
 * The tables are 771 bytes and derived mechanically from rd_len_base
 * and rd_dist_base, so they are baked as const data generated by the
 * same builder loops; const data has no initialiser to race on. */
static const uint8_t rd_length_code[259] = {  /* len 0..258 -> length symbol */
  0,0,0,0,1,2,3,4,5,6,7,8,8,9,9,10,10,11,11,12,
  12,12,12,13,13,13,13,14,14,14,14,15,15,15,15,16,16,16,16,16,
  16,16,16,17,17,17,17,17,17,17,17,18,18,18,18,18,18,18,18,19,
  19,19,19,19,19,19,19,20,20,20,20,20,20,20,20,20,20,20,20,20,
  20,20,20,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,21,22,
  22,22,22,22,22,22,22,22,22,22,22,22,22,22,22,23,23,23,23,23,
  23,23,23,23,23,23,23,23,23,23,23,24,24,24,24,24,24,24,24,24,
  24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,24,
  24,24,24,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,
  25,25,25,25,25,25,25,25,25,25,25,25,25,25,25,26,26,26,26,26,
  26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,
  26,26,26,26,26,26,26,27,27,27,27,27,27,27,27,27,27,27,27,27,
  27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,28 };
static const uint8_t rd_dist_code_lo[256] = { /* dist 1..256 -> dist symbol */
  0,1,2,3,4,4,5,5,6,6,6,6,7,7,7,7,8,8,8,8,
  8,8,8,8,9,9,9,9,9,9,9,9,10,10,10,10,10,10,10,10,
  10,10,10,10,10,10,10,10,11,11,11,11,11,11,11,11,11,11,11,11,
  11,11,11,11,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,
  12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,12,13,13,13,13,
  13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,13,
  13,13,13,13,13,13,13,13,14,14,14,14,14,14,14,14,14,14,14,14,
  14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,
  14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,14,
  14,14,14,14,14,14,14,14,14,14,14,14,15,15,15,15,15,15,15,15,
  15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
  15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,
  15,15,15,15,15,15,15,15,15,15,15,15,15,15,15,15 };
static const uint8_t rd_dist_code_hi[256] = { /* (dist-1)>>7 for 257..32768 */
  0,14,16,17,18,18,19,19,20,20,20,20,21,21,21,21,22,22,22,22,
  22,22,22,22,23,23,23,23,23,23,23,23,24,24,24,24,24,24,24,24,
  24,24,24,24,24,24,24,24,25,25,25,25,25,25,25,25,25,25,25,25,
  25,25,25,25,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,
  26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,26,27,27,27,27,
  27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,27,
  27,27,27,27,27,27,27,27,28,28,28,28,28,28,28,28,28,28,28,28,
  28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,
  28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,28,
  28,28,28,28,28,28,28,28,28,28,28,28,29,29,29,29,29,29,29,29,
  29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
  29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,
  29,29,29,29,29,29,29,29,29,29,29,29,29,29,29,29 };

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
 * table far more evenly than the shift-xor hash, which shortens the
 * per-bucket chains the match finder has to walk.  On a stock x86-64
 * build no flag enables __SSE4_2__, so the choice is made at run time
 * per stream (see rdeflate_new); the branch is on cold state and the
 * benefit is chain quality rather than hash latency, so nothing is
 * lost to the dispatch.  Falls back to the shift-xor hash on targets
 * without a usable CRC32 instruction. */
#if defined(__SSE4_2__)
#include <nmmintrin.h>
#define RD_CRC32_HASH_DIRECT 1
#elif defined(__ARM_FEATURE_CRC32)
#include <arm_acle.h>
#define RD_CRC32_HASH_DIRECT 1
#elif defined(__x86_64__) || defined(__i386__) \
   || defined(_M_X64)     || defined(_M_IX86)
/* Being on x86 is necessary but not sufficient: the toolchain also has
 * to be able to build the intrinsic.  Same predicate shape as the
 * carry-less multiply path in encoding_crc32.c, and for the same
 * reason: Clang reports itself as GCC 4.2 forever, so a bare version
 * comparison would reject every Clang build.  MSVC gained
 * nmmintrin.h in Visual Studio 2008 (_MSC_VER 1500); older MSVC
 * takes the shift-xor hash. */
#  if defined(_MSC_VER)
#    if _MSC_VER >= 1500
#      define RD_CRC32_HASH_RUNTIME 1
#    endif
#  elif defined(__has_attribute) && defined(__has_include)
#    if __has_attribute(target) && __has_include(<nmmintrin.h>)
#      define RD_CRC32_HASH_RUNTIME 1
#    endif
#  elif !defined(__clang__) && defined(__GNUC__) \
   && (__GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 9))
#    define RD_CRC32_HASH_RUNTIME 1
#  endif
#endif

/* Clang moved the SSE4.2 CRC32 intrinsics out of the smmintrin.h /
 * nmmintrin.h chain and into their own crc32intrin.h, which the umbrella
 * headers pull in only when __CRC32__ (or __SSE4_2__) is already defined
 * at the point of inclusion.  Neither is, in a translation unit that
 * reaches the instruction through __attribute__((target("sse4.2")))
 * rather than through a global -msse4.2 - so __has_include(<nmmintrin.h>)
 * answered yes above, the declaration never arrived, and the build broke
 * on _mm_crc32_u32 with clang telling us which header it wanted.  x86
 * only: the ARM path below reaches its intrinsic through arm_acle.h.
 *
 * crc32intrin.h carries the target attribute on the intrinsics
 * themselves, so including it directly is safe whether or not the TU is
 * built for SSE4.2, and harmless on toolchains that never split it out. */
#if (defined(RD_CRC32_HASH_DIRECT) || defined(RD_CRC32_HASH_RUNTIME)) \
   && (defined(__x86_64__) || defined(__i386__) \
    || defined(_M_X64)     || defined(_M_IX86))
#  if defined(__has_include)
#    if __has_include(<crc32intrin.h>)
#      include <crc32intrin.h>
#    endif
#  endif
#endif

#if defined(RD_CRC32_HASH_RUNTIME)
#include <nmmintrin.h>
#include <features/features_cpu.h>
#if defined(_MSC_VER)
#define RD_TARGET_CRC32
#else
#define RD_TARGET_CRC32 __attribute__((target("sse4.2")))
#endif
static RD_TARGET_CRC32 uint32_t rd_hash_crc(const uint8_t *p)
{
   uint32_t k = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
   return _mm_crc32_u32(0, k) & RD_HASH_MASK;
}
#endif

#if !defined(RD_CRC32_HASH_DIRECT)
static uint32_t rd_hash_soft(const uint8_t *p)
{
   return (uint32_t)(((p[0] << (2*RD_HASH_SHIFT)) ^
                      (p[1] <<   RD_HASH_SHIFT)  ^
                       p[2]) & RD_HASH_MASK);
}
#endif

static uint32_t rd_hash(struct rdeflate *s, const uint8_t *p)
{
#if defined(__SSE4_2__)
   uint32_t k = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
   (void)s;
   return _mm_crc32_u32(0, k) & RD_HASH_MASK;
#elif defined(__ARM_FEATURE_CRC32)
   uint32_t k = (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16);
   (void)s;
   return __crc32w(0, k) & RD_HASH_MASK;
#elif defined(RD_CRC32_HASH_RUNTIME)
   if (s->use_crc_hash)
      return rd_hash_crc(p);
   return rd_hash_soft(p);
#else
   (void)s;
   return rd_hash_soft(p);
#endif
}

/* Insert position `pos` into its hash chain; return the previous head
 * as a position, or -1 if the chain was empty.
 *
 * head[] and prev[] hold pos+1, so that 0 - what calloc already put
 * there - means "empty".  The tables are 128 KB together; filling
 * them with a -1 sentinel at construction cost more than compressing
 * a small payload does, and made stream setup 7x slower than zlib's.
 * With a zero sentinel the fill disappears and the pages are faulted
 * in on demand, so a short input only ever touches the buckets it
 * uses. */
static int32_t rd_insert(struct rdeflate *s, uint32_t pos)
{
   uint32_t h    = rd_hash(s, s->win + pos);
   int32_t  prev = s->head[h];
   s->prev[pos & RD_WMASK] = prev;
   s->head[h] = (int32_t)pos + 1;
   return prev - 1;
}

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
    * occurrence recorded in prev[].  Chain links are stored as
    * position+1 with 0 for "empty" (see rd_insert), so `cur` is
    * one-based throughout this walk and `cpos` is the position. */
   int32_t cur         = s->prev[pos & RD_WMASK];
   if (cur == 0)
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

      while (cur != 0 && (uint32_t)(cur - 1) >= limit && chain-- > 0)
      {
         uint32_t cpos    = (uint32_t)(cur - 1);
         const uint8_t *m = win + cpos;
         uint16_t m_end;
         /* Quick reject: a candidate can only beat best_len if the two bytes
          * bracketing the current best both match.  Compared as one 16-bit
          * load, this rejects the vast majority of candidates cheaply. */
         memcpy(&m_end, m + best_len - 1, 2);
         if (m_end != scan_end)
         {
            cur = prev[cpos & RD_WMASK];
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
                  /* first differing byte is the 
                   * most-significant nonzero byte */
                  l += compat_clz_u64(x) >> 3;
#else
                  /* first differing byte is the least-significant 
                   * nonzero byte */
                  l += compat_ctz_u64(x) >> 3;
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
               best_dist = pos - cpos;
               if (l >= max_len || l >= (uint32_t)s->nice)
                  break;
               memcpy(&scan_end, scan + best_len - 1, 2);
            }
         }
         cur = prev[cpos & RD_WMASK];
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
static void rd_gen_lengths(struct rdeflate *s,
      const uint32_t *freq, int n, int max_bits,
      uint8_t *lengths_out)
{
   int      *idx = s->gl_idx;
   uint32_t *fr  = s->gl_fr;
   int      *lc  = s->gl_lc;
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

   /* Build the Huffman tree with the two-queue method.  The previous
    * revision kept one sorted array of live nodes and, for each of
    * the m-1 merges, scanned it for the insertion point and shifted
    * the tail up - O(m^2) overall, and the dominant cost of
    * compressing data that compresses well, where blocks are short
    * and the tables are rebuilt often (36% of level-1 instructions
    * on a savestate-like corpus).
    *
    * No search is needed: the leaves are already sorted ascending
    * above, and merges produce internal nodes in nondecreasing
    * weight order too, so two FIFOs - leaves and internal nodes -
    * always hold the global minimum at one of their two fronts.
    * Each merge is then O(1) and the build is O(m).  Internal nodes
    * are allocated at indices >= m in creation order, so a parent's
    * index always exceeds both children's; walking nodes downward
    * once assigns every depth without the per-leaf parent-chain
    * walk the old code did. */
   {
      uint32_t *wt    = s->gl_wt;
      int      *left  = s->gl_left;
      int      *right = s->gl_right;
      int      *depth = s->gl_depth;
      int      lq = 0;   /* front of the leaf queue                   */
      int      iq = 288; /* front of the internal queue (base 288)    */
      int      node_used;

      for (i = 0; i < m; i++)
         wt[i] = fr[i];
      /* internal nodes live at 288.. so the two queues never alias */
      node_used = 288;
      iq        = 288;

      while ((m - lq) + (node_used - iq) > 1)
      {
         int x, y, nd;
         if (lq < m && (iq >= node_used || wt[lq] <= wt[iq]))
            x = lq++;
         else
            x = iq++;
         if (lq < m && (iq >= node_used || wt[lq] <= wt[iq]))
            y = lq++;
         else
            y = iq++;
         nd        = node_used++;
         wt[nd]    = wt[x] + wt[y];
         left[nd]  = x;
         right[nd] = y;
      }

      depth[node_used - 1] = 0;
      for (i = node_used - 1; i >= 288; i--)
      {
         int d = depth[i] + 1;
         depth[left[i]]  = d;
         depth[right[i]] = d;
      }
      for (i = 0; i < m; i++)
         lc[i] = depth[i] ? depth[i] : 1;
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

   rd_gen_lengths(s, s->freq_lit, 286, 15, s->dyn_lit_len);
   rd_gen_lengths(s, s->freq_dist, 30, 15, s->dyn_dist_len);

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
   rd_gen_lengths(s, cl_freq, 19, 7, s->dyn_cl_len);
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
   /* Same window_bits convention as the decoder: >= 16 selects gzip.
    * Auto-detect has no meaning when writing, so it is treated as gzip -
    * the container a caller passing 47 to zlib would have got. */
   s->wrap    = rinf_wrap_from_bits(window_bits);
   if (s->wrap == RINF_WRAP_AUTO)
      s->wrap = RINF_WRAP_GZIP;
   s->wrapped = (s->wrap != RINF_WRAP_RAW);
   s->crc     = encoding_crc32(0, NULL, 0);
   s->total_in = 0;
   s->adler   = 1;
   rd_set_level(s);
#if defined(RD_CRC32_HASH_RUNTIME)
   s->use_crc_hash = (cpu_features_get() & RETRO_SIMD_SSE42) ? 1 : 0;
#endif
   /* head[]/prev[] need no initialisation: calloc's zeroes already
    * mean "empty chain" (see rd_insert). */
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

   /* 0) container header once, at stream start */
   if (s->wrap == RINF_WRAP_GZIP && !s->header_done)
   {
      /* RFC 1952: ID1 ID2 CM FLG MTIME[4] XFL OS.  No optional fields,
       * no mtime (0 means "not available"), OS 255 "unknown" - the same
       * minimal header zlib emits when it is not given a gz_header. */
      static const uint8_t gz_hdr[10] =
         { 0x1f, 0x8b, 0x08, 0x00, 0, 0, 0, 0, 0x00, 0xff };
      if (s->out_pos + sizeof(gz_hdr) > s->out_size)
         goto suspend;
      memcpy(s->out + s->out_pos, gz_hdr, sizeof(gz_hdr));
      s->out_pos    += sizeof(gz_hdr);
      s->header_done = 1;
   }
   if (s->wrap == RINF_WRAP_ZLIB && !s->header_done)
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
      if (s->wrap == RINF_WRAP_ZLIB)
         s->adler = rd_adler32_update(s->adler, s->in + s->in_pos, n);
      else if (s->wrap == RINF_WRAP_GZIP)
      {
         s->crc      = encoding_crc32(s->crc, s->in + s->in_pos, n);
         s->total_in += (uint32_t)n;
      }
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
            /* one-based links: an entry survives when its position
             * (value-1) is at or above the slide, i.e. value > slide */
            for (i = 0; i < RD_HASH_SIZE; i++)
               s->head[i] = (s->head[i] > (int32_t)slide)
                  ? s->head[i] - (int32_t)slide : 0;
            for (i = 0; i < RD_WINDOW; i++)
               s->prev[i] = (s->prev[i] > (int32_t)slide)
                  ? s->prev[i] - (int32_t)slide : 0;
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
            if (s->wrap == RINF_WRAP_ZLIB)
               s->adler = rd_adler32_update(s->adler, s->in + s->in_pos, n);
            else if (s->wrap == RINF_WRAP_GZIP)
            {
               s->crc      = encoding_crc32(s->crc, s->in + s->in_pos, n);
               s->total_in += (uint32_t)n;
            }
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
      if (s->wrap == RINF_WRAP_ZLIB)
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
      else if (s->wrap == RINF_WRAP_GZIP)
      {
         /* crc32 then ISIZE, both little-endian, unlike zlib's adler */
         while (s->trailer_cursor < 8)
         {
            const uint32_t v = (s->trailer_cursor < 4) ? s->crc : s->total_in;
            const int      i = s->trailer_cursor & 3;
            if (s->out_pos >= s->out_size)
               goto suspend;
            s->out[s->out_pos++] = (uint8_t)((v >> (8 * i)) & 0xff);
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

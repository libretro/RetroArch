/* Copyright  (C) 2010-2020 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rjpeg.c).
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

/* Modified version of stb_image's JPEG sources. */

#include <stdint.h>
#include <stdarg.h>
#include <stddef.h> /* ptrdiff_t on osx */
#include <stdlib.h>
#include <string.h>

#include <retro_inline.h>
#include <boolean.h>
#include <formats/image.h>
#include <formats/rjpeg.h>
#include <features/features_cpu.h>

enum
{
   RJPEG_DEFAULT = 0, /* only used for req_comp */
   RJPEG_GREY,
   RJPEG_GREY_ALPHA,
   RJPEG_RGB,
   RJPEG_RGB_ALPHA
};

enum
{
   RJPEG_SCAN_LOAD = 0,
   RJPEG_SCAN_TYPE,
   RJPEG_SCAN_HEADER
};

typedef uint8_t *(*rjpeg_resample_row_func)(uint8_t *out, uint8_t *in0, uint8_t *in1,
                                    int w, int hs);

typedef struct
{
   rjpeg_resample_row_func resample;
   uint8_t *line0;
   uint8_t *line1;
   int hs,vs;   /* expansion factor in each axis */
   int w_lores; /* horizontal pixels pre-expansion */
   int ystep;   /* how far through vertical expansion we are */
   int ypos;    /* which pre-expansion row we're on */
} rjpeg_resample;

struct rjpeg
{
   uint8_t *buff_data;
};

#ifdef _MSC_VER
#define RJPEG_HAS_LROTL
#endif

#ifdef RJPEG_HAS_LROTL
   #define RJPEG_LROT(x,y)  _lrotl(x,y)
#else
   #define RJPEG_LROT(x,y)  (((x) << (y)) | ((x) >> (32 - (y))))
#endif

/* x86/x64 detection */
#if defined(__x86_64__) || defined(_M_X64)
#define RJPEG_X64_TARGET
#elif defined(__i386) || defined(_M_IX86)
#define RJPEG_X86_TARGET
#endif

#if defined(__GNUC__) && (defined(RJPEG_X86_TARGET) || defined(RJPEG_X64_TARGET)) && !defined(__SSE2__) && !defined(RJPEG_NO_SIMD)
/* NOTE: not clear do we actually need this for the 64-bit path?
 * gcc doesn't support sse2 intrinsics unless you compile with -msse2,
 * (but compiling with -msse2 allows the compiler to use SSE2 everywhere;
 * this is just broken and gcc are jerks for not fixing it properly
 * http://www.virtualdub.org/blog/pivot/entry.php?id=363 )
 */
#define RJPEG_NO_SIMD
#endif

#if defined(__MINGW32__) && defined(RJPEG_X86_TARGET) && !defined(RJPEG_MINGW_ENABLE_SSE2) && !defined(RJPEG_NO_SIMD)
/* Note that __MINGW32__ doesn't actually mean 32-bit, so we have to avoid RJPEG_X64_TARGET
 *
 * 32-bit MinGW wants ESP to be 16-byte aligned, but this is not in the
 * Windows ABI and VC++ as well as Windows DLLs don't maintain that invariant.
 * As a result, enabling SSE2 on 32-bit MinGW is dangerous when not
 * simultaneously enabling "-mstackrealign".
 *
 * See https://github.com/nothings/stb/issues/81 for more information.
 *
 * So default to no SSE2 on 32-bit MinGW. If you've read this far and added
 * -mstackrealign to your build settings, feel free to #define RJPEG_MINGW_ENABLE_SSE2.
 */
#define RJPEG_NO_SIMD
#endif

#if defined(__SSE2__)
#include <emmintrin.h>

#ifdef _MSC_VER
#define RJPEG_SIMD_ALIGN(type, name) __declspec(align(16)) type name
#else
#define RJPEG_SIMD_ALIGN(type, name) type name __attribute__((aligned(16)))
#endif

#endif

/* ARM NEON */
#if defined(RJPEG_NO_SIMD) && defined(RJPEG_NEON)
#undef RJPEG_NEON
#endif

#ifdef RJPEG_NEON
#include <arm_neon.h>
/* assume GCC or Clang on ARM targets */
#define RJPEG_SIMD_ALIGN(type, name) type name __attribute__((aligned(16)))
#endif

#ifndef RJPEG_SIMD_ALIGN
#define RJPEG_SIMD_ALIGN(type, name) type name
#endif

typedef struct
{
   uint8_t *img_buffer;
   uint8_t *img_buffer_end;
   uint8_t *img_buffer_original;
   int      img_n;
   int      img_out_n;
   int      buflen;
   uint32_t img_x;
   uint32_t img_y;
   uint8_t  buffer_start[128];
} rjpeg_context;

static INLINE uint8_t rjpeg_get8(rjpeg_context *s)
{
   if (s->img_buffer < s->img_buffer_end)
      return *s->img_buffer++;

   return 0;
}

#define RJPEG_AT_EOF(s)     ((s)->img_buffer >= (s)->img_buffer_end)

#define RJPEG_GET16BE(s)    ((rjpeg_get8((s)) << 8) + rjpeg_get8((s)))

/* huffman decoding acceleration */
#define FAST_BITS   9  /* larger handles more cases; smaller stomps less cache */

typedef struct
{
   unsigned int maxcode[18];
   int    delta[17];   /* old 'firstsymbol' - old 'firstcode' */
   /* weirdly, repacking this into AoS is a 10% speed loss, instead of a win */
   uint16_t code[256];
   uint8_t  fast[1 << FAST_BITS];
   uint8_t  values[256];
   uint8_t  size[257];
} rjpeg_huffman;

typedef struct
{
   rjpeg_context *s;
   /* kernels */
   void (*idct_block_kernel)(uint8_t *out, int out_stride, short data[64]);
   void (*YCbCr_to_RGB_kernel)(uint8_t *out, const uint8_t *y, const uint8_t *pcb,
         const uint8_t *pcr, int count, int step);
   uint8_t *(*resample_row_hv_2_kernel)(uint8_t *out, uint8_t *in_near,
         uint8_t *in_far, int w, int hs);

   /* definition of jpeg image component */
   struct
   {
      uint8_t *data;
      void *raw_data, *raw_coeff;
      uint8_t *linebuf;
      short   *coeff;            /* progressive only */
      int id;
      int h,v;
      int tq;
      int hd,ha;
      int dc_pred;

      int x,y,w2,h2;
      int      coeff_w;          /* number of 8x8 coefficient blocks */
      int      coeff_h;          /* number of 8x8 coefficient blocks */
   } img_comp[4];

   /* sizes for components, interleaved MCUs */
   int img_h_max, img_v_max;
   int img_mcu_x, img_mcu_y;
   int img_mcu_w, img_mcu_h;

   int            code_bits;     /* number of valid bits */
   int            nomore;        /* flag if we saw a marker so must stop */
   int            progressive;
   int            spec_start;
   int            spec_end;
   int            succ_high;
   int            succ_low;
   int            eob_run;
   int scan_n, order[4];
   int restart_interval, todo;
   uint32_t       code_buffer;   /* jpeg entropy-coded buffer */
   rjpeg_huffman huff_dc[4];     /* unsigned int alignment */
   rjpeg_huffman huff_ac[4];     /* unsigned int alignment */
   int16_t fast_ac[4][1 << FAST_BITS];
   unsigned char  marker;        /* marker seen while filling entropy buffer */
   uint8_t dequant[4][64];
} rjpeg_jpeg;

#define RJPEG_F2F(x)  ((int) (((x) * 4096 + 0.5)))
#define RJPEG_FSH(x)  ((x) << 12)

#define RJPEG_MARKER_NONE  0xff
/* if there's a pending marker from the entropy stream, return that
 * otherwise, fetch from the stream and get a marker. if there's no
 * marker, return 0xff, which is never a valid marker value
 */

/* in each scan, we'll have scan_n components, and the order
 * of the components is specified by order[]
 */
#define RJPEG_RESTART(x)     ((x) >= 0xd0 && (x) <= 0xd7)

#define JPEG_MARKER           0xFF
#define JPEG_MARKER_SOI       0xD8
#define JPEG_MARKER_SOS       0xDA
#define JPEG_MARKER_EOI       0xD9
#define JPEG_MARKER_APP1      0xE1
#define JPEG_MARKER_APP2      0xE2

/* use comparisons since in some cases we handle more than one case (e.g. SOF) */
#define RJPEG_SOF(x)               ((x) == 0xc0 || (x) == 0xc1 || (x) == 0xc2)

#define RJPEG_SOF_PROGRESSIVE(x)   ((x) == 0xc2)
#define RJPEG_DIV4(x)              ((uint8_t) ((x) >> 2))
#define RJPEG_DIV16(x)             ((uint8_t) ((x) >> 4))

static int rjpeg_build_huffman(rjpeg_huffman *h, int *count)
{
   int i,j,k = 0,code;

   /* build size list for each symbol (from JPEG spec) */
   for (i = 0; i < 16; ++i)
      for (j = 0; j < count[i]; ++j)
         h->size[k++] = (uint8_t) (i+1);

   h->size[k] = 0;
   /* compute actual symbols (from jpeg spec) */
   code       = 0;
   k          = 0;

   for (j = 1; j <= 16; ++j)
   {
      /* compute delta to add to code to compute symbol id */
      h->delta[j] = k - code;
      if (h->size[k] == j)
      {
         while (h->size[k] == j)
            h->code[k++] = (uint16_t) (code++);

         /* Bad code lengths, corrupt JPEG? */
         if (code-1 >= (1 << j))
            return 0;
      }
      /* compute largest code + 1 for this size, preshifted as needed later */
      h->maxcode[j] = code << (16-j);
      code <<= 1;
   }
   h->maxcode[j] = 0xffffffff;

   /* build non-spec acceleration table; 255 is flag for not-accelerated */
   memset(h->fast, 255, 1 << FAST_BITS);
   for (i = 0; i < k; ++i)
   {
      int s = h->size[i];
      if (s <= FAST_BITS)
      {
         int c = h->code[i] << (FAST_BITS-s);
         int m = 1 << (FAST_BITS-s);
         for (j = 0; j < m; ++j)
            h->fast[c+j] = (uint8_t) i;
      }
   }
   return 1;
}

/* build a table that decodes both magnitude and value of small ACs in
 * one go. */
static void rjpeg_build_fast_ac(int16_t *fast_ac, rjpeg_huffman *h)
{
   int i;

   for (i = 0; i < (1 << FAST_BITS); ++i)
   {
      uint8_t fast = h->fast[i];

      fast_ac[i] = 0;

      if (fast < 255)
      {
         int rs      = h->values[fast];
         int run     = (rs >> 4) & 15;
         int magbits = rs & 15;
         int len     = h->size[fast];

         if (magbits && len + magbits <= FAST_BITS)
         {
            /* magnitude code followed by receive_extend code */
            int k = ((i << len) & ((1 << FAST_BITS) - 1)) >> (FAST_BITS - magbits);
            int m = 1 << (magbits - 1);
            if (k < m)
               k += (-1 << magbits) + 1;

            /* if the result is small enough, we can fit it in fast_ac table */
            if (k >= -128 && k <= 127)
               fast_ac[i] = (int16_t) ((k << 8) + (run << 4) + (len + magbits));
         }
      }
   }
}

static void rjpeg_grow_buffer_unsafe(rjpeg_jpeg *j)
{
   do
   {
      int b = j->nomore ? 0 : rjpeg_get8(j->s);
      if (b == 0xff)
      {
         int c = rjpeg_get8(j->s);

         if (c != 0)
         {
            j->marker = (unsigned char) c;
            j->nomore = 1;
            return;
         }
      }
      j->code_buffer |= b << (24 - j->code_bits);
      j->code_bits   += 8;
   } while (j->code_bits <= 24);
}

/* (1 << n) - 1 */
static uint32_t rjpeg_bmask[17]={0,1,3,7,15,31,63,127,255,511,1023,2047,4095,8191,16383,32767,65535};

/* decode a JPEG huffman value from the bitstream */
static INLINE int rjpeg_jpeg_huff_decode(rjpeg_jpeg *j, rjpeg_huffman *h)
{
   unsigned int temp;
   int c,k;

   if (j->code_bits < 16)
      rjpeg_grow_buffer_unsafe(j);

   /* look at the top FAST_BITS and determine what symbol ID it is,
    * if the code is <= FAST_BITS */
   c = (j->code_buffer >> (32 - FAST_BITS)) & ((1 << FAST_BITS)-1);
   k = h->fast[c];

   if (k < 255)
   {
      int s = h->size[k];
      if (s > j->code_bits)
         return -1;
      j->code_buffer <<= s;
      j->code_bits -= s;
      return h->values[k];
   }

   /* naive test is to shift the code_buffer down so k bits are
    * valid, then test against maxcode. To speed this up, we've
    * preshifted maxcode left so that it has (16-k) 0s at the
    * end; in other words, regardless of the number of bits, it
    * wants to be compared against something shifted to have 16;
    * that way we don't need to shift inside the loop. */
   temp = j->code_buffer >> 16;
   for (k=FAST_BITS+1 ; ; ++k)
      if (temp < h->maxcode[k])
         break;

   if (k == 17)
   {
      /* error! code not found */
      j->code_bits -= 16;
      return -1;
   }

   if (k > j->code_bits)
      return -1;

   /* convert the huffman code to the symbol id */
   c = ((j->code_buffer >> (32 - k)) & rjpeg_bmask[k]) + h->delta[k];

   /* convert the id to a symbol */
   j->code_bits -= k;
   j->code_buffer <<= k;
   return h->values[c];
}

/* bias[n] = (-1<<n) + 1 */
static int const rjpeg_jbias[16] = {0,-1,-3,-7,-15,-31,-63,-127,-255,-511,-1023,-2047,-4095,-8191,-16383,-32767};

/* combined JPEG 'receive' and JPEG 'extend', since baseline
 * always extends everything it receives. */
static INLINE int rjpeg_extend_receive(rjpeg_jpeg *j, int n)
{
   unsigned int k;
   int sgn;
   if (j->code_bits < n)
      rjpeg_grow_buffer_unsafe(j);

   sgn             = (int32_t)j->code_buffer >> 31; /* sign bit is always in MSB */
   k               = RJPEG_LROT(j->code_buffer, n);
   j->code_buffer  = k & ~rjpeg_bmask[n];
   k              &= rjpeg_bmask[n];
   j->code_bits   -= n;
   return k + (rjpeg_jbias[n] & ~sgn);
}

/* get some unsigned bits */
static INLINE int rjpeg_jpeg_get_bits(rjpeg_jpeg *j, int n)
{
   unsigned int k;
   if (j->code_bits < n)
      rjpeg_grow_buffer_unsafe(j);
   k              = RJPEG_LROT(j->code_buffer, n);
   j->code_buffer = k & ~rjpeg_bmask[n];
   k             &= rjpeg_bmask[n];
   j->code_bits  -= n;
   return k;
}

static INLINE int rjpeg_jpeg_get_bit(rjpeg_jpeg *j)
{
   unsigned int k;
   if (j->code_bits < 1)
      rjpeg_grow_buffer_unsafe(j);

   k                = j->code_buffer;
   j->code_buffer <<= 1;
   --j->code_bits;
   return k & 0x80000000;
}

/* given a value that's at position X in the zigzag stream,
 * where does it appear in the 8x8 matrix coded as row-major? */
static uint8_t rjpeg_jpeg_dezigzag[64+15] =
{
    0,  1,  8, 16,  9,  2,  3, 10,
   17, 24, 32, 25, 18, 11,  4,  5,
   12, 19, 26, 33, 40, 48, 41, 34,
   27, 20, 13,  6,  7, 14, 21, 28,
   35, 42, 49, 56, 57, 50, 43, 36,
   29, 22, 15, 23, 30, 37, 44, 51,
   58, 59, 52, 45, 38, 31, 39, 46,
   53, 60, 61, 54, 47, 55, 62, 63,
   /* let corrupt input sample past end */
   63, 63, 63, 63, 63, 63, 63, 63,
   63, 63, 63, 63, 63, 63, 63
};

/* decode one 64-entry block-- */
static int rjpeg_jpeg_decode_block(
      rjpeg_jpeg *j, short data[64],
      rjpeg_huffman *hdc,
      rjpeg_huffman *hac,
      int16_t *fac,
      int b,
      uint8_t *dequant)
{
   int dc,k;
   int t;
   int diff      = 0;

   if (j->code_bits < 16)
      rjpeg_grow_buffer_unsafe(j);
   t = rjpeg_jpeg_huff_decode(j, hdc);

   /* Bad huffman code. Corrupt JPEG? */
   if (t < 0)
      return 0;

   /* 0 all the ac values now so we can do it 32-bits at a time */
   memset(data,0,64*sizeof(data[0]));

   if (t)
      diff                = rjpeg_extend_receive(j, t);
   dc                     = j->img_comp[b].dc_pred + diff;
   j->img_comp[b].dc_pred = dc;
   data[0]                = (short) (dc * dequant[0]);

   /* decode AC components, see JPEG spec */
   k                      = 1;
   do
   {
      unsigned int zig;
      int c,r,s;
      if (j->code_bits < 16)
         rjpeg_grow_buffer_unsafe(j);
      c = (j->code_buffer >> (32 - FAST_BITS)) & ((1 << FAST_BITS)-1);
      r = fac[c];
      if (r)
      {
         /* fast-AC path */
         k               += (r >> 4) & 15; /* run */
         s                = r & 15; /* combined length */
         j->code_buffer <<= s;
         j->code_bits    -= s;
         /* decode into unzigzag'd location */
         zig              = rjpeg_jpeg_dezigzag[k++];
         data[zig]        = (short) ((r >> 8) * dequant[zig]);
      }
      else
      {
         int rs = rjpeg_jpeg_huff_decode(j, hac);

         /* Bad huffman code. Corrupt JPEG? */
         if (rs < 0)
            return 0;

         s = rs & 15;
         r = rs >> 4;
         if (s == 0)
         {
            if (rs != 0xf0)
               break; /* end block */
            k += 16;
         }
         else
         {
            k += r;
            /* decode into unzigzag'd location */
            zig = rjpeg_jpeg_dezigzag[k++];
            data[zig] = (short) (rjpeg_extend_receive(j,s) * dequant[zig]);
         }
      }
   } while (k < 64);
   return 1;
}

static int rjpeg_jpeg_decode_block_prog_dc(
      rjpeg_jpeg *j,
      short data[64],
      rjpeg_huffman *hdc,
      int b)
{
   /* Can't merge DC and AC. Corrupt JPEG? */
   if (j->spec_end != 0)
      return 0;

   if (j->code_bits < 16)
      rjpeg_grow_buffer_unsafe(j);

   if (j->succ_high == 0)
   {
      int t;
      int dc;
      int diff = 0;

      /* first scan for DC coefficient, must be first */
      memset(data,0,64*sizeof(data[0])); /* 0 all the ac values now */
      t       = rjpeg_jpeg_huff_decode(j, hdc);
      if (t)
         diff = rjpeg_extend_receive(j, t);

      dc      = j->img_comp[b].dc_pred + diff;
      j->img_comp[b].dc_pred = dc;
      data[0] = (short) (dc << j->succ_low);
   }
   else
   {
      /* refinement scan for DC coefficient */
      if (rjpeg_jpeg_get_bit(j))
         data[0] += (short) (1 << j->succ_low);
   }
   return 1;
}

static int rjpeg_jpeg_decode_block_prog_ac(
      rjpeg_jpeg *j,
      short data[64],
      rjpeg_huffman *hac,
      int16_t *fac)
{
   int k;

   /* Can't merge DC and AC. Corrupt JPEG? */
   if (j->spec_start == 0)
      return 0;

   if (j->succ_high == 0)
   {
      int shift = j->succ_low;

      if (j->eob_run)
      {
         --j->eob_run;
         return 1;
      }

      k = j->spec_start;
      do
      {
         unsigned int zig;
         int c,r,s;
         if (j->code_bits < 16)
            rjpeg_grow_buffer_unsafe(j);
         c = (j->code_buffer >> (32 - FAST_BITS)) & ((1 << FAST_BITS)-1);
         r = fac[c];
         if (r)
         {
            /* fast-AC path */
            k               += (r >> 4) & 15; /* run */
            s                = r & 15; /* combined length */
            j->code_buffer <<= s;
            j->code_bits    -= s;
            zig              = rjpeg_jpeg_dezigzag[k++];
            data[zig]        = (short) ((r >> 8) << shift);
         }
         else
         {
            int rs = rjpeg_jpeg_huff_decode(j, hac);

            /* Bad huffman code. Corrupt JPEG? */
            if (rs < 0)
               return 0;

            s = rs & 15;
            r = rs >> 4;
            if (s == 0)
            {
               if (r < 15)
               {
                  j->eob_run = (1 << r);
                  if (r)
                     j->eob_run += rjpeg_jpeg_get_bits(j, r);
                  --j->eob_run;
                  break;
               }
               k += 16;
            }
            else
            {
               k         += r;
               zig        = rjpeg_jpeg_dezigzag[k++];
               data[zig]  = (short) (rjpeg_extend_receive(j,s) << shift);
            }
         }
      } while (k <= j->spec_end);
   }
   else
   {
      /* refinement scan for these AC coefficients */

      short bit = (short) (1 << j->succ_low);

      if (j->eob_run)
      {
         --j->eob_run;
         for (k = j->spec_start; k <= j->spec_end; ++k)
         {
            short *p = &data[rjpeg_jpeg_dezigzag[k]];
            if (*p != 0)
               if (rjpeg_jpeg_get_bit(j))
                  if ((*p & bit) == 0)
                  {
                     if (*p > 0)
                        *p += bit;
                     else
                        *p -= bit;
                  }
         }
      }
      else
      {
         k = j->spec_start;
         do
         {
            int r,s;
            int rs = rjpeg_jpeg_huff_decode(j, hac);

            /* Bad huffman code. Corrupt JPEG? */
            if (rs < 0)
               return 0;

            s = rs & 15;
            r = rs >> 4;
            if (s == 0)
            {
               if (r < 15)
               {
                  j->eob_run = (1 << r) - 1;
                  if (r)
                     j->eob_run += rjpeg_jpeg_get_bits(j, r);
                  r = 64; /* force end of block */
               }
               else
               {
                  /* r=15 s=0 should write 16 0s, so we just do
                   * a run of 15 0s and then write s (which is 0),
                   * so we don't have to do anything special here */
               }
            }
            else
            {
               /* Bad huffman code. Corrupt JPEG? */
               if (s != 1)
                  return 0;

               /* sign bit */
               if (rjpeg_jpeg_get_bit(j))
                  s = bit;
               else
                  s = -bit;
            }

            /* advance by r */
            while (k <= j->spec_end)
            {
               short *p = &data[rjpeg_jpeg_dezigzag[k++]];
               if (*p != 0)
               {
                  if (rjpeg_jpeg_get_bit(j))
                     if ((*p & bit) == 0)
                     {
                        if (*p > 0)
                           *p += bit;
                        else
                           *p -= bit;
                     }
               }
               else
               {
                  if (r == 0)
                  {
                     *p = (short) s;
                     break;
                  }
                  --r;
               }
            }
         } while (k <= j->spec_end);
      }
   }
   return 1;
}

/* take a -128..127 value and rjpeg_clamp it and convert to 0..255 */
static INLINE uint8_t rjpeg_clamp(int x)
{
   /* trick to use a single test to catch both cases */
   if ((unsigned int) x > 255)
      return 255;
   return (uint8_t) x;
}

/* derived from jidctint -- DCT_ISLOW */
#define RJPEG_IDCT_1D(s0,s1,s2,s3,s4,s5,s6,s7) \
   int t0,t1,p4,p5,x0,x1,x2,x3; \
   int p2 = s2;                                \
   int p3 = s6;                                \
   int p1 = (p2+p3) * RJPEG_F2F(0.5411961f);   \
   int t2 = p1 + p3 * RJPEG_F2F(-1.847759065f);\
   int t3 = p1 + p2 * RJPEG_F2F( 0.765366865f);\
   p2 = s0;                                    \
   p3 = s4;                                    \
   t0 = RJPEG_FSH(p2+p3);                      \
   t1 = RJPEG_FSH(p2-p3);                      \
   x0 = t0+t3;                                 \
   x3 = t0-t3;                                 \
   x1 = t1+t2;                                 \
   x2 = t1-t2;                                 \
   t0 = s7;                                    \
   t1 = s5;                                    \
   t2 = s3;                                    \
   t3 = s1;                                    \
   p3 = t0+t2;                                 \
   p4 = t1+t3;                                 \
   p1 = t0+t3;                                 \
   p2 = t1+t2;                                 \
   p5 = (p3+p4) * RJPEG_F2F( 1.175875602f);    \
   t0 = t0      * RJPEG_F2F( 0.298631336f);    \
   t1 = t1      * RJPEG_F2F( 2.053119869f);    \
   t2 = t2      * RJPEG_F2F( 3.072711026f);    \
   t3 = t3      * RJPEG_F2F( 1.501321110f);    \
   p1 = p5 + p1 * RJPEG_F2F(-0.899976223f);    \
   p2 = p5 + p2 * RJPEG_F2F(-2.562915447f);    \
   p3 = p3      * RJPEG_F2F(-1.961570560f);    \
   p4 = p4      * RJPEG_F2F(-0.390180644f);    \
   t3 += p1+p4;                                \
   t2 += p2+p3;                                \
   t1 += p2+p4;                                \
   t0 += p1+p3

static void rjpeg_idct_block(uint8_t *out, int out_stride, short data[64])
{
   int i,val[64],*v=val;
   uint8_t   *o = NULL;
   int16_t   *d = data;

   /* columns */
   for (i = 0; i < 8; ++i,++d, ++v)
   {
      /* if all zeroes, shortcut -- this avoids dequantizing 0s and IDCTing */
      if (     d[ 8] == 0
            && d[16] == 0
            && d[24] == 0
            && d[32] == 0
            && d[40] == 0
            && d[48] == 0
            && d[56] == 0)
      {
         /*    no shortcut                 0     seconds
          *    (1|2|3|4|5|6|7)==0          0     seconds
          *    all separate               -0.047 seconds
          *    1 && 2|3 && 4|5 && 6|7:    -0.047 seconds */
         int dcterm = d[0] << 2;
         v[0] = v[8] = v[16] = v[24] = v[32] = v[40] = v[48] = v[56] = dcterm;
      }
      else
      {
         RJPEG_IDCT_1D(d[ 0],d[ 8],d[16],d[24],d[32],d[40],d[48],d[56]);

         /* constants scaled things up by 1<<12; let's bring them back
          * down, but keep 2 extra bits of precision */
         x0 += 512;
         x1 += 512;
         x2 += 512;
         x3 += 512;

         v[ 0] = (x0+t3) >> 10;
         v[56] = (x0-t3) >> 10;
         v[ 8] = (x1+t2) >> 10;
         v[48] = (x1-t2) >> 10;
         v[16] = (x2+t1) >> 10;
         v[40] = (x2-t1) >> 10;
         v[24] = (x3+t0) >> 10;
         v[32] = (x3-t0) >> 10;
      }
   }

   for (i = 0, v=val, o=out; i < 8; ++i,v+=8,o+=out_stride)
   {
      /* no fast case since the first 1D IDCT spread components out */
      RJPEG_IDCT_1D(v[0],v[1],v[2],v[3],v[4],v[5],v[6],v[7]);

      /* constants scaled things up by 1<<12, plus we had 1<<2 from first
       * loop, plus horizontal and vertical each scale by sqrt(8) so together
       * we've got an extra 1<<3, so 1<<17 total we need to remove.
       * so we want to round that, which means adding 0.5 * 1<<17,
       * aka 65536. Also, we'll end up with -128 to 127 that we want
       * to encode as 0..255 by adding 128, so we'll add that before the shift
       */
      x0 += 65536 + (128<<17);
      x1 += 65536 + (128<<17);
      x2 += 65536 + (128<<17);
      x3 += 65536 + (128<<17);

      /* Tried computing the shifts into temps, or'ing the temps to see
       * if any were out of range, but that was slower */
      o[0] = rjpeg_clamp((x0+t3) >> 17);
      o[7] = rjpeg_clamp((x0-t3) >> 17);
      o[1] = rjpeg_clamp((x1+t2) >> 17);
      o[6] = rjpeg_clamp((x1-t2) >> 17);
      o[2] = rjpeg_clamp((x2+t1) >> 17);
      o[5] = rjpeg_clamp((x2-t1) >> 17);
      o[3] = rjpeg_clamp((x3+t0) >> 17);
      o[4] = rjpeg_clamp((x3-t0) >> 17);
   }
}

#if defined(__SSE2__)
/* sse2 integer IDCT. not the fastest possible implementation but it
 * produces bit-identical results to the generic C version so it's
 * fully "transparent".
 */
static void rjpeg_idct_simd(uint8_t *out, int out_stride, short data[64])
{
   /* This is constructed to match our regular (generic) integer IDCT exactly. */
   __m128i row0, row1, row2, row3, row4, row5, row6, row7;
   __m128i tmp;

   /* dot product constant: even elems=x, odd elems=y */
   #define dct_const(x,y)  _mm_setr_epi16((x),(y),(x),(y),(x),(y),(x),(y))

   /* out(0) = c0[even]*x + c0[odd]*y   (c0, x, y 16-bit, out 32-bit)
    * out(1) = c1[even]*x + c1[odd]*y
    */
   #define dct_rot(out0,out1, x,y,c0,c1) \
      __m128i c0##lo   = _mm_unpacklo_epi16((x),(y)); \
      __m128i c0##hi   = _mm_unpackhi_epi16((x),(y)); \
      __m128i out0##_l = _mm_madd_epi16(c0##lo, c0); \
      __m128i out0##_h = _mm_madd_epi16(c0##hi, c0); \
      __m128i out1##_l = _mm_madd_epi16(c0##lo, c1); \
      __m128i out1##_h = _mm_madd_epi16(c0##hi, c1)

   /* out = in << 12  (in 16-bit, out 32-bit) */
   #define dct_widen(out, in) \
      __m128i out##_l = _mm_srai_epi32(_mm_unpacklo_epi16(_mm_setzero_si128(), (in)), 4); \
      __m128i out##_h = _mm_srai_epi32(_mm_unpackhi_epi16(_mm_setzero_si128(), (in)), 4)

   /* wide add */
   #define dct_wadd(out, a, b) \
      __m128i out##_l = _mm_add_epi32(a##_l, b##_l); \
      __m128i out##_h = _mm_add_epi32(a##_h, b##_h)

   /* wide sub */
   #define dct_wsub(out, a, b) \
      __m128i out##_l = _mm_sub_epi32(a##_l, b##_l); \
      __m128i out##_h = _mm_sub_epi32(a##_h, b##_h)

   /* butterfly a/b, add bias, then shift by "s" and pack */
   #define dct_bfly32o(out0, out1, a,b,bias,s) \
      { \
         __m128i abiased_l = _mm_add_epi32(a##_l, bias); \
         __m128i abiased_h = _mm_add_epi32(a##_h, bias); \
         dct_wadd(sum, abiased, b); \
         dct_wsub(dif, abiased, b); \
         out0 = _mm_packs_epi32(_mm_srai_epi32(sum_l, s), _mm_srai_epi32(sum_h, s)); \
         out1 = _mm_packs_epi32(_mm_srai_epi32(dif_l, s), _mm_srai_epi32(dif_h, s)); \
      }

   /* 8-bit interleave step (for transposes) */
   #define dct_interleave8(a, b) \
      tmp = a; \
      a = _mm_unpacklo_epi8(a, b); \
      b = _mm_unpackhi_epi8(tmp, b)

   /* 16-bit interleave step (for transposes) */
   #define dct_interleave16(a, b) \
      tmp = a; \
      a = _mm_unpacklo_epi16(a, b); \
      b = _mm_unpackhi_epi16(tmp, b)

   #define dct_pass(bias,shift) \
      { \
         /* even part */ \
         dct_rot(t2e,t3e, row2,row6, rot0_0,rot0_1); \
         __m128i sum04 = _mm_add_epi16(row0, row4); \
         __m128i dif04 = _mm_sub_epi16(row0, row4); \
         dct_widen(t0e, sum04); \
         dct_widen(t1e, dif04); \
         dct_wadd(x0, t0e, t3e); \
         dct_wsub(x3, t0e, t3e); \
         dct_wadd(x1, t1e, t2e); \
         dct_wsub(x2, t1e, t2e); \
         /* odd part */ \
         dct_rot(y0o,y2o, row7,row3, rot2_0,rot2_1); \
         dct_rot(y1o,y3o, row5,row1, rot3_0,rot3_1); \
         __m128i sum17 = _mm_add_epi16(row1, row7); \
         __m128i sum35 = _mm_add_epi16(row3, row5); \
         dct_rot(y4o,y5o, sum17,sum35, rot1_0,rot1_1); \
         dct_wadd(x4, y0o, y4o); \
         dct_wadd(x5, y1o, y5o); \
         dct_wadd(x6, y2o, y5o); \
         dct_wadd(x7, y3o, y4o); \
         dct_bfly32o(row0,row7, x0,x7,bias,shift); \
         dct_bfly32o(row1,row6, x1,x6,bias,shift); \
         dct_bfly32o(row2,row5, x2,x5,bias,shift); \
         dct_bfly32o(row3,row4, x3,x4,bias,shift); \
      }

   __m128i rot0_0 = dct_const(RJPEG_F2F(0.5411961f), RJPEG_F2F(0.5411961f) + RJPEG_F2F(-1.847759065f));
   __m128i rot0_1 = dct_const(RJPEG_F2F(0.5411961f) + RJPEG_F2F( 0.765366865f), RJPEG_F2F(0.5411961f));
   __m128i rot1_0 = dct_const(RJPEG_F2F(1.175875602f) + RJPEG_F2F(-0.899976223f), RJPEG_F2F(1.175875602f));
   __m128i rot1_1 = dct_const(RJPEG_F2F(1.175875602f), RJPEG_F2F(1.175875602f) + RJPEG_F2F(-2.562915447f));
   __m128i rot2_0 = dct_const(RJPEG_F2F(-1.961570560f) + RJPEG_F2F( 0.298631336f), RJPEG_F2F(-1.961570560f));
   __m128i rot2_1 = dct_const(RJPEG_F2F(-1.961570560f), RJPEG_F2F(-1.961570560f) + RJPEG_F2F( 3.072711026f));
   __m128i rot3_0 = dct_const(RJPEG_F2F(-0.390180644f) + RJPEG_F2F( 2.053119869f), RJPEG_F2F(-0.390180644f));
   __m128i rot3_1 = dct_const(RJPEG_F2F(-0.390180644f), RJPEG_F2F(-0.390180644f) + RJPEG_F2F( 1.501321110f));

   /* rounding biases in column/row passes, see rjpeg_idct_block for explanation. */
   __m128i bias_0 = _mm_set1_epi32(512);
   __m128i bias_1 = _mm_set1_epi32(65536 + (128<<17));

   /* load */
   row0 = _mm_load_si128((const __m128i *) (data + 0*8));
   row1 = _mm_load_si128((const __m128i *) (data + 1*8));
   row2 = _mm_load_si128((const __m128i *) (data + 2*8));
   row3 = _mm_load_si128((const __m128i *) (data + 3*8));
   row4 = _mm_load_si128((const __m128i *) (data + 4*8));
   row5 = _mm_load_si128((const __m128i *) (data + 5*8));
   row6 = _mm_load_si128((const __m128i *) (data + 6*8));
   row7 = _mm_load_si128((const __m128i *) (data + 7*8));

   /* column pass */
   dct_pass(bias_0, 10);

   {
      /* 16bit 8x8 transpose pass 1 */
      dct_interleave16(row0, row4);
      dct_interleave16(row1, row5);
      dct_interleave16(row2, row6);
      dct_interleave16(row3, row7);

      /* transpose pass 2 */
      dct_interleave16(row0, row2);
      dct_interleave16(row1, row3);
      dct_interleave16(row4, row6);
      dct_interleave16(row5, row7);

      /* transpose pass 3 */
      dct_interleave16(row0, row1);
      dct_interleave16(row2, row3);
      dct_interleave16(row4, row5);
      dct_interleave16(row6, row7);
   }

   /* row pass */
   dct_pass(bias_1, 17);

   {
      /* pack */
      __m128i p0 = _mm_packus_epi16(row0, row1); /* a0a1a2a3...a7b0b1b2b3...b7 */
      __m128i p1 = _mm_packus_epi16(row2, row3);
      __m128i p2 = _mm_packus_epi16(row4, row5);
      __m128i p3 = _mm_packus_epi16(row6, row7);

      /* 8bit 8x8 transpose pass 1 */
      dct_interleave8(p0, p2); /* a0e0a1e1... */
      dct_interleave8(p1, p3); /* c0g0c1g1... */

      /* transpose pass 2 */
      dct_interleave8(p0, p1); /* a0c0e0g0... */
      dct_interleave8(p2, p3); /* b0d0f0h0... */

      /* transpose pass 3 */
      dct_interleave8(p0, p2); /* a0b0c0d0... */
      dct_interleave8(p1, p3); /* a4b4c4d4... */

      /* store */
      _mm_storel_epi64((__m128i *) out, p0); out += out_stride;
      _mm_storel_epi64((__m128i *) out, _mm_shuffle_epi32(p0, 0x4e)); out += out_stride;
      _mm_storel_epi64((__m128i *) out, p2); out += out_stride;
      _mm_storel_epi64((__m128i *) out, _mm_shuffle_epi32(p2, 0x4e)); out += out_stride;
      _mm_storel_epi64((__m128i *) out, p1); out += out_stride;
      _mm_storel_epi64((__m128i *) out, _mm_shuffle_epi32(p1, 0x4e)); out += out_stride;
      _mm_storel_epi64((__m128i *) out, p3); out += out_stride;
      _mm_storel_epi64((__m128i *) out, _mm_shuffle_epi32(p3, 0x4e));
   }

#undef dct_const
#undef dct_rot
#undef dct_widen
#undef dct_wadd
#undef dct_wsub
#undef dct_bfly32o
#undef dct_interleave8
#undef dct_interleave16
#undef dct_pass
}

#endif

#ifdef RJPEG_NEON

/* NEON integer IDCT. should produce bit-identical
 * results to the generic C version. */
static void rjpeg_idct_simd(uint8_t *out, int out_stride, short data[64])
{
   int16x8_t row0, row1, row2, row3, row4, row5, row6, row7;

   int16x4_t rot0_0 = vdup_n_s16(RJPEG_F2F(0.5411961f));
   int16x4_t rot0_1 = vdup_n_s16(RJPEG_F2F(-1.847759065f));
   int16x4_t rot0_2 = vdup_n_s16(RJPEG_F2F( 0.765366865f));
   int16x4_t rot1_0 = vdup_n_s16(RJPEG_F2F( 1.175875602f));
   int16x4_t rot1_1 = vdup_n_s16(RJPEG_F2F(-0.899976223f));
   int16x4_t rot1_2 = vdup_n_s16(RJPEG_F2F(-2.562915447f));
   int16x4_t rot2_0 = vdup_n_s16(RJPEG_F2F(-1.961570560f));
   int16x4_t rot2_1 = vdup_n_s16(RJPEG_F2F(-0.390180644f));
   int16x4_t rot3_0 = vdup_n_s16(RJPEG_F2F( 0.298631336f));
   int16x4_t rot3_1 = vdup_n_s16(RJPEG_F2F( 2.053119869f));
   int16x4_t rot3_2 = vdup_n_s16(RJPEG_F2F( 3.072711026f));
   int16x4_t rot3_3 = vdup_n_s16(RJPEG_F2F( 1.501321110f));

#define dct_long_mul(out, inq, coeff) \
   int32x4_t out##_l = vmull_s16(vget_low_s16(inq), coeff); \
   int32x4_t out##_h = vmull_s16(vget_high_s16(inq), coeff)

#define dct_long_mac(out, acc, inq, coeff) \
   int32x4_t out##_l = vmlal_s16(acc##_l, vget_low_s16(inq), coeff); \
   int32x4_t out##_h = vmlal_s16(acc##_h, vget_high_s16(inq), coeff)

#define dct_widen(out, inq) \
   int32x4_t out##_l = vshll_n_s16(vget_low_s16(inq), 12); \
   int32x4_t out##_h = vshll_n_s16(vget_high_s16(inq), 12)

/* wide add */
#define dct_wadd(out, a, b) \
   int32x4_t out##_l = vaddq_s32(a##_l, b##_l); \
   int32x4_t out##_h = vaddq_s32(a##_h, b##_h)

/* wide sub */
#define dct_wsub(out, a, b) \
   int32x4_t out##_l = vsubq_s32(a##_l, b##_l); \
   int32x4_t out##_h = vsubq_s32(a##_h, b##_h)

/* butterfly a/b, then shift using "shiftop" by "s" and pack */
#define dct_bfly32o(out0,out1, a,b,shiftop,s) \
   { \
      dct_wadd(sum, a, b); \
      dct_wsub(dif, a, b); \
      out0 = vcombine_s16(shiftop(sum_l, s), shiftop(sum_h, s)); \
      out1 = vcombine_s16(shiftop(dif_l, s), shiftop(dif_h, s)); \
   }

#define dct_pass(shiftop, shift) \
   { \
      /* even part */ \
      int16x8_t sum26 = vaddq_s16(row2, row6); \
      dct_long_mul(p1e, sum26, rot0_0); \
      dct_long_mac(t2e, p1e, row6, rot0_1); \
      dct_long_mac(t3e, p1e, row2, rot0_2); \
      int16x8_t sum04 = vaddq_s16(row0, row4); \
      int16x8_t dif04 = vsubq_s16(row0, row4); \
      dct_widen(t0e, sum04); \
      dct_widen(t1e, dif04); \
      dct_wadd(x0, t0e, t3e); \
      dct_wsub(x3, t0e, t3e); \
      dct_wadd(x1, t1e, t2e); \
      dct_wsub(x2, t1e, t2e); \
      /* odd part */ \
      int16x8_t sum15 = vaddq_s16(row1, row5); \
      int16x8_t sum17 = vaddq_s16(row1, row7); \
      int16x8_t sum35 = vaddq_s16(row3, row5); \
      int16x8_t sum37 = vaddq_s16(row3, row7); \
      int16x8_t sumodd = vaddq_s16(sum17, sum35); \
      dct_long_mul(p5o, sumodd, rot1_0); \
      dct_long_mac(p1o, p5o, sum17, rot1_1); \
      dct_long_mac(p2o, p5o, sum35, rot1_2); \
      dct_long_mul(p3o, sum37, rot2_0); \
      dct_long_mul(p4o, sum15, rot2_1); \
      dct_wadd(sump13o, p1o, p3o); \
      dct_wadd(sump24o, p2o, p4o); \
      dct_wadd(sump23o, p2o, p3o); \
      dct_wadd(sump14o, p1o, p4o); \
      dct_long_mac(x4, sump13o, row7, rot3_0); \
      dct_long_mac(x5, sump24o, row5, rot3_1); \
      dct_long_mac(x6, sump23o, row3, rot3_2); \
      dct_long_mac(x7, sump14o, row1, rot3_3); \
      dct_bfly32o(row0,row7, x0,x7,shiftop,shift); \
      dct_bfly32o(row1,row6, x1,x6,shiftop,shift); \
      dct_bfly32o(row2,row5, x2,x5,shiftop,shift); \
      dct_bfly32o(row3,row4, x3,x4,shiftop,shift); \
   }

   /* load */
   row0 = vld1q_s16(data + 0*8);
   row1 = vld1q_s16(data + 1*8);
   row2 = vld1q_s16(data + 2*8);
   row3 = vld1q_s16(data + 3*8);
   row4 = vld1q_s16(data + 4*8);
   row5 = vld1q_s16(data + 5*8);
   row6 = vld1q_s16(data + 6*8);
   row7 = vld1q_s16(data + 7*8);

   /* add DC bias */
   row0 = vaddq_s16(row0, vsetq_lane_s16(1024, vdupq_n_s16(0), 0));

   /* column pass */
   dct_pass(vrshrn_n_s32, 10);

   /* 16bit 8x8 transpose */
   {
/* these three map to a single VTRN.16, VTRN.32, and VSWP, respectively.
 * whether compilers actually get this is another story, sadly. */
#define dct_trn16(x, y) { int16x8x2_t t = vtrnq_s16(x, y); x = t.val[0]; y = t.val[1]; }
#define dct_trn32(x, y) { int32x4x2_t t = vtrnq_s32(vreinterpretq_s32_s16(x), vreinterpretq_s32_s16(y)); x = vreinterpretq_s16_s32(t.val[0]); y = vreinterpretq_s16_s32(t.val[1]); }
#define dct_trn64(x, y) { int16x8_t x0 = x; int16x8_t y0 = y; x = vcombine_s16(vget_low_s16(x0), vget_low_s16(y0)); y = vcombine_s16(vget_high_s16(x0), vget_high_s16(y0)); }

      /* pass 1 */
      dct_trn16(row0, row1); /* a0b0a2b2a4b4a6b6 */
      dct_trn16(row2, row3);
      dct_trn16(row4, row5);
      dct_trn16(row6, row7);

      /* pass 2 */
      dct_trn32(row0, row2); /* a0b0c0d0a4b4c4d4 */
      dct_trn32(row1, row3);
      dct_trn32(row4, row6);
      dct_trn32(row5, row7);

      /* pass 3 */
      dct_trn64(row0, row4); /* a0b0c0d0e0f0g0h0 */
      dct_trn64(row1, row5);
      dct_trn64(row2, row6);
      dct_trn64(row3, row7);

#undef dct_trn16
#undef dct_trn32
#undef dct_trn64
   }

   /* row pass
    * vrshrn_n_s32 only supports shifts up to 16, we need
    * 17. so do a non-rounding shift of 16 first then follow
    * up with a rounding shift by 1. */
   dct_pass(vshrn_n_s32, 16);

   {
      /* pack and round */
      uint8x8_t p0 = vqrshrun_n_s16(row0, 1);
      uint8x8_t p1 = vqrshrun_n_s16(row1, 1);
      uint8x8_t p2 = vqrshrun_n_s16(row2, 1);
      uint8x8_t p3 = vqrshrun_n_s16(row3, 1);
      uint8x8_t p4 = vqrshrun_n_s16(row4, 1);
      uint8x8_t p5 = vqrshrun_n_s16(row5, 1);
      uint8x8_t p6 = vqrshrun_n_s16(row6, 1);
      uint8x8_t p7 = vqrshrun_n_s16(row7, 1);

      /* again, these can translate into one instruction, but often don't. */
#define dct_trn8_8(x, y) { uint8x8x2_t t = vtrn_u8(x, y); x = t.val[0]; y = t.val[1]; }
#define dct_trn8_16(x, y) { uint16x4x2_t t = vtrn_u16(vreinterpret_u16_u8(x), vreinterpret_u16_u8(y)); x = vreinterpret_u8_u16(t.val[0]); y = vreinterpret_u8_u16(t.val[1]); }
#define dct_trn8_32(x, y) { uint32x2x2_t t = vtrn_u32(vreinterpret_u32_u8(x), vreinterpret_u32_u8(y)); x = vreinterpret_u8_u32(t.val[0]); y = vreinterpret_u8_u32(t.val[1]); }

      /* sadly can't use interleaved stores here since we only write
       * 8 bytes to each scan line! */

      /* 8x8 8-bit transpose pass 1 */
      dct_trn8_8(p0, p1);
      dct_trn8_8(p2, p3);
      dct_trn8_8(p4, p5);
      dct_trn8_8(p6, p7);

      /* pass 2 */
      dct_trn8_16(p0, p2);
      dct_trn8_16(p1, p3);
      dct_trn8_16(p4, p6);
      dct_trn8_16(p5, p7);

      /* pass 3 */
      dct_trn8_32(p0, p4);
      dct_trn8_32(p1, p5);
      dct_trn8_32(p2, p6);
      dct_trn8_32(p3, p7);

      /* store */
      vst1_u8(out, p0);
      out += out_stride;
      vst1_u8(out, p1);
      out += out_stride;
      vst1_u8(out, p2);
      out += out_stride;
      vst1_u8(out, p3);
      out += out_stride;
      vst1_u8(out, p4);
      out += out_stride;
      vst1_u8(out, p5);
      out += out_stride;
      vst1_u8(out, p6);
      out += out_stride;
      vst1_u8(out, p7);

#undef dct_trn8_8
#undef dct_trn8_16
#undef dct_trn8_32
   }

#undef dct_long_mul
#undef dct_long_mac
#undef dct_widen
#undef dct_wadd
#undef dct_wsub
#undef dct_bfly32o
#undef dct_pass
}

#endif /* RJPEG_NEON */

static uint8_t rjpeg_get_marker(rjpeg_jpeg *j)
{
   uint8_t x;

   if (j->marker != RJPEG_MARKER_NONE)
   {
      x = j->marker;
      j->marker = RJPEG_MARKER_NONE;
      return x;
   }

   x = rjpeg_get8(j->s);
   if (x != 0xff)
      return RJPEG_MARKER_NONE;
   while (x == 0xff)
      x = rjpeg_get8(j->s);
   return x;
}

/* after a restart interval, rjpeg_jpeg_reset the entropy decoder and
 * the dc prediction
 */
static void rjpeg_jpeg_reset(rjpeg_jpeg *j)
{
   j->code_bits           = 0;
   j->code_buffer         = 0;
   j->nomore              = 0;
   j->img_comp[0].dc_pred = 0;
   j->img_comp[1].dc_pred = 0;
   j->img_comp[2].dc_pred = 0;
   j->marker              = RJPEG_MARKER_NONE;
   j->todo                = j->restart_interval ? j->restart_interval : 0x7fffffff;
   j->eob_run             = 0;

   /* no more than 1<<31 MCUs if no restart_interval? that's plenty safe,
    * since we don't even allow 1<<30 pixels */
}

static int rjpeg_parse_entropy_coded_data(rjpeg_jpeg *z)
{
   rjpeg_jpeg_reset(z);

   if (z->scan_n == 1)
   {
      int i, j;
      int n = z->order[0];
      int w = (z->img_comp[n].x+7) >> 3;
      int h = (z->img_comp[n].y+7) >> 3;

      /* non-interleaved data, we just need to process one block at a time,
       * in trivial scanline order
       * number of blocks to do just depends on how many actual "pixels" this
       * component has, independent of interleaved MCU blocking and such */

      if (z->progressive)
      {
         for (j = 0; j < h; ++j)
         {
            for (i = 0; i < w; ++i)
            {
               short *data = z->img_comp[n].coeff + 64 * (i + j * z->img_comp[n].coeff_w);

               if (z->spec_start == 0)
               {
                  if (!rjpeg_jpeg_decode_block_prog_dc(z, data, &z->huff_dc[z->img_comp[n].hd], n))
                     return 0;
               }
               else
               {
                  int ha = z->img_comp[n].ha;
                  if (!rjpeg_jpeg_decode_block_prog_ac(z, data, &z->huff_ac[ha], z->fast_ac[ha]))
                     return 0;
               }

               /* every data block is an MCU, so countdown the restart interval */
               if (--z->todo <= 0)
               {
                  if (z->code_bits < 24)
                     rjpeg_grow_buffer_unsafe(z);

                  if (!RJPEG_RESTART(z->marker))
                     return 1;
                  rjpeg_jpeg_reset(z);
               }
            }
         }
      }
      else
      {
         RJPEG_SIMD_ALIGN(short, data[64]);

         for (j = 0; j < h; ++j)
         {
            for (i = 0; i < w; ++i)
            {
               int ha = z->img_comp[n].ha;
               if (!rjpeg_jpeg_decode_block(z, data, z->huff_dc+z->img_comp[n].hd,
                        z->huff_ac+ha, z->fast_ac[ha], n, z->dequant[z->img_comp[n].tq]))
                  return 0;

               z->idct_block_kernel(z->img_comp[n].data+z->img_comp[n].w2*j*8+i*8,
                     z->img_comp[n].w2, data);

               /* every data block is an MCU, so countdown the restart interval */
               if (--z->todo <= 0)
               {
                  if (z->code_bits < 24)
                     rjpeg_grow_buffer_unsafe(z);

                  /* if it's NOT a restart, then just bail,
                   * so we get corrupt data rather than no data */
                  if (!RJPEG_RESTART(z->marker))
                     return 1;
                  rjpeg_jpeg_reset(z);
               }
            }
         }
      }
   }
   else
   {
      /* interleaved */
      int i,j,k,x,y;

      if (z->progressive)
      {
         for (j = 0; j < z->img_mcu_y; ++j)
         {
            for (i = 0; i < z->img_mcu_x; ++i)
            {
               /* scan an interleaved MCU... process scan_n components in order */
               for (k = 0; k < z->scan_n; ++k)
               {
                  int n = z->order[k];
                  /* scan out an MCU's worth of this component; that's just determined
                   * by the basic H and V specified for the component */
                  for (y = 0; y < z->img_comp[n].v; ++y)
                  {
                     for (x = 0; x < z->img_comp[n].h; ++x)
                     {
                        int      x2 = (i*z->img_comp[n].h + x);
                        int      y2 = (j*z->img_comp[n].v + y);
                        short *data = z->img_comp[n].coeff + 64 * (x2 + y2 * z->img_comp[n].coeff_w);
                        if (!rjpeg_jpeg_decode_block_prog_dc(z, data, &z->huff_dc[z->img_comp[n].hd], n))
                           return 0;
                     }
                  }
               }

               /* after all interleaved components, that's an interleaved MCU,
                * so now count down the restart interval */
               if (--z->todo <= 0)
               {
                  if (z->code_bits < 24)
                     rjpeg_grow_buffer_unsafe(z);
                  if (!RJPEG_RESTART(z->marker))
                     return 1;
                  rjpeg_jpeg_reset(z);
               }
            }
         }
      }
      else
      {
         RJPEG_SIMD_ALIGN(short, data[64]);

         for (j = 0; j < z->img_mcu_y; ++j)
         {
            for (i = 0; i < z->img_mcu_x; ++i)
            {
               /* scan an interleaved MCU... process scan_n components in order */
               for (k = 0; k < z->scan_n; ++k)
               {
                  int n = z->order[k];
                  /* scan out an MCU's worth of this component; that's just determined
                   * by the basic H and V specified for the component */
                  for (y = 0; y < z->img_comp[n].v; ++y)
                  {
                     for (x = 0; x < z->img_comp[n].h; ++x)
                     {
                        int x2 = (i*z->img_comp[n].h + x)*8;
                        int y2 = (j*z->img_comp[n].v + y)*8;
                        int ha = z->img_comp[n].ha;

                        if (!rjpeg_jpeg_decode_block(z, data,
                                 z->huff_dc+z->img_comp[n].hd,
                                 z->huff_ac+ha, z->fast_ac[ha],
                                 n, z->dequant[z->img_comp[n].tq]))
                           return 0;

                        z->idct_block_kernel(z->img_comp[n].data+z->img_comp[n].w2*y2+x2,
                              z->img_comp[n].w2, data);
                     }
                  }
               }

               /* after all interleaved components, that's an interleaved MCU,
                * so now count down the restart interval */
               if (--z->todo <= 0)
               {
                  if (z->code_bits < 24)
                     rjpeg_grow_buffer_unsafe(z);
                  if (!RJPEG_RESTART(z->marker))
                     return 1;
                  rjpeg_jpeg_reset(z);
               }
            }
         }
      }
   }

   return 1;
}

static void rjpeg_jpeg_dequantize(short *data, uint8_t *dequant)
{
   int i;
   for (i = 0; i < 64; ++i)
      data[i] *= dequant[i];
}

static void rjpeg_jpeg_finish(rjpeg_jpeg *z)
{
   int i,j,n;

   if (!z->progressive)
      return;

   /* dequantize and IDCT the data */
   for (n = 0; n < z->s->img_n; ++n)
   {
      int w = (z->img_comp[n].x+7) >> 3;
      int h = (z->img_comp[n].y+7) >> 3;
      for (j = 0; j < h; ++j)
      {
         for (i = 0; i < w; ++i)
         {
            short *data = z->img_comp[n].coeff + 64 * (i + j * z->img_comp[n].coeff_w);
            rjpeg_jpeg_dequantize(data, z->dequant[z->img_comp[n].tq]);
            z->idct_block_kernel(z->img_comp[n].data+z->img_comp[n].w2*j*8+i*8,
                  z->img_comp[n].w2, data);
         }
      }
   }
}

static int rjpeg_process_marker(rjpeg_jpeg *z, int m)
{
   int L;
   switch (m)
   {
      case RJPEG_MARKER_NONE: /* no marker found */
         /* Expected marker. Corrupt JPEG? */
         return 0;

      case 0xDD: /* DRI - specify restart interval */

         /* Bad DRI length. Corrupt JPEG? */
         if (RJPEG_GET16BE(z->s) != 4)
            return 0;

         z->restart_interval = RJPEG_GET16BE(z->s);
         return 1;

      case 0xDB: /* DQT - define quantization table */
         L = RJPEG_GET16BE(z->s)-2;
         while (L > 0)
         {
            int q = rjpeg_get8(z->s);
            int p = q >> 4;
            int t = q & 15,i;

            /* Bad DQT type. Corrupt JPEG? */
            if (p != 0)
               return 0;

            /* Bad DQT table. Corrupt JPEG? */
            if (t > 3)
               return 0;

            for (i = 0; i < 64; ++i)
               z->dequant[t][rjpeg_jpeg_dezigzag[i]] = rjpeg_get8(z->s);
            L -= 65;
         }
         return L == 0;

      case 0xC4: /* DHT - define huffman table */
         L = RJPEG_GET16BE(z->s)-2;
         while (L > 0)
         {
            int sizes[16],i,n = 0;
            uint8_t *v = NULL;
            int q      = rjpeg_get8(z->s);
            int tc     = q >> 4;
            int th     = q & 15;

            /* Bad DHT header. Corrupt JPEG? */
            if (tc > 1 || th > 3)
               return 0;

            for (i = 0; i < 16; ++i)
            {
               sizes[i] = rjpeg_get8(z->s);
               n += sizes[i];
            }
            L -= 17;

            if (tc == 0)
            {
               if (!rjpeg_build_huffman(z->huff_dc+th, sizes))
                  return 0;
               v = z->huff_dc[th].values;
            }
            else
            {
               if (!rjpeg_build_huffman(z->huff_ac+th, sizes))
                  return 0;
               v = z->huff_ac[th].values;
            }
            for (i = 0; i < n; ++i)
               v[i] = rjpeg_get8(z->s);
            if (tc != 0)
               rjpeg_build_fast_ac(z->fast_ac[th], z->huff_ac + th);
            L -= n;
         }
         return L == 0;
   }

   /* check for comment block or APP blocks */
   if ((m >= 0xE0 && m <= 0xEF) || m == 0xFE)
   {
      int n = RJPEG_GET16BE(z->s)-2;

      if (n < 0)
         z->s->img_buffer = z->s->img_buffer_end;
      else
         z->s->img_buffer += n;

      return 1;
   }
   return 0;
}

/* after we see SOS */
static int rjpeg_process_scan_header(rjpeg_jpeg *z)
{
   int i;
   int aa;
   int Ls    = RJPEG_GET16BE(z->s);

   z->scan_n = rjpeg_get8(z->s);

   /* Bad SOS component count. Corrupt JPEG? */
   if (z->scan_n < 1 || z->scan_n > 4 || z->scan_n > (int) z->s->img_n)
      return 0;

   /* Bad SOS length. Corrupt JPEG? */
   if (Ls != 6+2*z->scan_n)
      return 0;

   for (i = 0; i < z->scan_n; ++i)
   {
      int which;
      int id = rjpeg_get8(z->s);
      int q  = rjpeg_get8(z->s);

      for (which = 0; which < z->s->img_n; ++which)
         if (z->img_comp[which].id == id)
            break;
      if (which == z->s->img_n)
         return 0; /* no match */

      /* Bad DC huff. Corrupt JPEG? */
      z->img_comp[which].hd = q >> 4;   if (z->img_comp[which].hd > 3)
         return 0;

      /* Bad AC huff. Corrupt JPEG? */
      z->img_comp[which].ha = q & 15;   if (z->img_comp[which].ha > 3)
         return 0;

      z->order[i] = which;
   }

   z->spec_start = rjpeg_get8(z->s);
   z->spec_end   = rjpeg_get8(z->s); /* should be 63, but might be 0 */
   aa            = rjpeg_get8(z->s);
   z->succ_high  = (aa >> 4);
   z->succ_low   = (aa & 15);

   if (z->progressive)
   {
      /* Bad SOS. Corrupt JPEG? */
      if (  z->spec_start > 63 ||
            z->spec_end > 63   ||
            z->spec_start > z->spec_end ||
            z->succ_high > 13           ||
            z->succ_low > 13)
         return 0;
   }
   else
   {
      /* Bad SOS. Corrupt JPEG? */
      if (z->spec_start != 0)
         return 0;
      if (z->succ_high != 0 || z->succ_low != 0)
         return 0;

      z->spec_end = 63;
   }

   return 1;
}

static int rjpeg_process_frame_header(rjpeg_jpeg *z, int scan)
{
   rjpeg_context *s = z->s;
   int Lf,p,i,q, h_max=1,v_max=1,c;
   Lf = RJPEG_GET16BE(s);

   /* JPEG */

   /* Bad SOF len. Corrupt JPEG? */
   if (Lf < 11)
      return 0;

   p  = rjpeg_get8(s);

   /* JPEG baseline */

   /* Only 8-bit. JPEG format not supported? */
   if (p != 8)
      return 0;

   s->img_y = RJPEG_GET16BE(s);

   /* Legal, but we don't handle it--but neither does IJG */

   /* No header height, JPEG format not supported? */
   if (s->img_y == 0)
      return 0;

   s->img_x = RJPEG_GET16BE(s);

   /* No header width. Corrupt JPEG? */
   if (s->img_x == 0)
      return 0;

   c = rjpeg_get8(s);

   /* JFIF requires */

   /* Bad component count. Corrupt JPEG? */
   if (c != 3 && c != 1)
      return 0;

   s->img_n = c;

   for (i = 0; i < c; ++i)
   {
      z->img_comp[i].data = NULL;
      z->img_comp[i].linebuf = NULL;
   }

   /* Bad SOF length. Corrupt JPEG? */
   if (Lf != 8+3*s->img_n)
      return 0;

   for (i = 0; i < s->img_n; ++i)
   {
      z->img_comp[i].id = rjpeg_get8(s);
      if (z->img_comp[i].id != i+1)   /* JFIF requires */
         if (z->img_comp[i].id != i)  /* some version of jpegtran outputs non-JFIF-compliant files! */
            return 0;

      q                = rjpeg_get8(s);
      z->img_comp[i].h = (q >> 4);

      /* Bad H. Corrupt JPEG? */
      if (!z->img_comp[i].h || z->img_comp[i].h > 4)
         return 0;

      z->img_comp[i].v = q & 15;

      /* Bad V. Corrupt JPEG? */
      if (!z->img_comp[i].v || z->img_comp[i].v > 4)
         return 0;

      z->img_comp[i].tq = rjpeg_get8(s);

      /* Bad TQ. Corrupt JPEG? */
      if (z->img_comp[i].tq > 3)
         return 0;
   }

   if (scan != RJPEG_SCAN_LOAD)
      return 1;

   /* Image too large to decode? */
   if ((1 << 30) / s->img_x / s->img_n < s->img_y)
      return 0;

   for (i = 0; i < s->img_n; ++i)
   {
      if (z->img_comp[i].h > h_max)
         h_max = z->img_comp[i].h;
      if (z->img_comp[i].v > v_max)
         v_max = z->img_comp[i].v;
   }

   /* compute interleaved MCU info */
   z->img_h_max = h_max;
   z->img_v_max = v_max;
   z->img_mcu_w = h_max * 8;
   z->img_mcu_h = v_max * 8;
   z->img_mcu_x = (s->img_x + z->img_mcu_w-1) / z->img_mcu_w;
   z->img_mcu_y = (s->img_y + z->img_mcu_h-1) / z->img_mcu_h;

   if (z->progressive)
   {
      for (i = 0; i < s->img_n; ++i)
      {
         /* number of effective pixels (e.g. for non-interleaved MCU) */
         z->img_comp[i].x        = (s->img_x * z->img_comp[i].h + h_max-1) / h_max;
         z->img_comp[i].y        = (s->img_y * z->img_comp[i].v + v_max-1) / v_max;

         /* to simplify generation, we'll allocate enough memory to decode
          * the bogus oversized data from using interleaved MCUs and their
          * big blocks (e.g. a 16x16 iMCU on an image of width 33); we won't
          * discard the extra data until colorspace conversion */
         z->img_comp[i].w2       = z->img_mcu_x * z->img_comp[i].h * 8;
         z->img_comp[i].h2       = z->img_mcu_y * z->img_comp[i].v * 8;
         z->img_comp[i].raw_data = malloc(z->img_comp[i].w2 * z->img_comp[i].h2+15);

         /* Out of memory? */
         if (!z->img_comp[i].raw_data)
         {
            for (--i; i >= 0; --i)
            {
               free(z->img_comp[i].raw_data);
               z->img_comp[i].data = NULL;
            }

            return 0;
         }

         /* align blocks for IDCT using MMX/SSE */
         z->img_comp[i].data      = (uint8_t*) (((size_t) z->img_comp[i].raw_data + 15) & ~15);
         z->img_comp[i].linebuf   = NULL;
         z->img_comp[i].coeff_w   = (z->img_comp[i].w2 + 7) >> 3;
         z->img_comp[i].coeff_h   = (z->img_comp[i].h2 + 7) >> 3;
         z->img_comp[i].raw_coeff = malloc(z->img_comp[i].coeff_w *
                                    z->img_comp[i].coeff_h * 64 * sizeof(short) + 15);
         z->img_comp[i].coeff     = (short*) (((size_t) z->img_comp[i].raw_coeff + 15) & ~15);
      }
   }
   else
   {
      for (i = 0; i < s->img_n; ++i)
      {
         /* number of effective pixels (e.g. for non-interleaved MCU) */
         z->img_comp[i].x        = (s->img_x * z->img_comp[i].h + h_max-1) / h_max;
         z->img_comp[i].y        = (s->img_y * z->img_comp[i].v + v_max-1) / v_max;

         /* to simplify generation, we'll allocate enough memory to decode
          * the bogus oversized data from using interleaved MCUs and their
          * big blocks (e.g. a 16x16 iMCU on an image of width 33); we won't
          * discard the extra data until colorspace conversion */
         z->img_comp[i].w2       = z->img_mcu_x * z->img_comp[i].h * 8;
         z->img_comp[i].h2       = z->img_mcu_y * z->img_comp[i].v * 8;
         z->img_comp[i].raw_data = malloc(z->img_comp[i].w2 * z->img_comp[i].h2+15);

         /* Out of memory? */
         if (!z->img_comp[i].raw_data)
         {
            for (--i; i >= 0; --i)
            {
               free(z->img_comp[i].raw_data);
               z->img_comp[i].data = NULL;
            }
         }

         /* align blocks for IDCT using MMX/SSE */
         z->img_comp[i].data      = (uint8_t*) (((size_t) z->img_comp[i].raw_data + 15) & ~15);
         z->img_comp[i].linebuf   = NULL;
         z->img_comp[i].coeff     = 0;
         z->img_comp[i].raw_coeff = 0;
      }
   }

   return 1;
}

static int rjpeg_decode_jpeg_header(rjpeg_jpeg *z, int scan)
{
   int m;
   z->marker = RJPEG_MARKER_NONE; /* initialize cached marker to empty */
   m         = rjpeg_get_marker(z);

   /* No SOI. Corrupt JPEG? */
   if (m != JPEG_MARKER_SOI)
      return 0;

   if (scan == RJPEG_SCAN_TYPE)
      return 1;

   m = rjpeg_get_marker(z);
   while (!RJPEG_SOF(m))
   {
      if (!rjpeg_process_marker(z,m))
         return 0;
      m = rjpeg_get_marker(z);
      while (m == RJPEG_MARKER_NONE)
      {
         /* some files have extra padding after their blocks, so ok, we'll scan */

         /* No SOF. Corrupt JPEG? */
         if (RJPEG_AT_EOF(z->s))
            return 0;

         m = rjpeg_get_marker(z);
      }
   }
   z->progressive = RJPEG_SOF_PROGRESSIVE(m);
   if (!rjpeg_process_frame_header(z, scan))
      return 0;
   return 1;
}

/* decode image to YCbCr format */
static int rjpeg_decode_jpeg_image(rjpeg_jpeg *j)
{
   int m;
   for (m = 0; m < 4; m++)
   {
      j->img_comp[m].raw_data = NULL;
      j->img_comp[m].raw_coeff = NULL;
   }
   j->restart_interval = 0;
   if (!rjpeg_decode_jpeg_header(j, RJPEG_SCAN_LOAD))
      return 0;
   m = rjpeg_get_marker(j);

   while (m != JPEG_MARKER_EOI)
   {
      if (m == JPEG_MARKER_SOS)
      {
         if (!rjpeg_process_scan_header(j))
            return 0;
         if (!rjpeg_parse_entropy_coded_data(j))
            return 0;

         if (j->marker == RJPEG_MARKER_NONE )
         {
            /* handle 0s at the end of image data from IP Kamera 9060 */

            while (!RJPEG_AT_EOF(j->s))
            {
               int x = rjpeg_get8(j->s);
               if (x == 255)
               {
                  j->marker = rjpeg_get8(j->s);
                  break;
               }
               else if (x != 0) /* Junk before marker. Corrupt JPEG? */
                  return 0;
            }

            /* if we reach eof without hitting a marker,
             * rjpeg_get_marker() below will fail and we'll eventually return 0 */
         }
      }
      else
      {
         if (!rjpeg_process_marker(j, m))
            return 0;
      }
      m = rjpeg_get_marker(j);
   }

   if (j->progressive)
      rjpeg_jpeg_finish(j);
   return 1;
}

/* static jfif-centered resampling (across block boundaries) */

static uint8_t *rjpeg_resample_row_1(uint8_t *out, uint8_t *in_near,
      uint8_t *in_far, int w, int hs)
{
   (void)out;
   (void)in_far;
   (void)w;
   (void)hs;
   return in_near;
}

static uint8_t* rjpeg_resample_row_v_2(uint8_t *out, uint8_t *in_near,
      uint8_t *in_far, int w, int hs)
{
   /* need to generate two samples vertically for every one in input */
   int i;
   (void)hs;
   for (i = 0; i < w; ++i)
      out[i] = RJPEG_DIV4(3*in_near[i] + in_far[i] + 2);
   return out;
}

static uint8_t*  rjpeg_resample_row_h_2(uint8_t *out, uint8_t *in_near,
      uint8_t *in_far, int w, int hs)
{
   /* need to generate two samples horizontally for every one in input */
   int i;
   uint8_t *input = in_near;

   if (w == 1)
   {
      /* if only one sample, can't do any interpolation */
      out[0] = out[1] = input[0];
      return out;
   }

   out[0] = input[0];
   out[1] = RJPEG_DIV4(input[0]*3 + input[1] + 2);

   for (i=1; i < w-1; ++i)
   {
      int n      = 3 * input[i] + 2;
      out[i*2+0] = RJPEG_DIV4(n+input[i-1]);
      out[i*2+1] = RJPEG_DIV4(n+input[i+1]);
   }
   out[i*2+0] = RJPEG_DIV4(input[w-2]*3 + input[w-1] + 2);
   out[i*2+1] = input[w-1];

   (void)in_far;
   (void)hs;

   return out;
}

static uint8_t *rjpeg_resample_row_hv_2(uint8_t *out, uint8_t *in_near,
      uint8_t *in_far, int w, int hs)
{
   /* need to generate 2x2 samples for every one in input */
   int i,t0,t1;
   if (w == 1)
   {
      out[0] = out[1] = RJPEG_DIV4(3*in_near[0] + in_far[0] + 2);
      return out;
   }

   t1     = 3*in_near[0] + in_far[0];
   out[0] = RJPEG_DIV4(t1+2);

   for (i = 1; i < w; ++i)
   {
      t0         = t1;
      t1         = 3*in_near[i]+in_far[i];
      out[i*2-1] = RJPEG_DIV16(3*t0 + t1 + 8);
      out[i*2  ] = RJPEG_DIV16(3*t1 + t0 + 8);
   }
   out[w*2-1] = RJPEG_DIV4(t1+2);

   (void)hs;

   return out;
}

#if defined(__SSE2__) || defined(RJPEG_NEON)
static uint8_t *rjpeg_resample_row_hv_2_simd(uint8_t *out, uint8_t *in_near,
      uint8_t *in_far, int w, int hs)
{
   /* need to generate 2x2 samples for every one in input */
   int i = 0,t0,t1;

   if (w == 1)
   {
      out[0] = out[1] = RJPEG_DIV4(3*in_near[0] + in_far[0] + 2);
      return out;
   }

   t1 = 3*in_near[0] + in_far[0];
   /* process groups of 8 pixels for as long as we can.
    * note we can't handle the last pixel in a row in this loop
    * because we need to handle the filter boundary conditions.
    */
   for (; i < ((w-1) & ~7); i += 8)
   {
#if defined(__SSE2__)
      /* load and perform the vertical filtering pass
       * this uses 3*x + y = 4*x + (y - x) */
      __m128i zero  = _mm_setzero_si128();
      __m128i farb  = _mm_loadl_epi64((__m128i *) (in_far + i));
      __m128i nearb = _mm_loadl_epi64((__m128i *) (in_near + i));
      __m128i farw  = _mm_unpacklo_epi8(farb, zero);
      __m128i nearw = _mm_unpacklo_epi8(nearb, zero);
      __m128i diff  = _mm_sub_epi16(farw, nearw);
      __m128i nears = _mm_slli_epi16(nearw, 2);
      __m128i curr  = _mm_add_epi16(nears, diff); /* current row */

      /* horizontal filter works the same based on shifted vers of current
       * row. "prev" is current row shifted right by 1 pixel; we need to
       * insert the previous pixel value (from t1).
       * "next" is current row shifted left by 1 pixel, with first pixel
       * of next block of 8 pixels added in.
       */
      __m128i prv0 = _mm_slli_si128(curr, 2);
      __m128i nxt0 = _mm_srli_si128(curr, 2);
      __m128i prev = _mm_insert_epi16(prv0, t1, 0);
      __m128i next = _mm_insert_epi16(nxt0, 3*in_near[i+8] + in_far[i+8], 7);

      /* horizontal filter, polyphase implementation since it's convenient:
       * even pixels = 3*cur + prev = cur*4 + (prev - cur)
       * odd  pixels = 3*cur + next = cur*4 + (next - cur)
       * note the shared term. */
      __m128i bias = _mm_set1_epi16(8);
      __m128i curs = _mm_slli_epi16(curr, 2);
      __m128i prvd = _mm_sub_epi16(prev, curr);
      __m128i nxtd = _mm_sub_epi16(next, curr);
      __m128i curb = _mm_add_epi16(curs, bias);
      __m128i even = _mm_add_epi16(prvd, curb);
      __m128i odd  = _mm_add_epi16(nxtd, curb);

      /* interleave even and odd pixels, then undo scaling. */
      __m128i int0 = _mm_unpacklo_epi16(even, odd);
      __m128i int1 = _mm_unpackhi_epi16(even, odd);
      __m128i de0  = _mm_srli_epi16(int0, 4);
      __m128i de1  = _mm_srli_epi16(int1, 4);

      /* pack and write output */
      __m128i outv = _mm_packus_epi16(de0, de1);
      _mm_storeu_si128((__m128i *) (out + i*2), outv);
#elif defined(RJPEG_NEON)
      /* load and perform the vertical filtering pass
       * this uses 3*x + y = 4*x + (y - x) */
      uint8x8_t farb  = vld1_u8(in_far + i);
      uint8x8_t nearb = vld1_u8(in_near + i);
      int16x8_t diff  = vreinterpretq_s16_u16(vsubl_u8(farb, nearb));
      int16x8_t nears = vreinterpretq_s16_u16(vshll_n_u8(nearb, 2));
      int16x8_t curr  = vaddq_s16(nears, diff); /* current row */

      /* horizontal filter works the same based on shifted vers of current
       * row. "prev" is current row shifted right by 1 pixel; we need to
       * insert the previous pixel value (from t1).
       * "next" is current row shifted left by 1 pixel, with first pixel
       * of next block of 8 pixels added in. */
      int16x8_t prv0 = vextq_s16(curr, curr, 7);
      int16x8_t nxt0 = vextq_s16(curr, curr, 1);
      int16x8_t prev = vsetq_lane_s16(t1, prv0, 0);
      int16x8_t next = vsetq_lane_s16(3*in_near[i+8] + in_far[i+8], nxt0, 7);

      /* horizontal filter, polyphase implementation since it's convenient:
       * even pixels = 3*cur + prev = cur*4 + (prev - cur)
       * odd  pixels = 3*cur + next = cur*4 + (next - cur)
       * note the shared term.
       */
      int16x8_t curs = vshlq_n_s16(curr, 2);
      int16x8_t prvd = vsubq_s16(prev, curr);
      int16x8_t nxtd = vsubq_s16(next, curr);
      int16x8_t even = vaddq_s16(curs, prvd);
      int16x8_t odd  = vaddq_s16(curs, nxtd);

      /* undo scaling and round, then store with even/odd phases interleaved */
      uint8x8x2_t o;
      o.val[0] = vqrshrun_n_s16(even, 4);
      o.val[1] = vqrshrun_n_s16(odd,  4);
      vst2_u8(out + i*2, o);
#endif

      /* "previous" value for next iteration */
      t1 = 3*in_near[i+7] + in_far[i+7];
   }

   t0       = t1;
   t1       = 3*in_near[i] + in_far[i];
   out[i*2] = RJPEG_DIV16(3*t1 + t0 + 8);

   for (++i; i < w; ++i)
   {
      t0         = t1;
      t1         = 3*in_near[i]+in_far[i];
      out[i*2-1] = RJPEG_DIV16(3*t0 + t1 + 8);
      out[i*2  ] = RJPEG_DIV16(3*t1 + t0 + 8);
   }
   out[w*2-1]    = RJPEG_DIV4(t1+2);

   (void)hs;

   return out;
}
#endif

static uint8_t *rjpeg_resample_row_generic(uint8_t *out,
      uint8_t *in_near, uint8_t *in_far, int w, int hs)
{
   /* resample with nearest-neighbor */
   int i,j;
   (void)in_far;

   for (i = 0; i < w; ++i)
      for (j = 0; j < hs; ++j)
         out[i*hs+j] = in_near[i];
   return out;
}

/* this is a reduced-precision calculation of YCbCr-to-RGB introduced
 * to make sure the code produces the same results in both SIMD and scalar */
#ifndef FLOAT2FIXED
#define FLOAT2FIXED(x)  (((int) ((x) * 4096.0f + 0.5f)) << 8)
#endif

static void rjpeg_YCbCr_to_RGB_row(uint8_t *out, const uint8_t *y,
      const uint8_t *pcb, const uint8_t *pcr, int count, int step)
{
   int i;
   for (i = 0; i < count; ++i)
   {
      int y_fixed = (y[i] << 20) + (1<<19); /* rounding */
      int cr = pcr[i] - 128;
      int cb = pcb[i] - 128;
      int r = y_fixed +  cr* FLOAT2FIXED(1.40200f);
      int g = y_fixed + (cr*-FLOAT2FIXED(0.71414f)) + ((cb*-FLOAT2FIXED(0.34414f)) & 0xffff0000);
      int b = y_fixed                               +   cb* FLOAT2FIXED(1.77200f);
      r >>= 20;
      g >>= 20;
      b >>= 20;
      if ((unsigned) r > 255)
         r = 255;
      if ((unsigned) g > 255)
         g = 255;
      if ((unsigned) b > 255)
         b = 255;
      out[0] = (uint8_t)r;
      out[1] = (uint8_t)g;
      out[2] = (uint8_t)b;
      out[3] = 255;
      out += step;
   }
}

#if defined(__SSE2__) || defined(RJPEG_NEON)
static void rjpeg_YCbCr_to_RGB_simd(uint8_t *out, const uint8_t *y,
      const uint8_t *pcb, const uint8_t *pcr, int count, int step)
{
   int i = 0;

#if defined(__SSE2__)
   /* step == 3 is pretty ugly on the final interleave, and i'm not convinced
    * it's useful in practice (you wouldn't use it for textures, for example).
    * so just accelerate step == 4 case.
    */
   if (step == 4)
   {
      /* this is a fairly straightforward implementation and not super-optimized. */
      __m128i signflip  = _mm_set1_epi8(-0x80);
      __m128i cr_const0 = _mm_set1_epi16(   (short) ( 1.40200f*4096.0f+0.5f));
      __m128i cr_const1 = _mm_set1_epi16( - (short) ( 0.71414f*4096.0f+0.5f));
      __m128i cb_const0 = _mm_set1_epi16( - (short) ( 0.34414f*4096.0f+0.5f));
      __m128i cb_const1 = _mm_set1_epi16(   (short) ( 1.77200f*4096.0f+0.5f));
      __m128i y_bias    = _mm_set1_epi8((char) (unsigned char) 128);
      __m128i xw        = _mm_set1_epi16(255); /* alpha channel */

      for (; i+7 < count; i += 8)
      {
         /* load */
         __m128i y_bytes = _mm_loadl_epi64((__m128i *) (y+i));
         __m128i cr_bytes = _mm_loadl_epi64((__m128i *) (pcr+i));
         __m128i cb_bytes = _mm_loadl_epi64((__m128i *) (pcb+i));
         __m128i cr_biased = _mm_xor_si128(cr_bytes, signflip); /* -128 */
         __m128i cb_biased = _mm_xor_si128(cb_bytes, signflip); /* -128 */

         /* unpack to short (and left-shift cr, cb by 8) */
         __m128i yw  = _mm_unpacklo_epi8(y_bias, y_bytes);
         __m128i crw = _mm_unpacklo_epi8(_mm_setzero_si128(), cr_biased);
         __m128i cbw = _mm_unpacklo_epi8(_mm_setzero_si128(), cb_biased);

         /* color transform */
         __m128i yws = _mm_srli_epi16(yw, 4);
         __m128i cr0 = _mm_mulhi_epi16(cr_const0, crw);
         __m128i cb0 = _mm_mulhi_epi16(cb_const0, cbw);
         __m128i cb1 = _mm_mulhi_epi16(cbw, cb_const1);
         __m128i cr1 = _mm_mulhi_epi16(crw, cr_const1);
         __m128i rws = _mm_add_epi16(cr0, yws);
         __m128i gwt = _mm_add_epi16(cb0, yws);
         __m128i bws = _mm_add_epi16(yws, cb1);
         __m128i gws = _mm_add_epi16(gwt, cr1);

         /* descale */
         __m128i rw = _mm_srai_epi16(rws, 4);
         __m128i bw = _mm_srai_epi16(bws, 4);
         __m128i gw = _mm_srai_epi16(gws, 4);

         /* back to byte, set up for transpose */
         __m128i brb = _mm_packus_epi16(rw, bw);
         __m128i gxb = _mm_packus_epi16(gw, xw);

         /* transpose to interleave channels */
         __m128i t0 = _mm_unpacklo_epi8(brb, gxb);
         __m128i t1 = _mm_unpackhi_epi8(brb, gxb);
         __m128i o0 = _mm_unpacklo_epi16(t0, t1);
         __m128i o1 = _mm_unpackhi_epi16(t0, t1);

         /* store */
         _mm_storeu_si128((__m128i *) (out + 0), o0);
         _mm_storeu_si128((__m128i *) (out + 16), o1);
         out += 32;
      }
   }
#endif

#ifdef RJPEG_NEON
   /* in this version, step=3 support would be easy to add. but is there demand? */
   if (step == 4)
   {
      /* this is a fairly straightforward implementation and not super-optimized. */
      uint8x8_t signflip = vdup_n_u8(0x80);
      int16x8_t cr_const0 = vdupq_n_s16(   (short) ( 1.40200f*4096.0f+0.5f));
      int16x8_t cr_const1 = vdupq_n_s16( - (short) ( 0.71414f*4096.0f+0.5f));
      int16x8_t cb_const0 = vdupq_n_s16( - (short) ( 0.34414f*4096.0f+0.5f));
      int16x8_t cb_const1 = vdupq_n_s16(   (short) ( 1.77200f*4096.0f+0.5f));

      for (; i+7 < count; i += 8)
      {
         uint8x8x4_t o;

         /* load */
         uint8x8_t y_bytes  = vld1_u8(y + i);
         uint8x8_t cr_bytes = vld1_u8(pcr + i);
         uint8x8_t cb_bytes = vld1_u8(pcb + i);
         int8x8_t cr_biased = vreinterpret_s8_u8(vsub_u8(cr_bytes, signflip));
         int8x8_t cb_biased = vreinterpret_s8_u8(vsub_u8(cb_bytes, signflip));

         /* expand to s16 */
         int16x8_t yws = vreinterpretq_s16_u16(vshll_n_u8(y_bytes, 4));
         int16x8_t crw = vshll_n_s8(cr_biased, 7);
         int16x8_t cbw = vshll_n_s8(cb_biased, 7);

         /* color transform */
         int16x8_t cr0 = vqdmulhq_s16(crw, cr_const0);
         int16x8_t cb0 = vqdmulhq_s16(cbw, cb_const0);
         int16x8_t cr1 = vqdmulhq_s16(crw, cr_const1);
         int16x8_t cb1 = vqdmulhq_s16(cbw, cb_const1);
         int16x8_t rws = vaddq_s16(yws, cr0);
         int16x8_t gws = vaddq_s16(vaddq_s16(yws, cb0), cr1);
         int16x8_t bws = vaddq_s16(yws, cb1);

         /* undo scaling, round, convert to byte */
         o.val[0] = vqrshrun_n_s16(rws, 4);
         o.val[1] = vqrshrun_n_s16(gws, 4);
         o.val[2] = vqrshrun_n_s16(bws, 4);
         o.val[3] = vdup_n_u8(255);

         /* store, interleaving r/g/b/a */
         vst4_u8(out, o);
         out += 8*4;
      }
   }
#endif

   for (; i < count; ++i)
   {
      int y_fixed = (y[i] << 20) + (1<<19); /* rounding */
      int cr      = pcr[i] - 128;
      int cb      = pcb[i] - 128;
      int r       = y_fixed + cr* FLOAT2FIXED(1.40200f);
      int g       = y_fixed + cr*-FLOAT2FIXED(0.71414f) + ((cb*-FLOAT2FIXED(0.34414f)) & 0xffff0000);
      int b       = y_fixed                             +   cb* FLOAT2FIXED(1.77200f);
      r >>= 20;
      g >>= 20;
      b >>= 20;
      if ((unsigned) r > 255)
         r = 255;
      if ((unsigned) g > 255)
         g = 255;
      if ((unsigned) b > 255)
         b = 255;
      out[0] = (uint8_t)r;
      out[1] = (uint8_t)g;
      out[2] = (uint8_t)b;
      out[3] = 255;
      out += step;
   }
}
#endif

/* set up the kernels */
static void rjpeg_setup_jpeg(rjpeg_jpeg *j)
{
   uint64_t mask = cpu_features_get();

   (void)mask;

   j->idct_block_kernel        = rjpeg_idct_block;
   j->YCbCr_to_RGB_kernel      = rjpeg_YCbCr_to_RGB_row;
   j->resample_row_hv_2_kernel = rjpeg_resample_row_hv_2;

#if defined(__SSE2__)
   if (mask & RETRO_SIMD_SSE2)
   {
      j->idct_block_kernel        = rjpeg_idct_simd;
      j->YCbCr_to_RGB_kernel      = rjpeg_YCbCr_to_RGB_simd;
      j->resample_row_hv_2_kernel = rjpeg_resample_row_hv_2_simd;
   }
#endif

#ifdef RJPEG_NEON
   j->idct_block_kernel           = rjpeg_idct_simd;
   j->YCbCr_to_RGB_kernel         = rjpeg_YCbCr_to_RGB_simd;
   j->resample_row_hv_2_kernel    = rjpeg_resample_row_hv_2_simd;
#endif
}

/* clean up the temporary component buffers */
static void rjpeg_cleanup_jpeg(rjpeg_jpeg *j)
{
   int i;
   for (i = 0; i < j->s->img_n; ++i)
   {
      if (j->img_comp[i].raw_data)
      {
         free(j->img_comp[i].raw_data);
         j->img_comp[i].raw_data = NULL;
         j->img_comp[i].data = NULL;
      }

      if (j->img_comp[i].raw_coeff)
      {
         free(j->img_comp[i].raw_coeff);
         j->img_comp[i].raw_coeff = 0;
         j->img_comp[i].coeff = 0;
      }

      if (j->img_comp[i].linebuf)
      {
         free(j->img_comp[i].linebuf);
         j->img_comp[i].linebuf = NULL;
      }
   }
}

static uint8_t *rjpeg_load_jpeg_image(rjpeg_jpeg *z,
      unsigned *out_x, unsigned *out_y, int *comp, int req_comp)
{
   int n, decode_n;
   int k;
   unsigned int i,j;
   rjpeg_resample res_comp[4];
   uint8_t *coutput[4] = {0};
   uint8_t *output     = NULL;
   z->s->img_n         = 0;

   /* load a jpeg image from whichever source, but leave in YCbCr format */
   if (!rjpeg_decode_jpeg_image(z))
      goto error;

   /* determine actual number of components to generate */
   n = req_comp ? req_comp : z->s->img_n;

   if (z->s->img_n == 3 && n < 3)
      decode_n = 1;
   else
      decode_n = z->s->img_n;

   /* resample and color-convert */
   for (k = 0; k < decode_n; ++k)
   {
      rjpeg_resample *r = &res_comp[k];

      /* allocate line buffer big enough for upsampling off the edges
       * with upsample factor of 4 */
      z->img_comp[k].linebuf = (uint8_t *) malloc(z->s->img_x + 3);
      if (!z->img_comp[k].linebuf)
         goto error;

      r->hs       = z->img_h_max / z->img_comp[k].h;
      r->vs       = z->img_v_max / z->img_comp[k].v;
      r->ystep    = r->vs >> 1;
      r->w_lores  = (z->s->img_x + r->hs-1) / r->hs;
      r->ypos     = 0;
      r->line0    = r->line1 = z->img_comp[k].data;
      r->resample = rjpeg_resample_row_generic;

      if      (r->hs == 1 && r->vs == 1)
         r->resample = rjpeg_resample_row_1;
      else if (r->hs == 1 && r->vs == 2)
         r->resample = rjpeg_resample_row_v_2;
      else if (r->hs == 2 && r->vs == 1)
         r->resample = rjpeg_resample_row_h_2;
      else if (r->hs == 2 && r->vs == 2)
         r->resample = z->resample_row_hv_2_kernel;
   }

   /* can't error after this so, this is safe */
   output = (uint8_t *) malloc(n * z->s->img_x * z->s->img_y + 1);

   if (!output)
      goto error;

   /* now go ahead and resample */
   for (j = 0; j < z->s->img_y; ++j)
   {
      uint8_t *out = output + n * z->s->img_x * j;
      for (k = 0; k < decode_n; ++k)
      {
         rjpeg_resample *r = &res_comp[k];
         int         y_bot  = r->ystep >= (r->vs >> 1);

         coutput[k]         = r->resample(z->img_comp[k].linebuf,
               y_bot ? r->line1 : r->line0,
               y_bot ? r->line0 : r->line1,
               r->w_lores, r->hs);

         if (++r->ystep >= r->vs)
         {
            r->ystep = 0;
            r->line0 = r->line1;
            if (++r->ypos < z->img_comp[k].y)
               r->line1 += z->img_comp[k].w2;
         }
      }

      if (n >= 3)
      {
         uint8_t *y = coutput[0];
         if (y)
         {
            if (z->s->img_n == 3)
               z->YCbCr_to_RGB_kernel(out, y, coutput[1], coutput[2], z->s->img_x, n);
            else
               for (i = 0; i < z->s->img_x; ++i)
               {
                  out[0]  = out[1] = out[2] = y[i];
                  out[3]  = 255; /* not used if n==3 */
                  out    += n;
               }
         }
      }
      else
      {
         uint8_t *y = coutput[0];
         if (n == 1)
            for (i = 0; i < z->s->img_x; ++i)
               out[i] = y[i];
         else
            for (i = 0; i < z->s->img_x; ++i)
            {
               *out++ = y[i];
               *out++ = 255;
            }
      }
   }

   rjpeg_cleanup_jpeg(z);
   *out_x = z->s->img_x;
   *out_y = z->s->img_y;

   if (comp)
      *comp  = z->s->img_n; /* report original components, not output */
   return output;

error:
   rjpeg_cleanup_jpeg(z);
   return NULL;
}

int rjpeg_process_image(rjpeg_t *rjpeg, void **buf_data,
      size_t size, unsigned *width, unsigned *height)
{
   rjpeg_jpeg j;
   rjpeg_context s;
   int comp;
   uint32_t *img         = NULL;
   uint32_t *pixels      = NULL;
   unsigned size_tex     = 0;

   if (!rjpeg)
      return IMAGE_PROCESS_ERROR;

   s.img_buffer          = (uint8_t*)rjpeg->buff_data;
   s.img_buffer_original = (uint8_t*)rjpeg->buff_data;
   s.img_buffer_end      = (uint8_t*)rjpeg->buff_data + (int)size;

   j.s                   = &s;

   rjpeg_setup_jpeg(&j);

   img                   =  (uint32_t*)rjpeg_load_jpeg_image(&j, width, height, &comp, 4);

   if (!img)
      return IMAGE_PROCESS_ERROR;

   size_tex = (*width) * (*height);
   pixels   = (uint32_t*)malloc(size_tex * sizeof(uint32_t));

   if (!pixels)
   {
      free(img);
      return IMAGE_PROCESS_ERROR;
   }

   *buf_data = pixels;

   /* Convert RGBA to ARGB */
   while (size_tex--)
   {
      unsigned int texel = img[size_tex];
      unsigned int A     = texel & 0xFF000000;
      unsigned int B     = texel & 0x00FF0000;
      unsigned int G     = texel & 0x0000FF00;
      unsigned int R     = texel & 0x000000FF;
      ((unsigned int*)pixels)[size_tex] = A | (R << 16) | G | (B >> 16);
   }

   free(img);

   return IMAGE_PROCESS_END;
}

bool rjpeg_set_buf_ptr(rjpeg_t *rjpeg, void *data)
{
   if (!rjpeg)
      return false;

   rjpeg->buff_data = (uint8_t*)data;

   return true;
}

void rjpeg_free(rjpeg_t *rjpeg)
{
   if (!rjpeg)
      return;

   free(rjpeg);
}

rjpeg_t *rjpeg_alloc(void)
{
   rjpeg_t *rjpeg = (rjpeg_t*)calloc(1, sizeof(*rjpeg));
   if (!rjpeg)
      return NULL;
   return rjpeg;
}

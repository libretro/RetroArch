/* Copyright  (C) 2010-2024 The RetroArch team
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

/* Self-contained WebP decoder for libretro. No external dependencies.
 * Supports VP8L (lossless, all 4 transforms) and VP8 (lossy): full
 * key-frame decode -- coefficient tokens, dequantisation, the DCT/WHT
 * inverse transforms, 4x4 and 16x16 intra prediction, both the simple
 * and normal loop filters, fancy chroma upsampling and YUV->RGB. Only
 * VP8 key frames occur in WebP, so inter-frame prediction (motion
 * vectors, golden/altref reference frames) is intentionally absent. */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <retro_inline.h>
#include <formats/image.h>
#include <formats/rwebp.h>
#include <formats/rvp8.h>

/* ===== RIFF Container ===== */

static uint32_t rw32(const uint8_t *p)
{
   return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
          ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}

#define RW_CC(a,b,c,d) \
   ((uint32_t)(a)|((uint32_t)(b)<<8)|((uint32_t)(c)<<16)|((uint32_t)(d)<<24))

typedef struct
{
   const uint8_t *vp8;  size_t vp8s;
   const uint8_t *vp8l; size_t vp8ls;
   const uint8_t *alph; size_t alphs;
   int lossless;
} rw_ctr;

static int rw_parse(const uint8_t *b, size_t l, rw_ctr *c)
{
   size_t p;
   const uint8_t *pend_alph = NULL;
   size_t pend_alphs = 0;
   memset(c, 0, sizeof(*c));
   if (l < 12 || rw32(b) != RW_CC('R','I','F','F')
       || rw32(b+8) != RW_CC('W','E','B','P'))
      return 0;
   for (p = 12; p + 8 <= l; )
   {
      uint32_t tag = rw32(b+p);
      uint32_t sz  = rw32(b+p+4);
      const uint8_t *d = b + p + 8;
      if (p + 8 + sz > l) break;
      if (tag == RW_CC('V','P','8',' ') && !c->vp8)
      {
         c->vp8 = d; c->vp8s = sz; c->lossless = 0;
         /* An ALPH chunk pairs only with the lossy image of the same
          * (top-level) image; it always precedes its VP8 chunk. */
         c->alph = pend_alph; c->alphs = pend_alphs;
      }
      else if (tag == RW_CC('V','P','8','L') && !c->vp8l)
      { c->vp8l = d; c->vp8ls = sz; c->lossless = 1; }
      else if (tag == RW_CC('A','L','P','H'))
      { pend_alph = d; pend_alphs = sz; }
      else if (tag == RW_CC('A','N','M','F') && sz >= 16)
      {
         /* Scan this frame's local chunks; commit the frame's image
          * together with the SAME frame's ALPH (if any). Alpha from a
          * different frame must never be applied - animation deltas
          * carry per-frame alpha covering only their changed region. */
         const uint8_t *fa = NULL, *fv = NULL, *fl = NULL;
         size_t fas = 0, fvs = 0, fls = 0;
         size_t sp;
         for (sp = 16; sp + 8 <= sz; )
         {
            uint32_t st = rw32(d+sp), ss = rw32(d+sp+4);
            if (sp+8+ss > sz) break;
            if (st == RW_CC('V','P','8',' ') && !fv)
            { fv = d+sp+8; fvs = ss; }
            else if (st == RW_CC('V','P','8','L') && !fl)
            { fl = d+sp+8; fls = ss; }
            else if (st == RW_CC('A','L','P','H') && !fa)
            { fa = d+sp+8; fas = ss; }
            sp += 8 + ((ss+1) & ~(size_t)1);
         }
         if (fl && !c->vp8l && !c->vp8)
         { c->vp8l = fl; c->vp8ls = fls; c->lossless = 1; }
         else if (fv && !c->vp8 && !c->vp8l)
         {
            c->vp8 = fv; c->vp8s = fvs; c->lossless = 0;
            c->alph = fa; c->alphs = fas;
         }
      }
      p += 8 + ((sz+1) & ~(size_t)1);
   }
   if (!c->vp8) { c->alph = NULL; c->alphs = 0; }
   return (c->vp8 || c->vp8l);
}

/* ===== VP8L LSB Bit Reader ===== */

typedef struct { const uint8_t *buf, *end; uint64_t val; int nb; } vbr;

static void vbr_init(vbr *b, const uint8_t *d, size_t l)
{ b->buf = d; b->end = d + l; b->val = 0; b->nb = 0; }

static INLINE void vbr_fill(vbr *b)
{ while (b->nb < 56 && b->buf < b->end)
  { b->val |= (uint64_t)(*b->buf++) << b->nb; b->nb += 8; } }

static INLINE uint32_t vbr_read(vbr *b, int n)
{
   uint32_t v;
   if (b->nb < n) vbr_fill(b);
   v = (uint32_t)(b->val & (((uint64_t)1 << n) - 1));
   b->val >>= n; b->nb -= n;
   return v;
}

/* ===== VP8L Huffman Tables ===== */

#define VH_MAXCL 15
#define VH_ROOT  8   /* HUFFMAN_TABLE_BITS */

/* Two-level canonical Huffman table, ported from libwebp BuildHuffmanTable.
 * Each entry packs (bits << 16) | value. In the root table (1 << VH_ROOT
 * entries) an entry with bits > VH_ROOT is a pointer: value is the offset
 * (relative to the entry) to its second-level table, and bits is the total
 * code length. Otherwise value is the symbol and bits its code length. */
typedef struct { uint32_t *t; int sz, rb; } vh;

static void vh_free(vh *h) { free(h->t); h->t = NULL; }

/* GetNextKey: reversed-prefix increment (libwebp). */
static uint32_t vh_next_key(uint32_t key, int len)
{
   uint32_t step = 1u << (len - 1);
   while (key & step) step >>= 1;
   return step ? (key & (step - 1)) + step : key;
}

/* ReplicateValue: fill table[0], table[step], ... table[end-step]. */
static void vh_replicate(uint32_t *table, int step, int end, uint32_t code)
{
   int cur = end;
   do { cur -= step; table[cur] = code; } while (cur > 0);
}

/* NextTableBitSize: size (in bits) of the 2nd-level table for prefix at len. */
static int vh_next_tbl_bits(const int *count, int len, int root_bits)
{
   int left = 1 << (len - root_bits);
   while (len < VH_MAXCL)
   {
      left -= count[len];
      if (left <= 0) break;
      ++len; left <<= 1;
   }
   return len - root_bits;
}

static int vh_build(vh *h, const uint8_t *lens, int ns, int root)
{
   int count[VH_MAXCL + 1], offset[VH_MAXCL + 1];
   int sorted[4096];
   int total_size = 1 << root;
   int len, symbol, i, pass;
   uint32_t *t = NULL;

   if (ns > 4096) return -1;
   memset(count, 0, sizeof(count));
   for (symbol = 0; symbol < ns; symbol++)
   {
      if (lens[symbol] > VH_MAXCL) return -1;
      count[lens[symbol]]++;
   }
   if (count[0] == ns) return -1;

   offset[1] = 0;
   for (len = 1; len < VH_MAXCL; len++)
   {
      if (count[len] > (1 << len)) return -1;
      offset[len + 1] = offset[len] + count[len];
   }
   for (symbol = 0; symbol < ns; symbol++)
   {
      int cl = lens[symbol];
      if (cl > 0) sorted[offset[cl]++] = symbol;
   }

   /* Single-symbol special case: 0-bit code returns that symbol. */
   if (offset[VH_MAXCL] == 1)
   {
      total_size = 1 << root;
      h->t = (uint32_t*)calloc(total_size, sizeof(uint32_t));
      if (!h->t) return -1;
      h->sz = total_size; h->rb = root;
      for (i = 0; i < total_size; i++)
         h->t[i] = (uint32_t)(sorted[0] & 0xFFFF);
      return 0;
   }

   /* Two passes over the identical libwebp walk: pass 0 measures total_size,
    * pass 1 fills. Structure mirrors libwebp BuildHuffmanTable exactly. */
   for (pass = 0; pass < 2; pass++)
   {
      int c2[VH_MAXCL + 1];
      int step;
      uint32_t low = 0xffffffffu;
      uint32_t mask = (1 << root) - 1;
      uint32_t key = 0;
      int table_bits = root, table_size = 1 << table_bits;
      int table_off = 0;   /* offset (from base) of current 2nd-level table */

      memcpy(c2, count, sizeof(c2));
      total_size = 1 << root;
      symbol = 0;

      /* Root table. */
      for (len = 1, step = 2; len <= root; len++, step <<= 1)
         for (; c2[len] > 0; c2[len]--)
         {
            if (pass == 1)
            {
               uint32_t code = (uint32_t)((len << 16) | (sorted[symbol] & 0xFFFF));
               vh_replicate(&t[key], step, table_size, code);
            }
            symbol++;
            key = vh_next_key(key, len);
         }

      /* Second-level tables. */
      for (len = root + 1, step = 2; len <= VH_MAXCL; len++, step <<= 1)
         for (; c2[len] > 0; c2[len]--)
         {
            if ((key & mask) != low)
            {
               table_off += table_size;        /* advance past previous table */
               table_bits = vh_next_tbl_bits(c2, len, root);  /* live count */
               table_size = 1 << table_bits;
               total_size += table_size;
               low = key & mask;
               if (pass == 1)
                  t[low] = 0x80000000u
                         | (uint32_t)((table_bits + root) << 16)
                         | (uint32_t)(table_off & 0xFFFF);
            }
            if (pass == 1)
            {
               uint32_t code = (uint32_t)(((len - root) << 16) | (sorted[symbol] & 0xFFFF));
               vh_replicate(&t[table_off + (key >> root)], step, table_size, code);
            }
            symbol++;
            key = vh_next_key(key, len);
         }

      if (pass == 0)
      {
         t = (uint32_t*)calloc(total_size + 1, sizeof(uint32_t));
         if (!t) return -1;
      }
   }

   h->t = t; h->sz = total_size; h->rb = root;
   return 0;
}

static INLINE int vh_read(const vh *h, vbr *b)
{
   uint32_t e;
   int idx, nbits;
   vbr_fill(b);
   idx = (int)(b->val & ((1u << h->rb) - 1));
   e = h->t[idx];
   if (e & 0x80000000u)
   {
      /* pointer entry: extra nbits in [30:16], subtable offset in [15:0] */
      int so = (int)(e & 0xFFFF);
      nbits = (int)((e >> 16) & 0x7FFF) - h->rb;
      b->val >>= h->rb; b->nb -= h->rb;
      e = h->t[so + (int)(b->val & ((1u << nbits) - 1))];
   }
   { int cl = (int)((e >> 16) & 0x7FFF); if (cl > 0) { b->val >>= cl; b->nb -= cl; } }
   return (int)(e & 0xFFFF);
}

/* Code-length alphabet order */
static const uint8_t vh_cl_order[19] =
   {17,18,0,1,2,3,4,5,16,6,7,8,9,10,11,12,13,14,15};

static int vh_read_codes(vbr *br, int ns, uint8_t *lens)
{
   int i;
   memset(lens, 0, ns);
   if (vbr_read(br, 1)) /* simple */
   {
      int n = vbr_read(br, 1) + 1;
      int f8 = vbr_read(br, 1);
      int s0 = vbr_read(br, f8 ? 8 : 1);
      if (s0 < ns) lens[s0] = 1;
      if (n == 2)
      {
         int s1 = vbr_read(br, 8);
         if (s1 < ns) lens[s1] = 1;
      }
   }
   else
   {
      uint8_t cl[19];
      vh clt;
      int ncl, prev = 8, si, ms;
      memset(&clt, 0, sizeof(clt));
      memset(cl, 0, 19);
      ncl = vbr_read(br, 4) + 4;
      if (ncl > 19) ncl = 19;
      for (i = 0; i < ncl; i++)
         cl[vh_cl_order[i]] = (uint8_t)vbr_read(br, 3);
      if (vh_build(&clt, cl, 19, 7) < 0) return -1;
      ms = ns;
      if (vbr_read(br, 1))
      {
         int nb = 2 + 2 * vbr_read(br, 3);
         ms = 2 + vbr_read(br, nb);
         if (ms > ns) { vh_free(&clt); return -1; }   /* invalid per libwebp */
      }
      si = 0;
      while (si < ns)
      {
         int c;
         if (ms-- == 0) break;     /* code-count budget, per libwebp */
         c = vh_read(&clt, br);
         if (c < 16) { lens[si++] = (uint8_t)c; if (c) prev = c; }
         else
         {
            int slot = c - 16;
            int extra = (slot == 0) ? 2 : (slot == 1) ? 3 : 7;
            int roff  = (slot == 0) ? 3 : (slot == 1) ? 3 : 11;
            int r = (int)vbr_read(br, extra) + roff;
            int val = (c == 16) ? prev : 0;
            if (si + r > ns) break;
            while (r-- > 0) lens[si++] = (uint8_t)val;
         }
      }
      vh_free(&clt);
   }
   return 0;
}

/* ===== VP8L Pixel Math ===== */

static INLINE uint32_t px_add(uint32_t a, uint32_t b)
{
   return ((((a>>8)&0xFF00FF)+((b>>8)&0xFF00FF))&0xFF00FF)<<8 |
          (((a&0xFF00FF)+(b&0xFF00FF))&0xFF00FF);
}
static INLINE uint32_t px_avg2(uint32_t a, uint32_t b)
{ return (((a^b)&0xFEFEFEFEu)>>1)+(a&b); }
static INLINE int px_abs(int x) { return x<0?-x:x; }
static INLINE int px_clb(int v) { return v<0?0:v>255?255:v; }

static uint32_t px_select(uint32_t TL, uint32_t T, uint32_t L)
{
   /* libwebp Select(top, left, top_left):
    * (sum |L-TL|) - (sum |T-TL|) <= 0 ? T : L */
   int d = px_abs((int)((L>>24)&0xFF)-(int)((TL>>24)&0xFF)) - px_abs((int)((T>>24)&0xFF)-(int)((TL>>24)&0xFF))
         + px_abs((int)((L>>16)&0xFF)-(int)((TL>>16)&0xFF)) - px_abs((int)((T>>16)&0xFF)-(int)((TL>>16)&0xFF))
         + px_abs((int)((L>> 8)&0xFF)-(int)((TL>> 8)&0xFF)) - px_abs((int)((T>> 8)&0xFF)-(int)((TL>> 8)&0xFF))
         + px_abs((int)( L     &0xFF)-(int)( TL     &0xFF)) - px_abs((int)( T     &0xFF)-(int)( TL     &0xFF));
   return d <= 0 ? T : L;
}
static uint32_t px_casf(uint32_t a, uint32_t b, uint32_t c)
{
   return ((uint32_t)px_clb((int)((a>>24)&0xFF)+(int)((b>>24)&0xFF)-(int)((c>>24)&0xFF))<<24)
        | ((uint32_t)px_clb((int)((a>>16)&0xFF)+(int)((b>>16)&0xFF)-(int)((c>>16)&0xFF))<<16)
        | ((uint32_t)px_clb((int)((a>> 8)&0xFF)+(int)((b>> 8)&0xFF)-(int)((c>> 8)&0xFF))<< 8)
        |  (uint32_t)px_clb((int)( a     &0xFF)+(int)( b     &0xFF)-(int)( c     &0xFF));
}
static uint32_t px_cash(uint32_t a, uint32_t b)
{
   return ((uint32_t)px_clb((int)((a>>24)&0xFF)+((int)((a>>24)&0xFF)-(int)((b>>24)&0xFF))/2)<<24)
        | ((uint32_t)px_clb((int)((a>>16)&0xFF)+((int)((a>>16)&0xFF)-(int)((b>>16)&0xFF))/2)<<16)
        | ((uint32_t)px_clb((int)((a>> 8)&0xFF)+((int)((a>> 8)&0xFF)-(int)((b>> 8)&0xFF))/2)<< 8)
        |  (uint32_t)px_clb((int)( a     &0xFF)+((int)( a     &0xFF)-(int)( b     &0xFF))/2);
}

static uint32_t px_predict(int m, uint32_t L, uint32_t T, uint32_t TL, uint32_t TR)
{
   switch (m)
   {
      case 0: return 0xFF000000u;
      case 1: return L;
      case 2: return T;
      case 3: return TR;
      case 4: return TL;
      case 5: return px_avg2(px_avg2(L,TR),T);
      case 6: return px_avg2(L,TL);
      case 7: return px_avg2(L,T);
      case 8: return px_avg2(TL,T);
      case 9: return px_avg2(T,TR);
      case 10: return px_avg2(px_avg2(L,TL),px_avg2(T,TR));
      case 11: return px_select(TL,T,L);
      case 12: return px_casf(L,T,TL);
      case 13: return px_cash(px_avg2(L,T),TL);
      default: return 0xFF000000u;
   }
}

/* Distance mapping, per libwebp PlaneCodeToDistance: the decoded distance
 * prefix value ("plane code") 1..120 maps through kCodeToPlane to a 2D
 * (x,y) offset; values above 120 are linear distances (code - 120). */
static const uint8_t vl_code_to_plane[120] = {
   0x18, 0x07, 0x17, 0x19, 0x28, 0x06, 0x27, 0x29, 0x16, 0x1a, 0x26, 0x2a,
   0x38, 0x05, 0x37, 0x39, 0x15, 0x1b, 0x36, 0x3a, 0x25, 0x2b, 0x48, 0x04,
   0x47, 0x49, 0x14, 0x1c, 0x35, 0x3b, 0x46, 0x4a, 0x24, 0x2c, 0x58, 0x45,
   0x4b, 0x34, 0x3c, 0x03, 0x57, 0x59, 0x13, 0x1d, 0x56, 0x5a, 0x23, 0x2d,
   0x44, 0x4c, 0x55, 0x5b, 0x33, 0x3d, 0x68, 0x02, 0x67, 0x69, 0x12, 0x1e,
   0x66, 0x6a, 0x22, 0x2e, 0x54, 0x5c, 0x43, 0x4d, 0x65, 0x6b, 0x32, 0x3e,
   0x78, 0x01, 0x77, 0x79, 0x53, 0x5d, 0x11, 0x1f, 0x64, 0x6c, 0x42, 0x4e,
   0x76, 0x7a, 0x21, 0x2f, 0x75, 0x7b, 0x31, 0x3f, 0x63, 0x6d, 0x52, 0x5e,
   0x00, 0x74, 0x7c, 0x41, 0x4f, 0x10, 0x20, 0x62, 0x6e, 0x30, 0x73, 0x7d,
   0x51, 0x5f, 0x40, 0x72, 0x7e, 0x61, 0x6f, 0x50, 0x71, 0x7f, 0x60, 0x70
};

static int vl_plane_to_dist(int xsize, int plane_code)
{
   int dist_code, yoffset, xoffset, dist;
   if (plane_code > 120)
      return plane_code - 120;
   dist_code = vl_code_to_plane[plane_code - 1];
   yoffset = dist_code >> 4;
   xoffset = 8 - (dist_code & 0xF);
   dist = yoffset * xsize + xoffset;
   return (dist >= 1) ? dist : 1;
}

static int vl_prefix(int c, vbr *br)
{
   int ri, extra, off;
   if (c < 4) return c + 1;
   ri = (c - 2) >> 1;
   extra = ri;
   if (extra > 24) extra = 24;
   off = (2 + ((c - 2) & 1)) << ri;
   return off + (int)vbr_read(br, extra) + 1;
}

/* ===== VP8L Pixel Decode ===== */

/* One Huffman group = 5 trees (green+len / red / blue / alpha / dist). */
typedef struct { vh t[5]; } vh_group;

/* Read a single Huffman group's 5 trees. The green tree's alphabet is
 * enlarged by the color-cache size (shared across all groups). */
static int vl_read_group(vbr *br, vh_group *g, int ccs, uint8_t *cl)
{
   int ns[5], i;
   ns[0] = 256 + 24 + ccs; ns[1] = 256; ns[2] = 256; ns[3] = 256; ns[4] = 40;
   for (i = 0; i < 5; i++)
   {
      if (vh_read_codes(br, ns[i], cl) < 0) return -1;
      if (vh_build(&g->t[i], cl, ns[i], VH_ROOT) < 0) return -1;
   }
   return 0;
}

static void vl_free_group(vh_group *g)
{
   int i;
   for (i = 0; i < 5; i++) vh_free(&g->t[i]);
}

/* Forward decl: entropy image is itself a VP8L image stream (no transforms,
 * no meta-Huffman recursion, but may carry its own color cache). */
static uint32_t *vl_decode_stream(vbr *br, int w, int h, int allow_meta);

/* Decode a spatially-coded ARGB image of size w x h.
 * allow_meta: if nonzero, a meta-Huffman (entropy) image may select a
 * different Huffman group per (x >> hbits, y >> hbits) block. Sub-images
 * (transform data, entropy image) pass allow_meta = 0. */
/* Resumable VP8L stream decode state: the bit reader, colour cache,
 * Huffman groups and entropy image persist between pixel batches. */
typedef struct vlds
{
   vbr *br;
   uint32_t *pix;
   uint32_t *huff_img;
   vh_group *groups;
   uint32_t *cc;
   uint8_t *cl;
   int ccb, ccs;
   int hbits, hxs;
   int num_groups;
   int w, h;
   int pi;
} vlds;

static void vlds_abort(vlds *s)
{
   int gi;
   free(s->cl);
   free(s->cc);
   free(s->huff_img);
   if (s->groups)
   {
      for (gi = 0; gi < s->num_groups; gi++)
         vl_free_group(&s->groups[gi]);
      free(s->groups);
   }
   free(s->pix);
   memset(s, 0, sizeof(*s));
}

/* Parse the stream prologue (colour cache, entropy image, Huffman
 * groups) and allocate the pixel buffer. Returns 0 on success. */
static int vlds_begin(vlds *s, vbr *br, int w, int h, int allow_meta)
{
   uint32_t *pix = NULL;
   uint32_t *huff_img = NULL;
   vh_group *groups = NULL;
   uint32_t *cc = NULL;
   uint8_t *cl = NULL;
   int ccb = 0, ccs = 0;
   int hbits = 0, hxs = 0;
   int num_groups = 1, gi;

   memset(s, 0, sizeof(*s));

   /* --- Color cache (before Huffman codes, per DecodeImageStream) --- */
   if (vbr_read(br, 1))
   {
      ccb = vbr_read(br, 4);
      if (ccb < 1 || ccb > 11) return -1;
      ccs = 1 << ccb;
      cc = (uint32_t*)calloc(ccs, sizeof(uint32_t));
      if (!cc) return -1;
   }

   /* --- Meta-Huffman (entropy image) --- */
   if (allow_meta && vbr_read(br, 1))
   {
      int hp = 2 + (int)vbr_read(br, 3);   /* MIN_HUFFMAN_BITS + bits(3) */
      int hys, hpix, i, maxg = 1;
      hxs = (w + (1 << hp) - 1) >> hp;
      hys = (h + (1 << hp) - 1) >> hp;
      hpix = hxs * hys;
      huff_img = vl_decode_stream(br, hxs, hys, 0);
      if (!huff_img) { free(cc); return -1; }
      hbits = hp;
      for (i = 0; i < hpix; i++)
      {
         int group = (int)((huff_img[i] >> 8) & 0xFFFF);  /* red<<8 | green */
         huff_img[i] = (uint32_t)group;
         if (group >= maxg) maxg = group + 1;
      }
      num_groups = maxg;
   }

   /* --- Read the Huffman groups --- */
   cl = (uint8_t*)malloc(4096);
   if (!cl) goto bfail;
   groups = (vh_group*)calloc(num_groups, sizeof(vh_group));
   if (!groups) goto bfail;
   for (gi = 0; gi < num_groups; gi++)
      if (vl_read_group(br, &groups[gi], ccs, cl) < 0) goto bfail;

   pix = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
   if (!pix) goto bfail;

   s->br = br;
   s->pix = pix;
   s->huff_img = huff_img;
   s->groups = groups;
   s->cc = cc;
   s->cl = cl;
   s->ccb = ccb; s->ccs = ccs;
   s->hbits = hbits; s->hxs = hxs;
   s->num_groups = num_groups;
   s->w = w; s->h = h;
   s->pi = 0;
   return 0;

bfail:
   free(cl);
   free(cc);
   free(huff_img);
   if (groups) { for (gi = 0; gi < num_groups; gi++) vl_free_group(&groups[gi]); free(groups); }
   return -1;
}

/* Decode up to npix pixels (an LZ77 run in progress may slightly
 * overshoot). Returns 1 while pixels remain, 0 when complete. The loop
 * body is the original decode loop verbatim over the state struct. */
static int vlds_pixels(vlds *s, int npix)
{
   vbr *br = s->br;
   uint32_t *pix = s->pix;
   uint32_t *huff_img = s->huff_img;
   uint32_t *cc = s->cc;
   const int ccb = s->ccb, ccs = s->ccs;
   const int hbits = s->hbits, hxs = s->hxs;
   const int num_groups = s->num_groups;
   const int w = s->w, h = s->h;
   int pi = s->pi;
   int stop = pi + npix;

   while (pi < w * h && pi < stop)
   {
      int x = pi % w, y = pi / w;
      vh_group *g;
      int sym;
      if (huff_img)
      {
         int mi = huff_img[hxs * (y >> hbits) + (x >> hbits)];
         if (mi < 0 || mi >= num_groups) mi = 0;
         g = &s->groups[mi];
      }
      else
         g = &s->groups[0];

      sym = vh_read(&g->t[0], br);
      if (sym < 256)
      {
         int r = vh_read(&g->t[1], br);
         int b = vh_read(&g->t[2], br);
         int a = vh_read(&g->t[3], br);
         uint32_t argb = ((uint32_t)a<<24)|((uint32_t)r<<16)|((uint32_t)sym<<8)|(uint32_t)b;
         pix[pi++] = argb;
         if (cc) cc[(0x1E35A7BDu * argb) >> (32 - ccb)] = argb;
      }
      else if (sym < 256 + 24)
      {
         int lc = sym - 256;
         int length = vl_prefix(lc, br);
         int dc = vh_read(&g->t[4], br);
         int dist = vl_plane_to_dist(w, vl_prefix(dc, br));
         int k;
         for (k = 0; k < length && pi < w * h; k++)
         {
            int src = pi - dist;
            uint32_t argb = (src >= 0) ? pix[src] : 0xFF000000u;
            pix[pi++] = argb;
            if (cc) cc[(0x1E35A7BDu * argb) >> (32 - ccb)] = argb;
         }
      }
      else
      {
         int ci = sym - 256 - 24;
         pix[pi++] = (cc && ci < ccs) ? cc[ci] : 0xFF000000u;
      }
   }
   s->pi = pi;
   return (pi < w * h) ? 1 : 0;
}

/* Detach the pixel buffer (ownership to caller) and free the rest. */
static uint32_t *vlds_finish(vlds *s)
{
   uint32_t *pix = s->pix;
   s->pix = NULL;
   vlds_abort(s);
   return pix;
}

static uint32_t *vl_decode_stream(vbr *br, int w, int h, int allow_meta)
{
   vlds s;
   if (vlds_begin(&s, br, w, h, allow_meta) != 0)
      return NULL;
   while (vlds_pixels(&s, w * h) > 0)
      ;
   return vlds_finish(&s);
}

/* Back-compat wrapper: sub-images never use meta-Huffman. */
static uint32_t *vl_decode_pixels(vbr *br, int w, int h)
{
   return vl_decode_stream(br, w, h, 0);
}
/* ===== VP8L Full Decode with Transforms ===== */

#define XF_PRED 0
#define XF_CCOL 1
#define XF_SUBG 2
#define XF_CIDX 3
#define XF_MAX  4

typedef struct { int type, bits, dw, dh; uint32_t *data; } xf_t;

/* Decode a VP8L stream body (transforms + pixels + inverse transforms).
 * The caller supplies dimensions and a positioned bit reader; used both by
 * regular VP8L images (after the signature/size header) and by headerless
 * ALPH-chunk lossless alpha streams. */
/* Resumable VP8L body state: parsed transforms plus the pixel-stream
 * state, and a row cursor for the inverse-transform stage. */
typedef struct vlbd
{
   vlds st;
   xf_t xf[XF_MAX];
   int nxf;
   uint32_t width, height;
   int cw, ch;
   int xi;          /* current inverse transform (reverse order) */
   int xrow;        /* row cursor within the current transform */
   uint32_t *xout;  /* CIDX expansion target */
} vlbd;

static void vlbd_abort(vlbd *s)
{
   int i;
   vlds_abort(&s->st);
   for (i = 0; i < s->nxf; i++)
      free(s->xf[i].data);
   free(s->xout);
   memset(s, 0, sizeof(*s));
}

/* Parse the transform table and the pixel-stream prologue. */
static int vlbd_begin(vlbd *s, vbr *br2, uint32_t width, uint32_t height)
{
   int nxf = 0, cw, ch;
   xf_t *xf;

   memset(s, 0, sizeof(*s));
   if (width > 16384 || height > 16384) return -1;
   xf = s->xf;
   cw = (int)width; ch = (int)height;

   /* Read transforms */
   while (vbr_read(br2, 1))
   {
      int tt = vbr_read(br2, 2);
      xf_t *x;
      if (nxf >= XF_MAX) goto xfail;
      x = &xf[nxf++];
      x->type = tt;

      switch (tt)
      {
         case XF_PRED:
         case XF_CCOL:
         {
            int bb = vbr_read(br2, 3) + 2;
            int bw = ((cw-1) >> bb) + 1;
            int bh = ((ch-1) >> bb) + 1;
            x->bits = bb; x->dw = bw; x->dh = bh;
            x->data = vl_decode_pixels(br2, bw, bh);
            if (!x->data) goto xfail;
            break;
         }
         case XF_SUBG:
            break;
         case XF_CIDX:
         {
            int nc = vbr_read(br2, 8) + 1, bits, pi2;
            x->dw = nc; x->dh = 1;
            x->data = vl_decode_pixels(br2, nc, 1);
            if (!x->data) goto xfail;
            /* Delta-decode palette */
            for (pi2 = 1; pi2 < nc; pi2++)
               x->data[pi2] = px_add(x->data[pi2], x->data[pi2-1]);
            if      (nc <= 2)  bits = 3;
            else if (nc <= 4)  bits = 2;
            else if (nc <= 16) bits = 1;
            else               bits = 0;
            x->bits = bits;
            if (bits > 0) cw = ((cw + (1 << bits) - 1) >> bits);
            break;
         }
      }
   }

   s->nxf = nxf;
   s->width = width; s->height = height;
   s->cw = cw; s->ch = ch;
   s->xi = nxf - 1;
   s->xrow = 0;

   if (vlds_begin(&s->st, br2, cw, ch, 1) != 0)
      goto xfail;
   return 0;

xfail:
   {
      int i;
      for (i = 0; i < nxf; i++) free(xf[i].data);
   }
   memset(s, 0, sizeof(*s));
   return -1;
}

/* Apply up to nrows rows of the current inverse transform, advancing to
 * the next transform (reverse order) as each completes. Returns 1 while
 * transform work remains, 0 when all transforms are done, -1 on error.
 * Row-range slicing preserves the original top-to-bottom order, which
 * the predictor transform relies on. The loop bodies are the original
 * inverse-transform passes verbatim. */
static int vlbd_xform_rows(vlbd *s, int nrows)
{
   uint32_t *pix = s->st.pix;
   const uint32_t width = s->width;
   const uint32_t height = s->height;
   int cw = s->cw;
   int r0, r1;

   if (s->xi < 0)
      return 0;
   r0 = s->xrow;
   r1 = r0 + nrows;
   if (r1 > (int)height) r1 = (int)height;

   {
      xf_t *x = &s->xf[s->xi];
      switch (x->type)
      {
         case XF_CIDX:
         {
            uint32_t *pal = x->data;
            int nc = x->dw, bits = x->bits, rw = (int)width;
            uint32_t *out;
            int px2, py2;
            if (!s->xout)
            {
               s->xout = (uint32_t*)malloc((size_t)rw * height * sizeof(uint32_t));
               if (!s->xout) return -1;
            }
            out = s->xout;
            for (py2 = r0; py2 < r1; py2++)
            {
               for (px2 = 0; px2 < rw; px2++)
               {
                  int idx;
                  if (bits > 0)
                  {
                     int pi2 = px2 >> bits;
                     int si  = px2 & ((1 << bits) - 1);
                     uint32_t raw = (pi2 < cw) ? pix[py2 * cw + pi2] : 0;
                     idx = ((int)((raw >> 8) & 0xFF) >> (si * (8 >> bits))) & ((1 << (8 >> bits)) - 1);
                  }
                  else
                  {
                     uint32_t raw = (px2 < cw) ? pix[py2 * cw + px2] : 0;
                     idx = (raw >> 8) & 0xFF;
                  }
                  if (idx >= nc) idx = 0;
                  out[py2 * rw + px2] = pal[idx];
               }
            }
            if (r1 >= (int)height)
            {
               free(s->st.pix);
               s->st.pix = s->xout;
               s->xout = NULL;
               s->cw = rw;
            }
            break;
         }
         case XF_SUBG:
         {
            int j, j0 = r0 * cw, j1 = r1 * cw;
            for (j = j0; j < j1; j++)
            {
               uint32_t c = pix[j];
               uint32_t g = (c >> 8) & 0xFF;
               uint32_t r = (((c >> 16) & 0xFF) + g) & 0xFF;
               uint32_t b2 = ((c & 0xFF) + g) & 0xFF;
               pix[j] = (c & 0xFF00FF00u) | (r << 16) | b2;
            }
            break;
         }
         case XF_PRED:
         {
            int bw = x->dw;
            uint32_t *td = x->data;
            int px2, py2;
            for (py2 = r0; py2 < r1; py2++)
            {
               for (px2 = 0; px2 < cw; px2++)
               {
                  uint32_t L  = (px2 > 0)            ? pix[py2*cw+px2-1]     : 0xFF000000u;
                  uint32_t T  = (py2 > 0)             ? pix[(py2-1)*cw+px2]   : 0xFF000000u;
                  uint32_t TL = (px2>0 && py2>0)      ? pix[(py2-1)*cw+px2-1] : 0xFF000000u;
                  /* TR at the last column wraps to the current row's
                   * first pixel (libwebp reads upper[x+1] in the flat
                   * buffer, which is contiguous with the next row). */
                  uint32_t TR = (py2 > 0)             ? pix[(py2-1)*cw+px2+1] : 0xFF000000u;
                  int bx = px2 >> x->bits, by = py2 >> x->bits;
                  int mode;
                  if (bx >= bw) bx = bw - 1;
                  mode = (td[by * bw + bx] >> 8) & 0xF;
                  if (px2 == 0 && py2 == 0) mode = 0;
                  else if (px2 == 0) mode = 2;
                  else if (py2 == 0) mode = 1;
                  pix[py2*cw+px2] = px_add(pix[py2*cw+px2], px_predict(mode, L, T, TL, TR));
               }
            }
            break;
         }
         case XF_CCOL:
         {
            int bw = x->dw;
            uint32_t *td = x->data;
            int px2, py2;
            for (py2 = r0; py2 < r1; py2++)
            {
               for (px2 = 0; px2 < cw; px2++)
               {
                  int bx = px2 >> x->bits, by = py2 >> x->bits;
                  uint32_t td2, c2;
                  int8_t g2r, g2b, r2b;
                  int r, g, b2;
                  if (bx >= bw) bx = bw - 1;
                  td2 = td[by * bw + bx];
                  /* libwebp ColorCodeToMultipliers: green_to_red in the
                   * BLUE byte, green_to_blue in GREEN, red_to_blue in RED. */
                  g2r = (int8_t)(td2 & 0xFF);
                  g2b = (int8_t)((td2 >>  8) & 0xFF);
                  r2b = (int8_t)((td2 >> 16) & 0xFF);
                  c2 = pix[py2 * cw + px2];
                  /* Channel values are SIGNED in the color transform
                   * (libwebp ColorTransformDelta takes int8_t). */
                  g = (int)(int8_t)((c2 >> 8) & 0xFF);
                  r = (int)((c2 >> 16) & 0xFF);
                  b2 = (int)(c2 & 0xFF);
                  r = (r + ((g2r * g) >> 5)) & 0xFF;
                  b2 = b2 + ((g2b * g) >> 5);
                  b2 = (b2 + ((r2b * (int)(int8_t)r) >> 5)) & 0xFF;
                  pix[py2*cw+px2] = (c2 & 0xFF00FF00u) | ((uint32_t)r << 16) | (uint32_t)b2;
               }
            }
            break;
         }
      }
   }

   if (r1 >= (int)height)
   {
      s->xi--;
      s->xrow = 0;
   }
   else
      s->xrow = r1;
   return (s->xi >= 0) ? 1 : 0;
}

/* Detach the finished pixel buffer and free the rest. */
static uint32_t *vlbd_finish(vlbd *s)
{
   uint32_t *pix = s->st.pix;
   int i;
   s->st.pix = NULL;
   vlds_abort(&s->st);
   for (i = 0; i < s->nxf; i++)
      free(s->xf[i].data);
   free(s->xout);
   memset(s, 0, sizeof(*s));
   return pix;
}

static uint32_t *vl_decode_body(vbr *brp, uint32_t width, uint32_t height)
{
   vlbd s;
   if (vlbd_begin(&s, brp, width, height) != 0)
      return NULL;
   while (vlds_pixels(&s.st, s.cw * s.ch) > 0)
      ;
   while (vlbd_xform_rows(&s, (int)height) > 0)
      ;
   return vlbd_finish(&s);
}

static uint32_t *vl_decode_full(const uint8_t *data, size_t len,
      unsigned *ow, unsigned *oh)
{
   vbr br;
   uint32_t sig, width, height;
   uint32_t *pix;

   vbr_init(&br, data, len);
   sig = vbr_read(&br, 8);
   if (sig != 0x2F) return NULL;
   width = vbr_read(&br, 14) + 1;
   height = vbr_read(&br, 14) + 1;
   vbr_read(&br, 1); /* alpha_is_used */
   if (vbr_read(&br, 3) != 0) return NULL; /* version */
   pix = vl_decode_body(&br, width, height);
   if (pix) { *ow = width; *oh = height; }
   return pix;
}

/* ===== ALPH chunk (lossy alpha plane) =====
 * Header byte: bits 0-1 compression (0 raw, 1 VP8L), bits 2-3 filter
 * (0 none, 1 horizontal, 2 vertical, 3 gradient), bits 4-5 preprocessing,
 * bits 6-7 reserved (must be 0). The VP8L stream is headerless (no
 * signature or size); dimensions come from the VP8 frame. Alpha values
 * live in the green channel of the decoded ARGB. */

static INLINE int alph_grad(int a, int b, int c)
{
   int g = a + b - c;
   return ((g & ~0xFF) == 0) ? g : (g < 0) ? 0 : 255;
}

/* In-place row unfilter, per libwebp WebPUnfilters. prev == NULL on row 0. */
static void alph_unfilter_row(int filter, const uint8_t *prev,
      uint8_t *row, int width)
{
   int i;
   switch (filter)
   {
      case 1: /* horizontal */
      {
         int pred = prev ? prev[0] : 0;
         for (i = 0; i < width; i++)
         {
            row[i] = (uint8_t)(pred + row[i]);
            pred = row[i];
         }
         break;
      }
      case 2: /* vertical */
         if (!prev) { alph_unfilter_row(1, NULL, row, width); break; }
         for (i = 0; i < width; i++)
            row[i] = (uint8_t)(prev[i] + row[i]);
         break;
      case 3: /* gradient */
         if (!prev) { alph_unfilter_row(1, NULL, row, width); break; }
         {
            int top = prev[0], top_left = top, left = top;
            for (i = 0; i < width; i++)
            {
               top = prev[i];
               left = (uint8_t)(row[i] + alph_grad(left, top, top_left));
               top_left = top;
               row[i] = (uint8_t)left;
            }
         }
         break;
      default:
         break;
   }
}

/* Decode an ALPH chunk into a w*h byte plane. Returns NULL on failure
 * (caller keeps opaque alpha). */
static uint8_t *alph_decode(const uint8_t *data, size_t len,
      unsigned w, unsigned h)
{
   int method, filter, rsrv;
   uint8_t *plane;
   unsigned y;

   if (len < 1 || w == 0 || h == 0)
      return NULL;
   method = data[0] & 3;
   filter = (data[0] >> 2) & 3;
   rsrv   = (data[0] >> 6) & 3;
   if (method > 1 || rsrv != 0)
      return NULL;

   plane = (uint8_t*)malloc((size_t)w * h);
   if (!plane)
      return NULL;

   if (method == 0)
   {
      if (len - 1 < (size_t)w * h) { free(plane); return NULL; }
      memcpy(plane, data + 1, (size_t)w * h);
   }
   else
   {
      vbr br;
      uint32_t *pix;
      size_t n = (size_t)w * h, k;
      vbr_init(&br, data + 1, len - 1);
      pix = vl_decode_body(&br, w, h);
      if (!pix) { free(plane); return NULL; }
      for (k = 0; k < n; k++)
         plane[k] = (uint8_t)((pix[k] >> 8) & 0xFF); /* green */
      free(pix);
   }

   for (y = 0; y < h; y++)
      alph_unfilter_row(filter, y ? plane + (size_t)(y-1)*w : NULL,
            plane + (size_t)y*w, (int)w);

   return plane;
}



/* ===== Top-level ===== */

static uint32_t *rwebp_do(const uint8_t *buf, size_t len,
      unsigned *w, unsigned *h, bool rgba)
{
   rw_ctr c;
   uint32_t *pix = NULL;
   if (!rw_parse(buf, len, &c)) return NULL;
   if (c.vp8l && c.vp8ls > 0) pix = vl_decode_full(c.vp8l, c.vp8ls, w, h);
   if (!pix && c.vp8 && c.vp8s > 0)
   {
      pix = rvp8_decode(c.vp8, c.vp8s, w, h);
      if (pix && c.alph && c.alphs > 0)
      {
         uint8_t *ap = alph_decode(c.alph, c.alphs, *w, *h);
         if (ap)
         {
            size_t k, n = (size_t)*w * *h;
            for (k = 0; k < n; k++)
               pix[k] = (pix[k] & 0x00FFFFFFu) | ((uint32_t)ap[k] << 24);
            free(ap);
         }
      }
   }
   if (!pix) return NULL;
   if (rgba)
   {
      unsigned i, n = (*w) * (*h);
      for (i = 0; i < n; i++)
      {
         uint32_t p = pix[i];
         pix[i] = (p & 0xFF00FF00u) | ((p & 0xFF) << 16) | ((p >> 16) & 0xFF);
      }
   }
   return pix;
}

/* Decode a single ANMF frame payload (the bytes after the 16-byte ANMF
 * header) into RGBA. Mirrors rwebp_do's lossy/lossless + ALPH handling
 * but operates on the frame's local chunk list. */
static uint32_t *rwebp_anim_frame_pixels(const uint8_t *d, size_t sz,
      unsigned *ow, unsigned *oh, int *out_opaque)
{
   const uint8_t *fv = NULL, *fl = NULL, *fa = NULL;
   size_t fvs = 0, fls = 0, fas = 0;
   size_t sp;
   uint32_t *pix = NULL;
   unsigned w = 0, h = 0;

   for (sp = 0; sp + 8 <= sz; )
   {
      uint32_t st = rw32(d+sp), ss = rw32(d+sp+4);
      if (sp + 8 + ss > sz) break;
      if      (st == RW_CC('V','P','8',' ') && !fv) { fv = d+sp+8; fvs = ss; }
      else if (st == RW_CC('V','P','8','L') && !fl) { fl = d+sp+8; fls = ss; }
      else if (st == RW_CC('A','L','P','H') && !fa) { fa = d+sp+8; fas = ss; }
      sp += 8 + ((ss+1) & ~(size_t)1);
   }

   if (out_opaque)
      *out_opaque = 0;
   if (fl && fls > 0)
      pix = vl_decode_full(fl, fls, &w, &h);
   else if (fv && fvs > 0)
   {
      /* A VP8 sub-image without an ALPH chunk decodes with every alpha
       * byte set to 0xFF; blending a fully opaque source over the
       * canvas is defined (and implemented) as a plain copy, so the
       * caller can skip per-pixel blending for such frames. */
      if (out_opaque && !(fa && fas > 0))
         *out_opaque = 1;
      pix = rvp8_decode(fv, fvs, &w, &h);
      if (pix && fa && fas > 0)
      {
         uint8_t *ap = alph_decode(fa, fas, w, h);
         if (ap)
         {
            size_t k, n = (size_t)w * h;
            for (k = 0; k < n; k++)
               pix[k] = (pix[k] & 0x00FFFFFFu) | ((uint32_t)ap[k] << 24);
            free(ap);
         }
      }
   }
   if (!pix) return NULL;
   /* Convert to R,B-swapped (memory R,G,B,A) to match the anim canvas.
    * While every pixel is being touched anyway, accumulate an AND-mask
    * of the alpha bytes: encoders routinely emit an ALPH chunk (or a
    * VP8L alpha channel) whose bytes are all 0xFF, and detecting that
    * here lets the compositor use a plain row copy instead of running
    * per-pixel blending over the whole frame. */
   {
      unsigned i, n = w * h;
      uint32_t acc = 0xFF000000u;
      for (i = 0; i < n; i++)
      {
         uint32_t px = pix[i];
         acc   &= px;
         pix[i] = (px & 0xFF00FF00u) | ((px & 0xFF) << 16) | ((px >> 16) & 0xFF);
      }
      if (out_opaque && (acc >> 24) == 0xFF)
         *out_opaque = 1;
   }
   *ow = w; *oh = h;
   return pix;
}

/* ===== Animation (ANMF) decoder =====
 * Decodes an animated WebP into a sequence of fully-composited RGBA
 * canvas frames plus per-frame durations, following the canvas/blend/
 * dispose model from the WebP container spec (mirrors libwebp's
 * anim_decode.c). Each returned frame is a complete canvas ready to be
 * uploaded as a texture; the caller advances frames on its own clock.
 *
 * This lives alongside the still-image path and does not affect it. */

typedef struct
{
   uint32_t *pixels;   /* canvas_w * canvas_h, memory order R,G,B,A */
   int       duration; /* milliseconds; 0 is treated as a single tick */
} rwebp_frame;

struct rwebp_anim
{
   rwebp_frame *frames;
   int          num_frames;
   int          canvas_w, canvas_h;
   int          loop_count;   /* 0 = infinite */
};

/* Non-premultiplied "src over dst", per libwebp BlendPixelNonPremult.
 * All pixels are R,G,B,A in memory (little-endian word A,B,G,R). */
static uint32_t rwebp_blend_px(uint32_t src, uint32_t dst)
{
   int src_a = (int)((src >> 24) & 0xFF);
   int dst_a, dst_fa, blend_a, i;
   uint32_t scale, out;
   /* libwebp BlendPixelRowNonPremult leaves fully-opaque source pixels
    * untouched; running the blend on them would round each channel down
    * by one. */
   if (src_a == 0xFF) return src;
   if (src_a == 0)    return dst;
   dst_a  = (int)((dst >> 24) & 0xFF);
   dst_fa = (dst_a * (256 - src_a)) >> 8;
   blend_a = src_a + dst_fa;
   if (blend_a == 0) return 0;
   scale = (1UL << 24) / (uint32_t)blend_a;
   out = (uint32_t)blend_a << 24;
   for (i = 0; i < 3; i++)
   {
      int sc = (int)((src >> (i*8)) & 0xFF);
      int dc = (int)((dst >> (i*8)) & 0xFF);
      uint32_t bu = (uint32_t)(sc * src_a + dc * dst_fa);
      uint32_t v  = (bu * scale) >> 24;
      out |= (v & 0xFF) << (i*8);
   }
   return out;
}

static int rwebp_full_frame(int w, int h, int cw, int ch)
{
   return (w == cw && h == ch);
}

void rwebp_anim_free(rwebp_anim_t *a)
{
   int i;
   if (!a) return;
   if (a->frames)
   {
      for (i = 0; i < a->num_frames; i++)
         free(a->frames[i].pixels);
      free(a->frames);
   }
   free(a);
}

int rwebp_anim_num_frames(const rwebp_anim_t *a)
{ return a ? a->num_frames : 0; }

void rwebp_anim_get_info(const rwebp_anim_t *a,
      unsigned *width, unsigned *height, int *loop_count)
{
   if (!a) return;
   if (width)      *width      = (unsigned)a->canvas_w;
   if (height)     *height     = (unsigned)a->canvas_h;
   if (loop_count) *loop_count = a->loop_count;
}

const uint32_t *rwebp_anim_get_frame(const rwebp_anim_t *a, int index,
      int *duration_ms)
{
   if (!a || index < 0 || index >= a->num_frames) return NULL;
   if (duration_ms) *duration_ms = a->frames[index].duration;
   return a->frames[index].pixels;
}

/* ---- Streaming iterator ----
 * Holds two canvases plus a BORROWED pointer to the caller's file
 * buffer, so memory stays bounded no matter how many frames the
 * animation has. The eager rwebp_anim_decode below is a thin wrapper
 * that collects every canvas from a stream. */

struct rwebp_anmf_ent { size_t off; uint32_t sz; };
struct rwebp_anim_stream
{
   const uint8_t *buf;    /* borrowed; must outlive the stream */
   size_t         len;
   struct rwebp_anmf_ent *anmf; /* ANMF payload offsets */
   int       num_anmf;
   int       cursor;      /* next ANMF index to try */
   int       emitted;     /* frames emitted since open/rewind */
   int       canvas_w, canvas_h;
   int       loop_count;
   uint32_t *canvas;
   /* Disposal is applied lazily: the returned canvas must stay intact
    * until the next call, and the post-disposal state differs from it
    * only inside the previous frame's rectangle, so instead of keeping
    * a second full canvas ('disposed') and copying the whole canvas
    * back and forth around every frame, remember the rectangle and
    * clear just that region when the next frame is prepared. */
   int       prev_fx, prev_fy, prev_fw, prev_fh;
   int       prev_disp_bg, prev_full, prev_key;
};

void rwebp_anim_stream_close(rwebp_anim_stream_t *s)
{
   if (!s) return;
   free(s->anmf);
   free(s->canvas);
   free(s);
}

rwebp_anim_stream_t *rwebp_anim_stream_open(const uint8_t *buf, size_t len)
{
   rwebp_anim_stream_t *s;
   int cw = 0, ch = 0, loop = 0, cap = 0;
   size_t p;

   if (len < 12 || rw32(buf) != RW_CC('R','I','F','F')
       || rw32(buf+8) != RW_CC('W','E','B','P'))
      return NULL;

   s = (rwebp_anim_stream_t*)calloc(1, sizeof(*s));
   if (!s) return NULL;
   s->buf = buf; s->len = len;

   for (p = 12; p + 8 <= len; )
   {
      uint32_t tag = rw32(buf+p), sz = rw32(buf+p+4);
      const uint8_t *d = buf + p + 8;
      if (p + 8 + sz > len) break;
      if (tag == RW_CC('V','P','8','X') && sz >= 10)
      {
         cw = (int)(((uint32_t)d[4] | ((uint32_t)d[5]<<8) | ((uint32_t)d[6]<<16)) + 1);
         ch = (int)(((uint32_t)d[7] | ((uint32_t)d[8]<<8) | ((uint32_t)d[9]<<16)) + 1);
      }
      else if (tag == RW_CC('A','N','I','M') && sz >= 6)
         loop = (int)((uint32_t)d[4] | ((uint32_t)d[5]<<8));
      else if (tag == RW_CC('A','N','M','F') && sz >= 16)
      {
         if (s->num_anmf >= cap)
         {
            struct rwebp_anmf_ent *na;
            int ncap = cap ? cap * 2 : 16;
            na = (struct rwebp_anmf_ent*)realloc(s->anmf,
                  (size_t)ncap * sizeof(*s->anmf));
            if (!na) goto ofail;
            s->anmf = na; cap = ncap;
         }
         s->anmf[s->num_anmf].off = p + 8;
         s->anmf[s->num_anmf].sz  = sz;
         s->num_anmf++;
      }
      p += 8 + ((sz+1) & ~(size_t)1);
   }

   if (cw <= 0 || ch <= 0 || cw > 16384 || ch > 16384 || s->num_anmf == 0)
      goto ofail;

   s->canvas_w = cw; s->canvas_h = ch; s->loop_count = loop;
   s->canvas   = (uint32_t*)calloc((size_t)cw * ch, sizeof(uint32_t));
   if (!s->canvas) goto ofail;
   return s;

ofail:
   rwebp_anim_stream_close(s);
   return NULL;
}

void rwebp_anim_stream_get_info(const rwebp_anim_stream_t *s,
      unsigned *width, unsigned *height, int *num_frames, int *loop_count)
{
   if (!s) return;
   if (width)      *width      = (unsigned)s->canvas_w;
   if (height)     *height     = (unsigned)s->canvas_h;
   if (num_frames) *num_frames = s->num_anmf;
   if (loop_count) *loop_count = s->loop_count;
}

void rwebp_anim_stream_rewind(rwebp_anim_stream_t *s)
{
   if (!s) return;
   s->cursor       = 0;
   s->emitted      = 0;
   s->prev_disp_bg = 0;
   s->prev_full    = 0;
   s->prev_key     = 0;
   /* No canvas clearing needed: the first emitted frame is always a
    * key frame, which memsets the canvas before compositing. */
}

const uint32_t *rwebp_anim_stream_next(rwebp_anim_stream_t *s,
      int *duration_ms)
{
   int cw, ch;
   if (!s) return NULL;
   cw = s->canvas_w; ch = s->canvas_h;

   while (s->cursor < s->num_anmf)
   {
      const uint8_t *d = s->buf + s->anmf[s->cursor].off;
      uint32_t sz      = s->anmf[s->cursor].sz;
      int fx = (int)(((uint32_t)d[0] | ((uint32_t)d[1]<<8) | ((uint32_t)d[2]<<16)) * 2);
      int fy = (int)(((uint32_t)d[3] | ((uint32_t)d[4]<<8) | ((uint32_t)d[5]<<16)) * 2);
      int fw = (int)(((uint32_t)d[6] | ((uint32_t)d[7]<<8) | ((uint32_t)d[8]<<16)) + 1);
      int fh = (int)(((uint32_t)d[9] | ((uint32_t)d[10]<<8) | ((uint32_t)d[11]<<16)) + 1);
      int dur = (int)((uint32_t)d[12] | ((uint32_t)d[13]<<8) | ((uint32_t)d[14]<<16));
      int disp_bg  = (d[15] & 1) ? 1 : 0;
      int no_blend = (d[15] & 2) ? 1 : 0;
      unsigned sub_w = 0, sub_h = 0;
      uint32_t *sub;
      int is_key, x, y;
      int sub_opaque = 0;

      s->cursor++;

      if (fx < 0 || fy < 0 || fw <= 0 || fh <= 0
            || fx + fw > cw || fy + fh > ch)
         continue;

      sub = rwebp_anim_frame_pixels(d + 16, sz - 16, &sub_w, &sub_h,
            &sub_opaque);
      if (!sub)
         continue;
      if ((int)sub_w != fw || (int)sub_h != fh)
      { free(sub); continue; }

      if (s->emitted == 0)
         is_key = 1;
      else if (no_blend && rwebp_full_frame(fw, fh, cw, ch))
         is_key = 1;
      else
         is_key = s->prev_disp_bg && (s->prev_full || s->prev_key);

      if (is_key)
      {
         /* When a full-canvas opaque copy follows, the memset would be
          * overwritten entirely - skip it. */
         if (!(   (no_blend || sub_opaque)
               && rwebp_full_frame(fw, fh, cw, ch)))
            memset(s->canvas, 0, (size_t)cw * ch * sizeof(uint32_t));
      }
      else if (s->prev_disp_bg)
      {
         /* Lazily apply the previous frame's dispose-to-background:
          * clear its rectangle (the only region where the disposal
          * state differs from the displayed canvas). */
         for (y = 0; y < s->prev_fh; y++)
            memset(s->canvas + (size_t)(s->prev_fy + y) * cw + s->prev_fx,
                  0, (size_t)s->prev_fw * sizeof(uint32_t));
      }

      for (y = 0; y < fh; y++)
      {
         uint32_t *crow = s->canvas + (size_t)(fy + y) * cw + fx;
         const uint32_t *srow = sub + (size_t)y * fw;
         /* A fully opaque source blends to a plain copy (the per-pixel
          * fast path in rwebp_blend_px), so frames known opaque at
          * parse time skip the per-pixel loop entirely. */
         if (no_blend || sub_opaque)
            memcpy(crow, srow, (size_t)fw * sizeof(uint32_t));
         else
            for (x = 0; x < fw; x++)
               crow[x] = rwebp_blend_px(srow[x], crow[x]);
      }
      free(sub);

      s->prev_fx      = fx;
      s->prev_fy      = fy;
      s->prev_fw      = fw;
      s->prev_fh      = fh;
      s->prev_disp_bg = disp_bg;
      s->prev_full    = rwebp_full_frame(fw, fh, cw, ch);
      s->prev_key     = is_key;
      s->emitted++;

      if (duration_ms) *duration_ms = dur;
      return s->canvas;
   }
   return NULL;
}

/* ---- Eager decode: collect every frame from a stream. ---- */
rwebp_anim_t *rwebp_anim_decode(const uint8_t *buf, size_t len)
{
   rwebp_anim_t *a;
   rwebp_anim_stream_t *s;
   const uint32_t *px;
   int dur, cap = 0;
   size_t canvas_px;

   s = rwebp_anim_stream_open(buf, len);
   if (!s) return NULL;

   a = (rwebp_anim_t*)calloc(1, sizeof(*a));
   if (!a) { rwebp_anim_stream_close(s); return NULL; }
   a->canvas_w   = s->canvas_w;
   a->canvas_h   = s->canvas_h;
   a->loop_count = s->loop_count;
   canvas_px     = (size_t)s->canvas_w * s->canvas_h;

   while ((px = rwebp_anim_stream_next(s, &dur)) != NULL)
   {
      if (a->num_frames >= cap)
      {
         int ncap = cap ? cap * 2 : 8;
         rwebp_frame *nf = (rwebp_frame*)realloc(a->frames,
               (size_t)ncap * sizeof(rwebp_frame));
         if (!nf) goto afail;
         a->frames = nf; cap = ncap;
      }
      a->frames[a->num_frames].pixels =
            (uint32_t*)malloc(canvas_px * sizeof(uint32_t));
      if (!a->frames[a->num_frames].pixels) goto afail;
      memcpy(a->frames[a->num_frames].pixels, px,
            canvas_px * sizeof(uint32_t));
      a->frames[a->num_frames].duration = dur;
      a->num_frames++;
   }

   rwebp_anim_stream_close(s);
   if (a->num_frames == 0) { rwebp_anim_free(a); return NULL; }
   return a;

afail:
   rwebp_anim_stream_close(s);
   rwebp_anim_free(a);
   return NULL;
}

/* ===== Public API ===== */

/* Incremental decode phases for rwebp_process_image. Mirrors the
 * rpng/rjpeg contract: each call does a bounded slice of work and
 * returns IMAGE_PROCESS_NEXT, so the caller's time-budgeted loop can
 * yield between slices instead of blocking on one monolithic decode.
 * The sliced path covers lossy (VP8) images without an alpha chunk -
 * the common large-photo case; lossless and alpha-bearing images fall
 * back to a single-shot decode on the first call. */
#define RWEBP_PHASE_IDLE       0
#define RWEBP_PHASE_ROWS       1
#define RWEBP_PHASE_FILTER     2
#define RWEBP_PHASE_UPSAMPLE   3
#define RWEBP_PHASE_SWIZZLE    4
#define RWEBP_PHASE_L_PIXELS   5
#define RWEBP_PHASE_L_XFORM    6

/* MB rows decoded per call (~0.5-3 ms depending on content and host),
 * loop-filter MB rows per call, upsampled luma rows per call, and
 * swizzled pixels per call. Each is sized to keep individual calls
 * well under a vsync so the caller's budget check stays responsive. */
#define RWEBP_ROWS_PER_CALL     4
#define RWEBP_LF_ROWS_PER_CALL  8
#define RWEBP_UPS_ROWS_PER_CALL 128
#define RWEBP_SWZ_PX_PER_CALL   (512 * 1024)
#define RWEBP_L_PX_PER_CALL     (64 * 1024)
#define RWEBP_L_XF_ROWS_PER_CALL 256

struct rwebp
{
   uint8_t *buff_data;
   size_t buff_len;
   uint32_t *output_image;
   union
   {
      rvp8_dec d;        /* lossy (VP8) incremental state */
      struct
      {
         vlbd b;     /* lossless (VP8L) incremental state */
         vbr br;     /* bit reader referenced by b (must not move) */
      } l;
   } u;
   int phase;
   int cursor;       /* row / pixel progress within the current phase */
   int swizzle;      /* supports_rgba latched at phase start */
};

/* Tear down an in-flight incremental decode. Frees the pending output
 * buffer as well - callers that hand the buffer out (the END paths)
 * must clear output_image first, transferring ownership. */
static void rwebp_proc_reset(rwebp_t *rwebp)
{
   if (rwebp->phase != RWEBP_PHASE_IDLE)
   {
      if (rwebp->phase == RWEBP_PHASE_L_PIXELS
            || rwebp->phase == RWEBP_PHASE_L_XFORM)
         vlbd_abort(&rwebp->u.l.b);
      else
         rvp8_abort(&rwebp->u.d);
      free(rwebp->output_image);
      rwebp->output_image = NULL;
   }
   rwebp->phase  = RWEBP_PHASE_IDLE;
   rwebp->cursor = 0;
}

int rwebp_process_image(rwebp_t *rwebp, void **buf_data,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba)
{
   size_t len;
   if (!rwebp || !rwebp->buff_data) return IMAGE_PROCESS_ERROR;
   len = rwebp->buff_len > 0 ? rwebp->buff_len : size;

   switch (rwebp->phase)
   {
      case RWEBP_PHASE_IDLE:
      {
         rw_ctr c;
         if (rw_parse(rwebp->buff_data, len, &c)
               && c.vp8 && c.vp8s > 0
               && !(c.vp8l && c.vp8ls > 0)
               && !(c.alph && c.alphs > 0))
         {
            /* Sliced lossy path */
            if (rvp8_begin(c.vp8, c.vp8s, &rwebp->u.d) != 0)
               return IMAGE_PROCESS_ERROR;
            rwebp->output_image = rvp8_output(&rwebp->u.d);
            if (!rwebp->output_image)
            {
               rvp8_abort(&rwebp->u.d);
               return IMAGE_PROCESS_ERROR;
            }
            rwebp->phase   = RWEBP_PHASE_ROWS;
            rwebp->cursor  = 0;
            rwebp->swizzle = supports_rgba ? 1 : 0;
            *width  = (unsigned)rwebp->u.d.w;
            *height = (unsigned)rwebp->u.d.h;
            return IMAGE_PROCESS_NEXT;
         }
         if (rw_parse(rwebp->buff_data, len, &c)
               && c.vp8l && c.vp8ls > 0)
         {
            /* Sliced lossless path. The header is parsed here; the
             * bit reader must live in the context because the stream
             * state keeps a pointer to it across calls. */
            uint32_t sig, lw, lh;
            vbr *br = &rwebp->u.l.br;
            vbr_init(br, c.vp8l, c.vp8ls);
            sig = vbr_read(br, 8);
            lw  = vbr_read(br, 14) + 1;
            lh  = vbr_read(br, 14) + 1;
            vbr_read(br, 1);                    /* alpha_is_used */
            if (sig == 0x2F && vbr_read(br, 3) == 0
                  && vlbd_begin(&rwebp->u.l.b, br, lw, lh) == 0)
            {
               rwebp->phase   = RWEBP_PHASE_L_PIXELS;
               rwebp->cursor  = 0;
               rwebp->swizzle = supports_rgba ? 1 : 0;
               *width  = lw;
               *height = lh;
               return IMAGE_PROCESS_NEXT;
            }
            /* Malformed header: fall through to the one-shot path,
             * which reports the failure through the usual route. */
         }
         /* Everything else: single-shot decode */
         rwebp->output_image = rwebp_do(rwebp->buff_data, len,
               width, height, supports_rgba);
         *buf_data = rwebp->output_image;
         return rwebp->output_image ? IMAGE_PROCESS_END : IMAGE_PROCESS_ERROR;
      }

      case RWEBP_PHASE_L_PIXELS:
         if (vlds_pixels(&rwebp->u.l.b.st, RWEBP_L_PX_PER_CALL) == 0)
            rwebp->phase = RWEBP_PHASE_L_XFORM;
         *width  = rwebp->u.l.b.width;
         *height = rwebp->u.l.b.height;
         return IMAGE_PROCESS_NEXT;

      case RWEBP_PHASE_L_XFORM:
      {
         /* Capture dimensions first: vlbd_finish clears the state. */
         unsigned lw = rwebp->u.l.b.width;
         unsigned lh = rwebp->u.l.b.height;
         int rc = vlbd_xform_rows(&rwebp->u.l.b, RWEBP_L_XF_ROWS_PER_CALL);
         *width  = lw;
         *height = lh;
         if (rc < 0)
         {
            rwebp_proc_reset(rwebp);
            return IMAGE_PROCESS_ERROR;
         }
         if (rc > 0)
            return IMAGE_PROCESS_NEXT;
         rwebp->output_image = vlbd_finish(&rwebp->u.l.b);
         if (!rwebp->output_image)
         {
            rwebp->phase = RWEBP_PHASE_IDLE;
            return IMAGE_PROCESS_ERROR;
         }
         if (rwebp->swizzle)
         {
            /* Reuse the shared swizzle phase; it reads dimensions from
             * the VP8 state slot, so park them there (the union member
             * holding the lossless state is dead after vlbd_finish). */
            rwebp->u.d.w = (int)lw;
            rwebp->u.d.h = (int)lh;
            rwebp->phase  = RWEBP_PHASE_SWIZZLE;
            rwebp->cursor = 0;
            return IMAGE_PROCESS_NEXT;
         }
         *buf_data = rwebp->output_image;
         rwebp->output_image = NULL; /* ownership -> caller */
         rwebp->phase = RWEBP_PHASE_IDLE;
         return IMAGE_PROCESS_END;
      }

      case RWEBP_PHASE_ROWS:
         if (rvp8_rows(&rwebp->u.d, RWEBP_ROWS_PER_CALL) == 0)
         {
            rwebp->phase  = RWEBP_PHASE_FILTER;
            rwebp->cursor = 0;
         }
         *width  = (unsigned)rwebp->u.d.w;
         *height = (unsigned)rwebp->u.d.h;
         return IMAGE_PROCESS_NEXT;

      case RWEBP_PHASE_FILTER:
         if (rwebp->u.d.lf_level <= 0)
         {
            rwebp->phase  = RWEBP_PHASE_UPSAMPLE;
            rwebp->cursor = 0;
         }
         else
         {
            rvp8_filter_rows(&rwebp->u.d, rwebp->cursor,
                  rwebp->cursor + RWEBP_LF_ROWS_PER_CALL);
            rwebp->cursor += RWEBP_LF_ROWS_PER_CALL;
            if (rwebp->cursor >= rwebp->u.d.mbh)
            {
               rwebp->phase  = RWEBP_PHASE_UPSAMPLE;
               rwebp->cursor = 0;
            }
         }
         *width  = (unsigned)rwebp->u.d.w;
         *height = (unsigned)rwebp->u.d.h;
         return IMAGE_PROCESS_NEXT;

      case RWEBP_PHASE_UPSAMPLE:
         rwebp->cursor = rvp8_upsample_rows(&rwebp->u.d,
               rwebp->output_image, rwebp->cursor,
               RWEBP_UPS_ROWS_PER_CALL);
         *width  = (unsigned)rwebp->u.d.w;
         *height = (unsigned)rwebp->u.d.h;
         if (rwebp->cursor >= rwebp->u.d.h)
         {
            if (rwebp->swizzle)
            {
               rwebp->phase  = RWEBP_PHASE_SWIZZLE;
               rwebp->cursor = 0;
               return IMAGE_PROCESS_NEXT;
            }
            *buf_data = rwebp->output_image;
            rwebp->output_image = NULL; /* ownership -> caller */
            rwebp_proc_reset(rwebp);
            return IMAGE_PROCESS_END;
         }
         return IMAGE_PROCESS_NEXT;

      case RWEBP_PHASE_SWIZZLE:
      {
         /* ARGB words -> ABGR (memory R,G,B,A), a bounded pixel batch
          * per call; matches the rwebp_do output conversion. */
         uint32_t *pix = rwebp->output_image;
         int n = rwebp->u.d.w * rwebp->u.d.h;
         int i = rwebp->cursor;
         int e = i + RWEBP_SWZ_PX_PER_CALL;
         if (e > n) e = n;
         for (; i < e; i++)
         {
            uint32_t p = pix[i];
            pix[i] = (p & 0xFF00FF00u) | ((p & 0xFF) << 16) | ((p >> 16) & 0xFF);
         }
         rwebp->cursor = e;
         *width  = (unsigned)rwebp->u.d.w;
         *height = (unsigned)rwebp->u.d.h;
         if (e >= n)
         {
            *buf_data = rwebp->output_image;
            rwebp->output_image = NULL; /* ownership -> caller */
            rwebp_proc_reset(rwebp);
            return IMAGE_PROCESS_END;
         }
         return IMAGE_PROCESS_NEXT;
      }
   }
   return IMAGE_PROCESS_ERROR;
}

bool rwebp_set_buf_ptr(rwebp_t *rwebp, void *data, size_t len)
{
   if (!rwebp) return false;
   rwebp->buff_data = (uint8_t*)data;
   rwebp->buff_len = len;
   return true;
}

void rwebp_free(rwebp_t *rwebp)
{
   if (!rwebp)
      return;
   rwebp_proc_reset(rwebp);
   free(rwebp);
}
rwebp_t *rwebp_alloc(void) { return (rwebp_t*)calloc(1, sizeof(rwebp_t)); }

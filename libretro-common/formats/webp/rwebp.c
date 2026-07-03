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
 * Supports VP8L (lossless, all 4 transforms) and VP8 (lossy, prediction-only). */

#include <stdio.h>
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <retro_inline.h>
#include <formats/image.h>
#include <formats/rwebp.h>

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
         if (ms > ns) return -1;   /* invalid per libwebp */
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
static uint32_t *vl_decode_stream(vbr *br, int w, int h, int allow_meta)
{
   uint32_t *pix = NULL;
   uint32_t *huff_img = NULL;
   vh_group *groups = NULL;
   uint32_t *cc = NULL;
   uint8_t *cl = NULL;
   int ccb = 0, ccs = 0;
   int hbits = 0, hxs = 0;
   int num_groups = 1, gi, pi;

   /* --- Color cache (before Huffman codes, per DecodeImageStream) --- */
   if (vbr_read(br, 1))
   {
      ccb = vbr_read(br, 4);
      if (ccb < 1 || ccb > 11) return NULL;
      ccs = 1 << ccb;
      cc = (uint32_t*)calloc(ccs, sizeof(uint32_t));
      if (!cc) return NULL;
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
      if (!huff_img) { free(cc); return NULL; }
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
   if (!cl) goto sfail;
   groups = (vh_group*)calloc(num_groups, sizeof(vh_group));
   if (!groups) goto sfail;
   for (gi = 0; gi < num_groups; gi++)
      if (vl_read_group(br, &groups[gi], ccs, cl) < 0) goto sfail;

   pix = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
   if (!pix) goto sfail;

   /* --- Decode pixels; select group per block from the entropy image --- */
   pi = 0;
   while (pi < w * h)
   {
      int x = pi % w, y = pi / w;
      vh_group *g;
      int sym;
      if (huff_img)
      {
         int mi = huff_img[hxs * (y >> hbits) + (x >> hbits)];
         if (mi < 0 || mi >= num_groups) mi = 0;
         g = &groups[mi];
      }
      else
         g = &groups[0];

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

   free(cl);
   free(cc);
   free(huff_img);
   for (gi = 0; gi < num_groups; gi++) vl_free_group(&groups[gi]);
   free(groups);
   return pix;

sfail:
   free(cl);
   free(cc);
   free(huff_img);
   if (groups) { for (gi = 0; gi < num_groups; gi++) vl_free_group(&groups[gi]); free(groups); }
   free(pix);
   return NULL;
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
static uint32_t *vl_decode_body(vbr *brp, uint32_t width, uint32_t height)
{
   vbr *br2 = brp;
   uint32_t *pix = NULL;
   xf_t xf[XF_MAX];
   int nxf = 0, i, cw, ch;

   if (width > 16384 || height > 16384) return NULL;
   memset(xf, 0, sizeof(xf));
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

   /* Decode main image */
   pix = vl_decode_stream(br2, cw, ch, 1);
   if (!pix) goto xfail;

   /* Inverse transforms in reverse order */
   for (i = nxf - 1; i >= 0; i--)
   {
      xf_t *x = &xf[i];
      switch (x->type)
      {
         case XF_CIDX:
         {
            uint32_t *pal = x->data;
            int nc = x->dw, bits = x->bits, rw = (int)width;
            uint32_t *out = (uint32_t*)malloc((size_t)rw * height * sizeof(uint32_t));
            int px2, py2;
            if (!out) goto xfail;
            for (py2 = 0; py2 < (int)height; py2++)
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
            free(pix); pix = out; cw = rw;
            break;
         }
         case XF_SUBG:
         {
            int j, n = cw * (int)height;
            for (j = 0; j < n; j++)
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
            for (py2 = 0; py2 < (int)height; py2++)
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
            for (py2 = 0; py2 < (int)height; py2++)
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

   for (i = 0; i < nxf; i++) free(xf[i].data);
   return pix;
xfail:
   free(pix);
   for (i = 0; i < nxf; i++) free(xf[i].data);
   return NULL;
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

/* ===== VP8 Lossy — full decode with coefficients ===== */

typedef struct { const uint8_t *buf, *end; uint32_t range; uint64_t value; int count; } vp8b;

static void vp8b_fill(vp8b *b)
{
   int shift = 48 - b->count;
   while (shift >= 0 && b->buf < b->end)
   {
      b->count += 8;
      b->value |= (uint64_t)(*b->buf++) << shift;
      shift -= 8;
   }
}

static void vp8b_init(vp8b *b, const uint8_t *d, size_t s)
{
   b->buf = d; b->end = d + s; b->range = 255;
   b->value = 0; b->count = -8;
   vp8b_fill(b);
}

static INLINE int vp8b_get(vp8b *b, int prob)
{
   uint32_t split = 1 + (((b->range - 1) * (uint32_t)prob) >> 8);
   uint64_t bigsplit = (uint64_t)split << 56;
   int bit, shift;
   if (b->value >= bigsplit)
   {
      bit = 1; b->range -= split; b->value -= bigsplit;
   }
   else
   {
      bit = 0; b->range = split;
   }
   shift = 0;
   while (b->range < 128) { b->range <<= 1; shift++; }
   b->value <<= shift;
   b->count -= shift;
   if (b->count < 0) vp8b_fill(b);
   return bit;
}


static INLINE int     vp8b_bit(vp8b *b)       { return vp8b_get(b, 128); }
static INLINE uint32_t vp8b_lit(vp8b *b, int n)
{ uint32_t v = 0; int i; for (i = n-1; i >= 0; i--) v |= (uint32_t)vp8b_get(b,128) << i; return v; }
static INLINE int32_t vp8b_sig(vp8b *b, int n)
{ int32_t v = (int32_t)vp8b_lit(b,n); return vp8b_bit(b) ? -v : v; }

static INLINE uint8_t vp8_cl(int v) { return (uint8_t)(v<0?0:v>255?255:v); }

/* libwebp fixed-point YUV -> RGB (yuv.h, YUV_FIX2 = 6): matches dwebp
 * output exactly. */
static INLINE int vp8_mulhi(int v, int coeff) { return (v * coeff) >> 8; }

static INLINE uint8_t vp8_clip8(int v)
{
   return ((v & ~16383) == 0) ? (uint8_t)(v >> 6) : (v < 0) ? 0 : 255;
}

static void vp8_yuv2rgb(int y, int u, int v, uint8_t *r, uint8_t *g, uint8_t *bo)
{
   int yg = vp8_mulhi(y, 19077);
   *r  = vp8_clip8(yg + vp8_mulhi(v, 26149) - 14234);
   *g  = vp8_clip8(yg - vp8_mulhi(u, 6419) - vp8_mulhi(v, 13320) + 8708);
   *bo = vp8_clip8(yg + vp8_mulhi(u, 33050) - 17685);
}

/* Fancy chroma upsampling (libwebp upsampling.c): interpolate the chroma
 * plane bilinearly with the 9-3-3-1 diagonal scheme while converting a
 * pair of luma rows. top/cur are chroma rows; either may alias for the
 * mirrored first and last rows. bot_y may be NULL (single-row case). */
static void vp8_fancy_pair(const uint8_t *top_y, const uint8_t *bot_y,
      const uint8_t *top_u, const uint8_t *top_v,
      const uint8_t *cur_u, const uint8_t *cur_v,
      uint32_t *top_dst, uint32_t *bot_dst, int len)
{
   int x, last_pair = (len - 1) >> 1;
   int tl_u = top_u[0], tl_v = top_v[0];
   int l_u = cur_u[0], l_v = cur_v[0];
   uint8_t r, g, b2;

   vp8_yuv2rgb(top_y[0], (3*tl_u + l_u + 2) >> 2, (3*tl_v + l_v + 2) >> 2,
         &r, &g, &b2);
   top_dst[0] = 0xFF000000u | ((uint32_t)r<<16) | ((uint32_t)g<<8) | b2;
   if (bot_y)
   {
      vp8_yuv2rgb(bot_y[0], (3*l_u + tl_u + 2) >> 2, (3*l_v + tl_v + 2) >> 2,
            &r, &g, &b2);
      bot_dst[0] = 0xFF000000u | ((uint32_t)r<<16) | ((uint32_t)g<<8) | b2;
   }
   for (x = 1; x <= last_pair; x++)
   {
      int t_u = top_u[x], t_v = top_v[x];
      int c_u = cur_u[x], c_v = cur_v[x];
      int avg_u = tl_u + t_u + l_u + c_u + 8;
      int avg_v = tl_v + t_v + l_v + c_v + 8;
      int d12_u = (avg_u + 2*(t_u + l_u)) >> 3, d12_v = (avg_v + 2*(t_v + l_v)) >> 3;
      int d03_u = (avg_u + 2*(tl_u + c_u)) >> 3, d03_v = (avg_v + 2*(tl_v + c_v)) >> 3;

      vp8_yuv2rgb(top_y[2*x-1], (d12_u + tl_u) >> 1, (d12_v + tl_v) >> 1, &r, &g, &b2);
      top_dst[2*x-1] = 0xFF000000u | ((uint32_t)r<<16) | ((uint32_t)g<<8) | b2;
      vp8_yuv2rgb(top_y[2*x], (d03_u + t_u) >> 1, (d03_v + t_v) >> 1, &r, &g, &b2);
      top_dst[2*x] = 0xFF000000u | ((uint32_t)r<<16) | ((uint32_t)g<<8) | b2;
      if (bot_y)
      {
         vp8_yuv2rgb(bot_y[2*x-1], (d03_u + l_u) >> 1, (d03_v + l_v) >> 1, &r, &g, &b2);
         bot_dst[2*x-1] = 0xFF000000u | ((uint32_t)r<<16) | ((uint32_t)g<<8) | b2;
         vp8_yuv2rgb(bot_y[2*x], (d12_u + c_u) >> 1, (d12_v + c_v) >> 1, &r, &g, &b2);
         bot_dst[2*x] = 0xFF000000u | ((uint32_t)r<<16) | ((uint32_t)g<<8) | b2;
      }
      tl_u = t_u; tl_v = t_v; l_u = c_u; l_v = c_v;
   }
   if (!(len & 1))
   {
      vp8_yuv2rgb(top_y[len-1], (3*tl_u + l_u + 2) >> 2, (3*tl_v + l_v + 2) >> 2,
            &r, &g, &b2);
      top_dst[len-1] = 0xFF000000u | ((uint32_t)r<<16) | ((uint32_t)g<<8) | b2;
      if (bot_y)
      {
         vp8_yuv2rgb(bot_y[len-1], (3*l_u + tl_u + 2) >> 2, (3*l_v + tl_v + 2) >> 2,
               &r, &g, &b2);
         bot_dst[len-1] = 0xFF000000u | ((uint32_t)r<<16) | ((uint32_t)g<<8) | b2;
      }
   }
}

/* Coefficient tables */
static const uint8_t vp8_bands[16 + 1] = {0,1,2,3,6,4,5,6,6,6,6,6,6,6,6,7, 0};
static const uint8_t vp8_zigzag[16] = {0,1,4,8,5,2,3,6,9,12,13,10,7,11,14,15};

static const int16_t vp8_dc_qlut[128] = {
   4,5,6,7,8,9,10,10,11,12,13,14,15,16,17,17,18,19,20,20,21,21,22,22,23,23,24,25,25,26,27,28,
   29,30,31,32,33,34,35,36,37,37,38,39,40,41,42,43,44,45,46,46,47,48,49,50,51,52,53,54,55,56,57,58,
   59,60,61,62,63,64,65,66,67,68,69,70,71,72,73,74,75,76,76,77,78,79,80,81,82,83,84,85,86,87,88,89,
   91,93,95,96,98,100,101,102,104,106,108,110,112,114,116,118,122,124,126,128,130,132,134,136,138,140,143,145,148,151,154,157};
static const int16_t vp8_ac_qlut[128] = {
   4,5,6,7,8,9,10,11,12,13,14,15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,
   36,37,38,39,40,41,42,43,44,45,46,47,48,49,50,51,52,53,54,55,56,57,58,60,62,64,66,68,70,72,74,76,
   78,80,82,84,86,88,90,92,94,96,98,100,102,104,106,108,110,112,114,116,119,122,125,128,131,134,137,140,143,146,149,152,
   155,158,161,164,167,170,173,177,181,185,189,193,197,201,205,209,213,217,221,225,229,234,239,245,249,254,259,264,269,274,279,284};

/* Default coefficient probabilities — simplified: using a representative subset
 * that covers the most common cases in typical WebP images.
 * Full tables are 4*8*3*11 = 1056 bytes. We embed them directly. */
static uint8_t vp8_cprob[4][8][3][11];

static void vp8_init_default_cprob(void)
{
   static const uint8_t def[4][8][3][11] = {
   {
    {{128,128,128,128,128,128,128,128,128,128,128},{128,128,128,128,128,128,128,128,128,128,128},{128,128,128,128,128,128,128,128,128,128,128}},
    {{253,136,254,255,228,219,128,128,128,128,128},{189,129,242,255,227,213,255,219,128,128,128},{106,126,227,252,214,209,255,255,128,128,128}},
    {{1,98,248,255,236,226,255,255,128,128,128},{181,133,238,254,221,234,255,154,128,128,128},{78,134,202,247,198,180,255,219,128,128,128}},
    {{1,185,249,255,243,255,128,128,128,128,128},{184,150,247,255,236,224,128,128,128,128,128},{77,110,216,255,236,230,128,128,128,128,128}},
    {{1,101,251,255,241,255,128,128,128,128,128},{170,139,241,252,236,209,255,255,128,128,128},{37,116,196,243,228,255,255,255,128,128,128}},
    {{1,204,254,255,245,255,128,128,128,128,128},{207,160,250,255,238,128,128,128,128,128,128},{102,103,231,255,211,171,128,128,128,128,128}},
    {{1,152,252,255,240,255,128,128,128,128,128},{177,135,243,255,234,225,128,128,128,128,128},{80,129,211,255,194,224,128,128,128,128,128}},
    {{1,1,255,128,128,128,128,128,128,128,128},{246,1,255,128,128,128,128,128,128,128,128},{255,128,128,128,128,128,128,128,128,128,128}},
   },
   {
    {{198,35,237,223,193,187,162,160,145,155,62},{131,45,198,221,172,176,220,157,252,221,1},{68,47,146,208,149,167,221,162,255,223,128}},
    {{1,149,241,255,221,224,255,255,128,128,128},{184,141,234,253,222,220,255,199,128,128,128},{81,99,181,242,176,190,249,202,255,255,128}},
    {{1,129,232,253,214,197,242,196,255,255,128},{99,121,210,250,201,198,255,202,128,128,128},{23,91,163,242,170,187,247,210,255,255,128}},
    {{1,200,246,255,234,255,128,128,128,128,128},{109,178,241,255,231,245,255,255,128,128,128},{44,130,201,253,205,192,255,255,128,128,128}},
    {{1,132,239,251,219,209,255,165,128,128,128},{94,136,225,251,218,190,255,255,128,128,128},{22,100,174,245,186,161,255,199,128,128,128}},
    {{1,182,249,255,232,235,128,128,128,128,128},{124,143,241,255,227,234,128,128,128,128,128},{35,77,181,251,193,211,255,205,128,128,128}},
    {{1,157,247,255,236,231,255,255,128,128,128},{121,141,235,255,225,227,255,255,128,128,128},{45,99,188,251,195,217,255,224,128,128,128}},
    {{1,1,251,255,213,255,128,128,128,128,128},{203,1,248,255,255,128,128,128,128,128,128},{137,1,177,255,224,255,128,128,128,128,128}},
   },
   {
    {{253,9,248,251,207,208,255,192,128,128,128},{175,13,224,243,193,185,249,198,255,255,128},{73,17,171,221,161,179,236,167,255,234,128}},
    {{1,95,247,253,212,183,255,255,128,128,128},{239,90,244,250,211,209,255,255,128,128,128},{155,77,195,248,188,195,255,255,128,128,128}},
    {{1,24,239,251,218,219,255,205,128,128,128},{201,51,219,255,196,186,128,128,128,128,128},{69,46,190,239,201,218,255,228,128,128,128}},
    {{1,191,251,255,255,128,128,128,128,128,128},{223,165,249,255,213,255,128,128,128,128,128},{141,124,248,255,255,128,128,128,128,128,128}},
    {{1,16,248,255,255,128,128,128,128,128,128},{190,36,230,255,236,255,128,128,128,128,128},{149,1,255,128,128,128,128,128,128,128,128}},
    {{1,226,255,128,128,128,128,128,128,128,128},{247,192,255,128,128,128,128,128,128,128,128},{240,128,255,128,128,128,128,128,128,128,128}},
    {{1,134,252,255,255,128,128,128,128,128,128},{213,62,250,255,255,128,128,128,128,128,128},{55,93,255,128,128,128,128,128,128,128,128}},
    {{128,128,128,128,128,128,128,128,128,128,128},{128,128,128,128,128,128,128,128,128,128,128},{128,128,128,128,128,128,128,128,128,128,128}},
   },
   {
    {{202,24,213,235,186,191,220,160,240,175,255},{126,38,182,232,169,184,228,174,255,187,128},{61,46,138,219,151,178,240,170,255,216,128}},
    {{1,112,230,250,199,191,247,159,255,255,128},{166,109,228,252,211,215,255,174,128,128,128},{39,77,162,232,172,180,245,178,255,255,128}},
    {{1,52,220,246,198,199,249,220,255,255,128},{124,74,191,243,183,193,250,221,255,255,128},{24,71,130,219,154,170,243,182,255,255,128}},
    {{1,182,225,249,219,240,255,224,128,128,128},{149,150,226,252,216,205,255,171,128,128,128},{28,108,170,242,183,194,254,223,255,255,128}},
    {{1,81,230,252,204,203,255,192,128,128,128},{123,102,209,247,188,196,255,233,128,128,128},{20,95,153,243,164,173,255,203,128,128,128}},
    {{1,222,248,255,216,213,128,128,128,128,128},{168,175,246,252,235,205,255,255,128,128,128},{47,116,215,255,211,212,255,255,128,128,128}},
    {{1,121,236,253,212,214,255,255,128,128,128},{141,84,213,252,201,202,255,219,128,128,128},{42,80,160,240,162,185,255,205,128,128,128}},
    {{1,1,255,128,128,128,128,128,128,128,128},{244,1,255,128,128,128,128,128,128,128,128},{238,1,255,128,128,128,128,128,128,128,128}},
   },
   };
   memcpy(vp8_cprob, def, sizeof(def));
}

/* Decode one 4x4 block of DCT coefficients (matching libvpx GetCoeffs).
 * init_ctx: initial probability context from neighbor non-zero status
 * Returns the position of the last non-zero coeff + 1 (0 if all zero). */
static int vp8_decode_block(vp8b *br, int16_t coeffs[16],
      uint8_t probs[8][3][11], int start_at, int init_ctx)
{
   static const uint8_t kCat3[] = {173,148,140};
   static const uint8_t kCat4[] = {176,155,140,135};
   static const uint8_t kCat5[] = {180,157,141,134,130};
   static const uint8_t kCat6[] = {254,254,243,230,196,177,153,140,133,130,129};
   int n = start_at;
   const uint8_t *p = probs[n][init_ctx];
   memset(coeffs, 0, 16 * sizeof(int16_t));

   /* First "CBP" bit: EOB for entire block */
   if (!vp8b_get(br, p[0]))
      return 0;

   for (;;)
   {
      int v;
      ++n;
      if (!vp8b_get(br, p[1]))
      {
         /* zero coefficient */
         p = probs[vp8_bands[n]][0];
      }
      else
      {
         /* non-zero coefficient */
         if (!vp8b_get(br, p[2]))
         {
            v = 1;
            p = probs[vp8_bands[n]][1];
         }
         else
         {
            if (!vp8b_get(br, p[3]))
            {
               if (!vp8b_get(br, p[4]))
                  v = 2;
               else
                  v = 3 + vp8b_get(br, p[5]);
            }
            else
            {
               if (!vp8b_get(br, p[6]))
               {
                  if (!vp8b_get(br, p[7]))
                     v = 5 + vp8b_get(br, 159);
                  else
                  {
                     v = 7 + 2 * vp8b_get(br, 165);
                     v += vp8b_get(br, 145);
                  }
               }
               else
               {
                  int bit1 = vp8b_get(br, p[8]);
                  int bit0 = vp8b_get(br, p[9 + bit1]);
                  int cat = 2 * bit1 + bit0, k;
                  v = 0;
                  if (cat == 0) { for(k=0;k<3;k++) v = v*2 + vp8b_get(br, kCat3[k]); v += 11; }
                  else if (cat == 1) { for(k=0;k<4;k++) v = v*2 + vp8b_get(br, kCat4[k]); v += 19; }
                  else if (cat == 2) { for(k=0;k<5;k++) v = v*2 + vp8b_get(br, kCat5[k]); v += 35; }
                  else { for(k=0;k<11;k++) v = v*2 + vp8b_get(br, kCat6[k]); v += 67; }
               }
            }
            p = probs[vp8_bands[n]][2];
         }
         /* Sign bit and store */
         coeffs[vp8_zigzag[n-1]] = (int16_t)(vp8b_get(br, 128) ? -v : v);

         if (n == 16 || !vp8b_get(br, p[0])) /* EOB */
            return n;
      }
      if (n == 16)
         return 16;
   }
}

/* VP8 4x4 inverse DCT (from RFC 6386 §14.3) */
static void vp8_idct4x4_add(const int16_t in[16], uint8_t *dst, int stride)
{
   /* Pass order matches libvpx vp8_short_idct4x4llm_c exactly:
    * columns first, then rows with final rounding. The >>16
    * truncations do not commute, so the order matters for
    * bit-exactness. */
   int i, tmp[16];
   for (i = 0; i < 4; i++)
   {
      int a = in[i] + in[8+i];
      int b = in[i] - in[8+i];
      int c = (in[4+i] * 35468 >> 16) - (in[12+i] + (in[12+i] * 20091 >> 16));
      int d = (in[4+i] + (in[4+i] * 20091 >> 16)) + (in[12+i] * 35468 >> 16);
      tmp[i]    = a + d; tmp[4+i]  = b + c;
      tmp[8+i]  = b - c; tmp[12+i] = a - d;
   }
   for (i = 0; i < 4; i++)
   {
      int a = tmp[i*4+0] + tmp[i*4+2];
      int b = tmp[i*4+0] - tmp[i*4+2];
      int c = (tmp[i*4+1] * 35468 >> 16) - (tmp[i*4+3] + (tmp[i*4+3] * 20091 >> 16));
      int d = (tmp[i*4+1] + (tmp[i*4+1] * 20091 >> 16)) + (tmp[i*4+3] * 35468 >> 16);
      dst[i*stride+0] = vp8_cl(dst[i*stride+0] + ((a+d+4) >> 3));
      dst[i*stride+1] = vp8_cl(dst[i*stride+1] + ((b+c+4) >> 3));
      dst[i*stride+2] = vp8_cl(dst[i*stride+2] + ((b-c+4) >> 3));
      dst[i*stride+3] = vp8_cl(dst[i*stride+3] + ((a-d+4) >> 3));
   }
}

/* Inverse Walsh-Hadamard Transform for Y2 DC block.
 * Output goes directly as DC coefficients to the 4x4 IDCT,
 * so NO >>3 normalization here (IDCT applies its own). */
static void vp8_iwht4x4(const int16_t in[16], int16_t out[16])
{
   int i, tmp[16];
   for (i = 0; i < 4; i++)
   {
      int a = in[i*4+0]+in[i*4+3], b = in[i*4+1]+in[i*4+2];
      int c = in[i*4+1]-in[i*4+2], d = in[i*4+0]-in[i*4+3];
      tmp[i*4+0]=a+b; tmp[i*4+1]=c+d; tmp[i*4+2]=a-b; tmp[i*4+3]=d-c;
   }
   for (i = 0; i < 4; i++)
   {
      int a = tmp[i]+tmp[12+i], b = tmp[4+i]+tmp[8+i];
      int c = tmp[4+i]-tmp[8+i], d = tmp[i]-tmp[12+i];
      out[i]=(int16_t)((a+b+3)>>3); out[4+i]=(int16_t)((c+d+3)>>3);
      out[8+i]=(int16_t)((a-b+3)>>3); out[12+i]=(int16_t)((d-c+3)>>3);
   }
}

static const uint8_t vp8_ymp[4] = {145,156,163,128};
static const uint8_t vp8_uvmp[3] = {142,114,183};

/* Key-frame B_PRED sub-block mode probabilities (RFC 6386 §12.1)
 * Indexed by [above_bmode][left_bmode][tree_node 0..8] */
static const uint8_t kf_bmode_prob[10][10][9] = {
 {{231,120,48,89,115,113,120,152,112},{152,179,64,126,170,118,46,70,95},{175,69,143,80,85,82,72,155,103},{56,58,10,171,218,189,17,13,152},{144,71,10,38,171,213,144,34,26},{114,26,17,163,44,195,21,10,173},{121,24,80,195,26,62,44,64,85},{170,46,55,19,136,160,33,206,71},{63,20,8,114,114,208,12,9,226},{81,40,11,96,182,84,29,16,36}},
 {{134,183,89,137,98,101,106,165,148},{72,187,100,130,157,111,32,75,80},{66,102,167,99,74,62,40,234,128},{41,53,9,178,241,141,26,8,107},{104,79,12,27,217,255,87,17,7},{74,43,26,146,73,166,49,23,157},{65,38,105,160,51,52,31,115,128},{87,68,71,44,114,51,15,186,23},{47,41,14,110,182,183,21,17,194},{66,45,25,102,197,189,23,18,22}},
 {{88,88,147,150,42,46,45,196,205},{43,97,183,117,85,38,35,179,61},{39,53,200,87,26,21,43,232,171},{56,34,51,104,114,102,29,93,77},{107,54,32,26,51,1,81,43,31},{39,28,85,171,58,165,90,98,64},{34,22,116,206,23,34,43,166,73},{68,25,106,22,64,171,36,225,114},{34,19,21,102,132,188,16,76,124},{62,18,78,95,85,57,50,48,51}},
 {{193,101,35,159,215,111,89,46,111},{60,148,31,172,219,228,21,18,111},{112,113,77,85,179,255,38,120,114},{40,42,1,196,245,209,10,25,109},{100,80,8,43,154,1,51,26,71},{88,43,29,140,166,213,37,43,154},{61,63,30,155,67,45,68,1,209},{142,78,78,16,255,128,34,197,171},{41,40,5,102,211,183,4,1,221},{51,50,17,168,209,192,23,25,82}},
 {{125,98,42,88,104,85,117,175,82},{95,84,53,89,128,100,113,101,45},{75,79,123,47,51,128,81,171,1},{57,17,5,71,102,57,53,41,49},{115,21,2,10,102,255,166,23,6},{38,33,13,121,57,73,26,1,85},{41,10,67,138,77,110,90,47,114},{101,29,16,10,85,128,101,196,26},{57,18,10,102,102,213,34,20,43},{117,20,15,36,163,128,68,1,26}},
 {{138,31,36,171,27,166,38,44,229},{67,87,58,169,82,115,26,59,179},{63,59,90,180,59,166,93,73,154},{40,40,21,116,143,209,34,39,175},{57,46,22,24,128,1,54,17,37},{47,15,16,183,34,223,49,45,183},{46,17,33,183,6,98,15,32,183},{65,32,73,115,28,128,23,128,205},{40,3,9,115,51,192,18,6,223},{87,37,9,115,59,77,64,21,47}},
 {{104,55,44,218,9,54,53,130,226},{64,90,70,205,40,41,23,26,57},{54,57,112,184,5,41,38,166,213},{30,34,26,133,152,116,10,32,134},{75,32,12,51,192,255,160,43,51},{39,19,53,221,26,114,32,73,255},{31,9,65,234,2,15,1,118,73},{88,31,35,67,102,85,55,186,85},{56,21,23,111,59,205,45,37,192},{55,38,70,124,73,102,1,34,98}},
 {{102,61,71,37,34,53,31,243,192},{69,60,71,38,73,119,28,222,37},{68,45,128,34,1,47,11,245,171},{62,17,19,70,146,85,55,62,70},{75,15,9,9,64,255,184,119,16},{37,43,37,154,100,163,85,160,1},{63,9,92,136,28,64,32,201,85},{86,6,28,5,64,255,25,248,1},{56,8,17,132,137,255,55,116,128},{58,15,20,82,135,57,26,121,40}},
 {{164,50,31,137,154,133,25,35,218},{51,103,44,131,131,123,31,6,158},{86,40,64,135,148,224,45,183,128},{22,26,17,131,240,154,14,1,209},{83,12,13,54,192,255,68,47,28},{45,16,21,91,64,222,7,1,197},{56,21,39,155,60,138,23,102,213},{85,26,85,85,128,128,32,146,171},{18,11,7,63,144,171,4,4,246},{35,27,10,146,174,171,12,26,128}},
 {{190,80,35,99,180,80,126,54,45},{85,126,47,87,176,51,41,20,32},{101,75,128,139,118,146,116,128,85},{56,41,15,176,236,85,37,9,62},{146,36,19,30,171,255,97,27,20},{71,30,17,119,118,255,17,18,138},{101,38,60,138,55,70,43,26,142},{138,45,61,62,219,1,81,188,64},{32,41,20,117,151,142,20,21,163},{112,19,12,61,195,128,48,4,24}}
};

/* Decode a B_PRED sub-block mode from the key-frame tree (RFC 6386 §12.1) */
static int vp8_read_bmode(vp8b *br, int above, int left)
{
   const uint8_t *p = kf_bmode_prob[above][left];
   if (!vp8b_get(br, p[0])) return 0; /* B_DC_PRED */
   if (!vp8b_get(br, p[1])) return 1; /* B_TM_PRED */
   if (!vp8b_get(br, p[2])) return 2; /* B_VE_PRED */
   if (!vp8b_get(br, p[3])) {
      if (!vp8b_get(br, p[4])) return 3; /* B_HE_PRED */
      if (!vp8b_get(br, p[5])) return 5; /* B_RD_PRED */
      return 6; /* B_VR_PRED */
   } else {
      if (!vp8b_get(br, p[6])) return 4; /* B_LD_PRED */
      if (!vp8b_get(br, p[7])) return 7; /* B_VL_PRED */
      if (!vp8b_get(br, p[8])) return 8; /* B_HD_PRED */
      return 9; /* B_HU_PRED */
   }
}

/* 4x4 sub-block intra prediction for B_PRED.
 * dst: output 4x4 block, stride s.
 * a[0..7]: 8 above pixels (a[0..3]=directly above, a[4..7]=above-right)
 * l[0..3]: left pixels, tl: top-left pixel */
static void vp8_pred4x4(uint8_t *d, int s, int m,
      const uint8_t *a, const uint8_t *l, uint8_t tl)
{
   int i, j;
   switch (m)
   {
   case 0: /* B_DC_PRED */
   {  int sum=0;
      for(i=0;i<4;i++) sum+=a[i]+l[i];
      { uint8_t dc=(uint8_t)((sum+4)>>3);
        for(j=0;j<4;j++) memset(d+j*s,dc,4); }
      break;
   }
   case 1: /* B_TM_PRED */
      for(j=0;j<4;j++) for(i=0;i<4;i++)
         d[j*s+i]=vp8_cl((int)a[i]+(int)l[j]-(int)tl);
      break;
   case 2: /* B_VE_PRED (vertical/above with smoothing) */
      for(i=0;i<4;i++) {
         int v = (i==0) ? (tl+2*a[0]+a[1]+2)>>2 : (a[i-1]+2*a[i]+a[i+1]+2)>>2;
         for(j=0;j<4;j++) d[j*s+i]=(uint8_t)v;
      }
      break;
   case 3: /* B_HE_PRED (horizontal/left with smoothing) */
      for(j=0;j<4;j++) {
         int v = (j==0) ? (tl+2*l[0]+l[1]+2)>>2 : (j==3) ? (l[2]+3*l[3]+2)>>2 : (l[j-1]+2*l[j]+l[j+1]+2)>>2;
         memset(d+j*s,(uint8_t)v,4);
      }
      break;
   case 6: /* B_VR_PRED */
      d[3*s+0]=(uint8_t)((l[2]+2*l[1]+l[0]+2)>>2);
      d[2*s+0]=(uint8_t)((l[1]+2*l[0]+tl+2)>>2);
      d[1*s+0]=d[3*s+1]=(uint8_t)((l[0]+2*tl+a[0]+2)>>2);
      d[0*s+0]=d[2*s+1]=(uint8_t)((tl+a[0]+1)>>1);
      d[0*s+1]=d[2*s+2]=(uint8_t)((a[0]+a[1]+1)>>1);
      d[1*s+1]=d[3*s+2]=(uint8_t)((tl+2*a[0]+a[1]+2)>>2);
      d[0*s+2]=d[2*s+3]=(uint8_t)((a[1]+a[2]+1)>>1);
      d[1*s+2]=d[3*s+3]=(uint8_t)((a[0]+2*a[1]+a[2]+2)>>2);
      d[0*s+3]=(uint8_t)((a[2]+a[3]+1)>>1);
      d[1*s+3]=(uint8_t)((a[1]+2*a[2]+a[3]+2)>>2);
      break;
   case 4: /* B_LD_PRED */
      d[0*s+0]=(uint8_t)((a[0]+2*a[1]+a[2]+2)>>2);
      d[0*s+1]=d[1*s+0]=(uint8_t)((a[1]+2*a[2]+a[3]+2)>>2);
      d[0*s+2]=d[1*s+1]=d[2*s+0]=(uint8_t)((a[2]+2*a[3]+a[4]+2)>>2);
      d[0*s+3]=d[1*s+2]=d[2*s+1]=d[3*s+0]=(uint8_t)((a[3]+2*a[4]+a[5]+2)>>2);
      d[1*s+3]=d[2*s+2]=d[3*s+1]=(uint8_t)((a[4]+2*a[5]+a[6]+2)>>2);
      d[2*s+3]=d[3*s+2]=(uint8_t)((a[5]+2*a[6]+a[7]+2)>>2);
      d[3*s+3]=(uint8_t)((a[6]+2*a[7]+a[7]+2)>>2);
      break;
   case 5: /* B_RD_PRED */
      d[3*s+0]=(uint8_t)((l[3]+2*l[2]+l[1]+2)>>2);
      d[2*s+0]=d[3*s+1]=(uint8_t)((l[2]+2*l[1]+l[0]+2)>>2);
      d[1*s+0]=d[2*s+1]=d[3*s+2]=(uint8_t)((l[1]+2*l[0]+tl+2)>>2);
      d[0*s+0]=d[1*s+1]=d[2*s+2]=d[3*s+3]=(uint8_t)((l[0]+2*tl+a[0]+2)>>2);
      d[0*s+1]=d[1*s+2]=d[2*s+3]=(uint8_t)((tl+2*a[0]+a[1]+2)>>2);
      d[0*s+2]=d[1*s+3]=(uint8_t)((a[0]+2*a[1]+a[2]+2)>>2);
      d[0*s+3]=(uint8_t)((a[1]+2*a[2]+a[3]+2)>>2);
      break;
   case 7: /* B_VL_PRED */
      d[0*s+0]=(uint8_t)((a[0]+a[1]+1)>>1); d[1*s+0]=(uint8_t)((a[0]+2*a[1]+a[2]+2)>>2);
      d[0*s+1]=d[2*s+0]=(uint8_t)((a[1]+a[2]+1)>>1); d[1*s+1]=d[3*s+0]=(uint8_t)((a[1]+2*a[2]+a[3]+2)>>2);
      d[0*s+2]=d[2*s+1]=(uint8_t)((a[2]+a[3]+1)>>1); d[1*s+2]=d[3*s+1]=(uint8_t)((a[2]+2*a[3]+a[4]+2)>>2);
      d[0*s+3]=d[2*s+2]=(uint8_t)((a[3]+a[4]+1)>>1); d[1*s+3]=d[3*s+2]=(uint8_t)((a[3]+2*a[4]+a[5]+2)>>2);
      d[2*s+3]=(uint8_t)((a[4]+2*a[5]+a[6]+2)>>2); d[3*s+3]=(uint8_t)((a[5]+2*a[6]+a[7]+2)>>2);
      break;
   case 8: /* B_HD_PRED */
      d[3*s+0]=(uint8_t)((l[3]+l[2]+1)>>1); d[3*s+1]=(uint8_t)((l[3]+2*l[2]+l[1]+2)>>2);
      d[2*s+0]=d[3*s+2]=(uint8_t)((l[2]+l[1]+1)>>1); d[2*s+1]=d[3*s+3]=(uint8_t)((l[2]+2*l[1]+l[0]+2)>>2);
      d[1*s+0]=d[2*s+2]=(uint8_t)((l[1]+l[0]+1)>>1); d[1*s+1]=d[2*s+3]=(uint8_t)((l[1]+2*l[0]+tl+2)>>2);
      d[0*s+0]=d[1*s+2]=(uint8_t)((l[0]+tl+1)>>1); d[0*s+1]=d[1*s+3]=(uint8_t)((l[0]+2*tl+a[0]+2)>>2);
      d[0*s+2]=(uint8_t)((tl+2*a[0]+a[1]+2)>>2); d[0*s+3]=(uint8_t)((a[0]+2*a[1]+a[2]+2)>>2);
      break;
   case 9: /* B_HU_PRED */
      d[0*s+0]=(uint8_t)((l[0]+l[1]+1)>>1); d[0*s+1]=(uint8_t)((l[0]+2*l[1]+l[2]+2)>>2);
      d[0*s+2]=d[1*s+0]=(uint8_t)((l[1]+l[2]+1)>>1); d[0*s+3]=d[1*s+1]=(uint8_t)((l[1]+2*l[2]+l[3]+2)>>2);
      d[1*s+2]=d[2*s+0]=(uint8_t)((l[2]+l[3]+1)>>1); d[1*s+3]=d[2*s+1]=(uint8_t)((l[2]+2*l[3]+l[3]+2)>>2);
      d[2*s+2]=d[2*s+3]=d[3*s+0]=d[3*s+1]=d[3*s+2]=d[3*s+3]=(uint8_t)l[3];
      break;
   default:
   {  int sum=0; for(i=0;i<4;i++) sum+=a[i]+l[i];
      { uint8_t dc=(uint8_t)((sum+4)>>3); for(j=0;j<4;j++) memset(d+j*s,dc,4); } break; }
   }
}


/* VP8 Simple Loop Filter (RFC 6386 §15.2) */
static INLINE int vp8_sc(int v) { return v < -128 ? -128 : v > 127 ? 127 : v; }

static void vp8_simple_lf_edge(uint8_t *p1p, uint8_t *p0p, uint8_t *q0p, uint8_t *q1p, int lim)
{
   int p1 = *p1p, p0 = *p0p, q0 = *q0p, q1 = *q1p;
   int d0 = p0 - q0, d1 = p1 - q1;
   int mask = ((d0 < 0 ? -d0 : d0) * 2 + (d1 < 0 ? -d1 : d1) / 2) <= lim;
   if (mask)
   {
      int fv = vp8_sc(vp8_sc(d1) + 3 * (q0 - p0));
      int f1 = vp8_sc(fv + 4) >> 3;
      int f2 = vp8_sc(fv + 3) >> 3;
      *q0p = vp8_cl(q0 - f1);
      *p0p = vp8_cl(p0 + f2);
   }
}

static void vp8_loop_filter_simple(uint8_t *y, int ys,
   int mbw, int mbh, int lf_level, int sharpness,
   int seg_enabled, int seg_abs, const int *seg_lf,
   const uint8_t *seg_map, const uint8_t *skip_lf_map)
{
   int mx, my, i, e;
   for (my = 0; my < mbh; my++)
   {
      for (mx = 0; mx < mbw; mx++)
      {
         int mb_lf = lf_level;
         int lim, blim, mblim, skip_lf;
         uint8_t *my0 = y + my * 16 * ys + mx * 16;
         if (seg_enabled && seg_abs)
            mb_lf = seg_lf[seg_map ? seg_map[my * mbw + mx] : 0];
         else if (seg_enabled)
            mb_lf = lf_level + seg_lf[seg_map ? seg_map[my * mbw + mx] : 0];
         if (mb_lf < 0) mb_lf = 0;
         if (mb_lf > 63) mb_lf = 63;
         if (mb_lf == 0) continue;
         /* Limits per libvpx vp8_loop_filter_update_sharpness */
         lim = mb_lf >> ((sharpness > 0) + (sharpness > 4));
         if (sharpness > 0 && lim > 9 - sharpness) lim = 9 - sharpness;
         if (lim < 1) lim = 1;
         mblim = 2 * (mb_lf + 2) + lim;
         blim  = 2 * mb_lf + lim;
         skip_lf = skip_lf_map ? skip_lf_map[my * mbw + mx] : 0;
         /* Edge order per libvpx: mbv, bv, mbh, bh */
         if (mx > 0)
            for (i = 0; i < 16; i++) {
               uint8_t *r = my0 + i * ys;
               vp8_simple_lf_edge(r - 2, r - 1, r, r + 1, mblim);
            }
         if (!skip_lf)
            for (e = 4; e <= 12; e += 4)
               for (i = 0; i < 16; i++) {
                  uint8_t *r = my0 + i * ys + e;
                  vp8_simple_lf_edge(r - 2, r - 1, r, r + 1, blim);
               }
         if (my > 0)
            for (i = 0; i < 16; i++) {
               uint8_t *c = my0 + i;
               vp8_simple_lf_edge(c - 2 * ys, c - ys, c, c + ys, mblim);
            }
         if (!skip_lf)
            for (e = 4; e <= 12; e += 4)
               for (i = 0; i < 16; i++) {
                  uint8_t *c = my0 + e * ys + i;
                  vp8_simple_lf_edge(c - 2 * ys, c - ys, c, c + ys, blim);
               }
      }
   }
}

/* VP8 Normal Loop Filter (RFC 6386 section 15.3), ported from libvpx
 * loopfilter_filters.c. Kernels operate in the signed (^0x80) domain. */

static INLINE int vp8_nlf_mask(int lim, int blim,
      int p3, int p2, int p1, int p0, int q0, int q1, int q2, int q3)
{
   int m = 0;
   m |= (px_abs(p3 - p2) > lim);
   m |= (px_abs(p2 - p1) > lim);
   m |= (px_abs(p1 - p0) > lim);
   m |= (px_abs(q1 - q0) > lim);
   m |= (px_abs(q2 - q1) > lim);
   m |= (px_abs(q3 - q2) > lim);
   m |= (px_abs(p0 - q0) * 2 + px_abs(p1 - q1) / 2 > blim);
   return m - 1; /* 0 -> -1 (all ones), 1 -> 0 */
}

static INLINE int vp8_nlf_hev(int thr, int p1, int p0, int q0, int q1)
{
   int h = 0;
   h |= (px_abs(p1 - p0) > thr) * -1;
   h |= (px_abs(q1 - q0) > thr) * -1;
   return h;
}

/* Inner (sub-block) 4-tap filter: adjusts p1,p0,q0,q1. */
static void vp8_nlf_inner(int mask, int hev,
      uint8_t *op1, uint8_t *op0, uint8_t *oq0, uint8_t *oq1)
{
   int ps1 = (int)*op1 - 128, ps0 = (int)*op0 - 128;
   int qs0 = (int)*oq0 - 128, qs1 = (int)*oq1 - 128;
   int fv, f1, f2;

   fv = vp8_sc(ps1 - qs1);
   fv &= hev;
   fv = vp8_sc(fv + 3 * (qs0 - ps0));
   fv &= mask;

   f1 = vp8_sc(fv + 4) >> 3;
   f2 = vp8_sc(fv + 3) >> 3;
   *oq0 = (uint8_t)(vp8_sc(qs0 - f1) + 128);
   *op0 = (uint8_t)(vp8_sc(ps0 + f2) + 128);

   fv = (f1 + 1) >> 1;
   fv &= ~hev;
   *oq1 = (uint8_t)(vp8_sc(qs1 - fv) + 128);
   *op1 = (uint8_t)(vp8_sc(ps1 + fv) + 128);
}

/* Macroblock-edge 6-tap filter: adjusts p2..q2. */
static void vp8_nlf_mb(int mask, int hev,
      uint8_t *op2, uint8_t *op1, uint8_t *op0,
      uint8_t *oq0, uint8_t *oq1, uint8_t *oq2)
{
   int ps2 = (int)*op2 - 128, ps1 = (int)*op1 - 128, ps0 = (int)*op0 - 128;
   int qs0 = (int)*oq0 - 128, qs1 = (int)*oq1 - 128, qs2 = (int)*oq2 - 128;
   int fv, f1, f2, u;

   fv = vp8_sc(ps1 - qs1);
   fv = vp8_sc(fv + 3 * (qs0 - ps0));
   fv &= mask;

   f2 = fv & hev;
   f1 = vp8_sc(f2 + 4) >> 3;
   f2 = vp8_sc(f2 + 3) >> 3;
   qs0 = vp8_sc(qs0 - f1);
   ps0 = vp8_sc(ps0 + f2);

   fv &= ~hev;

   u = vp8_sc((63 + fv * 27) >> 7);
   *oq0 = (uint8_t)(vp8_sc(qs0 - u) + 128);
   *op0 = (uint8_t)(vp8_sc(ps0 + u) + 128);

   u = vp8_sc((63 + fv * 18) >> 7);
   *oq1 = (uint8_t)(vp8_sc(qs1 - u) + 128);
   *op1 = (uint8_t)(vp8_sc(ps1 + u) + 128);

   u = vp8_sc((63 + fv * 9) >> 7);
   *oq2 = (uint8_t)(vp8_sc(qs2 - u) + 128);
   *op2 = (uint8_t)(vp8_sc(ps2 + u) + 128);
}

/* Walk one edge: n filtered positions, taps tp apart, positions sp apart. */
static void vp8_nlf_edge(uint8_t *s, int tp, int sp, int n,
      int edge_lim, int lim, int thr, int is_mb)
{
   int i;
   for (i = 0; i < n; i++)
   {
      int p3 = s[-4*tp], p2 = s[-3*tp], p1 = s[-2*tp], p0 = s[-1*tp];
      int q0 = s[0],     q1 = s[1*tp],  q2 = s[2*tp],  q3 = s[3*tp];
      int mask = vp8_nlf_mask(lim, edge_lim, p3, p2, p1, p0, q0, q1, q2, q3);
      int hev  = vp8_nlf_hev(thr, p1, p0, q0, q1);
      if (is_mb)
         vp8_nlf_mb(mask, hev, s-3*tp, s-2*tp, s-1*tp, s, s+1*tp, s+2*tp);
      else
         vp8_nlf_inner(mask, hev, s-2*tp, s-1*tp, s, s+1*tp);
      s += sp;
   }
}

static void vp8_loop_filter_normal(uint8_t *y, int ys,
   uint8_t *u, uint8_t *v_plane, int uvs,
   int mbw, int mbh, int lf_level, int sharpness,
   int seg_enabled, int seg_abs, const int *seg_lf,
   int lf_delta_enabled, const int *ref_lf_delta, const int *mode_lf_delta,
   const uint8_t *seg_map, const uint8_t *skip_lf_map, const uint8_t *bpred_map)
{
   int mx, my, e;
   for (my = 0; my < mbh; my++)
   {
      for (mx = 0; mx < mbw; mx++)
      {
         int n = my * mbw + mx;
         int lvl = lf_level;
         int lim, blim, mblim, thr, skip_lf, is_bpred;
         uint8_t *my0 = y + my * 16 * ys + mx * 16;
         uint8_t *mu0 = u + my * 8 * uvs + mx * 8;
         uint8_t *mv0 = v_plane + my * 8 * uvs + mx * 8;

         if (seg_enabled && seg_abs)
            lvl = seg_lf[seg_map ? seg_map[n] : 0];
         else if (seg_enabled)
            lvl = lf_level + seg_lf[seg_map ? seg_map[n] : 0];
         if (lvl < 0) lvl = 0;
         if (lvl > 63) lvl = 63;
         is_bpred = bpred_map ? bpred_map[n] : 0;
         /* Keyframe delta adjustment (libvpx vp8_loop_filter_frame_init):
          * INTRA ref delta applies to all MBs; mode delta 0 to B_PRED. */
         if (lf_delta_enabled)
         {
            lvl += ref_lf_delta[0];
            if (is_bpred) lvl += mode_lf_delta[0];
            if (lvl < 0) lvl = 0;
            if (lvl > 63) lvl = 63;
         }
         if (lvl == 0) continue;

         /* Limits per vp8_loop_filter_update_sharpness */
         lim = lvl >> ((sharpness > 0) + (sharpness > 4));
         if (sharpness > 0 && lim > 9 - sharpness) lim = 9 - sharpness;
         if (lim < 1) lim = 1;
         mblim = 2 * (lvl + 2) + lim;
         blim  = 2 * lvl + lim;
         /* Keyframe high-edge-variance threshold */
         thr = (lvl >= 40) ? 2 : (lvl >= 15) ? 1 : 0;

         skip_lf = skip_lf_map ? skip_lf_map[n] : 0;

         /* Edge order per libvpx: mbv, bv, mbh, bh; chroma included. */
         if (mx > 0)
         {
            vp8_nlf_edge(my0, 1, ys, 16, mblim, lim, thr, 1);
            vp8_nlf_edge(mu0, 1, uvs, 8, mblim, lim, thr, 1);
            vp8_nlf_edge(mv0, 1, uvs, 8, mblim, lim, thr, 1);
         }
         if (!skip_lf)
         {
            for (e = 4; e <= 12; e += 4)
               vp8_nlf_edge(my0 + e, 1, ys, 16, blim, lim, thr, 0);
            vp8_nlf_edge(mu0 + 4, 1, uvs, 8, blim, lim, thr, 0);
            vp8_nlf_edge(mv0 + 4, 1, uvs, 8, blim, lim, thr, 0);
         }
         if (my > 0)
         {
            vp8_nlf_edge(my0, ys, 1, 16, mblim, lim, thr, 1);
            vp8_nlf_edge(mu0, uvs, 1, 8, mblim, lim, thr, 1);
            vp8_nlf_edge(mv0, uvs, 1, 8, mblim, lim, thr, 1);
         }
         if (!skip_lf)
         {
            for (e = 4; e <= 12; e += 4)
               vp8_nlf_edge(my0 + e * ys, ys, 1, 16, blim, lim, thr, 0);
            vp8_nlf_edge(mu0 + 4 * uvs, uvs, 1, 8, blim, lim, thr, 0);
            vp8_nlf_edge(mv0 + 4 * uvs, uvs, 1, 8, blim, lim, thr, 0);
         }
      }
   }
}

static void vp8_pred16(uint8_t *d, int s, int m, const uint8_t *a, const uint8_t *l, uint8_t tl,
      int up_avail, int left_avail)
{
   int i, j;
   switch (m) {
   case 0: { /* DC with libvpx availability logic */
             int dc = 128;
             if (up_avail || left_avail) {
                int sum = 0, shift = 3 + up_avail + left_avail;
                if (up_avail)   for(i=0;i<16;i++) sum += a[i];
                if (left_avail) for(i=0;i<16;i++) sum += l[i];
                dc = (sum + (1 << (shift - 1))) >> shift;
             }
             for(j=0;j<16;j++) memset(d+j*s,(uint8_t)dc,16);
             break; }
   case 1: for(j=0;j<16;j++) memcpy(d+j*s,a,16); break;
   case 2: for(j=0;j<16;j++) memset(d+j*s,l[j],16); break;
   case 3: for(j=0;j<16;j++) for(i=0;i<16;i++) d[j*s+i]=vp8_cl((int)a[i]+(int)l[j]-(int)tl); break;
   }
}

static void vp8_pred8(uint8_t *d, int s, int m, const uint8_t *a, const uint8_t *l, uint8_t tl,
      int up_avail, int left_avail)
{
   int i, j;
   switch (m) {
   case 0: { /* DC with libvpx availability logic */
             int dc = 128;
             if (up_avail || left_avail) {
                int sum = 0, shift = 2 + up_avail + left_avail;
                if (up_avail)   for(i=0;i<8;i++) sum += a[i];
                if (left_avail) for(i=0;i<8;i++) sum += l[i];
                dc = (sum + (1 << (shift - 1))) >> shift;
             }
             for(j=0;j<8;j++) memset(d+j*s,(uint8_t)dc,8);
             break; }
   case 1: for(j=0;j<8;j++) memcpy(d+j*s,a,8); break;
   case 2: for(j=0;j<8;j++) memset(d+j*s,l[j],8); break;
   case 3: for(j=0;j<8;j++) for(i=0;i<8;i++) d[j*s+i]=vp8_cl((int)a[i]+(int)l[j]-(int)tl); break;
   }
}

static uint32_t *vp8_decode(const uint8_t *data, size_t len,
      unsigned *ow, unsigned *oh)
{
   uint32_t ft;
   int kf, w, h, mbw, mbh, ys, uvs, mx, my, i, j;
   uint32_t p0s;
   int base_qp, y1dc_dq, y2dc_dq, y2ac_dq, uvdc_dq, uvac_dq;
   int qp, y1_dc_q, y1_ac_q, y2_dc_q, y2_ac_q, uv_dc_q, uv_ac_q;
   int skip_enabled, prob_skip, log2parts, num_parts;
   int filter_type, lf_level, sharpness;
   int lf_delta_enabled = 0;
   int ref_lf_delta[4] = {0,0,0,0}, mode_lf_delta[4] = {0,0,0,0};
   int seg_enabled, seg_abs, seg_qp[4], seg_lf[4], seg_prob[3];
   vp8b br;
   vp8b tbr[8]; /* up to 8 token partitions */
   uint8_t *seg_map_buf = NULL;
   uint8_t *skip_lf_buf = NULL;
   uint8_t *bpred_buf = NULL;
   const uint8_t *p0;
   uint8_t *yb = NULL, *ub = NULL, *vb = NULL;
   uint32_t *pix = NULL;

   if (len < 10) return NULL;
   ft = (uint32_t)data[0] | ((uint32_t)data[1]<<8) | ((uint32_t)data[2]<<16);
   kf = !(ft & 1);
   p0s = (ft >> 5) & 0x7FFFF;
   if (!kf) return NULL;
   if (data[3]!=0x9D || data[4]!=0x01 || data[5]!=0x2A) return NULL;
   w = (data[6]|(data[7]<<8)) & 0x3FFF;
   h = (data[8]|(data[9]<<8)) & 0x3FFF;
   if (!w || !h || w > 16384 || h > 16384) return NULL;
   mbw = (w+15) >> 4; mbh = (h+15) >> 4;
   p0 = data + 10;
   if ((size_t)(p0 - data) + p0s > len) return NULL;
   vp8b_init(&br, p0, (size_t)(data + len - p0));

   vp8b_bit(&br); vp8b_bit(&br); /* color_space, clamping */

   /* Segmentation */
   seg_enabled = vp8b_bit(&br);
   seg_abs = 0;
   seg_qp[0] = seg_qp[1] = seg_qp[2] = seg_qp[3] = 0;
   seg_prob[0] = seg_prob[1] = seg_prob[2] = 255;
   seg_lf[0] = seg_lf[1] = seg_lf[2] = seg_lf[3] = 0;
   if (seg_enabled)
   {
      int um = vp8b_bit(&br), ud = vp8b_bit(&br);
      if (ud) {
         seg_abs = vp8b_bit(&br);
         for (i=0;i<4;i++) seg_qp[i] = vp8b_bit(&br) ? vp8b_sig(&br,7) : 0;
         for (i=0;i<4;i++) seg_lf[i] = vp8b_bit(&br) ? vp8b_sig(&br,6) : 0;
      }
      if (um) for (i=0;i<3;i++) { if (vp8b_bit(&br)) seg_prob[i] = (int)vp8b_lit(&br,8); }
   }

   filter_type = vp8b_bit(&br); lf_level = (int)vp8b_lit(&br,6); sharpness = (int)vp8b_lit(&br,3);
   lf_delta_enabled = vp8b_bit(&br);
   if (lf_delta_enabled && vp8b_bit(&br))
   {
      for(i=0;i<4;i++) if(vp8b_bit(&br)) ref_lf_delta[i] = vp8b_sig(&br,6);
      for(i=0;i<4;i++) if(vp8b_bit(&br)) mode_lf_delta[i] = vp8b_sig(&br,6);
   }
   log2parts = vp8b_lit(&br,2);
   num_parts = 1 << log2parts;

   /* Quantizer */
   base_qp = vp8b_lit(&br,7);
   y1dc_dq = vp8b_bit(&br) ? vp8b_sig(&br,4) : 0;
   y2dc_dq = vp8b_bit(&br) ? vp8b_sig(&br,4) : 0;
   y2ac_dq = vp8b_bit(&br) ? vp8b_sig(&br,4) : 0;
   uvdc_dq = vp8b_bit(&br) ? vp8b_sig(&br,4) : 0;
   uvac_dq = vp8b_bit(&br) ? vp8b_sig(&br,4) : 0;

   /* refresh_entropy_probs (RFC 6386) */
   (void)vp8b_bit(&br);

   /* We'll compute per-MB quantizer in the loop using segment info */
   qp = base_qp < 0 ? 0 : (base_qp > 127 ? 127 : base_qp);
   { int q2 = qp + y1dc_dq; y1_dc_q = vp8_dc_qlut[q2<0?0:q2>127?127:q2]; }
   y1_ac_q = vp8_ac_qlut[qp];
   { int q2 = qp + y2dc_dq; y2_dc_q = vp8_dc_qlut[q2<0?0:q2>127?127:q2] * 2; }
   { int q2 = qp + y2ac_dq; y2_ac_q = vp8_ac_qlut[q2<0?0:q2>127?127:q2] * 155 / 100;
     if (y2_ac_q < 8) y2_ac_q = 8; }
   { int q2 = qp + uvdc_dq; uv_dc_q = vp8_dc_qlut[q2<0?0:q2>127?127:q2]; if(uv_dc_q>132)uv_dc_q=132; }
   { int q2 = qp + uvac_dq; uv_ac_q = vp8_ac_qlut[q2<0?0:q2>127?127:q2]; }

   /* Initialize coefficient probabilities */
   vp8_init_default_cprob();

   /* Read coefficient probability updates using the fixed update probabilities
    * defined in RFC 6386 §13.4 (Table 2). Each prob may be updated if a flag
    * is read with the corresponding update probability. */
   {
      static const uint8_t cup[4][8][3][11] = {
      {{{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{176,246,255,255,255,255,255,255,255,255,255},{223,241,252,255,255,255,255,255,255,255,255},{249,253,253,255,255,255,255,255,255,255,255}},
       {{255,244,252,255,255,255,255,255,255,255,255},{234,254,254,255,255,255,255,255,255,255,255},{253,255,255,255,255,255,255,255,255,255,255}},
       {{255,246,254,255,255,255,255,255,255,255,255},{239,253,254,255,255,255,255,255,255,255,255},{254,255,254,255,255,255,255,255,255,255,255}},
       {{255,248,254,255,255,255,255,255,255,255,255},{251,255,254,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,253,254,255,255,255,255,255,255,255,255},{251,254,254,255,255,255,255,255,255,255,255},{254,255,254,255,255,255,255,255,255,255,255}},
       {{255,254,253,255,254,255,255,255,255,255,255},{250,255,254,255,254,255,255,255,255,255,255},{254,255,255,255,255,255,255,255,255,255,255}},
       {{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}}},
      {{{217,255,255,255,255,255,255,255,255,255,255},{225,252,241,253,255,255,254,255,255,255,255},{234,250,241,250,253,255,253,254,255,255,255}},
       {{255,254,255,255,255,255,255,255,255,255,255},{223,254,254,255,255,255,255,255,255,255,255},{238,253,254,254,255,255,255,255,255,255,255}},
       {{255,248,254,255,255,255,255,255,255,255,255},{249,254,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,253,255,255,255,255,255,255,255,255,255},{247,254,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,253,254,255,255,255,255,255,255,255,255},{252,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,254,254,255,255,255,255,255,255,255,255},{253,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,254,253,255,255,255,255,255,255,255,255},{250,255,255,255,255,255,255,255,255,255,255},{254,255,255,255,255,255,255,255,255,255,255}},
       {{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}}},
      {{{186,251,250,255,255,255,255,255,255,255,255},{234,251,244,254,255,255,255,255,255,255,255},{251,251,243,253,254,255,254,255,255,255,255}},
       {{255,253,254,255,255,255,255,255,255,255,255},{236,253,254,255,255,255,255,255,255,255,255},{251,253,253,254,254,255,255,255,255,255,255}},
       {{255,254,254,255,255,255,255,255,255,255,255},{254,254,254,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,254,255,255,255,255,255,255,255,255,255},{254,254,255,255,255,255,255,255,255,255,255},{254,255,255,255,255,255,255,255,255,255,255}},
       {{255,255,255,255,255,255,255,255,255,255,255},{254,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}}},
      {{{248,255,255,255,255,255,255,255,255,255,255},{250,254,252,254,255,255,255,255,255,255,255},{248,254,249,253,255,255,255,255,255,255,255}},
       {{255,253,253,255,255,255,255,255,255,255,255},{246,253,253,255,255,255,255,255,255,255,255},{252,254,251,254,254,255,255,255,255,255,255}},
       {{255,254,252,255,255,255,255,255,255,255,255},{248,254,253,255,255,255,255,255,255,255,255},{253,255,254,254,255,255,255,255,255,255,255}},
       {{255,251,254,255,255,255,255,255,255,255,255},{245,251,254,255,255,255,255,255,255,255,255},{253,253,254,255,255,255,255,255,255,255,255}},
       {{255,251,253,255,255,255,255,255,255,255,255},{252,253,254,255,255,255,255,255,255,255,255},{255,254,255,255,255,255,255,255,255,255,255}},
       {{255,252,255,255,255,255,255,255,255,255,255},{249,255,254,255,255,255,255,255,255,255,255},{255,255,254,255,255,255,255,255,255,255,255}},
       {{255,255,253,255,255,255,255,255,255,255,255},{250,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}},
       {{255,255,255,255,255,255,255,255,255,255,255},{254,255,255,255,255,255,255,255,255,255,255},{255,255,255,255,255,255,255,255,255,255,255}}}
      };
      int t, b, c, p;
      for (t = 0; t < 4; t++)
         for (b = 0; b < 8; b++)
            for (c = 0; c < 3; c++)
               for (p = 0; p < 11; p++)
                  if (vp8b_get(&br, cup[t][b][c][p]))
                     vp8_cprob[t][b][c][p] = (uint8_t)vp8b_lit(&br, 8);
   }

   /* Dump some probs */
   skip_enabled = vp8b_bit(&br);
   prob_skip = skip_enabled ? (int)vp8b_lit(&br, 8) : 0;

   /* Initialize token partitions */
   {
      const uint8_t *tp_base = p0 + p0s;
      const uint8_t *tp_sizes = tp_base; /* partition size bytes */
      const uint8_t *tp_data;
      size_t part_sizes[8];
      int np;

      if (num_parts > 8) num_parts = 8;
      /* Read partition sizes: (num_parts - 1) * 3 bytes, little-endian 24-bit each */
      tp_data = tp_base + 3 * (num_parts - 1);
      for (np = 0; np < num_parts - 1; np++)
      {
         part_sizes[np] = (size_t)tp_sizes[np*3]
                        | ((size_t)tp_sizes[np*3+1] << 8)
                        | ((size_t)tp_sizes[np*3+2] << 16);
      }
      /* Last partition gets the remainder */
      {
         size_t used = 0;
         for (np = 0; np < num_parts - 1; np++) used += part_sizes[np];
         part_sizes[num_parts - 1] = (data + len) - tp_data - used;
      }
      /* Initialize each partition's bool decoder */
      for (np = 0; np < num_parts; np++)
      {
         if (tp_data + part_sizes[np] > data + len)
            part_sizes[np] = (data + len) - tp_data;
         vp8b_init(&tbr[np], tp_data, part_sizes[np]);
         tp_data += part_sizes[np];
      }
   }

   ys = mbw * 16; uvs = mbw * 8;
   seg_map_buf = (uint8_t*)calloc(mbw * mbh, 1);
   skip_lf_buf = (uint8_t*)calloc(mbw * mbh, 1);
   bpred_buf = (uint8_t*)calloc(mbw * mbh, 1);
   yb = (uint8_t*)calloc(ys * mbh * 16, 1);
   ub = (uint8_t*)calloc(uvs * mbh * 8, 1);
   vb = (uint8_t*)calloc(uvs * mbh * 8, 1);
   if (!yb || !ub || !vb) goto lfail;
   memset(yb, 127, ys * mbh * 16);
   memset(ub, 127, uvs * mbh * 8);
   memset(vb, 127, uvs * mbh * 8);

   /* Non-zero coefficient context tracking (RFC 6386 §13.3).
    * above_nz_*: one entry per sub-block column across the MB row.
    * left_nz_*: one entry per sub-block row within current MB. */
   {
      uint8_t *above_nz_y  = (uint8_t*)calloc(mbw * 4, 1); /* 4 Y sub-block cols per MB */
      uint8_t *above_nz_u  = (uint8_t*)calloc(mbw * 2, 1); /* 2 U sub-block cols per MB */
      uint8_t *above_nz_v  = (uint8_t*)calloc(mbw * 2, 1);
      uint8_t *above_nz_dc = (uint8_t*)calloc(mbw, 1);     /* Y2 DC block */
      uint8_t *above_bmodes = (uint8_t*)calloc(mbw * 4, 1); /* B_PRED sub-block modes */
      uint8_t left_nz_y[4], left_nz_u[2], left_nz_v[2];
      uint8_t left_bmodes[4];
      int left_nz_dc;

      if (!above_nz_y || !above_nz_u || !above_nz_v || !above_nz_dc || !above_bmodes)
      { free(above_nz_y); free(above_nz_u); free(above_nz_v); free(above_nz_dc); free(above_bmodes); goto lfail; }

   for (my = 0; my < mbh; my++)
   {
      vp8b *tp = &tbr[my % num_parts]; /* token partition for this row */
      /* Reset left context at start of each row */
      memset(left_nz_y, 0, sizeof(left_nz_y));
      memset(left_nz_u, 0, sizeof(left_nz_u));
      memset(left_nz_v, 0, sizeof(left_nz_v));
      memset(left_bmodes, 0, sizeof(left_bmodes));
      left_nz_dc = 0;
      for (mx = 0; mx < mbw; mx++)
      {
         int ym, uvm, is_skip = 0, seg_id = 0, mb_has_coeffs = 0;
         uint8_t ay[16], ly[16], au[8], lu[8], av[8], lv[8];
         uint8_t tly=128, tlu=128, tlv=128;
         int16_t coeffs[16], y2_block[16], dc_vals[16];
         uint8_t bmodes[16]; /* B_PRED sub-block modes */
         int bx, by;
         int mb_qp;

         /* Read segment ID if segmentation is enabled */
         if (seg_enabled)
         {
            /* VP8 segment tree: prob[0] -> left(prob[1]->seg0/seg1) / right(prob[2]->seg2/seg3) */
            if (vp8b_get(&br, seg_prob[0]))
               seg_id = 2 + vp8b_get(&br, seg_prob[2]);
            else
               seg_id = vp8b_get(&br, seg_prob[1]);
         }

         if (seg_map_buf) seg_map_buf[my * mbw + mx] = (uint8_t)seg_id;
         /* Compute per-MB quantizer based on segment */
         if (seg_enabled && seg_abs)
            mb_qp = seg_qp[seg_id];
         else if (seg_enabled)
            mb_qp = base_qp + seg_qp[seg_id];
         else
            mb_qp = base_qp;
         if (mb_qp < 0) mb_qp = 0;
         if (mb_qp > 127) mb_qp = 127;
         /* Recompute quantizer tables for this MB's QP */
         { int q2 = mb_qp + y1dc_dq; y1_dc_q = vp8_dc_qlut[q2<0?0:q2>127?127:q2]; }
         y1_ac_q = vp8_ac_qlut[mb_qp];
         { int q2 = mb_qp + y2dc_dq; y2_dc_q = vp8_dc_qlut[q2<0?0:q2>127?127:q2] * 2; }
         { int q2 = mb_qp + y2ac_dq; y2_ac_q = vp8_ac_qlut[q2<0?0:q2>127?127:q2] * 155 / 100;
           if (y2_ac_q < 8) y2_ac_q = 8; }
         { int q2 = mb_qp + uvdc_dq; uv_dc_q = vp8_dc_qlut[q2<0?0:q2>127?127:q2]; if(uv_dc_q>132)uv_dc_q=132; }
         { int q2 = mb_qp + uvac_dq; uv_ac_q = vp8_ac_qlut[q2<0?0:q2>127?127:q2]; }

         /* Skip flag (after segment, before y_mode — libvpx order) */
         if (skip_enabled)
            is_skip = vp8b_get(&br, prob_skip);

         /* Y mode */
         if (!vp8b_get(&br, vp8_ymp[0])) {
            ym = 4; /* B_PRED */
         } else if (!vp8b_get(&br, vp8_ymp[1])) {
            /* Left subtree: DC, V */
            ym = vp8b_get(&br, vp8_ymp[2]) ? 1 : 0;
         } else {
            /* Right subtree: H, TM */
            ym = vp8b_get(&br, vp8_ymp[3]) ? 3 : 2;
         }

         if (ym == 4)
         {
            /* B_PRED: read 16 sub-block modes using key-frame context probs. */
            for (i = 0; i < 16; i++)
            {
               int sb_row = i / 4, sb_col = i % 4;
               int above_mode, left_mode;
               /* Above mode: from previous MB row's bottom sub-block, or default 0 (DC) */
               if (sb_row > 0)
                  above_mode = bmodes[i - 4];
               else if (my > 0)
                  above_mode = above_bmodes[mx * 4 + sb_col];
               else
                  above_mode = 0;
               /* Left mode: from left sub-block in this MB, or previous MB's right col */
               if (sb_col > 0)
                  left_mode = bmodes[i - 1];
               else if (mx > 0)
                  left_mode = left_bmodes[sb_row];
               else
                  left_mode = 0;
               bmodes[i] = (uint8_t)vp8_read_bmode(&br, above_mode, left_mode);
            }
            /* Store bottom row for next MB row's above context */
            for (i = 0; i < 4; i++)
               above_bmodes[mx * 4 + i] = bmodes[12 + i];
            /* Store right column for next MB's left context */
            for (i = 0; i < 4; i++)
               left_bmodes[i] = bmodes[i * 4 + 3];
         }
         else
         {
            /* Non B_PRED: clear bmode context (default DC=0 for neighbors) */
            /* Map 16x16 mode to equivalent bmode for context (libvpx
             * above_block_mode/left_block_mode): DC->B_DC(0), V->B_VE(2),
             * H->B_HE(3), TM->B_TM(1). */
            {
               uint8_t eq = (ym == 1) ? 2 : (ym == 2) ? 3 : (ym == 3) ? 1 : 0;
               for (i = 0; i < 4; i++) above_bmodes[mx * 4 + i] = eq;
               for (i = 0; i < 4; i++) left_bmodes[i] = eq;
            }
         }

         /* UV mode */
         if      (!vp8b_get(&br, vp8_uvmp[0])) uvm = 0;
         else if (!vp8b_get(&br, vp8_uvmp[1])) uvm = 1;
         else if (!vp8b_get(&br, vp8_uvmp[2])) uvm = 2;
         else uvm = 3;

         /* Gather prediction context. Border semantics per libvpx
          * vp8_setup_intra_recon: row above frame = 127 (including the
          * top-left corner), column left of frame = 129. */
         if (my > 0) {
            memcpy(ay, yb+(my*16-1)*ys+mx*16, 16);
            memcpy(au, ub+(my*8-1)*uvs+mx*8, 8);
            memcpy(av, vb+(my*8-1)*uvs+mx*8, 8);
            if (mx > 0) { tly=yb[(my*16-1)*ys+mx*16-1]; tlu=ub[(my*8-1)*uvs+mx*8-1]; tlv=vb[(my*8-1)*uvs+mx*8-1]; }
            else        { tly=129; tlu=129; tlv=129; }
         } else {
            memset(ay,127,16); memset(au,127,8); memset(av,127,8);
            tly=127; tlu=127; tlv=127;
         }
         if (mx > 0) {
            for(j=0;j<16;j++) ly[j]=yb[(my*16+j)*ys+mx*16-1];
            for(j=0;j<8;j++) lu[j]=ub[(my*8+j)*uvs+mx*8-1];
            for(j=0;j<8;j++) lv[j]=vb[(my*8+j)*uvs+mx*8-1];
         } else { memset(ly,129,16); memset(lu,129,8); memset(lv,129,8); }

         /* Predict */
         if (ym != 4)
            vp8_pred16(yb+my*16*ys+mx*16, ys, ym, ay, ly, tly, my > 0, mx > 0);
         /* B_PRED Y prediction is done per sub-block below */
         vp8_pred8(ub+my*8*uvs+mx*8, uvs, uvm, au, lu, tlu, my > 0, mx > 0);
         vp8_pred8(vb+my*8*uvs+mx*8, uvs, uvm, av, lv, tlv, my > 0, mx > 0);

         /* Decode and add residual */
         mb_has_coeffs = 0;
         if (!is_skip || ym == 4)
         {
            /* Non-zero coefficient tracking for context. Skipped B_PRED
             * MBs still take this path: sub-block prediction must run
             * even when all residuals are skipped (libvpx zeroes eobs
             * but still predicts). */
            int nz_y2 = 0;

            /* Y2 block (DC for 16x16 prediction) */
            memset(dc_vals, 0, sizeof(dc_vals));
            if (ym != 4) /* not B_PRED */
            {
               int y2_above = (my > 0) ? above_nz_dc[mx] : 0;
               int y2_left  = (mx > 0) ? left_nz_dc : 0;
               int y2_ctx   = (y2_above + y2_left > 1) ? 2 : (y2_above + y2_left);
               memset(y2_block, 0, sizeof(y2_block));
               nz_y2 = vp8_decode_block(tp, y2_block, vp8_cprob[1], 0, y2_ctx);
               above_nz_dc[mx] = (nz_y2 > 0) ? 1 : 0;
               left_nz_dc = (nz_y2 > 0) ? 1 : 0;
               if (nz_y2 > 0) mb_has_coeffs = 1;
               /* Dequantize Y2 */
               y2_block[0] = (int16_t)(y2_block[0] * y2_dc_q);
               for (i = 1; i < 16; i++)
                  y2_block[i] = (int16_t)(y2_block[i] * y2_ac_q);
               /* Inverse WHT to get DC values for each sub-block */
               vp8_iwht4x4(y2_block, dc_vals);
            }

            /* 16 Y sub-blocks */
            for (by = 0; by < 4; by++)
            {
               for (bx = 0; bx < 4; bx++)
               {
                  int sb_above = (my > 0 || by > 0) ? above_nz_y[mx*4+bx] : 0;
                  int sb_left  = (mx > 0 || bx > 0) ? left_nz_y[by] : 0;
                  int sb_ctx   = (sb_above + sb_left > 1) ? 2 : (sb_above + sb_left);
                  int start, nz_cnt;
                  uint8_t *sb_dst = yb + (my*16 + by*4) * ys + mx*16 + bx*4;

                  if (ym == 4) /* B_PRED: per-sub-block prediction */
                  {
                     uint8_t sa[8], sl[4]; uint8_t stl;
                     int sb_idx = by * 4 + bx;
                     /* Gather 4x4 context from already-reconstructed neighbors */
                     if (by > 0) { for(i=0;i<4;i++) sa[i]=sb_dst[-ys+i]; }
                     else if (my > 0) { for(i=0;i<4;i++) sa[i]=yb[(my*16-1)*ys+mx*16+bx*4+i]; }
                     else { memset(sa,127,4); }
                     /* Above-right: next 4 pixels. For bx==3 libvpx
                      * (intra_prediction_down_copy) replicates the above
                      * MB row's pixels at cols +16..19 down the right
                      * edge, so ALL bx==3 sub-blocks see the above-right
                      * MB's bottom-left 4 pixels (127 border if my==0 or
                      * at the last MB column). */
                     if (bx < 3 && by > 0) { for(i=0;i<4;i++) sa[4+i]=sb_dst[-ys+4+i]; }
                     else if (bx < 3 && my > 0) { for(i=0;i<4;i++) sa[4+i]=yb[(my*16-1)*ys+mx*16+bx*4+4+i]; }
                     else if (bx < 3) { for(i=0;i<4;i++) sa[4+i]=127; }
                     else if (my > 0 && mx < mbw-1) { for(i=0;i<4;i++) sa[4+i]=yb[(my*16-1)*ys+mx*16+16+i]; }
                     else if (my > 0)
                     {
                        /* Last MB column: libvpx's border extension
                         * (vp8_extend_mb_row) replicates the last real
                         * pixel of the row above across the right border,
                         * so above-right = 4 copies of that pixel. */
                        uint8_t rep = yb[(my*16-1)*ys + mbw*16 - 1];
                        for(i=0;i<4;i++) sa[4+i]=rep;
                     }
                     else { for(i=0;i<4;i++) sa[4+i]=127; }
                     if (bx > 0) { for(i=0;i<4;i++) sl[i]=sb_dst[i*ys-1]; }
                     else if (mx > 0) { for(i=0;i<4;i++) sl[i]=yb[(my*16+by*4+i)*ys+mx*16-1]; }
                     else { memset(sl,129,4); }
                     if (by > 0) {
                        if (bx > 0 || mx > 0) stl = sb_dst[-ys-1];
                        else stl = 129; /* mx==0: left frame border */
                     } else if (my > 0) {
                        if (bx > 0 || mx > 0) stl = yb[(my*16-1)*ys+mx*16+bx*4-1];
                        else stl = 129; /* mx==0: left frame border */
                     } else
                        stl = 127; /* my==0: above frame border (covers corner) */
                     vp8_pred4x4(sb_dst, ys, bmodes[sb_idx], sa, sl, stl);
                     start = 0; /* B_PRED: decode DC from tokens (type 1) */
                  }
                  else
                  {
                     start = 1; /* non-B_PRED: DC comes from Y2 */
                  }

                  if (is_skip)
                  {
                     memset(coeffs, 0, sizeof(coeffs[0]) * 16);
                     nz_cnt = 0;
                  }
                  else
                     nz_cnt = vp8_decode_block(tp, coeffs,
                           vp8_cprob[(ym == 4) ? 3 : 0], start, sb_ctx);
                  /* Dequantize */
                  if (ym != 4)
                     coeffs[0] = dc_vals[by * 4 + bx]; /* DC from WHT */
                  else
                     coeffs[0] = (int16_t)(coeffs[0] * y1_dc_q);
                  for (i = 1; i < 16; i++)
                     coeffs[i] = (int16_t)(coeffs[i] * y1_ac_q);
                  /* Inverse DCT + add to prediction */
                  vp8_idct4x4_add(coeffs, sb_dst, ys);
                  /* Update context tracking */
                  above_nz_y[mx*4+bx] = (nz_cnt > 0) ? 1 : 0;
                  left_nz_y[by] = (nz_cnt > 0) ? 1 : 0;
                  if (nz_cnt > 0) mb_has_coeffs = 1;
               }
            }

            /* 4 U sub-blocks */
            for (by = 0; by < 2; by++)
            {
               for (bx = 0; bx < 2; bx++)
               {
                  int sb_above = (my > 0 || by > 0) ? above_nz_u[mx*2+bx] : 0;
                  int sb_left  = (mx > 0 || bx > 0) ? left_nz_u[by] : 0;
                  int sb_ctx   = (sb_above + sb_left > 1) ? 2 : (sb_above + sb_left);
                  int nz_cnt;
                  if (is_skip)
                  {
                     memset(coeffs, 0, sizeof(coeffs[0]) * 16);
                     nz_cnt = 0;
                  }
                  else
                     nz_cnt = vp8_decode_block(tp, coeffs, vp8_cprob[2], 0, sb_ctx);
                  coeffs[0] = (int16_t)(coeffs[0] * uv_dc_q);
                  for (i = 1; i < 16; i++)
                     coeffs[i] = (int16_t)(coeffs[i] * uv_ac_q);
                  vp8_idct4x4_add(coeffs,
                        ub + (my*8 + by*4) * uvs + mx*8 + bx*4, uvs);
                  above_nz_u[mx*2+bx] = (nz_cnt > 0) ? 1 : 0;
                  left_nz_u[by] = (nz_cnt > 0) ? 1 : 0;
                  if (nz_cnt > 0) mb_has_coeffs = 1;
               }
            }

            /* 4 V sub-blocks */
            for (by = 0; by < 2; by++)
            {
               for (bx = 0; bx < 2; bx++)
               {
                  int sb_above = (my > 0 || by > 0) ? above_nz_v[mx*2+bx] : 0;
                  int sb_left  = (mx > 0 || bx > 0) ? left_nz_v[by] : 0;
                  int sb_ctx   = (sb_above + sb_left > 1) ? 2 : (sb_above + sb_left);
                  int nz_cnt;
                  if (is_skip)
                  {
                     memset(coeffs, 0, sizeof(coeffs[0]) * 16);
                     nz_cnt = 0;
                  }
                  else
                     nz_cnt = vp8_decode_block(tp, coeffs, vp8_cprob[2], 0, sb_ctx);
                  coeffs[0] = (int16_t)(coeffs[0] * uv_dc_q);
                  for (i = 1; i < 16; i++)
                     coeffs[i] = (int16_t)(coeffs[i] * uv_ac_q);
                  vp8_idct4x4_add(coeffs,
                        vb + (my*8 + by*4) * uvs + mx*8 + bx*4, uvs);
                  above_nz_v[mx*2+bx] = (nz_cnt > 0) ? 1 : 0;
                  left_nz_v[by] = (nz_cnt > 0) ? 1 : 0;
                  if (nz_cnt > 0) mb_has_coeffs = 1;
               }
            }
         }
         else
         {
            /* Skipped MB: clear non-zero context (libvpx
             * vp8_reset_mb_tokens_context). The Y2 context is only
             * reset when this MB actually has a Y2 block (non-B_PRED). */
            for (bx = 0; bx < 4; bx++) above_nz_y[mx*4+bx] = 0;
            for (bx = 0; bx < 2; bx++) { above_nz_u[mx*2+bx] = 0; above_nz_v[mx*2+bx] = 0; }
            memset(left_nz_y, 0, sizeof(left_nz_y));
            memset(left_nz_u, 0, sizeof(left_nz_u));
            memset(left_nz_v, 0, sizeof(left_nz_v));
            if (ym != 4) { above_nz_dc[mx] = 0; left_nz_dc = 0; }
         }
         /* libvpx: filter inner edges unless MB has no coefficients
          * (parsed skip OR eobtotal==0) and is not B_PRED. */
         if (skip_lf_buf)
            skip_lf_buf[my * mbw + mx] =
               (uint8_t)(((is_skip || !mb_has_coeffs) && ym != 4) ? 1 : 0);
         if (bpred_buf)
            bpred_buf[my * mbw + mx] = (uint8_t)(ym == 4 ? 1 : 0);

      }
   }

   free(above_nz_y); free(above_nz_u); free(above_nz_v); free(above_nz_dc); free(above_bmodes);
   } /* end context tracking block */

   /* Apply post-decode loop filter */
   if (lf_level > 0)
   {
      if (filter_type == 1)
         vp8_loop_filter_simple(yb, ys, mbw, mbh, lf_level, sharpness,
               seg_enabled, seg_abs, seg_lf, seg_map_buf, skip_lf_buf);
      else
         vp8_loop_filter_normal(yb, ys, ub, vb, uvs, mbw, mbh, lf_level,
               sharpness, seg_enabled, seg_abs, seg_lf,
               lf_delta_enabled, ref_lf_delta, mode_lf_delta,
               seg_map_buf, skip_lf_buf, bpred_buf);
   }

   /* YUV -> ARGB with fancy chroma upsampling (matches libwebp/dwebp).
    * Row 0 and the final even row mirror the chroma plane at the border;
    * interior luma row pairs (2r+1, 2r+2) interpolate chroma rows (r, r+1). */
   pix = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
   if (!pix) goto lfail;
   vp8_fancy_pair(yb, NULL, ub, vb, ub, vb, pix, NULL, w);
   for (j = 0; j + 2 < h; j += 2)
   {
      const uint8_t *tu = ub + (j >> 1) * uvs, *tv = vb + (j >> 1) * uvs;
      vp8_fancy_pair(yb + (j+1)*ys, yb + (j+2)*ys,
            tu, tv, tu + uvs, tv + uvs,
            pix + (size_t)(j+1)*w, pix + (size_t)(j+2)*w, w);
   }
   if (!(h & 1) && h >= 2)
   {
      const uint8_t *lu = ub + ((h-1) >> 1) * uvs, *lv = vb + ((h-1) >> 1) * uvs;
      vp8_fancy_pair(yb + (size_t)(h-1)*ys, NULL, lu, lv, lu, lv,
            pix + (size_t)(h-1)*w, NULL, w);
   }
   free(yb); free(ub); free(vb); free(seg_map_buf); free(skip_lf_buf); free(bpred_buf);
   *ow = (unsigned)w; *oh = (unsigned)h;
   return pix;
lfail:
   free(yb); free(ub); free(vb); free(seg_map_buf); free(skip_lf_buf); free(bpred_buf); free(pix);
   return NULL;
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
      pix = vp8_decode(c.vp8, c.vp8s, w, h);
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

/* ===== Public API ===== */

struct rwebp { uint8_t *buff_data; size_t buff_len; uint32_t *output_image; };

int rwebp_process_image(rwebp_t *rwebp, void **buf_data,
      size_t size, unsigned *width, unsigned *height,
      bool supports_rgba)
{
   if (!rwebp || !rwebp->buff_data) return IMAGE_PROCESS_ERROR;
   rwebp->output_image = rwebp_do(rwebp->buff_data,
         rwebp->buff_len > 0 ? rwebp->buff_len : size,
         width, height, supports_rgba);
   *buf_data = rwebp->output_image;
   return rwebp->output_image ? IMAGE_PROCESS_END : IMAGE_PROCESS_ERROR;
}

bool rwebp_set_buf_ptr(rwebp_t *rwebp, void *data, size_t len)
{
   if (!rwebp) return false;
   rwebp->buff_data = (uint8_t*)data;
   rwebp->buff_len = len;
   return true;
}

void rwebp_free(rwebp_t *rwebp) { if (rwebp) free(rwebp); }
rwebp_t *rwebp_alloc(void) { return (rwebp_t*)calloc(1, sizeof(rwebp_t)); }

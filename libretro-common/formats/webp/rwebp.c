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
   int lossless;
} rw_ctr;

static int rw_parse(const uint8_t *b, size_t l, rw_ctr *c)
{
   size_t p;
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
      { c->vp8 = d; c->vp8s = sz; c->lossless = 0; }
      else if (tag == RW_CC('V','P','8','L') && !c->vp8l)
      { c->vp8l = d; c->vp8ls = sz; c->lossless = 1; }
      else if (tag == RW_CC('A','N','M','F') && sz >= 16)
      {
         size_t sp;
         for (sp = 16; sp + 8 <= sz; )
         {
            uint32_t st = rw32(d+sp), ss = rw32(d+sp+4);
            if (sp+8+ss > sz) break;
            if (st == RW_CC('V','P','8',' ') && !c->vp8)
            { c->vp8 = d+sp+8; c->vp8s = ss; c->lossless = 0; }
            else if (st == RW_CC('V','P','8','L') && !c->vp8l)
            { c->vp8l = d+sp+8; c->vp8ls = ss; c->lossless = 1; }
            sp += 8 + ((ss+1) & ~(size_t)1);
         }
      }
      p += 8 + ((sz+1) & ~(size_t)1);
   }
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
#define VH_ROOT  8

typedef struct { uint32_t *t; int sz, rb; } vh;

static void vh_free(vh *h) { free(h->t); h->t = NULL; }

static int vh_build(vh *h, const uint8_t *lens, int ns, int root)
{
   int cnt[VH_MAXCL+1], off[VH_MAXCL+1], sorted[4096];
   int i, len, key, sym, total, step, tc;
   uint32_t *t;
   if (ns > 4096) return -1;
   memset(cnt, 0, sizeof(cnt));
   for (i = 0; i < ns; i++)
   {
      if (lens[i] > VH_MAXCL) return -1;
      cnt[lens[i]]++;
   }
   off[0] = 0; off[1] = 0;
   for (i = 1; i < VH_MAXCL; i++) off[i+1] = off[i] + cnt[i];
   for (i = 0; i < ns; i++) if (lens[i]) sorted[off[lens[i]]++] = i;

   total = 1 << root;
   for (len = root+1; len <= VH_MAXCL; len++)
      total += cnt[len] << (len - root);
   if (total < (1 << root)) total = 1 << root;

   h->t = (uint32_t*)calloc(total + 64, sizeof(uint32_t));
   if (!h->t) return -1;
   h->sz = total; h->rb = root;
   t = h->t; step = 1 << root;

   /* Trivial tree: 0 or 1 symbols -> every entry returns that symbol, 0 bits consumed */
   tc = 0;
   for (i = 1; i <= VH_MAXCL; i++) tc += cnt[i];
   if (tc <= 1)
   {
      int s = (tc == 1) ? sorted[0] : 0;
      uint32_t e = (uint32_t)(s << 16); /* code_length = 0 */
      for (i = 0; i < (1 << root); i++) t[i] = e;
      return 0;
   }

   key = 0; sym = 0;
   for (len = 1; len <= VH_MAXCL; len++)
   {
      for (i = 0; i < cnt[len]; i++, sym++)
      {
         int s = sorted[sym], j;
         if (len <= root)
         {
            int rk = 0;
            uint32_t e = (uint32_t)((s << 16) | len);
            for (j = 0; j < len; j++) rk |= ((key >> j) & 1) << (len-1-j);
            for (j = rk; j < (1 << root); j += (1 << len)) t[j] = e;
         }
         else
         {
            int rk2 = 0, sb = len - root, sk = 0, j2;
            uint32_t e = (uint32_t)((s << 16) | sb);
            for (j = 0; j < root; j++) rk2 |= ((key >> j) & 1) << (root-1-j);
            if (!(t[rk2] & 0x80000000u))
            {
               t[rk2] = (uint32_t)((step << 16) | sb | 0x80000000u);
               step += (1 << sb);
            }
            for (j = 0; j < sb; j++) sk |= ((key >> (root+j)) & 1) << (sb-1-j);
            { int so = (t[rk2] >> 16) & 0x7FFF, stb = t[rk2] & 0x1F;
              for (j2 = sk; j2 < (1 << stb); j2 += (1 << sb))
                 if (so + j2 < total + 64) t[so + j2] = e; }
         }
         key++;
      }
      key <<= 1;
   }
   return 0;
}

static INLINE int vh_read(const vh *h, vbr *b)
{
   uint32_t e;
   int idx;
   vbr_fill(b);
   idx = (int)(b->val & ((1u << h->rb) - 1));
   e = h->t[idx];
   if (e & 0x80000000u)
   {
      int sb = e & 0x1F, so = (e >> 16) & 0x7FFF;
      b->val >>= h->rb; b->nb -= h->rb;
      e = h->t[so + ((int)(b->val & ((1u << sb) - 1)))];
      { int cl = e & 0xFFFF; b->val >>= cl; b->nb -= cl; }
   }
   else
   {
      int cl = e & 0xFFFF;
      if (cl > 0) { b->val >>= cl; b->nb -= cl; }
   }
   return (int)(e >> 16);
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
         if (ms > ns) ms = ns;
      }
      si = 0;
      while (si < ms)
      {
         int c = vh_read(&clt, br);
         if      (c < 16) { lens[si++] = (uint8_t)c; if (c) prev = c; }
         else if (c == 16) { int r = vbr_read(br,2)+3; while (r-- > 0 && si < ms) lens[si++] = (uint8_t)prev; }
         else if (c == 17) { int r = vbr_read(br,3)+3; while (r-- > 0 && si < ms) lens[si++] = 0; }
         else if (c == 18) { int r = vbr_read(br,7)+11;while (r-- > 0 && si < ms) lens[si++] = 0; }
         else break;
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
   int d = px_abs((int)((T>>24)&0xFF)-(int)((TL>>24)&0xFF)) - px_abs((int)((L>>24)&0xFF)-(int)((TL>>24)&0xFF))
         + px_abs((int)((T>>16)&0xFF)-(int)((TL>>16)&0xFF)) - px_abs((int)((L>>16)&0xFF)-(int)((TL>>16)&0xFF))
         + px_abs((int)((T>> 8)&0xFF)-(int)((TL>> 8)&0xFF)) - px_abs((int)((L>> 8)&0xFF)-(int)((TL>> 8)&0xFF))
         + px_abs((int)( T     &0xFF)-(int)( TL     &0xFF)) - px_abs((int)( L     &0xFF)-(int)( TL     &0xFF));
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

/* Distance mapping */
static const int8_t vl_dx[] = {0,1,1,1,0,-1,-1,-1,0,2,2,2,1,1,-1,-1,-2,-2,-2,0,3,3,3,3,2,2,1,-1,-2,-2,-3,-3,-3,-3,0,4};
static const int8_t vl_dy[] = {1,0,1,-1,2,1,0,-1,2,0,1,-1,2,-2,2,-2,1,0,-1,2,0,1,-1,-2,2,-2,3,3,2,1,2,1,0,-1,3,0};

static int vl_dist(int c, int xs)
{
   if (c < 4) return c + 1;
   if (c < 40) { int d = vl_dy[c-4]*xs + vl_dx[c-4]; return d < 1 ? 1 : d; }
   return c - 2 + 1;
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

static uint32_t *vl_decode_pixels(vbr *br, int w, int h)
{
   uint32_t *pix;
   int ccb = 0, ccs = 0, ns[5], i, pi;
   uint32_t *cc = NULL;
   vh ht[5];
   uint8_t *cl;

   memset(ht, 0, sizeof(ht));
   if (vbr_read(br, 1))
   {
      ccb = vbr_read(br, 4);
      if (ccb < 1 || ccb > 11) return NULL;
      ccs = 1 << ccb;
      cc = (uint32_t*)calloc(ccs, sizeof(uint32_t));
      if (!cc) return NULL;
   }
   ns[0] = 256 + 24 + ccs; ns[1] = 256; ns[2] = 256; ns[3] = 256; ns[4] = 40;
   cl = (uint8_t*)malloc(4096);
   if (!cl) { free(cc); return NULL; }
   for (i = 0; i < 5; i++)
   {
      if (vh_read_codes(br, ns[i], cl) < 0) goto pfail;
      if (vh_build(&ht[i], cl, ns[i], VH_ROOT) < 0) goto pfail;
   }
   pix = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
   if (!pix) goto pfail;

   pi = 0;
   while (pi < w * h)
   {
      int g = vh_read(&ht[0], br);
      if (g < 256)
      {
         int r = vh_read(&ht[1], br);
         int b = vh_read(&ht[2], br);
         int a = vh_read(&ht[3], br);
         uint32_t argb = ((uint32_t)a<<24)|((uint32_t)r<<16)|((uint32_t)g<<8)|(uint32_t)b;
         pix[pi++] = argb;
         if (cc) cc[(0x1E35A7BDu * argb) >> (32 - ccb)] = argb;
      }
      else if (g < 256 + 24)
      {
         int lc = g - 256;
         int length = vl_prefix(lc, br);
         int dc = vh_read(&ht[4], br);
         int dist = (dc < 40) ? vl_dist(dc, w) : vl_prefix(dc - 2, br) + 38;
         int k;
         if (dist < 1) dist = 1;
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
         int ci = g - 256 - 24;
         pix[pi++] = (cc && ci < ccs) ? cc[ci] : 0xFF000000u;
      }
   }
   free(cl); free(cc);
   for (i = 0; i < 5; i++) vh_free(&ht[i]);
   return pix;
pfail:
   free(cl); free(cc);
   for (i = 0; i < 5; i++) vh_free(&ht[i]);
   return NULL;
}

/* ===== VP8L Full Decode with Transforms ===== */

#define XF_PRED 0
#define XF_CCOL 1
#define XF_SUBG 2
#define XF_CIDX 3
#define XF_MAX  4

typedef struct { int type, bits, dw, dh; uint32_t *data; } xf_t;

static uint32_t *vl_decode_full(const uint8_t *data, size_t len,
      unsigned *ow, unsigned *oh)
{
   vbr br;
   uint32_t sig, width, height;
   uint32_t *pix = NULL;
   xf_t xf[XF_MAX];
   int nxf = 0, i, cw, ch;

   vbr_init(&br, data, len);
   sig = vbr_read(&br, 8);
   if (sig != 0x2F) return NULL;
   width = vbr_read(&br, 14) + 1;
   height = vbr_read(&br, 14) + 1;
   vbr_read(&br, 1); /* alpha_is_used */
   if (vbr_read(&br, 3) != 0) return NULL; /* version */
   if (width > 16384 || height > 16384) return NULL;
   memset(xf, 0, sizeof(xf));
   cw = (int)width; ch = (int)height;

   /* Read transforms */
   while (vbr_read(&br, 1))
   {
      int tt = vbr_read(&br, 2);
      xf_t *x;
      if (nxf >= XF_MAX) goto xfail;
      x = &xf[nxf++];
      x->type = tt;

      switch (tt)
      {
         case XF_PRED:
         case XF_CCOL:
         {
            int bb = vbr_read(&br, 3) + 2;
            int bw = ((cw-1) >> bb) + 1;
            int bh = ((ch-1) >> bb) + 1;
            x->bits = bb; x->dw = bw; x->dh = bh;
            x->data = vl_decode_pixels(&br, bw, bh);
            if (!x->data) goto xfail;
            break;
         }
         case XF_SUBG:
            break;
         case XF_CIDX:
         {
            int nc = vbr_read(&br, 8) + 1, bits, pi2;
            x->dw = nc; x->dh = 1;
            x->data = vl_decode_pixels(&br, nc, 1);
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
   pix = vl_decode_pixels(&br, cw, ch);
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
                  uint32_t TR = (px2<cw-1 && py2>0)   ? pix[(py2-1)*cw+px2+1] : 0xFF000000u;
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
                  g2r = (int8_t)((td2 >> 16) & 0xFF);
                  g2b = (int8_t)((td2 >>  8) & 0xFF);
                  r2b = (int8_t)(td2 & 0xFF);
                  c2 = pix[py2 * cw + px2];
                  g = (int)((c2 >> 8) & 0xFF);
                  r = (int)((c2 >> 16) & 0xFF);
                  b2 = (int)(c2 & 0xFF);
                  r = (r + ((g2r * g) >> 5)) & 0xFF;
                  b2 = (b2 + ((g2b * g) >> 5) + ((r2b * r) >> 5)) & 0xFF;
                  pix[py2*cw+px2] = (c2 & 0xFF00FF00u) | ((uint32_t)r << 16) | (uint32_t)b2;
               }
            }
            break;
         }
      }
   }

   for (i = 0; i < nxf; i++) free(xf[i].data);
   *ow = width; *oh = height;
   return pix;
xfail:
   free(pix);
   for (i = 0; i < nxf; i++) free(xf[i].data);
   return NULL;
}

/* ===== VP8 Lossy — full decode with coefficients ===== */

typedef struct { const uint8_t *buf, *end; uint32_t range, value; int bit_count; } vp8b;

static void vp8b_init(vp8b *b, const uint8_t *d, size_t s)
{
   b->buf = d; b->end = d + s; b->range = 255;
   b->value = 0; b->bit_count = 0;
   if (b->buf < b->end) { b->value = (uint32_t)(*b->buf++) << 8; }
   if (b->buf < b->end) { b->value |= (uint32_t)(*b->buf++); }
}

static INLINE int vp8b_get(vp8b *b, int prob)
{
   uint32_t split = 1 + (((b->range - 1) * (uint32_t)prob) >> 8);
   uint32_t bigsplit = split << 8;
   int bit;
   if (b->value >= bigsplit)
   { bit = 1; b->range -= split; b->value -= bigsplit; }
   else
   { bit = 0; b->range = split; }
   while (b->range < 128)
   {
      b->value <<= 1;
      b->range <<= 1;
      if (++b->bit_count == 8)
      {
         b->bit_count = 0;
         if (b->buf < b->end)
            b->value |= *b->buf++;
      }
   }
   return bit;
}

static INLINE int     vp8b_bit(vp8b *b)       { return vp8b_get(b, 128); }
static INLINE uint32_t vp8b_lit(vp8b *b, int n)
{ uint32_t v = 0; int i; for (i = n-1; i >= 0; i--) v |= (uint32_t)vp8b_get(b,128) << i; return v; }
static INLINE int32_t vp8b_sig(vp8b *b, int n)
{ int32_t v = (int32_t)vp8b_lit(b,n); return vp8b_bit(b) ? -v : v; }

static INLINE uint8_t vp8_cl(int v) { return (uint8_t)(v<0?0:v>255?255:v); }

static void vp8_yuv2rgb(int y, int u, int v, uint8_t *r, uint8_t *g, uint8_t *bo)
{
   int c = y - 16, d = u - 128, e = v - 128;
   *r  = vp8_cl((298*c + 409*e + 128) >> 8);
   *g  = vp8_cl((298*c - 100*d - 208*e + 128) >> 8);
   *bo = vp8_cl((298*c + 516*d + 128) >> 8);
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
static int vp8_decode_block(vp8b *br, int16_t coeffs[16], int type,
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
   int i, tmp[16];
   for (i = 0; i < 4; i++)
   {
      int a = in[i*4+0] + in[i*4+2];
      int b = in[i*4+0] - in[i*4+2];
      int c = (in[i*4+1] * 35468 >> 16) - (in[i*4+3] + (in[i*4+3] * 20091 >> 16));
      int d = (in[i*4+1] + (in[i*4+1] * 20091 >> 16)) + (in[i*4+3] * 35468 >> 16);
      tmp[i*4+0] = a + d; tmp[i*4+1] = b + c;
      tmp[i*4+2] = b - c; tmp[i*4+3] = a - d;
   }
   for (i = 0; i < 4; i++)
   {
      int a = tmp[i] + tmp[8+i];
      int b = tmp[i] - tmp[8+i];
      int c = (tmp[4+i] * 35468 >> 16) - (tmp[12+i] + (tmp[12+i] * 20091 >> 16));
      int d = (tmp[4+i] + (tmp[4+i] * 20091 >> 16)) + (tmp[12+i] * 35468 >> 16);
      dst[0*stride+i] = vp8_cl(dst[0*stride+i] + ((a+d+4) >> 3));
      dst[1*stride+i] = vp8_cl(dst[1*stride+i] + ((b+c+4) >> 3));
      dst[2*stride+i] = vp8_cl(dst[2*stride+i] + ((b-c+4) >> 3));
      dst[3*stride+i] = vp8_cl(dst[3*stride+i] + ((a-d+4) >> 3));
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
      out[i]=(int16_t)(a+b); out[4+i]=(int16_t)(c+d);
      out[8+i]=(int16_t)(a-b); out[12+i]=(int16_t)(d-c);
   }
}

static const uint8_t vp8_ymp[4] = {145,156,163,128};
static const uint8_t vp8_uvmp[3] = {142,114,183};

/* Key-frame B_PRED sub-block mode probabilities (RFC 6386 §12.1)
 * Indexed by [above_bmode][left_bmode][tree_node 0..8] */
static const uint8_t kf_bmode_prob[10][10][9] = {
 {{231,120,48,89,115,113,120,152,112},{152,179,64,126,170,118,46,70,95},{175,69,143,80,85,82,72,155,103},{56,58,10,171,218,189,17,13,152},{114,26,17,163,44,195,21,10,173},{121,24,80,195,26,62,44,64,85},{144,71,10,38,171,213,144,34,26},{170,46,55,19,136,160,33,206,71},{63,20,8,114,114,208,12,9,226},{81,40,11,96,182,84,29,16,36}},
 {{134,183,89,137,98,101,106,165,148},{72,187,100,130,157,111,32,75,80},{66,102,167,99,74,62,40,234,128},{41,53,9,178,241,141,26,8,107},{74,43,26,146,73,166,49,23,157},{65,38,105,160,51,52,31,115,128},{104,79,12,27,217,255,87,17,7},{87,68,71,44,114,51,15,186,23},{47,41,14,110,182,183,21,17,194},{66,45,25,102,197,189,23,18,22}},
 {{88,88,147,150,42,46,45,196,205},{43,97,183,117,85,38,35,179,61},{39,53,200,87,26,21,43,232,171},{56,34,51,104,114,102,29,93,77},{39,28,85,171,58,165,90,98,64},{34,22,116,206,23,34,43,166,73},{107,54,32,26,51,1,81,43,31},{68,25,106,22,64,171,36,225,114},{34,19,21,102,132,188,16,76,124},{62,18,78,95,85,57,50,48,51}},
 {{193,101,35,159,215,111,89,46,111},{60,148,31,172,219,228,21,18,111},{112,113,77,85,179,255,38,120,114},{40,42,1,196,245,209,10,25,109},{88,43,29,140,166,213,37,43,154},{61,63,30,155,67,45,68,1,209},{100,80,8,43,154,1,51,26,71},{142,78,78,16,255,128,34,197,171},{41,40,5,102,211,183,4,1,221},{51,50,17,168,209,192,23,25,82}},
 {{138,31,36,171,27,166,38,44,229},{67,87,58,169,82,115,26,59,179},{63,59,90,180,59,166,93,73,154},{40,40,21,116,143,209,34,39,175},{47,15,16,183,34,223,49,45,183},{46,17,33,183,6,98,15,32,183},{57,46,22,24,128,1,54,17,37},{65,32,73,115,28,128,23,128,205},{40,3,9,115,51,192,18,6,223},{87,37,9,115,59,77,64,21,47}},
 {{104,55,44,218,9,54,53,130,226},{64,90,70,205,40,41,23,26,57},{54,57,112,184,5,41,38,166,213},{30,34,26,133,152,116,10,32,134},{75,32,12,51,192,255,160,43,51},{23,49,45,156,8,111,16,71,82},{42,158,42,48,22,234,13,1,1},{65,70,60,146,72,31,16,1,64},{57,18,10,102,102,213,34,20,43},{47,22,24,138,187,187,49,44,165}},
 {{182,24,21,242,2,2,7,40,219},{72,79,60,205,60,75,7,32,145},{62,68,86,130,73,119,27,149,192},{31,28,25,110,169,100,24,131,101},{58,45,4,75,114,193,102,44,42},{33,19,38,212,3,52,3,109,198},{22,27,3,47,244,255,78,7,3},{39,50,59,59,64,128,34,69,210},{44,7,5,85,101,214,14,9,187},{55,47,4,55,151,7,89,38,35}},
 {{164,50,31,137,154,133,25,35,218},{67,68,71,186,114,84,28,30,163},{90,67,64,90,153,132,25,119,188},{36,44,18,145,190,119,14,26,97},{63,43,20,116,100,152,48,93,127},{58,24,47,157,116,25,41,99,163},{75,55,11,14,8,30,22,51,130},{86,55,80,64,32,60,9,158,77},{37,20,14,111,138,163,11,11,214},{63,51,14,79,118,34,64,42,75}},
 {{141,28,36,162,27,128,43,116,227},{76,82,63,185,121,102,40,52,143},{63,58,96,152,115,128,58,102,152},{44,38,34,121,181,149,12,48,116},{79,28,16,128,167,239,41,38,109},{42,22,47,187,41,89,16,42,132},{54,45,15,27,188,213,53,22,16},{89,41,66,20,74,79,8,128,147},{53,18,15,93,181,196,21,20,153},{72,47,25,107,160,81,53,27,53}},
 {{124,68,51,98,125,189,82,82,200},{76,100,69,192,134,147,45,75,83},{57,59,107,115,109,131,43,139,143},{38,43,27,131,152,136,32,34,107},{80,45,17,134,81,175,99,50,100},{48,25,51,199,33,104,33,84,108},{60,53,16,24,158,220,44,24,44},{82,42,51,57,73,70,25,157,113},{51,24,14,115,133,209,18,16,209},{66,47,20,122,148,176,39,30,57}}
};

/* Decode a B_PRED sub-block mode from the key-frame tree (RFC 6386 §12.1) */
static int vp8_read_bmode(vp8b *br, int above, int left)
{
   const uint8_t *p = kf_bmode_prob[above][left];
   if (!vp8b_get(br, p[0])) return 0; /* B_DC_PRED */
   if (!vp8b_get(br, p[1])) return 1; /* B_TM_PRED */
   if (!vp8b_get(br, p[2])) return 2; /* B_VE_PRED */
   if (!vp8b_get(br, p[3])) return 3; /* B_HE_PRED */
   if (!vp8b_get(br, p[4])) return 4; /* B_LD_PRED */
   if (!vp8b_get(br, p[5])) return 5; /* B_RD_PRED */
   if (!vp8b_get(br, p[6])) return 6; /* B_VR_PRED */
   if (!vp8b_get(br, p[7])) return 7; /* B_VL_PRED */
   if (!vp8b_get(br, p[8])) return 8; /* B_HD_PRED */
   return 9; /* B_HU_PRED */
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
   default: /* B_VL, B_HD, B_HU — simplified to DC for now */
   {  int sum=0;
      for(i=0;i<4;i++) sum+=a[i]+l[i];
      { uint8_t dc=(uint8_t)((sum+4)>>3);
        for(j=0;j<4;j++) memset(d+j*s,dc,4); }
      break;
   }
   }
}

static void vp8_pred16(uint8_t *d, int s, int m, const uint8_t *a, const uint8_t *l, uint8_t tl)
{
   int i, j;
   switch (m) {
   case 0: { int sum=0; for(i=0;i<16;i++) sum+=a[i]+l[i];
             { uint8_t dc=(uint8_t)((sum+16)>>5); for(j=0;j<16;j++) memset(d+j*s,dc,16); } break; }
   case 1: for(j=0;j<16;j++) memcpy(d+j*s,a,16); break;
   case 2: for(j=0;j<16;j++) memset(d+j*s,l[j],16); break;
   case 3: for(j=0;j<16;j++) for(i=0;i<16;i++) d[j*s+i]=vp8_cl((int)a[i]+(int)l[j]-(int)tl); break;
   }
}

static void vp8_pred8(uint8_t *d, int s, int m, const uint8_t *a, const uint8_t *l, uint8_t tl)
{
   int i, j;
   switch (m) {
   case 0: { int sum=0; for(i=0;i<8;i++) sum+=a[i]+l[i];
             { uint8_t dc=(uint8_t)((sum+8)>>4); for(j=0;j<8;j++) memset(d+j*s,dc,8); } break; }
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
   int seg_enabled, seg_abs, seg_qp[4], seg_prob[3];
   vp8b br;
   vp8b tbr[8]; /* up to 8 token partitions */
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
   vp8b_init(&br, p0, p0s);

   vp8b_bit(&br); vp8b_bit(&br); /* color_space, clamping */

   /* Segmentation */
   seg_enabled = vp8b_bit(&br);
   seg_abs = 0;
   seg_qp[0] = seg_qp[1] = seg_qp[2] = seg_qp[3] = 0;
   seg_prob[0] = seg_prob[1] = seg_prob[2] = 255;
   if (seg_enabled)
   {
      int um = vp8b_bit(&br), ud = vp8b_bit(&br);
      if (ud) {
         seg_abs = vp8b_bit(&br);
         for (i=0;i<4;i++) seg_qp[i] = vp8b_bit(&br) ? vp8b_sig(&br,7) : 0;
         for (i=0;i<4;i++) if (vp8b_bit(&br)) vp8b_sig(&br,6); /* lf deltas - discard */
      }
      if (um) for (i=0;i<3;i++) { if (vp8b_bit(&br)) seg_prob[i] = (int)vp8b_lit(&br,8); }
   }

   vp8b_bit(&br); vp8b_lit(&br,6); vp8b_lit(&br,3); /* filter */
   { int mrd = vp8b_bit(&br);
     if (mrd && vp8b_bit(&br))
     { for(i=0;i<4;i++) if(vp8b_bit(&br)) vp8b_sig(&br,6);
       for(i=0;i<4;i++) if(vp8b_bit(&br)) vp8b_sig(&br,6); } }
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
   { int q2 = qp + y2dc_dq; y2_dc_q = vp8_dc_qlut[q2<0?0:q2>127?127:q2]; }
   { int q2 = qp + y2ac_dq; y2_ac_q = vp8_ac_qlut[q2<0?0:q2>127?127:q2]; }
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
   yb = (uint8_t*)calloc(ys * mbh * 16, 1);
   ub = (uint8_t*)calloc(uvs * mbh * 8, 1);
   vb = (uint8_t*)calloc(uvs * mbh * 8, 1);
   if (!yb || !ub || !vb) goto lfail;
   memset(yb, 128, ys * mbh * 16);
   memset(ub, 128, uvs * mbh * 8);
   memset(vb, 128, uvs * mbh * 8);

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
         int ym, uvm, is_skip = 0, seg_id = 0;
         uint8_t ay[16], ly[16], au[8], lu[8], av[8], lv[8];
         uint8_t tly=128, tlu=128, tlv=128;
         int16_t coeffs[16], y2_block[16], dc_vals[16];
         uint8_t bmodes[16]; /* B_PRED sub-block modes */
         int bx, by;
         int mb_qp;

         /* Read segment ID if segmentation is enabled */
         if (seg_enabled)
         {
            /* VP8 segment tree (RFC 6386 §10.2): linear, not binary */
            if (!vp8b_get(&br, seg_prob[0]))
               seg_id = 0;
            else if (!vp8b_get(&br, seg_prob[1]))
               seg_id = 1;
            else if (!vp8b_get(&br, seg_prob[2]))
               seg_id = 2;
            else
               seg_id = 3;
         }

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
         { int q2 = mb_qp + y2dc_dq; y2_dc_q = vp8_dc_qlut[q2<0?0:q2>127?127:q2]; }
         { int q2 = mb_qp + y2ac_dq; y2_ac_q = vp8_ac_qlut[q2<0?0:q2>127?127:q2]; }
         { int q2 = mb_qp + uvdc_dq; uv_dc_q = vp8_dc_qlut[q2<0?0:q2>127?127:q2]; if(uv_dc_q>132)uv_dc_q=132; }
         { int q2 = mb_qp + uvac_dq; uv_ac_q = vp8_ac_qlut[q2<0?0:q2>127?127:q2]; }

         /* Y mode — keyframe tree: B_PRED first (RFC 6386 §11.2) */
         if      (!vp8b_get(&br, vp8_ymp[0])) ym = 4; /* B_PRED */
         else if (!vp8b_get(&br, vp8_ymp[1])) ym = 0; /* DC_PRED */
         else if (!vp8b_get(&br, vp8_ymp[2])) ym = 1; /* V_PRED */
         else if (!vp8b_get(&br, vp8_ymp[3])) ym = 2; /* H_PRED */
         else ym = 3; /* TM_PRED */

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
            for (i = 0; i < 4; i++) above_bmodes[mx * 4 + i] = 0;
            for (i = 0; i < 4; i++) left_bmodes[i] = 0;
         }

         /* UV mode */
         if      (!vp8b_get(&br, vp8_uvmp[0])) uvm = 0;
         else if (!vp8b_get(&br, vp8_uvmp[1])) uvm = 1;
         else if (!vp8b_get(&br, vp8_uvmp[2])) uvm = 2;
         else uvm = 3;

         /* Skip flag */
         if (skip_enabled)
            is_skip = vp8b_get(&br, prob_skip);

         /* Gather prediction context */
         if (my > 0) {
            memcpy(ay, yb+(my*16-1)*ys+mx*16, 16);
            memcpy(au, ub+(my*8-1)*uvs+mx*8, 8);
            memcpy(av, vb+(my*8-1)*uvs+mx*8, 8);
            if (mx > 0) { tly=yb[(my*16-1)*ys+mx*16-1]; tlu=ub[(my*8-1)*uvs+mx*8-1]; tlv=vb[(my*8-1)*uvs+mx*8-1]; }
         } else { memset(ay,127,16); memset(au,127,8); memset(av,127,8); }
         if (mx > 0) {
            for(j=0;j<16;j++) ly[j]=yb[(my*16+j)*ys+mx*16-1];
            for(j=0;j<8;j++) lu[j]=ub[(my*8+j)*uvs+mx*8-1];
            for(j=0;j<8;j++) lv[j]=vb[(my*8+j)*uvs+mx*8-1];
         } else { memset(ly,129,16); memset(lu,129,8); memset(lv,129,8); }

         /* Predict */
         if (ym != 4)
            vp8_pred16(yb+my*16*ys+mx*16, ys, ym, ay, ly, tly);
         /* B_PRED Y prediction is done per sub-block below */
         vp8_pred8(ub+my*8*uvs+mx*8, uvs, uvm, au, lu, tlu);
         vp8_pred8(vb+my*8*uvs+mx*8, uvs, uvm, av, lv, tlv);

         /* Decode and add residual */
         if (!is_skip)
         {
            /* Non-zero coefficient tracking for context. */
            int nz_y2 = 0;

            /* Y2 block (DC for 16x16 prediction) */
            memset(dc_vals, 0, sizeof(dc_vals));
            if (ym != 4) /* not B_PRED */
            {
               int y2_above = (my > 0) ? above_nz_dc[mx] : 0;
               int y2_left  = (mx > 0) ? left_nz_dc : 0;
               int y2_ctx   = (y2_above + y2_left > 1) ? 2 : (y2_above + y2_left);
               memset(y2_block, 0, sizeof(y2_block));
               nz_y2 = vp8_decode_block(tp, y2_block, 1, vp8_cprob[1], 0, y2_ctx);
               above_nz_dc[mx] = (nz_y2 > 0) ? 1 : 0;
               left_nz_dc = (nz_y2 > 0) ? 1 : 0;
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
                     /* Above-right: next 4 pixels */
                     if (bx < 3 && by > 0) { for(i=0;i<4;i++) sa[4+i]=sb_dst[-ys+4+i]; }
                     else if (bx < 3 && my > 0) { for(i=0;i<4;i++) sa[4+i]=yb[(my*16-1)*ys+mx*16+bx*4+4+i]; }
                     else { for(i=0;i<4;i++) sa[4+i]=sa[3]; }
                     if (bx > 0) { for(i=0;i<4;i++) sl[i]=sb_dst[i*ys-1]; }
                     else if (mx > 0) { for(i=0;i<4;i++) sl[i]=yb[(my*16+by*4+i)*ys+mx*16-1]; }
                     else { memset(sl,129,4); }
                     if (bx > 0 && by > 0) stl = sb_dst[-ys-1];
                     else if (by > 0 && mx > 0) stl = sb_dst[-ys-1];
                     else if (bx > 0 && my > 0) stl = yb[(my*16-1)*ys+mx*16+bx*4-1];
                     else stl = 128;
                     vp8_pred4x4(sb_dst, ys, bmodes[sb_idx], sa, sl, stl);
                     start = 0; /* B_PRED: decode DC from tokens (type 1) */
                  }
                  else
                  {
                     start = 1; /* non-B_PRED: DC comes from Y2 */
                  }

                  nz_cnt = vp8_decode_block(tp, coeffs, (ym == 4) ? 3 : 0,
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
                  int nz_cnt = vp8_decode_block(tp, coeffs, 2, vp8_cprob[2], 0, sb_ctx);
                  coeffs[0] = (int16_t)(coeffs[0] * uv_dc_q);
                  for (i = 1; i < 16; i++)
                     coeffs[i] = (int16_t)(coeffs[i] * uv_ac_q);
                  vp8_idct4x4_add(coeffs,
                        ub + (my*8 + by*4) * uvs + mx*8 + bx*4, uvs);
                  above_nz_u[mx*2+bx] = (nz_cnt > 0) ? 1 : 0;
                  left_nz_u[by] = (nz_cnt > 0) ? 1 : 0;
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
                  int nz_cnt = vp8_decode_block(tp, coeffs, 2, vp8_cprob[2], 0, sb_ctx);
                  coeffs[0] = (int16_t)(coeffs[0] * uv_dc_q);
                  for (i = 1; i < 16; i++)
                     coeffs[i] = (int16_t)(coeffs[i] * uv_ac_q);
                  vp8_idct4x4_add(coeffs,
                        vb + (my*8 + by*4) * uvs + mx*8 + bx*4, uvs);
                  above_nz_v[mx*2+bx] = (nz_cnt > 0) ? 1 : 0;
                  left_nz_v[by] = (nz_cnt > 0) ? 1 : 0;
               }
            }
         }
         else
         {
            /* Skipped MB: clear all non-zero context */
            for (bx = 0; bx < 4; bx++) above_nz_y[mx*4+bx] = 0;
            for (bx = 0; bx < 2; bx++) { above_nz_u[mx*2+bx] = 0; above_nz_v[mx*2+bx] = 0; }
            above_nz_dc[mx] = 0;
            memset(left_nz_y, 0, sizeof(left_nz_y));
            memset(left_nz_u, 0, sizeof(left_nz_u));
            memset(left_nz_v, 0, sizeof(left_nz_v));
            left_nz_dc = 0;
         }
      }
   }

   free(above_nz_y); free(above_nz_u); free(above_nz_v); free(above_nz_dc); free(above_bmodes);
   } /* end context tracking block */

   /* YUV -> ARGB */
   pix = (uint32_t*)malloc((size_t)w * h * sizeof(uint32_t));
   if (!pix) goto lfail;
   for (j = 0; j < h; j++)
      for (i = 0; i < w; i++)
      {
         uint8_t r, g, b2;
         vp8_yuv2rgb(yb[j*ys+i], ub[(j>>1)*uvs+(i>>1)], vb[(j>>1)*uvs+(i>>1)], &r, &g, &b2);
         pix[j*w+i] = 0xFF000000u | ((uint32_t)r<<16) | ((uint32_t)g<<8) | (uint32_t)b2;
      }
   free(yb); free(ub); free(vb);
   *ow = (unsigned)w; *oh = (unsigned)h;
   return pix;
lfail:
   free(yb); free(ub); free(vb); free(pix);
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
   if (!pix && c.vp8 && c.vp8s > 0) pix = vp8_decode(c.vp8, c.vp8s, w, h);
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

/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------------------------
 * The following license statement only applies to this file (rtt.c).
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
#include <math.h>
#include <stdint.h>

#include <retro_inline.h>
#include <gfx/font/rtt.h>

/* Max composite recursion; TrueType nesting deeper than this is
 * either malicious or broken. */
#define RTT_MAX_COMPOSITE_DEPTH 6


/* ------------------------------------------------------------------ */
/* Bounds-checked big-endian readers. Out-of-range reads yield 0.     */
/* ------------------------------------------------------------------ */

static uint8_t rtt__u8(const rtt_font_t *f, uint32_t off)
{
   if (off >= f->size)
      return 0;
   return f->data[off];
}

static uint16_t rtt__u16(const rtt_font_t *f, uint32_t off)
{
   if (off + 2 > f->size || off + 2 < off)
      return 0;
   return (uint16_t)((f->data[off] << 8) | f->data[off + 1]);
}

static uint32_t rtt__u32(const rtt_font_t *f, uint32_t off)
{
   if (off + 4 > f->size || off + 4 < off)
      return 0;
   return  ( (uint32_t)f->data[off]     << 24)
          | ((uint32_t)f->data[off + 1] << 16)
          | ((uint32_t)f->data[off + 2] <<  8)
          | ((uint32_t)f->data[off + 3]);
}

/* ------------------------------------------------------------------ */
/* Font directory                                                     */
/* ------------------------------------------------------------------ */

static int rtt__is_sfnt(const uint8_t *d, size_t size, size_t off)
{
   uint32_t v;
   if (off + 4 > size)
      return 0;
   v = ((uint32_t)d[off] << 24) | ((uint32_t)d[off + 1] << 16) |
       ((uint32_t)d[off + 2] << 8) | (uint32_t)d[off + 3];
   /* 1.0, 'true', or 'OTTO' for CFF outlines */
   return (v == 0x00010000u || v == 0x74727565u || v == 0x4F54544Fu);
}

int rtt_font_offset_for_index(const uint8_t *data, size_t size,
      int index)
{
   uint32_t tag;
   if (!data || size < 12)
      return -1;
   tag = ((uint32_t)data[0] << 24) | ((uint32_t)data[1] << 16) |
         ((uint32_t)data[2] << 8) | (uint32_t)data[3];
   if (tag == 0x74746366u) /* 'ttcf' */
   {
      uint32_t n = ((uint32_t)data[8] << 24) | ((uint32_t)data[9] << 16) |
                   ((uint32_t)data[10] << 8) | (uint32_t)data[11];
      uint32_t rec;
      if (index < 0 || (uint32_t)index >= n)
         return -1;
      rec = 12u + 4u * (uint32_t)index;
      if (rec + 4 > size)
         return -1;
      return (int)(((uint32_t)data[rec]     << 24) |
                   ((uint32_t)data[rec + 1] << 16) |
                   ((uint32_t)data[rec + 2] <<  8) |
                   ((uint32_t)data[rec + 3]));
   }
   if (index != 0)
      return -1;
   if (rtt__is_sfnt(data, size, 0))
      return 0;
   return -1;
}

static uint32_t rtt__find_table(const rtt_font_t *f, uint32_t fontstart,
      const char *tag, uint32_t *len)
{
   uint32_t num = rtt__u16(f, fontstart + 4);
   uint32_t i;
   for (i = 0; i < num; i++)
   {
      uint32_t rec = fontstart + 12 + 16 * i;
      if (rec + 16 > f->size)
         return 0;
      if (   f->data[rec]     == (uint8_t)tag[0]
          && f->data[rec + 1] == (uint8_t)tag[1]
          && f->data[rec + 2] == (uint8_t)tag[2]
          && f->data[rec + 3] == (uint8_t)tag[3])
      {
         uint32_t off = rtt__u32(f, rec + 8);
         uint32_t l   = rtt__u32(f, rec + 12);
         if (off >= f->size || l > f->size - off)
            return 0;
         if (len)
            *len = l;
         return off;
      }
   }
   return 0;
}

static void rtt__select_cmap(rtt_font_t *f, uint32_t cmap, uint32_t cmap_len)
{
   uint32_t ntab = rtt__u16(f, cmap + 2);
   uint32_t best = 0;
   int best_rank = -1;
   uint32_t i;

   for (i = 0; i < ntab; i++)
   {
      uint32_t rec  = cmap + 4 + 8 * i;
      uint16_t plat = rtt__u16(f, rec);
      uint16_t enc  = rtt__u16(f, rec + 2);
      uint32_t off  = rtt__u32(f, rec + 4);
      int rank      = -1;

      if (off >= cmap_len)
         continue;

      if (plat == 3 && enc == 10)      rank = 4; /* Windows UCS-4    */
      else if (plat == 3 && enc == 1)  rank = 3; /* Windows BMP      */
      else if (plat == 0)              rank = 2; /* Unicode platform */
      else if (plat == 3 && enc == 0)  rank = 1; /* Windows symbol   */

      if (rank > best_rank)
      {
         best_rank = rank;
         best      = cmap + off;
      }
   }

   if (best_rank < 0)
      return;

   f->cmap_sub     = best;
   f->cmap_sub_len = cmap_len - (best - cmap);
   f->cmap_format  = rtt__u16(f, best);
}

/* ------------------------------------------------------------------ */
/* CFF: INDEXes, DICTs, and the charstrings, subroutines and font     */
/* dicts a glyph needs (Adobe TN 5176).                               */
/* ------------------------------------------------------------------ */

static uint32_t rtt__offn(const rtt_font_t *f, uint32_t off, uint32_t n)
{
   uint32_t v = 0;
   while (n--)
      v = (v << 8) | rtt__u8(f, off++);
   return v;
}

/* Reads the INDEX at @off; returns the offset just past it, 0 when it
 * does not fit in @end. */
static uint32_t rtt__cff_index(const rtt_font_t *f, uint32_t off,
      uint32_t end, rtt_cff_index_t *idx)
{
   uint32_t last;
   memset(idx, 0, sizeof(*idx));
   if (off + 2 > end)
      return 0;
   idx->count = rtt__u16(f, off);
   if (!idx->count)
      return off + 2;
   idx->off_size = rtt__u8(f, off + 2);
   if (idx->off_size < 1 || idx->off_size > 4)
      return 0;
   idx->offsets = off + 3;
   if (idx->offsets + (idx->count + 1) * idx->off_size > end)
      return 0;
   idx->data = idx->offsets + (idx->count + 1) * idx->off_size - 1;
   last      = rtt__offn(f, idx->offsets + idx->count * idx->off_size,
         idx->off_size);
   if (last < 1 || idx->data + last > end)
      return 0;
   return idx->data + last;
}

/* Item @i of an INDEX; returns 0 when it is out of range. */
static int rtt__cff_item(const rtt_font_t *f, const rtt_cff_index_t *idx,
      uint32_t i, uint32_t *start, uint32_t *len)
{
   uint32_t a, b;
   if (i >= idx->count)
      return 0;
   a = rtt__offn(f, idx->offsets + i * idx->off_size, idx->off_size);
   b = rtt__offn(f, idx->offsets + (i + 1) * idx->off_size, idx->off_size);
   if (a < 1 || b < a)
      return 0;
   *start = idx->data + a;
   *len   = b - a;
   return 1;
}

/* The operands of DICT operator @op (12 x as 0x0C00 | x), up to @max of
 * them; returns how many, -1 when the operator is absent. */
static int rtt__cff_dict(const rtt_font_t *f, uint32_t p, uint32_t end,
      unsigned op, double *out, int max)
{
   double st[48];
   int    n = 0;
   while (p < end)
   {
      unsigned b = rtt__u8(f, p++);
      if (b >= 32 && b <= 246)
      {
         if (n < 48) st[n++] = (double)((int)b - 139);
      }
      else if (b >= 247 && b <= 250)
      {
         if (n < 48) st[n++] = (double)(((int)b - 247) * 256
               + rtt__u8(f, p) + 108);
         p++;
      }
      else if (b >= 251 && b <= 254)
      {
         if (n < 48) st[n++] = (double)(-((int)b - 251) * 256
               - rtt__u8(f, p) - 108);
         p++;
      }
      else if (b == 28)
      {
         if (n < 48) st[n++] = (double)(int16_t)rtt__u16(f, p);
         p += 2;
      }
      else if (b == 29)
      {
         if (n < 48) st[n++] = (double)(int32_t)rtt__u32(f, p);
         p += 4;
      }
      else if (b == 30)
      {
         /* A real, as nibbles; only its end matters for offsets */
         for (;;)
         {
            unsigned v = rtt__u8(f, p++);
            if ((v & 0x0F) == 0x0F || (v >> 4) == 0x0F || p >= end)
               break;
         }
         if (n < 48) st[n++] = 0.0;
      }
      else
      {
         unsigned o = b;
         if (b == 12)
            o = 0x0C00u | rtt__u8(f, p++);
         if (o == op)
         {
            int i;
            if (n > max)
               n = max;
            for (i = 0; i < n; i++)
               out[i] = st[i];
            return n;
         }
         n = 0;
      }
   }
   return -1;
}

/* The local subroutines of the Private DICT a font dict points at. */
static void rtt__cff_private_subrs(const rtt_font_t *f, uint32_t dict,
      uint32_t dict_len, rtt_cff_index_t *subrs)
{
   double   v[2];
   uint32_t priv, size;
   memset(subrs, 0, sizeof(*subrs));
   if (rtt__cff_dict(f, dict, dict + dict_len, 18, v, 2) != 2
         || v[0] < 0 || v[1] < 0)
      return;
   size = (uint32_t)v[0];
   priv = f->cff + (uint32_t)v[1];
   if (priv + size > f->cff + f->cff_len)
      return;
   if (rtt__cff_dict(f, priv, priv + size, 19, v, 1) == 1 && v[0] > 0)
      rtt__cff_index(f, priv + (uint32_t)v[0], f->cff + f->cff_len, subrs);
}

static int rtt__cff_init(rtt_font_t *f)
{
   uint32_t end = f->cff + f->cff_len;
   uint32_t p, top, top_len;
   rtt_cff_index_t names, tops, strings;
   double   v[2];

   if (f->cff_len < 4 || rtt__u8(f, f->cff) != 1)
      return 0; /* CFF2 and later majors are not read */
   p = f->cff + rtt__u8(f, f->cff + 2);
   if (!(p = rtt__cff_index(f, p, end, &names))
         || !(p = rtt__cff_index(f, p, end, &tops))
         || !(p = rtt__cff_index(f, p, end, &strings))
         || !rtt__cff_index(f, p, end, &f->cff_gsubrs)
         || !rtt__cff_item(f, &tops, 0, &top, &top_len))
      return 0;

   if (rtt__cff_dict(f, top, top + top_len, 0x0C06u, v, 1) == 1 && v[0] != 2)
      return 0; /* Type 1 charstrings */
   if (rtt__cff_dict(f, top, top + top_len, 17, v, 1) != 1 || v[0] <= 0
         || !rtt__cff_index(f, f->cff + (uint32_t)v[0], end,
            &f->cff_charstrings)
         || !f->cff_charstrings.count)
      return 0;

   if (rtt__cff_dict(f, top, top + top_len, 0x0C24u, v, 1) == 1 && v[0] > 0)
   {
      /* CID-keyed: each glyph's font dict holds its own subroutines */
      uint32_t i;
      if (!rtt__cff_index(f, f->cff + (uint32_t)v[0], end, &f->cff_fdarray)
            || rtt__cff_dict(f, top, top + top_len, 0x0C25u, v, 1) != 1
            || v[0] <= 0)
         return 0;
      f->cff_fdselect = f->cff + (uint32_t)v[0];
      f->cff_num_fds  = f->cff_fdarray.count < RTT_CFF_MAX_FDS
            ? (int)f->cff_fdarray.count : RTT_CFF_MAX_FDS;
      for (i = 0; i < (uint32_t)f->cff_num_fds; i++)
      {
         uint32_t d, dl;
         if (rtt__cff_item(f, &f->cff_fdarray, i, &d, &dl))
            rtt__cff_private_subrs(f, d, dl, &f->cff_fd_subrs[i]);
      }
   }
   else
      rtt__cff_private_subrs(f, top, top_len, &f->cff_subrs);
   return 1;
}

/* The local subroutines for glyph @gi. */
static void rtt__cff_glyph_subrs(const rtt_font_t *f, int gi,
      rtt_cff_index_t *subrs)
{
   uint32_t fd = 0;
   uint32_t d, dl;
   if (!f->cff_fdselect)
   {
      *subrs = f->cff_subrs;
      return;
   }
   switch (rtt__u8(f, f->cff_fdselect))
   {
      case 0:
         fd = rtt__u8(f, f->cff_fdselect + 1 + (uint32_t)gi);
         break;
      case 3:
      {
         uint32_t n  = rtt__u16(f, f->cff_fdselect + 1);
         uint32_t lo = 0, hi = n;
         /* ranges of (first glyph, fd), then a sentinel glyph */
         while (lo < hi)
         {
            uint32_t mid = (lo + hi) >> 1;
            if (rtt__u16(f, f->cff_fdselect + 3 + 3 * mid) <= (uint32_t)gi)
               lo = mid + 1;
            else
               hi = mid;
         }
         if (lo > 0)
            fd = rtt__u8(f, f->cff_fdselect + 3 + 3 * (lo - 1) + 2);
         break;
      }
      default:
         break;
   }
   if (fd < (uint32_t)f->cff_num_fds)
      *subrs = f->cff_fd_subrs[fd];
   else if (rtt__cff_item(f, &f->cff_fdarray, fd, &d, &dl))
      rtt__cff_private_subrs(f, d, dl, subrs);
   else
      memset(subrs, 0, sizeof(*subrs));
}

int rtt_init(rtt_font_t *f, const uint8_t *data, size_t size,
      int fontstart)
{
   uint32_t cmap, cmap_len = 0, maxp, maxp_len = 0, head, hhea, dummy;

   memset(f, 0, sizeof(*f));
   if (!data || fontstart < 0 || (size_t)fontstart >= size)
      return 0;
   f->data = data;
   f->size = size;

   if (!rtt__is_sfnt(data, size, (size_t)fontstart))
      return 0;

   f->fontstart = (uint32_t)fontstart;
   cmap    = rtt__find_table(f, (uint32_t)fontstart, "cmap", &cmap_len);
   head    = rtt__find_table(f, (uint32_t)fontstart, "head", &dummy);
   hhea    = rtt__find_table(f, (uint32_t)fontstart, "hhea", &dummy);
   maxp    = rtt__find_table(f, (uint32_t)fontstart, "maxp", &maxp_len);
   f->hmtx = rtt__find_table(f, (uint32_t)fontstart, "hmtx", &f->hmtx_len);
   f->glyf = rtt__find_table(f, (uint32_t)fontstart, "glyf", &f->glyf_len);
   f->loca = rtt__find_table(f, (uint32_t)fontstart, "loca", &f->loca_len);

   if (!cmap || !head || !hhea || !maxp || !f->hmtx)
      return 0;
   if (!f->glyf || !f->loca)
   {
      /* CFF outlines in place of glyf */
      f->cff = rtt__find_table(f, (uint32_t)fontstart, "CFF ", &f->cff_len);
      f->glyf = f->loca = f->glyf_len = f->loca_len = 0;
      if (!f->cff)
         return 0;
      f->is_cff = 1;
   }
   f->vhea = rtt__find_table(f, (uint32_t)fontstart, "vhea", &dummy);
   f->vmtx = rtt__find_table(f, (uint32_t)fontstart, "vmtx", &f->vmtx_len);
   f->kern = rtt__find_table(f, (uint32_t)fontstart, "kern", &f->kern_len);
   if (f->vhea && f->vmtx)
      f->num_vmetrics = rtt__u16(f, f->vhea + 34);

   f->head         = head;
   f->hhea         = hhea;
   f->units_per_em = rtt__u16(f, head + 18);
   f->loca_long    = (int16_t)rtt__u16(f, head + 50) != 0;
   f->num_glyphs   = rtt__u16(f, maxp + 4);
   f->max_points   = maxp_len >= 8 ? (int)rtt__u16(f, maxp + 6) : 0;
   f->num_hmetrics = rtt__u16(f, hhea + 34);

   if (f->units_per_em <= 0 || f->num_glyphs <= 0)
      return 0;

   rtt__select_cmap(f, cmap, cmap_len);
   if (!f->cmap_sub)
      return 0;
   if (f->is_cff && !rtt__cff_init(f))
      return 0;

   return 1;
}

/* ------------------------------------------------------------------ */
/* cmap lookup                                                        */
/* ------------------------------------------------------------------ */

int rtt_find_glyph(const rtt_font_t *f, uint32_t cp)
{
   uint32_t sub = f->cmap_sub;

   switch (f->cmap_format)
   {
      case 0:
         if (cp < 256)
            return rtt__u8(f, sub + 6 + cp);
         return 0;

      case 4:
      {
         uint32_t segx2   = rtt__u16(f, sub + 6);
         uint32_t segs    = segx2 >> 1;
         uint32_t end_a   = sub + 14;
         uint32_t start_a = sub + 16 + segx2;
         uint32_t delta_a = sub + 16 + 2 * segx2;
         uint32_t range_a = sub + 16 + 3 * segx2;
         uint32_t lo = 0, hi = segs, seg;
         uint16_t start_c, range;

         if (cp > 0xFFFFu || segs == 0)
            return 0;

         while (lo < hi) /* first segment with endCode >= cp */
         {
            uint32_t mid = (lo + hi) >> 1;
            if (rtt__u16(f, end_a + 2 * mid) < cp)
               lo = mid + 1;
            else
               hi = mid;
         }
         seg = lo;
         if (seg >= segs)
            return 0;
         start_c = rtt__u16(f, start_a + 2 * seg);
         if (cp < start_c)
            return 0;
         range = rtt__u16(f, range_a + 2 * seg);
         if (range == 0)
            return (int)((cp + rtt__u16(f, delta_a + 2 * seg)) & 0xFFFFu);
         else
         {
            uint32_t gaddr = range_a + 2 * seg + range + 2 * (cp - start_c);
            uint16_t g     = rtt__u16(f, gaddr);
            if (!g)
               return 0;
            return (int)((g + rtt__u16(f, delta_a + 2 * seg)) & 0xFFFFu);
         }
      }

      case 6:
      {
         uint32_t first = rtt__u16(f, sub + 6);
         uint32_t count = rtt__u16(f, sub + 8);
         if (cp < first || cp >= first + count)
            return 0;
         return rtt__u16(f, sub + 10 + 2 * (cp - first));
      }

      case 12:
      {
         uint32_t ngroups = rtt__u32(f, sub + 12);
         uint32_t maxg    = f->cmap_sub_len >= 16 ?
               (f->cmap_sub_len - 16) / 12u : 0;
         uint32_t lo = 0, hi;
         if (ngroups > maxg)
            ngroups = maxg;
         hi = ngroups;
         while (lo < hi)
         {
            uint32_t mid = (lo + hi) >> 1;
            if (rtt__u32(f, sub + 16 + 12 * mid + 4) < cp) /* endCharCode */
               lo = mid + 1;
            else
               hi = mid;
         }
         if (lo < ngroups)
         {
            uint32_t g = sub + 16 + 12 * lo;
            uint32_t s = rtt__u32(f, g);
            if (cp >= s && cp <= rtt__u32(f, g + 4))
               return (int)(rtt__u32(f, g + 8) + (cp - s));
         }
         return 0;
      }

      default:
         break;
   }
   return 0;
}

/* ------------------------------------------------------------------ */
/* Metrics                                                            */
/* ------------------------------------------------------------------ */

void rtt_glyph_hmetrics(const rtt_font_t *f, int gi,
      int *advance, int *lsb)
{
   int nh = f->num_hmetrics;
   if (gi < 0 || gi >= f->num_glyphs || nh <= 0)
   {
      if (advance) *advance = 0;
      if (lsb)     *lsb     = 0;
      return;
   }
   if (gi < nh)
   {
      if (advance) *advance = rtt__u16(f, f->hmtx + 4 * (uint32_t)gi);
      if (lsb)     *lsb     = (int16_t)rtt__u16(f, f->hmtx + 4 * (uint32_t)gi + 2);
   }
   else
   {
      if (advance) *advance = rtt__u16(f, f->hmtx + 4 * ((uint32_t)nh - 1));
      if (lsb)     *lsb     = (int16_t)rtt__u16(f, f->hmtx + 4 * (uint32_t)nh
            + 2 * ((uint32_t)gi - (uint32_t)nh));
   }
}

static uint32_t rtt__glyf_offset(const rtt_font_t *f, int gi, uint32_t *glen)
{
   uint32_t o0, o1;
   *glen = 0;
   if (gi < 0 || gi >= f->num_glyphs)
      return 0;
   if (f->loca_long)
   {
      o0 = rtt__u32(f, f->loca + 4 * (uint32_t)gi);
      o1 = rtt__u32(f, f->loca + 4 * (uint32_t)gi + 4);
   }
   else
   {
      o0 = 2u * rtt__u16(f, f->loca + 2 * (uint32_t)gi);
      o1 = 2u * rtt__u16(f, f->loca + 2 * (uint32_t)gi + 2);
   }
   if (o1 <= o0 || o1 > f->glyf_len)
      return 0;
   *glen = o1 - o0;
   return f->glyf + o0;
}

int rtt_glyph_box(const rtt_font_t *f, int gi,
      int *x0, int *y0, int *x1, int *y1)
{
   uint32_t glen;
   uint32_t g;
   if (f->is_cff)
   {
      /* no stored box: the control box, out to whole units */
      float bx0, by0, bx1, by1;
      if (!rtt_glyph_cbox(f, NULL, gi, &bx0, &by0, &bx1, &by1))
         return 0;
      if (x0) *x0 = (int)floor((double)bx0);
      if (y0) *y0 = (int)floor((double)by0);
      if (x1) *x1 = (int)ceil((double)bx1);
      if (y1) *y1 = (int)ceil((double)by1);
      return 1;
   }
   g = rtt__glyf_offset(f, gi, &glen);
   if (!g || glen < 10)
      return 0;
   if (x0) *x0 = (int16_t)rtt__u16(f, g + 2);
   if (y0) *y0 = (int16_t)rtt__u16(f, g + 4);
   if (x1) *x1 = (int16_t)rtt__u16(f, g + 6);
   if (y1) *y1 = (int16_t)rtt__u16(f, g + 8);
   return 1;
}

void rtt_vmetrics(const rtt_font_t *f, int *ascent, int *descent,
      int *line_gap)
{
   if (ascent)   *ascent   = (int16_t)rtt__u16(f, f->hhea + 4);
   if (descent)  *descent  = (int16_t)rtt__u16(f, f->hhea + 6);
   if (line_gap) *line_gap = (int16_t)rtt__u16(f, f->hhea + 8);
}

float rtt_scale_for_em_to_pixels(const rtt_font_t *f, float px)
{
   return px / (float)f->units_per_em;
}

/* ------------------------------------------------------------------ */
/* Rasterizer: signed-area delta accumulation.                        */
/*                                                                    */
/* For each line segment and each scanline it crosses, the exact area */
/* swept is distributed as per-cell deltas into a float buffer of     */
/* (w + 2) columns per row. A left-to-right prefix sum of a row gives */
/* the winding-weighted coverage of each pixel; |cov| clamped to 1    */
/* implements the TrueType non-zero fill rule. For cells fully inside */
/* the x-ramp of a crossing the per-cell delta is constant (dy * s);  */
/* the entry and exit cells carry the quadratic caps                  */
/* 0.5*s*(1-x0f)^2 and 0.5*s*x1f^2.                                   */
/* ------------------------------------------------------------------ */

typedef struct rtt__raster
{
   float *acc;   /* (w + 2) * h delta cells */
   int   *rowmax; /* per-row dirty extent (exclusive), for resolve */
   int    w;
   int    h;
   /* font units -> device transform (y flipped inside emit) */
   float  sx;
   float  sy;
   float  ox;
   float  oy;
   /* squared flatness: how far a flattened curve may stray */
   float  tol2;
   /* current point in device space */
   float  cx, cy;
   float  sx0, sy0; /* contour start */
} rtt__raster_t;


static void rtt__acc_line(rtt__raster_t *r, float x0, float y0,
      float x1, float y1)
{
   float dir, dxdy;
   int   y, ylast;
   int   W  = r->w;
   float fw = (float)W;

   if (y0 == y1)
      return;
   if (y0 < y1)
      dir = 1.0f;
   else
   {
      float t;
      dir = -1.0f;
      t = x0; x0 = x1; x1 = t;
      t = y0; y0 = y1; y1 = t;
   }

   if (y1 <= 0.0f || y0 >= (float)r->h)
      return;
   dxdy = (x1 - x0) / (y1 - y0);
   if (y0 < 0.0f)
   {
      x0 -= dxdy * y0;
      y0  = 0.0f;
   }
   if (y1 > (float)r->h)
   {
      x1 += dxdy * ((float)r->h - y1);
      y1  = (float)r->h;
   }

   y     = (int)y0;
   ylast = (int)ceil((double)y1) - 1;
   if (ylast >= r->h)
      ylast = r->h - 1;

   for (; y <= ylast; y++)
   {
      float *row = r->acc + (size_t)y * (size_t)(W + 2);
      float xa, xb, dy, xl, xr, d;
      float ytop = (float)y;
      float ybot = ytop + 1.0f;
      int   ixl, ixr;

      if (ytop < y0) ytop = y0;
      if (ybot > y1) ybot = y1;
      dy = ybot - ytop;
      if (dy <= 0.0f)
         continue;

      xa = x0 + dxdy * (ytop - y0);
      xb = x0 + dxdy * (ybot - y0);

      if (xa < xb) { xl = xa; xr = xb; }
      else         { xl = xb; xr = xa; }

      /* horizontal clamp: geometry outside collapses onto the border,
       * matching a polygon clip for coverage purposes */
      if (xl < 0.0f) xl = 0.0f;
      if (xr < 0.0f) xr = 0.0f;
      if (xl > fw)   xl = fw;
      if (xr > fw)   xr = fw;

      d   = dir * dy;
      ixl = (int)xl;
      ixr = (int)xr;
      if (ixl > W - 1) ixl = W - 1;
      if (ixr > W - 1) ixr = W - 1;

      if (ixr + 2 > r->rowmax[y])
         r->rowmax[y] = ixr + 2;

      if (ixl == ixr)
      {
         float xmf = 0.5f * (xl + xr) - (float)ixl;
         row[ixl]     += d * (1.0f - xmf);
         row[ixl + 1] += d * xmf;
      }
      else
      {
         float s   = 1.0f / (xr - xl);
         float x0f = xl - (float)ixl;
         float x1f = xr - (float)ixr;
         float a0  = 0.5f * s * (1.0f - x0f) * (1.0f - x0f);
         float am  = 0.5f * s * x1f * x1f;

         row[ixl] += d * a0;
         if (ixr == ixl + 1)
            row[ixr] += d * (1.0f - a0 - am);
         else
         {
            float a1 = s * (1.5f - x0f);
            float a2 = a1 + (float)(ixr - ixl - 2) * s;
            int   ix;
            row[ixl + 1] += d * (a1 - a0);
            for (ix = ixl + 2; ix < ixr; ix++)
               row[ix] += d * s;
            row[ixr] += d * (1.0f - a2 - am);
         }
         row[ixr + 1] += d * am;
      }
   }
}

/* Resolve one row of deltas to 8-bit coverage. The ONLY 8-bit specific
 * code in the rasterizer; a 10-bit HDR output path adds a sibling of
 * this routine and nothing else.
 *
 * rtt__resolve_row_u8() is defined once per architecture below (SSE2,
 * NEON, scalar). All variants sum with the same fixed block-of-4
 * association tree, so every build produces byte-identical output. */
static uint8_t rtt__cov_to_u8(float v)
{
   float c = v < 0.0f ? -v : v;
   if (c > 1.0f)
      c = 1.0f;
   return (uint8_t)(c * 255.0f + 0.5f);
}

static uint16_t rtt__cov_to_u16(float v)
{
   float c = v < 0.0f ? -v : v;
   if (c > 1.0f)
      c = 1.0f;
   return (uint16_t)(c * 65535.0f + 0.5f);
}

#if defined(__SSE2__) || (defined(_MSC_VER) && (defined(_M_X64) || (defined(_M_IX86_FP) && _M_IX86_FP >= 2)))
#include <emmintrin.h>

static void rtt__resolve_row_u8(const float *acc, uint8_t *dst, int w)
{
   __m128 carry = _mm_setzero_ps();
   __m128 sign  = _mm_castsi128_ps(_mm_set1_epi32((int)0x80000000u));
   __m128 one   = _mm_set1_ps(1.0f);
   __m128 k255  = _mm_set1_ps(255.0f);
   __m128 half  = _mm_set1_ps(0.5f);
   int i = 0;

   for (; i + 4 <= w; i += 4)
   {
      __m128 x = _mm_loadu_ps(acc + i);
      __m128i q;
      /* 4-lane inclusive prefix sum */
      x = _mm_add_ps(x, _mm_castsi128_ps(
            _mm_slli_si128(_mm_castps_si128(x), 4)));
      x = _mm_add_ps(x, _mm_castsi128_ps(
            _mm_slli_si128(_mm_castps_si128(x), 8)));
      x = _mm_add_ps(x, carry);
      carry = _mm_shuffle_ps(x, x, _MM_SHUFFLE(3, 3, 3, 3));

      /* |sum| clamped to 1, scaled; truncate(x*255+0.5) matches the
       * scalar (uint8_t)(c*255.0f + 0.5f) exactly */
      x = _mm_andnot_ps(sign, x);
      x = _mm_min_ps(x, one);
      q = _mm_cvttps_epi32(_mm_add_ps(_mm_mul_ps(x, k255), half));
      q = _mm_packs_epi32(q, q);
      q = _mm_packus_epi16(q, q);
      {
         int32_t v4 = _mm_cvtsi128_si32(q);
         memcpy(dst + i, &v4, 4);
      }
   }

   if (i < w)
   {
      float sum = _mm_cvtss_f32(carry);
      for (; i < w; i++)
      {
         sum   += acc[i];
         dst[i] = rtt__cov_to_u8(sum);
      }
   }
}


/* 16-bit resolve sibling for A16 (HDR) atlases; identical summation
 * tree, quantized to 65535 with an unsigned saturating 16-bit pack. */
static void rtt__resolve_row_u16(const float *acc, uint16_t *dst, int w)
{
   __m128 carry = _mm_setzero_ps();
   __m128 sign  = _mm_castsi128_ps(_mm_set1_epi32((int)0x80000000u));
   __m128 one   = _mm_set1_ps(1.0f);
   __m128 kmax  = _mm_set1_ps(65535.0f);
   __m128 half  = _mm_set1_ps(0.5f);
   int i = 0;

   for (; i + 4 <= w; i += 4)
   {
      __m128 x = _mm_loadu_ps(acc + i);
      __m128i q;
      x = _mm_add_ps(x, _mm_castsi128_ps(
            _mm_slli_si128(_mm_castps_si128(x), 4)));
      x = _mm_add_ps(x, _mm_castsi128_ps(
            _mm_slli_si128(_mm_castps_si128(x), 8)));
      x = _mm_add_ps(x, carry);
      carry = _mm_shuffle_ps(x, x, _MM_SHUFFLE(3, 3, 3, 3));

      x = _mm_andnot_ps(sign, x);
      x = _mm_min_ps(x, one);
      q = _mm_cvttps_epi32(_mm_add_ps(_mm_mul_ps(x, kmax), half));
      /* unsigned 16-bit saturating pack: bias into signed range */
      q = _mm_sub_epi32(q, _mm_set1_epi32(32768));
      q = _mm_packs_epi32(q, q);
      q = _mm_add_epi16(q, _mm_set1_epi16((short)0x8000));
      {
         int64_t v8;
         memcpy(&v8, &q, 8);
         memcpy(dst + i, &v8, 8);
      }
   }

   if (i < w)
   {
      float sum = _mm_cvtss_f32(carry);
      for (; i < w; i++)
      {
         sum   += acc[i];
         dst[i] = rtt__cov_to_u16(sum);
      }
   }
}

#elif defined(__ARM_NEON) || defined(__ARM_NEON__)
#include <arm_neon.h>

static void rtt__resolve_row_u8(const float *acc, uint8_t *dst, int w)
{
   float32x4_t carry = vdupq_n_f32(0.0f);
   float32x4_t one   = vdupq_n_f32(1.0f);
   float32x4_t k255  = vdupq_n_f32(255.0f);
   float32x4_t half  = vdupq_n_f32(0.5f);
   float32x4_t zero  = vdupq_n_f32(0.0f);
   int i = 0;

   for (; i + 4 <= w; i += 4)
   {
      float32x4_t x = vld1q_f32(acc + i);
      uint32x4_t  q;
      uint16x4_t  q16;
      uint8x8_t   q8;
      /* 4-lane inclusive prefix sum via lane shifts */
      x = vaddq_f32(x, vextq_f32(zero, x, 3));
      x = vaddq_f32(x, vextq_f32(zero, x, 2));
      x = vaddq_f32(x, carry);
      carry = vdupq_n_f32(vgetq_lane_f32(x, 3));

      x   = vabsq_f32(x);
      x   = vminq_f32(x, one);
      /* vcvtq truncates, matching the scalar cast */
      q   = vcvtq_u32_f32(vaddq_f32(vmulq_f32(x, k255), half));
      q16 = vmovn_u32(q);
      q8  = vmovn_u16(vcombine_u16(q16, q16));
      {
         uint32_t v4 = vget_lane_u32(vreinterpret_u32_u8(q8), 0);
         memcpy(dst + i, &v4, 4);
      }
   }

   if (i < w)
   {
      float sum = vgetq_lane_f32(carry, 0);
      for (; i < w; i++)
      {
         sum   += acc[i];
         dst[i] = rtt__cov_to_u8(sum);
      }
   }
}


/* 16-bit resolve sibling for A16 (HDR) atlases */
static void rtt__resolve_row_u16(const float *acc, uint16_t *dst, int w)
{
   float32x4_t carry = vdupq_n_f32(0.0f);
   float32x4_t one   = vdupq_n_f32(1.0f);
   float32x4_t kmax  = vdupq_n_f32(65535.0f);
   float32x4_t half  = vdupq_n_f32(0.5f);
   float32x4_t zero  = vdupq_n_f32(0.0f);
   int i = 0;

   for (; i + 4 <= w; i += 4)
   {
      float32x4_t x = vld1q_f32(acc + i);
      uint32x4_t  q;
      x = vaddq_f32(x, vextq_f32(zero, x, 3));
      x = vaddq_f32(x, vextq_f32(zero, x, 2));
      x = vaddq_f32(x, carry);
      carry = vdupq_n_f32(vgetq_lane_f32(x, 3));

      x = vabsq_f32(x);
      x = vminq_f32(x, one);
      q = vcvtq_u32_f32(vaddq_f32(vmulq_f32(x, kmax), half));
      vst1_u16(dst + i, vmovn_u32(q));
   }

   if (i < w)
   {
      float sum = vgetq_lane_f32(carry, 0);
      for (; i < w; i++)
      {
         sum   += acc[i];
         dst[i] = rtt__cov_to_u16(sum);
      }
   }
}

#else

static void rtt__resolve_row_u8(const float *acc, uint8_t *dst, int w)
{
   float carry = 0.0f;
   int   i = 0;

   for (; i + 4 <= w; i += 4)
   {
      /* identical association to the 4-lane prefix sum:
       * t1=x1+x0 t2=x2+x1 t3=x3+x2; u2=t2+x0 u3=t3+t1; +carry */
      float x0 = acc[i],     x1 = acc[i + 1];
      float x2 = acc[i + 2], x3 = acc[i + 3];
      float t1 = x1 + x0;
      float t2 = x2 + x1;
      float t3 = x3 + x2;
      float u2 = t2 + x0;
      float u3 = t3 + t1;
      dst[i]     = rtt__cov_to_u8(x0 + carry);
      dst[i + 1] = rtt__cov_to_u8(t1 + carry);
      dst[i + 2] = rtt__cov_to_u8(u2 + carry);
      dst[i + 3] = rtt__cov_to_u8(u3 + carry);
      carry      = u3 + carry;
   }
   for (; i < w; i++)
   {
      carry += acc[i];
      dst[i] = rtt__cov_to_u8(carry);
   }
}


/* 16-bit resolve sibling for A16 (HDR) atlases */
static void rtt__resolve_row_u16(const float *acc, uint16_t *dst, int w)
{
   float carry = 0.0f;
   int   i = 0;

   for (; i + 4 <= w; i += 4)
   {
      float x0 = acc[i],     x1 = acc[i + 1];
      float x2 = acc[i + 2], x3 = acc[i + 3];
      float t1 = x1 + x0;
      float t2 = x2 + x1;
      float t3 = x3 + x2;
      float u2 = t2 + x0;
      float u3 = t3 + t1;
      dst[i]     = rtt__cov_to_u16(x0 + carry);
      dst[i + 1] = rtt__cov_to_u16(t1 + carry);
      dst[i + 2] = rtt__cov_to_u16(u2 + carry);
      dst[i + 3] = rtt__cov_to_u16(u3 + carry);
      carry      = u3 + carry;
   }
   for (; i < w; i++)
   {
      carry += acc[i];
      dst[i] = rtt__cov_to_u16(carry);
   }
}

#endif

/* ------------------------------------------------------------------ */
/* Outline loading / flattening                                       */
/* ------------------------------------------------------------------ */

static INLINE void rtt__emit_move(rtt__raster_t *r, float x, float y)
{
   /* close previous contour if left open */
   if (r->cx != r->sx0 || r->cy != r->sy0)
      rtt__acc_line(r, r->cx, r->cy, r->sx0, r->sy0);
   r->cx = r->sx0 = x;
   r->cy = r->sy0 = y;
}

static INLINE void rtt__emit_line(rtt__raster_t *r, float x, float y)
{
   rtt__acc_line(r, r->cx, r->cy, x, y);
   r->cx = x;
   r->cy = y;
}

/* Flatten a quadratic bezier (device space) with subdivision count
 * derived from its deviation from the chord. */
static void rtt__emit_quad(rtt__raster_t *r, float cx, float cy,
      float x, float y)
{
   float x0  = r->cx, y0 = r->cy;
   float ddx = x0 - 2.0f * cx + x;
   float ddy = y0 - 2.0f * cy + y;
   /* squared midpoint deviation of the curve from its chord is
    * (ddx^2 + ddy^2) / 16; each midpoint subdivision divides it by 16.
    * Subdivide until it is within the flatness, 0.35px unless the
    * caller asked for finer. */
   float md2 = (ddx * ddx + ddy * ddy) * (1.0f / 16.0f);
   float inv;
   int   n   = 1;
   int   i;

   while (md2 > r->tol2 && n < 256)
   {
      md2 *= (1.0f / 16.0f);
      n  <<= 1;
   }

   /* n is a power of two, so the reciprocal is exact */
   inv = 1.0f / (float)n;
   for (i = 1; i < n; i++)
   {
      float t  = (float)i * inv;
      float u  = 1.0f - t;
      float qx = u * u * x0 + 2.0f * u * t * cx + t * t * x;
      float qy = u * u * y0 + 2.0f * u * t * cy + t * t * y;
      rtt__emit_line(r, qx, qy);
   }
   rtt__emit_line(r, x, y);
}

/* Flatten a cubic bezier (device space). The chord deviation is at
 * most 3/4 of the larger second difference; split until each piece is
 * within the same 0.35px as a quadratic. */
static void rtt__emit_cubic(rtt__raster_t *r, float c1x, float c1y,
      float c2x, float c2y, float x, float y)
{
   float x0  = r->cx, y0 = r->cy;
   float ax  = x0 - 2.0f * c1x + c2x, ay = y0 - 2.0f * c1y + c2y;
   float bx  = c1x - 2.0f * c2x + x,  by = c1y - 2.0f * c2y + y;
   float da  = ax * ax + ay * ay;
   float db  = bx * bx + by * by;
   float md2 = (da > db ? da : db) * (9.0f / 16.0f);
   float inv;
   int   n   = 1;
   int   i;

   while (md2 > r->tol2 && n < 256)
   {
      md2 *= (1.0f / 16.0f);
      n  <<= 1;
   }

   /* n is a power of two, so the reciprocal is exact */
   inv = 1.0f / (float)n;
   for (i = 1; i < n; i++)
   {
      float t  = (float)i * inv;
      float u  = 1.0f - t;
      float w0 = u * u * u, w1 = 3.0f * u * u * t;
      float w2 = 3.0f * u * t * t, w3 = t * t * t;
      rtt__emit_line(r, w0 * x0 + w1 * c1x + w2 * c2x + w3 * x,
            w0 * y0 + w1 * c1y + w2 * c2y + w3 * y);
   }
   rtt__emit_line(r, x, y);
}

/* 2x3 transform (composite glyphs): device = M * unit_point + offset */
typedef struct
{
   float a, b, c, d; /* [a b; c d] */
   float dx, dy;     /* offset in font units */
} rtt__xform_t;

static INLINE void rtt__xf_point(const rtt__xform_t *m, float x, float y,
      float *ox, float *oy)
{
   *ox = m->a * x + m->c * y + m->dx;
   *oy = m->b * x + m->d * y + m->dy;
}

/* Convert a font-unit point through the component transform chain and
 * the device scale/flip into device space, then emit. */
static INLINE void rtt__dev(const rtt__raster_t *r, const rtt__xform_t *m,
      float ux, float uy, float *dx, float *dy)
{
   float tx, ty;
   rtt__xf_point(m, ux, uy, &tx, &ty);
   *dx =  tx * r->sx - r->ox;
   *dy = -ty * r->sy - r->oy;
}

/* Where an outline goes: into a raster (device space), to a caller's
 * sink, or into a control box (both in font units). Points are carried
 * through rtt__pen_pt first, so implied TrueType midpoints are taken in
 * the space they are drawn in. */
typedef struct rtt__pen
{
   rtt__raster_t          *r;
   const rtt_outline_sink *sink;
   /* control box, when neither of the above */
   float bx0, by0, bx1, by1;
   int   any;
   /* point scratch for simple glyphs: flags and coordinates for up to
    * pts_cap points */
   uint8_t *pts_flags;
   float   *pts_xs;
   float   *pts_ys;
   int      pts_cap;
} rtt__pen_t;

static INLINE void rtt__pen_pt(const rtt__pen_t *p, const rtt__xform_t *m,
      float ux, float uy, float *x, float *y)
{
   if (p->r)
      rtt__dev(p->r, m, ux, uy, x, y);
   else
      rtt__xf_point(m, ux, uy, x, y);
}

static void rtt__pen_box(rtt__pen_t *p, float x, float y)
{
   if (!p->any)
   {
      p->bx0 = p->bx1 = x;
      p->by0 = p->by1 = y;
      p->any = 1;
      return;
   }
   if (x < p->bx0) p->bx0 = x;
   if (x > p->bx1) p->bx1 = x;
   if (y < p->by0) p->by0 = y;
   if (y > p->by1) p->by1 = y;
}

static INLINE void rtt__pen_move(rtt__pen_t *p, float x, float y)
{
   if (p->r)
      rtt__emit_move(p->r, x, y);
   else if (p->sink)
      p->sink->move_to(p->sink->user, x, y);
   else
      rtt__pen_box(p, x, y);
}

static INLINE void rtt__pen_line(rtt__pen_t *p, float x, float y)
{
   if (p->r)
      rtt__emit_line(p->r, x, y);
   else if (p->sink)
      p->sink->line_to(p->sink->user, x, y);
   else
      rtt__pen_box(p, x, y);
}

static INLINE void rtt__pen_quad(rtt__pen_t *p, float cx, float cy, float x, float y)
{
   if (p->r)
      rtt__emit_quad(p->r, cx, cy, x, y);
   else if (p->sink)
      p->sink->quad_to(p->sink->user, cx, cy, x, y);
   else
   {
      rtt__pen_box(p, cx, cy);
      rtt__pen_box(p, x, y);
   }
}

static INLINE void rtt__pen_cubic(rtt__pen_t *p, float c1x, float c1y,
      float c2x, float c2y, float x, float y)
{
   if (p->r)
      rtt__emit_cubic(p->r, c1x, c1y, c2x, c2y, x, y);
   else if (p->sink)
      p->sink->cubic_to(p->sink->user, c1x, c1y, c2x, c2y, x, y);
   else
   {
      rtt__pen_box(p, c1x, c1y);
      rtt__pen_box(p, c2x, c2y);
      rtt__pen_box(p, x, y);
   }
}

/* ------------------------------------------------------------------ */
/* Type 2 charstrings (Adobe TN 5177)                                 */
/* ------------------------------------------------------------------ */

#define RTT_CS_STACK 48
#define RTT_CS_DEPTH 10

typedef struct rtt__cs
{
   const rtt_font_t   *f;
   rtt__pen_t         *pen;
   rtt__xform_t        ident;
   rtt_cff_index_t     subrs;
   float st[RTT_CS_STACK];
   float trans[32];
   float x, y;
   int   sp;
   int   nstems;
   int   width_done;
   int   open;
   int   ended;
} rtt__cs_t;

static int32_t rtt__cs_bias(uint32_t count)
{
   return count < 1240 ? 107 : count < 33900 ? 1131 : 32768;
}

/* Drops the width that may come first, given how many operands the
 * operator takes: odd for stem hints, or more than @need. */
static void rtt__cs_width(rtt__cs_t *cs, int has_extra)
{
   if (!cs->width_done)
   {
      cs->width_done = 1;
      if (has_extra && cs->sp > 0)
      {
         int i;
         for (i = 1; i < cs->sp; i++)
            cs->st[i - 1] = cs->st[i];
         cs->sp--;
      }
   }
}

static void rtt__cs_point(rtt__cs_t *cs, float x, float y,
      float *ox, float *oy)
{
   rtt__pen_pt(cs->pen, &cs->ident, x, y, ox, oy);
}

static void rtt__cs_move(rtt__cs_t *cs, float dx, float dy)
{
   float px, py;
   cs->x += dx;
   cs->y += dy;
   rtt__cs_point(cs, cs->x, cs->y, &px, &py);
   rtt__pen_move(cs->pen, px, py);
   cs->open = 1;
}

static void rtt__cs_line(rtt__cs_t *cs, float dx, float dy)
{
   float px, py;
   if (!cs->open)
      rtt__cs_move(cs, 0.0f, 0.0f);
   cs->x += dx;
   cs->y += dy;
   rtt__cs_point(cs, cs->x, cs->y, &px, &py);
   rtt__pen_line(cs->pen, px, py);
}

static void rtt__cs_curve(rtt__cs_t *cs, float dx1, float dy1,
      float dx2, float dy2, float dx3, float dy3)
{
   float ax, ay, bx, by, cx, cy;
   float x1, y1, x2, y2;
   if (!cs->open)
      rtt__cs_move(cs, 0.0f, 0.0f);
   x1 = cs->x + dx1; y1 = cs->y + dy1;
   x2 = x1 + dx2;    y2 = y1 + dy2;
   cs->x = x2 + dx3; cs->y = y2 + dy3;
   rtt__cs_point(cs, x1, y1, &ax, &ay);
   rtt__cs_point(cs, x2, y2, &bx, &by);
   rtt__cs_point(cs, cs->x, cs->y, &cx, &cy);
   rtt__pen_cubic(cs->pen, ax, ay, bx, by, cx, cy);
}

static void rtt__cs_hints(rtt__cs_t *cs)
{
   rtt__cs_width(cs, cs->sp & 1);
   cs->nstems += cs->sp / 2;
   cs->sp      = 0;
}

/* Runs one charstring or subroutine; returns 0 on malformed data. */
static int rtt__cs_run(rtt__cs_t *cs, uint32_t p, uint32_t end, int depth)
{
   const rtt_font_t *f = cs->f;
   float *st = cs->st;

   if (depth > RTT_CS_DEPTH)
      return 0;

   while (p < end && !cs->ended)
   {
      unsigned b = rtt__u8(f, p++);
      int i;

      if (b >= 32 || b == 28)
      {
         float v;
         if (b == 28)
         {
            v  = (float)(int16_t)rtt__u16(f, p);
            p += 2;
         }
         else if (b <= 246)
            v = (float)((int)b - 139);
         else if (b <= 250)
            v = (float)(((int)b - 247) * 256 + rtt__u8(f, p++) + 108);
         else if (b <= 254)
            v = (float)(-((int)b - 251) * 256 - rtt__u8(f, p++) - 108);
         else
         {
            v  = (float)((double)(int32_t)rtt__u32(f, p) / 65536.0);
            p += 4;
         }
         if (cs->sp >= RTT_CS_STACK)
            return 0;
         st[cs->sp++] = v;
         continue;
      }

      switch (b)
      {
         case 1:  /* hstem */
         case 3:  /* vstem */
         case 18: /* hstemhm */
         case 23: /* vstemhm */
            rtt__cs_hints(cs);
            break;

         case 19: /* hintmask */
         case 20: /* cntrmask */
            /* stems given right before the mask are vstems */
            rtt__cs_hints(cs);
            p += (uint32_t)(cs->nstems + 7) / 8;
            break;

         case 21: /* rmoveto */
            rtt__cs_width(cs, cs->sp > 2);
            if (cs->sp < 2)
               return 0;
            rtt__cs_move(cs, st[cs->sp - 2], st[cs->sp - 1]);
            cs->sp = 0;
            break;
         case 22: /* hmoveto */
            rtt__cs_width(cs, cs->sp > 1);
            if (cs->sp < 1)
               return 0;
            rtt__cs_move(cs, st[cs->sp - 1], 0.0f);
            cs->sp = 0;
            break;
         case 4:  /* vmoveto */
            rtt__cs_width(cs, cs->sp > 1);
            if (cs->sp < 1)
               return 0;
            rtt__cs_move(cs, 0.0f, st[cs->sp - 1]);
            cs->sp = 0;
            break;

         case 5:  /* rlineto */
            for (i = 0; i + 1 < cs->sp; i += 2)
               rtt__cs_line(cs, st[i], st[i + 1]);
            cs->sp = 0;
            break;
         case 6:  /* hlineto */
         case 7:  /* vlineto */
         {
            int horiz = (b == 6);
            for (i = 0; i < cs->sp; i++, horiz = !horiz)
               if (horiz)
                  rtt__cs_line(cs, st[i], 0.0f);
               else
                  rtt__cs_line(cs, 0.0f, st[i]);
            cs->sp = 0;
            break;
         }

         case 8:  /* rrcurveto */
            for (i = 0; i + 5 < cs->sp; i += 6)
               rtt__cs_curve(cs, st[i], st[i + 1], st[i + 2], st[i + 3],
                     st[i + 4], st[i + 5]);
            cs->sp = 0;
            break;
         case 24: /* rcurveline */
            for (i = 0; i + 5 < cs->sp - 2; i += 6)
               rtt__cs_curve(cs, st[i], st[i + 1], st[i + 2], st[i + 3],
                     st[i + 4], st[i + 5]);
            if (i + 1 < cs->sp)
               rtt__cs_line(cs, st[i], st[i + 1]);
            cs->sp = 0;
            break;
         case 25: /* rlinecurve */
            for (i = 0; i + 1 < cs->sp - 6; i += 2)
               rtt__cs_line(cs, st[i], st[i + 1]);
            if (i + 5 < cs->sp)
               rtt__cs_curve(cs, st[i], st[i + 1], st[i + 2], st[i + 3],
                     st[i + 4], st[i + 5]);
            cs->sp = 0;
            break;
         case 26: /* vvcurveto: dx1? {dya dxb dyb dyc}+ */
         {
            float dx1 = 0.0f;
            i = 0;
            if (cs->sp & 1)
               dx1 = st[i++];
            for (; i + 3 < cs->sp; i += 4)
            {
               rtt__cs_curve(cs, dx1, st[i], st[i + 1], st[i + 2],
                     0.0f, st[i + 3]);
               dx1 = 0.0f;
            }
            cs->sp = 0;
            break;
         }
         case 27: /* hhcurveto: dy1? {dxa dxb dyb dxc}+ */
         {
            float dy1 = 0.0f;
            i = 0;
            if (cs->sp & 1)
               dy1 = st[i++];
            for (; i + 3 < cs->sp; i += 4)
            {
               rtt__cs_curve(cs, st[i], dy1, st[i + 1], st[i + 2],
                     st[i + 3], 0.0f);
               dy1 = 0.0f;
            }
            cs->sp = 0;
            break;
         }
         case 30: /* vhcurveto */
         case 31: /* hvcurveto */
         {
            /* alternating; the last curve may take one more operand */
            int horiz = (b == 31);
            for (i = 0; i + 3 < cs->sp; i += 4, horiz = !horiz)
            {
               float last = (i + 5 == cs->sp) ? st[i + 4] : 0.0f;
               if (horiz)
                  rtt__cs_curve(cs, st[i], 0.0f, st[i + 1], st[i + 2],
                        last, st[i + 3]);
               else
                  rtt__cs_curve(cs, 0.0f, st[i], st[i + 1], st[i + 2],
                        st[i + 3], last);
            }
            cs->sp = 0;
            break;
         }

         case 10: /* callsubr */
         case 29: /* callgsubr */
         {
            const rtt_cff_index_t *idx = (b == 10) ? &cs->subrs
                  : &f->cff_gsubrs;
            uint32_t s, sl;
            int32_t  n;
            if (cs->sp < 1)
               return 0;
            n = (int32_t)st[--cs->sp] + rtt__cs_bias(idx->count);
            if (n < 0 || !rtt__cff_item(f, idx, (uint32_t)n, &s, &sl))
               return 0;
            if (!rtt__cs_run(cs, s, s + sl, depth + 1))
               return 0;
            break;
         }
         case 11: /* return */
            return 1;

         case 14: /* endchar */
            rtt__cs_width(cs, cs->sp == 1 || cs->sp == 5);
            /* seac's accented composition (4 operands) is not drawn */
            cs->ended = 1;
            cs->sp    = 0;
            break;

         case 12:
         {
            unsigned op = rtt__u8(f, p++);
            float    a, c;
            switch (op)
            {
               case 35: /* flex */
                  if (cs->sp < 13)
                     return 0;
                  rtt__cs_curve(cs, st[0], st[1], st[2], st[3], st[4], st[5]);
                  rtt__cs_curve(cs, st[6], st[7], st[8], st[9], st[10], st[11]);
                  break;
               case 34: /* hflex */
                  if (cs->sp < 7)
                     return 0;
                  rtt__cs_curve(cs, st[0], 0.0f, st[1], st[2], st[3], 0.0f);
                  rtt__cs_curve(cs, st[4], 0.0f, st[5], -st[2], st[6], 0.0f);
                  break;
               case 36: /* hflex1 */
                  if (cs->sp < 9)
                     return 0;
                  rtt__cs_curve(cs, st[0], st[1], st[2], st[3], st[4], 0.0f);
                  rtt__cs_curve(cs, st[5], 0.0f, st[6], st[7], st[8],
                        -(st[1] + st[3] + st[7]));
                  break;
               case 37: /* flex1 */
               {
                  float dx, dy;
                  if (cs->sp < 11)
                     return 0;
                  dx = st[0] + st[2] + st[4] + st[6] + st[8];
                  dy = st[1] + st[3] + st[5] + st[7] + st[9];
                  rtt__cs_curve(cs, st[0], st[1], st[2], st[3], st[4], st[5]);
                  if ((dx < 0 ? -dx : dx) > (dy < 0 ? -dy : dy))
                     rtt__cs_curve(cs, st[6], st[7], st[8], st[9],
                           st[10], -dy);
                  else
                     rtt__cs_curve(cs, st[6], st[7], st[8], st[9],
                           -dx, st[10]);
                  break;
               }

               /* arithmetic and storage, rare in fonts */
               case 9:  /* abs */
                  if (cs->sp >= 1 && st[cs->sp - 1] < 0)
                     st[cs->sp - 1] = -st[cs->sp - 1];
                  continue;
               case 10: /* add */
               case 11: /* sub */
               case 12: /* div */
               case 24: /* mul */
               case 15: /* eq */
               case 3:  /* and */
               case 4:  /* or */
                  if (cs->sp < 2)
                     return 0;
                  a = st[cs->sp - 2];
                  c = st[cs->sp - 1];
                  cs->sp--;
                  st[cs->sp - 1] =
                        op == 10 ? a + c
                      : op == 11 ? a - c
                      : op == 12 ? (c != 0.0f ? a / c : 0.0f)
                      : op == 24 ? a * c
                      : op == 15 ? (float)(a == c)
                      : op == 3  ? (float)(a != 0.0f && c != 0.0f)
                      :            (float)(a != 0.0f || c != 0.0f);
                  continue;
               case 5:  /* not */
                  if (cs->sp >= 1)
                     st[cs->sp - 1] = (float)(st[cs->sp - 1] == 0.0f);
                  continue;
               case 14: /* neg */
                  if (cs->sp >= 1)
                     st[cs->sp - 1] = -st[cs->sp - 1];
                  continue;
               case 26: /* sqrt */
                  if (cs->sp >= 1 && st[cs->sp - 1] > 0)
                     st[cs->sp - 1] = (float)sqrt((double)st[cs->sp - 1]);
                  continue;
               case 18: /* drop */
                  if (cs->sp >= 1)
                     cs->sp--;
                  continue;
               case 27: /* dup */
                  if (cs->sp >= 1 && cs->sp < RTT_CS_STACK)
                  {
                     st[cs->sp] = st[cs->sp - 1];
                     cs->sp++;
                  }
                  continue;
               case 28: /* exch */
                  if (cs->sp >= 2)
                  {
                     a = st[cs->sp - 1];
                     st[cs->sp - 1] = st[cs->sp - 2];
                     st[cs->sp - 2] = a;
                  }
                  continue;
               case 29: /* index */
                  if (cs->sp >= 1)
                  {
                     int k = (int)st[cs->sp - 1];
                     if (k < 0)
                        k = 0;
                     st[cs->sp - 1] = (k < cs->sp - 1)
                           ? st[cs->sp - 2 - k] : 0.0f;
                  }
                  continue;
               case 30: /* roll */
                  if (cs->sp >= 2)
                  {
                     int n = (int)st[cs->sp - 2];
                     int j = (int)st[cs->sp - 1];
                     cs->sp -= 2;
                     if (n > 0 && n <= cs->sp)
                     {
                        float tmp[RTT_CS_STACK];
                        int   base = cs->sp - n, k;
                        j %= n;
                        if (j < 0)
                           j += n;
                        for (k = 0; k < n; k++)
                           tmp[(k + j) % n] = st[base + k];
                        for (k = 0; k < n; k++)
                           st[base + k] = tmp[k];
                     }
                  }
                  continue;
               case 20: /* put */
                  if (cs->sp >= 2)
                  {
                     int k = (int)st[cs->sp - 1];
                     if (k >= 0 && k < 32)
                        cs->trans[k] = st[cs->sp - 2];
                     cs->sp -= 2;
                  }
                  continue;
               case 21: /* get */
                  if (cs->sp >= 1)
                  {
                     int k = (int)st[cs->sp - 1];
                     st[cs->sp - 1] = (k >= 0 && k < 32) ? cs->trans[k] : 0.0f;
                  }
                  continue;
               case 22: /* ifelse */
                  if (cs->sp >= 4)
                  {
                     float v = st[cs->sp - 2] <= st[cs->sp - 1]
                           ? st[cs->sp - 4] : st[cs->sp - 3];
                     cs->sp -= 3;
                     st[cs->sp - 1] = v;
                  }
                  continue;
               case 23: /* random: any value in (0, 1] serves */
                  if (cs->sp < RTT_CS_STACK)
                     st[cs->sp++] = 0.5f;
                  continue;
               default:
                  break;
            }
            cs->sp = 0;
            break;
         }

         default:
            /* reserved operators clear the stack */
            cs->sp = 0;
            break;
      }
   }
   return 1;
}

/* Walks CFF glyph @gi into @pen. */
static int rtt__walk_cff(const rtt_font_t *f, rtt__pen_t *pen, int gi)
{
   rtt__cs_t cs;
   uint32_t  s, sl;
   if (gi < 0 || !rtt__cff_item(f, &f->cff_charstrings, (uint32_t)gi, &s, &sl))
      return 0;
   memset(&cs, 0, sizeof(cs));
   cs.f       = f;
   cs.pen     = pen;
   cs.ident.a = cs.ident.d = 1.0f;
   rtt__cff_glyph_subrs(f, gi, &cs.subrs);
   return rtt__cs_run(&cs, s, s + sl, 0);
}

/* A simple glyph's flags and coordinates, from bytes known to hold
 * them: 5 bytes a point at most. Returns the bytes read. */
static uint32_t rtt__glyf_points_fast(const uint8_t *q, int npts,
      uint8_t *flags, float *xs, float *ys)
{
   const uint8_t *q0 = q;
   int i, v;
   for (i = 0; i < npts; )
   {
      uint8_t fl = *q++;
      flags[i++] = fl;
      if (fl & 0x08) /* REPEAT */
      {
         uint8_t rep = *q++;
         while (rep-- && i < npts)
            flags[i++] = fl;
      }
   }
   for (i = 0, v = 0; i < npts; i++)
   {
      uint8_t fl = flags[i];
      if (fl & 0x02) /* X_SHORT */
      {
         v += (fl & 0x10) ? (int)q[0] : -(int)q[0];
         q++;
      }
      else if (!(fl & 0x10))
      {
         v += (int16_t)((q[0] << 8) | q[1]);
         q += 2;
      }
      xs[i] = (float)v;
   }
   for (i = 0, v = 0; i < npts; i++)
   {
      uint8_t fl = flags[i];
      if (fl & 0x04) /* Y_SHORT */
      {
         v += (fl & 0x20) ? (int)q[0] : -(int)q[0];
         q++;
      }
      else if (!(fl & 0x20))
      {
         v += (int16_t)((q[0] << 8) | q[1]);
         q += 2;
      }
      ys[i] = (float)v;
   }
   return (uint32_t)(q - q0);
}

/* Walk one glyph's outline, emitting flattened segments. Returns 0 on
 * malformed data (treated as empty). */
static int rtt__walk_glyph(const rtt_font_t *f, rtt__pen_t *r,
      int gi, const rtt__xform_t *m, int depth)
{
   uint32_t glen;
   uint32_t g = rtt__glyf_offset(f, gi, &glen);
   int ncont;

   if (!g || glen < 10 || depth > RTT_MAX_COMPOSITE_DEPTH)
      return 0;

   ncont = (int16_t)rtt__u16(f, g);

   if (ncont >= 0)
   {
      /* ---- simple glyph ---- */
      uint32_t endpts = g + 10;
      uint32_t inslen, p;
      int npts, i, c;
      uint8_t *flags = NULL;
      float   *xs    = NULL;
      float   *ys    = NULL;
      uint8_t *own   = NULL;
      int ok = 0;

      if (ncont == 0)
         return 1;
      npts = (int)rtt__u16(f, endpts + 2 * ((uint32_t)ncont - 1)) + 1;
      if (npts <= 0 || npts > 0xFFFF)
         return 0;

      inslen = rtt__u16(f, g + 10 + 2 * (uint32_t)ncont);
      p      = g + 12 + 2 * (uint32_t)ncont + inslen;

      if (npts <= r->pts_cap)
      {
         /* the render's block already holds room for this glyph */
         flags = r->pts_flags;
         xs    = r->pts_xs;
         ys    = r->pts_ys;
      }
      else
      {
         /* a glyph beyond what maxp promised: one block of its own */
         size_t o_xs = ((size_t)npts + 63) & ~(size_t)63;
         size_t o_ys = o_xs + ((sizeof(float) * (size_t)npts + 63) & ~(size_t)63);
         own   = (uint8_t*)malloc(o_ys + sizeof(float) * (size_t)npts);
         if (!own)
            goto simple_done;
         flags = own;
         xs    = (float*)(own + o_xs);
         ys    = (float*)(own + o_ys);
      }

      if ((size_t)p + 5 * (size_t)npts <= f->size)
         /* flags, x and y take at most 5 bytes a point: all in the
          * font, so they are read without a check per byte */
         p += rtt__glyf_points_fast(f->data + p, npts, flags, xs, ys);
      else
      {
         /* flags, run-length encoded */
         for (i = 0; i < npts; )
         {
            uint8_t fl = rtt__u8(f, p++);
            flags[i++] = fl;
            if (fl & 0x08) /* REPEAT */
            {
               uint8_t rep = rtt__u8(f, p++);
               while (rep-- && i < npts)
                  flags[i++] = fl;
            }
         }

         /* x deltas */
         {
            int v = 0;
            for (i = 0; i < npts; i++)
            {
               uint8_t fl = flags[i];
               if (fl & 0x02) /* X_SHORT */
               {
                  uint8_t d8 = rtt__u8(f, p++);
                  v += (fl & 0x10) ? (int)d8 : -(int)d8;
               }
               else if (!(fl & 0x10))
               {
                  v += (int16_t)rtt__u16(f, p);
                  p += 2;
               }
               xs[i] = (float)v;
            }
         }
         /* y deltas */
         {
            int v = 0;
            for (i = 0; i < npts; i++)
            {
               uint8_t fl = flags[i];
               if (fl & 0x04) /* Y_SHORT */
               {
                  uint8_t d8 = rtt__u8(f, p++);
                  v += (fl & 0x20) ? (int)d8 : -(int)d8;
               }
               else if (!(fl & 0x20))
               {
                  v += (int16_t)rtt__u16(f, p);
                  p += 2;
               }
               ys[i] = (float)v;
            }
         }
      }
      if (p > f->size)
         goto simple_done;

      /* every point to where it is drawn, once, before the contours
       * visit them */
      if (r->r)
      {
         const rtt__raster_t *rr = r->r;
         float sx = rr->sx, sy = rr->sy, ox = rr->ox, oy = rr->oy;
         if (     m->a == 1.0f && m->d == 1.0f && m->b == 0.0f
               && m->c == 0.0f && m->dx == 0.0f && m->dy == 0.0f)
            for (i = 0; i < npts; i++)
            {
               xs[i] =  xs[i] * sx - ox;
               ys[i] = -ys[i] * sy - oy;
            }
         else
            for (i = 0; i < npts; i++)
            {
               float tx, ty;
               rtt__xf_point(m, xs[i], ys[i], &tx, &ty);
               xs[i] =  tx * sx - ox;
               ys[i] = -ty * sy - oy;
            }
      }
      else
         for (i = 0; i < npts; i++)
         {
            float tx, ty;
            rtt__xf_point(m, xs[i], ys[i], &tx, &ty);
            xs[i] = tx;
            ys[i] = ty;
         }

      /* contours -> segments */
      {
         int start = 0;
         for (c = 0; c < ncont; c++)
         {
            int end = (int)rtt__u16(f, endpts + 2 * (uint32_t)c);
            int n, j, first_on, idx;
            float fx, fy;       /* contour start point, device */
            float px, py;       /* pending control point       */
            int   have_ctrl;

            if (end < start || end >= npts)
               goto simple_done;
            n = end - start + 1;
            if (n < 2)
            {
               /* nothing to draw, but a control box takes the point */
               if (n == 1 && !r->r && !r->sink)
                  rtt__pen_box(r, xs[start], ys[start]);
               start = end + 1;
               continue;
            }

            /* find the starting on-curve point; if none, synthesize
             * the midpoint of the first two off-curve points */
            first_on = -1;
            for (j = 0; j < n; j++)
               if (flags[start + j] & 0x01)
               {
                  first_on = j;
                  break;
               }

            if (first_on >= 0)
            {
               fx = xs[start + first_on];
               fy = ys[start + first_on];
            }
            else
            {
               fx = 0.5f * (xs[start] + xs[start + 1]);
               fy = 0.5f * (ys[start] + ys[start + 1]);
               first_on = 0; /* iterate from point 0; start is implicit */
            }

            rtt__pen_move(r, fx, fy);
            have_ctrl = 0;
            px = py = 0.0f;

            for (j = 1, idx = start + first_on; j <= n; j++)
            {
               float vx, vy;
               int   on;
               if (++idx > end)
                  idx = start;
               on = flags[idx] & 0x01;
               vx = xs[idx];
               vy = ys[idx];

               if (on)
               {
                  if (have_ctrl)
                  {
                     rtt__pen_quad(r, px, py, vx, vy);
                     have_ctrl = 0;
                  }
                  else
                     rtt__pen_line(r, vx, vy);
               }
               else
               {
                  if (have_ctrl)
                  {
                     /* two consecutive off-curve points: implied
                      * on-curve midpoint between them */
                     float mx = 0.5f * (px + vx);
                     float my = 0.5f * (py + vy);
                     rtt__pen_quad(r, px, py, mx, my);
                  }
                  px = vx;
                  py = vy;
                  have_ctrl = 1;
               }
            }
            /* close: back to the contour start */
            if (have_ctrl)
               rtt__pen_quad(r, px, py, fx, fy);
            else
               rtt__pen_line(r, fx, fy);

            start = end + 1;
         }
      }
      ok = 1;

simple_done:
      free(own);
      return ok;
   }
   else
   {
      /* ---- composite glyph ---- */
      uint32_t p = g + 10;
      int guard  = 0;

      for (;;)
      {
         uint16_t fl  = rtt__u16(f, p);
         uint16_t cgi = rtt__u16(f, p + 2);
         float dx = 0.0f, dy = 0.0f;
         rtt__xform_t cm;
         p += 4;

         if (fl & 0x0001) /* ARG_1_AND_2_ARE_WORDS */
         {
            if (fl & 0x0002) /* ARGS_ARE_XY_VALUES */
            {
               dx = (float)(int16_t)rtt__u16(f, p);
               dy = (float)(int16_t)rtt__u16(f, p + 2);
            }
            p += 4;
         }
         else
         {
            if (fl & 0x0002)
            {
               dx = (float)(int8_t)rtt__u8(f, p);
               dy = (float)(int8_t)rtt__u8(f, p + 1);
            }
            p += 2;
         }
         /* point-matching placement (rare) is unsupported: offsets 0 */

         cm.a = cm.d = 1.0f;
         cm.b = cm.c = 0.0f;
         if (fl & 0x0008) /* WE_HAVE_A_SCALE */
         {
            cm.a = cm.d = (float)(int16_t)rtt__u16(f, p) / 16384.0f;
            p += 2;
         }
         else if (fl & 0x0040) /* X_AND_Y_SCALE */
         {
            cm.a = (float)(int16_t)rtt__u16(f, p)     / 16384.0f;
            cm.d = (float)(int16_t)rtt__u16(f, p + 2) / 16384.0f;
            p += 4;
         }
         else if (fl & 0x0080) /* 2x2 */
         {
            cm.a = (float)(int16_t)rtt__u16(f, p)     / 16384.0f;
            cm.b = (float)(int16_t)rtt__u16(f, p + 2) / 16384.0f;
            cm.c = (float)(int16_t)rtt__u16(f, p + 4) / 16384.0f;
            cm.d = (float)(int16_t)rtt__u16(f, p + 6) / 16384.0f;
            p += 8;
         }

         /* compose child transform with parent: child point q ->
          * parent-space M_parent * (M_child * q + t_child) */
         {
            rtt__xform_t out;
            out.a  = m->a * cm.a + m->c * cm.b;
            out.b  = m->b * cm.a + m->d * cm.b;
            out.c  = m->a * cm.c + m->c * cm.d;
            out.d  = m->b * cm.c + m->d * cm.d;
            out.dx = m->a * dx + m->c * dy + m->dx;
            out.dy = m->b * dx + m->d * dy + m->dy;
            rtt__walk_glyph(f, r, (int)cgi, &out, depth + 1);
         }

         if (!(fl & 0x0020)) /* MORE_COMPONENTS */
            break;
         if (++guard > 64 || p >= f->size)
            break;
      }
      return 1;
   }
}

/* Lays out the scratch for an out_w x out_h raster plus room for
 * pts points; returns 0 when memory runs out. */
static int rtt__scratch_layout(rtt_scratch_t *sc, int out_w, int out_h,
      size_t pts, rtt__raster_t *r, rtt__pen_t *pen)
{
   size_t   cells   = (size_t)(out_w + 2) * (size_t)out_h;
   /* float cells, per-row dirty extents, then the outline scratch
    * sized from maxp so no glyph allocates on its own */
   size_t   o_flags = (cells + (size_t)out_h) * sizeof(float);
   size_t   o_xs    = o_flags + ((pts + 63) & ~(size_t)63);
   size_t   o_ys    = o_xs + ((pts * sizeof(float) + 63) & ~(size_t)63);
   size_t   need    = o_ys + pts * sizeof(float);
   unsigned dims    = ((unsigned)out_w << 16) | (unsigned)out_h;

   /* The cells and extents are left zeroed by every glyph for the
    * next; a new layout, or memory just had, is cleared in full */
   if (need > sc->size)
   {
      float *grown = (float*)malloc(need);
      if (!grown)
         return 0;
      free(sc->mem);
      sc->mem  = grown;
      sc->size = need;
      sc->dims = 0;
   }
   if (sc->dims != dims)
   {
      memset(sc->mem, 0, o_flags);
      sc->dims = dims;
   }
   if (r)
   {
      r->acc    = sc->mem;
      r->rowmax = (int*)(r->acc + cells);
   }
   pen->pts_flags = (uint8_t*)sc->mem + o_flags;
   pen->pts_xs    = (float*)((uint8_t*)sc->mem + o_xs);
   pen->pts_ys    = (float*)((uint8_t*)sc->mem + o_ys);
   pen->pts_cap   = (int)pts;
   return 1;
}

/* Walks glyph @gi, of either outline kind, into @pen. */
static int rtt__walk(const rtt_font_t *f, rtt__pen_t *pen, int gi)
{
   rtt__xform_t ident;
   if (f->is_cff)
      return rtt__walk_cff(f, pen, gi);
   ident.a = ident.d = 1.0f;
   ident.b = ident.c = 0.0f;
   ident.dx = ident.dy = 0.0f;
   return rtt__walk_glyph(f, pen, gi, &ident, 0);
}

/* Render glyph 'gi', whose bbox has xmin bx0 and ymax by1, into dst
 * (out_w x out_h, given stride), scaled by sx/sy: the bitmap origin is
 * (floor(xmin*sx), floor(-ymax*sy)). The full out_w x out_h region is
 * always written. */
void rtt_render_glyph(const rtt_font_t *f, rtt_scratch_t *sc,
      void *dst, int out_w, int out_h, int stride, float sx, float sy,
      int gi, int bx0, int by1, int fmt16)
{
   rtt_render_glyph_at(f, sc, dst, out_w, out_h, stride, sx, sy,
         (float)floor((double)((float)bx0 * sx)),
         (float)floor((double)(-(float)by1 * sy)), gi, fmt16);
}

/* dst points at coverage elements of the atlas format: uint8_t rows
 * when fmt16 is 0, native-endian uint16_t rows when it is nonzero.
 * 'stride' is in elements either way. Quantization happens only in
 * the per-row resolve, so both formats share every other stage. */
void rtt_render_glyph_at(const rtt_font_t *f, rtt_scratch_t *sc,
      void *dst, int out_w, int out_h, int stride, float sx, float sy,
      float ox, float oy, int gi, int fmt16)
{
   size_t esz = fmt16 ? sizeof(uint16_t) : sizeof(uint8_t);
   rtt__raster_t r;
   rtt__pen_t    pen;
   int y;

   if (!dst || out_w <= 0 || out_h <= 0)
      return;

   r.w  = out_w;
   r.h  = out_h;
   r.sx = sx;
   r.sy = sy;
   r.ox = ox;
   r.oy = oy;
   r.cx = r.sx0 = 0.0f;
   r.cy = r.sy0 = 0.0f;
   r.tol2 = f->flatness > 0.0f ? f->flatness * f->flatness : 0.1225f;

   memset(&pen, 0, sizeof(pen));
   pen.r = &r;
   if (!rtt__scratch_layout(sc, out_w, out_h,
            f->max_points > 0 ? (size_t)f->max_points : 0, &r, &pen))
   {
      for (y = 0; y < out_h; y++)
         memset((uint8_t*)dst + (size_t)y * (size_t)stride * esz, 0,
               (size_t)out_w * esz);
      return;
   }

   rtt__walk(f, &pen, gi);
   /* close the final contour */
   if (r.cx != r.sx0 || r.cy != r.sy0)
      rtt__acc_line(&r, r.cx, r.cy, r.sx0, r.sy0);

   for (y = 0; y < out_h; y++)
   {
      uint8_t *drow = (uint8_t*)dst + (size_t)y * (size_t)stride * esz;
      float   *arow = r.acc + (size_t)y * (size_t)(out_w + 2);
      int      span = r.rowmax[y];
      int      dw   = span > out_w ? out_w : span;
      if (dw > 0)
      {
         if (fmt16)
            rtt__resolve_row_u16(arow, (uint16_t*)(void*)drow, dw);
         else
            rtt__resolve_row_u8(arow, drow, dw);
      }
      /* past the last edge the winding sum of a closed outline is
       * zero, so the tail of the row is empty */
      if (dw < out_w)
         memset(drow + (size_t)dw * esz, 0, (size_t)(out_w - dw) * esz);
      /* every cell an edge touched lies before the row's extent */
      if (span > 0)
      {
         memset(arow, 0, (size_t)span * sizeof(float));
         r.rowmax[y] = 0;
      }
   }
}

/* Points for a TrueType outline walk outside a render: the scratch's
 * point area, behind an empty raster. */
static void rtt__pen_points(const rtt_font_t *f, rtt_scratch_t *sc,
      rtt__pen_t *pen)
{
   if (sc && !f->is_cff && f->max_points > 0)
   {
      /* a 1x1 raster layout keeps the scratch's zeroed-cell promise */
      if (!rtt__scratch_layout(sc, 1, 1, (size_t)f->max_points, NULL, pen))
         pen->pts_cap = 0;
   }
}

void rtt_set_flatness(rtt_font_t *f, float px)
{
   f->flatness = px > 0.0f ? px : 0.0f;
}

int rtt_glyph_components(const rtt_font_t *f, int gi,
      rtt_component_t *out, int max)
{
   uint32_t glen, p;
   uint32_t g;
   int      n = 0;
   if (f->is_cff)
      return -1;
   g = rtt__glyf_offset(f, gi, &glen);
   if (!g || glen < 10)
      return -1;
   if ((int16_t)rtt__u16(f, g) >= 0)
      return 0;
   p = g + 10;
   for (;;)
   {
      uint16_t        fl = rtt__u16(f, p);
      rtt_component_t c;
      memset(&c, 0, sizeof(c));
      c.flags = fl;
      c.glyph = rtt__u16(f, p + 2);
      p += 4;
      if (fl & 0x0001) /* ARG_1_AND_2_ARE_WORDS */
      {
         if (fl & 0x0002)
         {
            c.arg1 = (int16_t)rtt__u16(f, p);
            c.arg2 = (int16_t)rtt__u16(f, p + 2);
         }
         else
         {
            c.arg1 = rtt__u16(f, p);
            c.arg2 = rtt__u16(f, p + 2);
         }
         p += 4;
      }
      else
      {
         if (fl & 0x0002)
         {
            c.arg1 = (int8_t)rtt__u8(f, p);
            c.arg2 = (int8_t)rtt__u8(f, p + 1);
         }
         else
         {
            c.arg1 = rtt__u8(f, p);
            c.arg2 = rtt__u8(f, p + 1);
         }
         p += 2;
      }
      c.xx = c.yy = 0x4000;
      if (fl & 0x0008) /* WE_HAVE_A_SCALE */
      {
         c.xx = c.yy = (int16_t)rtt__u16(f, p);
         p += 2;
      }
      else if (fl & 0x0040) /* X_AND_Y_SCALE */
      {
         c.xx = (int16_t)rtt__u16(f, p);
         c.yy = (int16_t)rtt__u16(f, p + 2);
         p += 4;
      }
      else if (fl & 0x0080) /* 2x2 */
      {
         c.xx = (int16_t)rtt__u16(f, p);
         c.yx = (int16_t)rtt__u16(f, p + 2);
         c.xy = (int16_t)rtt__u16(f, p + 4);
         c.yy = (int16_t)rtt__u16(f, p + 6);
         p += 8;
      }
      if (n < max && out)
         out[n] = c;
      n++;
      if (!(fl & 0x0020) || n > 64 || p >= f->glyf + f->glyf_len)
         break;
   }
   return n;
}

int rtt_glyph_points(const rtt_font_t *f, int gi, int32_t *xs, int32_t *ys,
      uint8_t *on, uint16_t *ends, int max_points, int max_contours,
      int *num_contours)
{
   uint32_t glen, g, p, endpts;
   int      ncont, npts, i;
   if (num_contours)
      *num_contours = 0;
   if (f->is_cff)
      return -1;
   g = rtt__glyf_offset(f, gi, &glen);
   if (!g)
      return 0; /* an empty glyph */
   if (glen < 10 || (ncont = (int16_t)rtt__u16(f, g)) < 0)
      return -1;
   if (num_contours)
      *num_contours = ncont;
   if (!ncont)
      return 0;
   endpts = g + 10;
   npts   = (int)rtt__u16(f, endpts + 2 * ((uint32_t)ncont - 1)) + 1;
   if (!xs || !ys || !on || !ends)
      return npts;
   if (npts > max_points || ncont > max_contours)
      return -1;
   for (i = 0; i < ncont; i++)
      ends[i] = rtt__u16(f, endpts + 2 * (uint32_t)i);
   p = g + 12 + 2 * (uint32_t)ncont + rtt__u16(f, g + 10 + 2 * (uint32_t)ncont);
   for (i = 0; i < npts; )
   {
      uint8_t fl = rtt__u8(f, p++);
      on[i++] = fl;
      if (fl & 0x08) /* REPEAT */
      {
         uint8_t rep = rtt__u8(f, p++);
         while (rep-- && i < npts)
            on[i++] = fl;
      }
   }
   {
      int32_t v = 0;
      for (i = 0; i < npts; i++)
      {
         uint8_t fl = on[i];
         if (fl & 0x02)
         {
            uint8_t d8 = rtt__u8(f, p++);
            v += (fl & 0x10) ? (int32_t)d8 : -(int32_t)d8;
         }
         else if (!(fl & 0x10))
         {
            v += (int16_t)rtt__u16(f, p);
            p += 2;
         }
         xs[i] = v;
      }
      v = 0;
      for (i = 0; i < npts; i++)
      {
         uint8_t fl = on[i];
         if (fl & 0x04)
         {
            uint8_t d8 = rtt__u8(f, p++);
            v += (fl & 0x20) ? (int32_t)d8 : -(int32_t)d8;
         }
         else if (!(fl & 0x20))
         {
            v += (int16_t)rtt__u16(f, p);
            p += 2;
         }
         ys[i] = v;
         on[i] = (uint8_t)(on[i] & 1);
      }
   }
   if (p > f->size)
      return -1;
   return npts;
}

int rtt_glyph_outline(const rtt_font_t *f, rtt_scratch_t *sc, int gi,
      const rtt_outline_sink *sink)
{
   rtt__pen_t pen;
   if (!f || !sink)
      return 0;
   memset(&pen, 0, sizeof(pen));
   pen.sink = sink;
   rtt__pen_points(f, sc, &pen);
   return rtt__walk(f, &pen, gi);
}

int rtt_glyph_cbox(const rtt_font_t *f, rtt_scratch_t *sc, int gi,
      float *x0, float *y0, float *x1, float *y1)
{
   rtt__pen_t pen;
   if (!f)
      return 0;
   memset(&pen, 0, sizeof(pen));
   rtt__pen_points(f, sc, &pen);
   if (!rtt__walk(f, &pen, gi) || !pen.any)
      return 0;
   if (x0) *x0 = pen.bx0;
   if (y0) *y0 = pen.by0;
   if (x1) *x1 = pen.bx1;
   if (y1) *y1 = pen.by1;
   return 1;
}

int rtt_glyph_vmetrics(const rtt_font_t *f, int gi, int *advance, int *tsb)
{
   int nv = f->num_vmetrics;
   if (!f->vmtx || nv <= 0 || gi < 0 || gi >= f->num_glyphs)
      return 0;
   if (gi < nv)
   {
      if (advance) *advance = rtt__u16(f, f->vmtx + 4 * (uint32_t)gi);
      if (tsb)     *tsb     = (int16_t)rtt__u16(f, f->vmtx + 4 * (uint32_t)gi + 2);
   }
   else
   {
      if (advance) *advance = rtt__u16(f, f->vmtx + 4 * ((uint32_t)nv - 1));
      if (tsb)     *tsb     = (int16_t)rtt__u16(f, f->vmtx + 4 * (uint32_t)nv
            + 2 * ((uint32_t)gi - (uint32_t)nv));
   }
   return 1;
}

int rtt_vhea_metrics(const rtt_font_t *f, int *ascent, int *descent,
      int *line_gap)
{
   if (!f->vhea)
      return 0;
   if (ascent)   *ascent   = (int16_t)rtt__u16(f, f->vhea + 4);
   if (descent)  *descent  = (int16_t)rtt__u16(f, f->vhea + 6);
   if (line_gap) *line_gap = (int16_t)rtt__u16(f, f->vhea + 8);
   return 1;
}

int rtt_kerning(const rtt_font_t *f, int left, int right)
{
   uint32_t p, end, n, t;
   uint32_t key = ((uint32_t)left << 16) | (uint32_t)(right & 0xFFFF);
   int      value = 0;
   if (!f->kern || f->kern_len < 4 || rtt__u16(f, f->kern) != 0)
      return 0; /* only the version 0 (Microsoft) layout */
   n   = rtt__u16(f, f->kern + 2);
   p   = f->kern + 4;
   end = f->kern + f->kern_len;
   for (t = 0; t < n && p + 6 <= end; t++)
   {
      uint32_t len      = rtt__u16(f, p + 2);
      uint32_t coverage = rtt__u16(f, p + 4);
      /* format 0, horizontal, not minimum or cross-stream */
      if ((coverage >> 8) == 0 && (coverage & 0x07) == 0x01 && p + 14 <= end)
      {
         uint32_t pairs = rtt__u16(f, p + 6);
         uint32_t lo = 0, hi = pairs;
         uint32_t base = p + 14;
         if (base + pairs * 6 > end)
            pairs = hi = (end - base) / 6;
         while (lo < hi)
         {
            uint32_t mid = (lo + hi) >> 1;
            uint32_t k   = rtt__u32(f, base + mid * 6);
            if (k < key)
               lo = mid + 1;
            else
               hi = mid;
         }
         if (lo < pairs && rtt__u32(f, base + lo * 6) == key)
         {
            int v = (int16_t)rtt__u16(f, base + lo * 6 + 4);
            /* override replaces what came before; else they add */
            value = (coverage & 0x08) ? v : value + v;
         }
      }
      if (len < 6)
         break;
      p += len;
   }
   return value;
}

const uint8_t *rtt_table(const rtt_font_t *f, const char *tag, uint32_t *len)
{
   uint32_t l   = 0;
   uint32_t off = rtt__find_table(f, f->fontstart, tag, &l);
   if (!off)
      return NULL;
   if (len)
      *len = l;
   return f->data + off;
}

void rtt_scratch_free(rtt_scratch_t *sc)
{
   if (!sc)
      return;
   free(sc->mem);
   sc->mem  = NULL;
   sc->size = 0;
   sc->dims = 0;
}

/* Copyright  (C) 2010-2026 The RetroArch team
 *
 * ---------------------------------------------------------------------
 * The following license statement only applies to this file (rh265_bd.inc).
 * ---------------------------------------------------------------------
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
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
 * DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE,
 * ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

/* Bit-depth template for rh265: every function whose inner loops touch
 * picture samples, instantiated once per supported depth from rh265.c
 * with RH265_BD (8 or 10) and RH265_PEL (uint8_t or uint16_t) defined.
 * Picture planes are stored as bytes in rh265_dec and cast to
 * RH265_PEL here; all strides count samples, not bytes.  Everything
 * depth-dependent is expressed in RH265_BD so the 8-bit instantiation
 * compiles to exactly the pre-template code. */

#define RH265_PIX_MAX ((1 << RH265_BD) - 1)
#define RH265_HALF    (1 << (RH265_BD - 1))

static RH265_INLINE RH265_PEL RH265_FN(clip_pel)(int v)
{
   return (RH265_PEL)(v < 0 ? 0 : (v > RH265_PIX_MAX ? RH265_PIX_MAX : v));
}

static void RH265_FN(intra_pred)(rh265_dec *d, int x0, int y0, int log2_size,
      int c_idx, int mode)
{
   const rh265_sps *sps = d->sps;
   int size = 1 << log2_size;
   int shift = c_idx ? 1 : 0;
   int px = x0 >> shift, py = y0 >> shift;
   int stride = d->strd[c_idx];
   RH265_PEL *dst = (RH265_PEL*)d->pl[c_idx] + py * stride + px;
   int cur = d->cur_zaddr;
   /* linear reference array: ref[0] = p[-1][2N-1] .. ref[2N] = p[-1][-1]
    * .. ref[4N] = p[2N-1][-1] */
   RH265_PEL ref[4 * RH265_MAX_TB + 1];
   uint8_t avail[4 * RH265_MAX_TB + 1];
   RH265_PEL left[2 * RH265_MAX_TB + 1];
   RH265_PEL *top;
   int n4 = 4 * size;
   int i, x, y;
   int any = 0;

   for (i = 0; i <= n4; i++)
   {
      int sx, sy;
      if (i < 2 * size)      { sx = -1; sy = 2 * size - 1 - i; }
      else if (i == 2 * size){ sx = -1; sy = -1; }
      else                   { sx = i - 2 * size - 1; sy = -1; }
      /* sx/sy are -1 for the left/top ring, so scale by multiplication:
       * left-shifting a negative value is undefined behaviour. */
      avail[i] = (uint8_t)rh265_avail(d,
            x0 + sx * (1 << shift), y0 + sy * (1 << shift), cur);
      if (avail[i])
      {
         ref[i] = ((const RH265_PEL*)d->pl[c_idx])
               [(py + sy) * stride + (px + sx)];
         any = 1;
      }
   }
   if (!any)
      for (i = 0; i <= n4; i++)
         ref[i] = RH265_HALF;
   else
   {
      /* 8.4.4.2.2 substitution: bottom-left up, then left to right */
      if (!avail[0])
      {
         for (i = 1; i <= n4 && !avail[i]; i++) ;
         ref[0] = ref[i];
      }
      for (i = 1; i <= n4; i++)
         if (!avail[i])
            ref[i] = ref[i - 1];
   }

   /* split into left[-1..2N-1] (top-down) and top[-1..2N-1] */
   left[0] = ref[2 * size];                  /* corner at left[-1] slot */
   for (i = 0; i < 2 * size; i++)
      left[1 + i] = ref[2 * size - 1 - i];
   top = &ref[2 * size];                     /* top[-1] = corner .. */

   /* 8.4.4.2.3 reference smoothing (luma only) */
   if (c_idx == 0 && mode != 1 && size > 4)
   {
      int min_dist = rh265_min(abs(mode - 26), abs(mode - 10));
      int thres = (size == 8) ? 7 : ((size == 16) ? 1 : 0);
      if (mode == 0 || min_dist > thres)
      {
         int strong = 0;
         if (sps->strong_intra_smoothing && size == 32)
         {
            int c  = top[0];
            if (abs(c + top[64] - 2 * top[32]) < (1 << (RH265_BD - 5)) &&
                abs(c + left[64] - 2 * left[32]) < (1 << (RH265_BD - 5)))
               strong = 1;
         }
         if (strong)
         {
            RH265_PEL tl = top[0], tr = top[64], bl = left[64];
            for (i = 0; i < 63; i++)
            {
               top[1 + i]  = (RH265_PEL)(((63 - i) * tl + (i + 1) * tr + 32) >> 6);
               left[1 + i] = (RH265_PEL)(((63 - i) * tl + (i + 1) * bl + 32) >> 6);
            }
         }
         else
         {
            RH265_PEL ftop[2 * RH265_MAX_TB + 1];
            RH265_PEL fleft[2 * RH265_MAX_TB + 1];
            ftop[0] = fleft[0] =
               (RH265_PEL)((left[1] + 2 * top[0] + top[1] + 2) >> 2);
            for (i = 1; i < 2 * size; i++)
            {
               ftop[i]  = (RH265_PEL)((top[i - 1] + 2 * top[i] + top[i + 1] + 2) >> 2);
               fleft[i] = (RH265_PEL)((left[i - 1] + 2 * left[i] + left[i + 1] + 2) >> 2);
            }
            ftop[2 * size]  = top[2 * size];
            fleft[2 * size] = left[2 * size];
            memcpy(top, ftop, (size_t)(2 * size + 1) * sizeof(RH265_PEL));
            memcpy(left, fleft, (size_t)(2 * size + 1) * sizeof(RH265_PEL));
         }
      }
   }

   if (mode == 0)
   {
      /* 8.4.4.2.4 planar */
      for (y = 0; y < size; y++)
         for (x = 0; x < size; x++)
            dst[y * stride + x] = (RH265_PEL)(
               ((size - 1 - x) * left[1 + y] + (x + 1) * top[1 + size] +
                (size - 1 - y) * top[1 + x] + (y + 1) * left[1 + size] +
                size) >> (log2_size + 1));
   }
   else if (mode == 1)
   {
      /* 8.4.4.2.5 DC */
      int dc = size;
      for (i = 0; i < size; i++)
         dc += top[1 + i] + left[1 + i];
      dc >>= (log2_size + 1);
      for (y = 0; y < size; y++)
         for (x = 0; x < size; x++)
            dst[y * stride + x] = (RH265_PEL)dc;
      if (c_idx == 0 && size < 32)
      {
         dst[0] = (RH265_PEL)((left[1] + 2 * dc + top[1] + 2) >> 2);
         for (x = 1; x < size; x++)
            dst[x] = (RH265_PEL)((top[1 + x] + 3 * dc + 2) >> 2);
         for (y = 1; y < size; y++)
            dst[y * stride] = (RH265_PEL)((left[1 + y] + 3 * dc + 2) >> 2);
      }
   }
   else
   {
      /* 8.4.4.2.6 angular; rr[] carries the spec's ref[] array, where
       * ref[0] is the corner and ref[x] = p[x-1][-1] (vertical modes) or
       * p[-1][x-1] (horizontal modes); the local top[]/left[] arrays are
       * laid out the same way, so the main reference copies verbatim */
      int angle = rh265_pred_angle[mode];
      int last = (size * angle) >> 5;
      RH265_PEL mref[4 * RH265_MAX_TB + 2];
      RH265_PEL *rr = mref + RH265_MAX_TB;      /* rr[-N..2N] valid */
      if (mode >= 18)
      {
         memcpy(rr, top, (size_t)(2 * size + 1) * sizeof(RH265_PEL));
         if (angle < 0 && last < -1)
         {
            int inv = rh265_inv_angle[mode];
            for (i = last; i <= -1; i++)
               rr[i] = left[((i * inv + 128) >> 8)];
         }
         for (y = 0; y < size; y++)
         {
            int idx  = ((y + 1) * angle) >> 5;
            int fact = ((y + 1) * angle) & 31;
            if (fact)
               for (x = 0; x < size; x++)
                  dst[y * stride + x] = (RH265_PEL)(
                        ((32 - fact) * rr[x + idx + 1] +
                         fact * rr[x + idx + 2] + 16) >> 5);
            else
               for (x = 0; x < size; x++)
                  dst[y * stride + x] = rr[x + idx + 1];
         }
         if (mode == 26 && c_idx == 0 && size < 32)
            for (y = 0; y < size; y++)
               dst[y * stride] = RH265_FN(clip_pel)(top[1] +
                     ((left[1 + y] - top[0]) >> 1));
      }
      else
      {
         memcpy(rr, left, (size_t)(2 * size + 1) * sizeof(RH265_PEL));
         if (angle < 0 && last < -1)
         {
            int inv = rh265_inv_angle[mode];
            for (i = last; i <= -1; i++)
               rr[i] = top[((i * inv + 128) >> 8)];
         }
         for (x = 0; x < size; x++)
         {
            int idx  = ((x + 1) * angle) >> 5;
            int fact = ((x + 1) * angle) & 31;
            if (fact)
               for (y = 0; y < size; y++)
                  dst[y * stride + x] = (RH265_PEL)(
                        ((32 - fact) * rr[y + idx + 1] +
                         fact * rr[y + idx + 2] + 16) >> 5);
            else
               for (y = 0; y < size; y++)
                  dst[y * stride + x] = rr[y + idx + 1];
         }
         if (mode == 10 && c_idx == 0 && size < 32)
            for (x = 0; x < size; x++)
               dst[x] = RH265_FN(clip_pel)(left[1] + ((top[1 + x] - top[0]) >> 1));
      }
   }
}

static void RH265_FN(add_residual)(rh265_dec *d, int c_idx,
      int px, int py, const int16_t *coeffs, int size)
{
   int stride = d->strd[c_idx];
   RH265_PEL *dst = (RH265_PEL*)d->pl[c_idx] + py * stride + px;
   int x, y;
   for (y = 0; y < size; y++)
   {
      for (x = 0; x < size; x++)
         dst[x] = RH265_FN(clip_pel)(dst[x] + coeffs[x]);
      dst += stride;
      coeffs += size;
   }
}

static void RH265_FN(mc_fetch)(const RH265_PEL *src, int stride,
      int pw, int ph, int x, int y, RH265_PEL *dst, int w, int h)
{
   int i, j;
   for (j = 0; j < h; j++)
   {
      int sy = y + j;
      const RH265_PEL *row;
      if (sy < 0) sy = 0;
      if (sy > ph - 1) sy = ph - 1;
      row = src + sy * stride;
      for (i = 0; i < w; i++)
      {
         int sx = x + i;
         if (sx < 0) sx = 0;
         if (sx > pw - 1) sx = pw - 1;
         dst[j * w + i] = row[sx];
      }
   }
}

/* Luma quarter-pel interpolation into the intermediate domain.
 * (x, y) are the integer-pel block origin in the reference, (mx, my)
 * the fractional phases 0..3.  out is w*h ints. */
static void RH265_FN(mc_luma)(rh265_dec *d,
      const RH265_PEL *ref, int stride, int pw, int ph,
      int x, int y, int mx, int my, int *out, int w, int h)
{
   RH265_PEL *patch = (RH265_PEL*)d->mc_patch;
   int *tmp = d->mc_tmp;
   const int8_t *f;
   int i, j;
   if (!mx && !my)
   {
      RH265_FN(mc_fetch)(ref, stride, pw, ph, x, y, patch, w, h);
      for (j = 0; j < h; j++)
         for (i = 0; i < w; i++)
            out[j * w + i] = (int)patch[j * w + i]
                  << (14 - RH265_BD);
      return;
   }
   if (my == 0)
   {
      RH265_FN(mc_fetch)(ref, stride, pw, ph, x - 3, y, patch, w + 7, h);
      f = rh265_qpel_filt[mx];
      for (j = 0; j < h; j++)
         for (i = 0; i < w; i++)
         {
            const RH265_PEL *p = patch + j * (w + 7) + i;
            out[j * w + i] = (f[0]*p[0] + f[1]*p[1] + f[2]*p[2]
                  + f[3]*p[3] + f[4]*p[4] + f[5]*p[5]
                  + f[6]*p[6] + f[7]*p[7]) >> (RH265_BD - 8);
         }
      return;
   }
   if (mx == 0)
   {
      RH265_FN(mc_fetch)(ref, stride, pw, ph, x, y - 3, patch, w, h + 7);
      f = rh265_qpel_filt[my];
      for (j = 0; j < h; j++)
         for (i = 0; i < w; i++)
         {
            const RH265_PEL *p = patch + j * w + i;
            out[j * w + i] = (f[0]*p[0] + f[1]*p[w] + f[2]*p[2*w]
                  + f[3]*p[3*w] + f[4]*p[4*w] + f[5]*p[5*w]
                  + f[6]*p[6*w] + f[7]*p[7*w])
                  >> (RH265_BD - 8);
         }
      return;
   }
   /* separable h then v; the horizontal pass output feeds the vertical
    * filter and the result is scaled back by >> 6 (dsp_template hv) */
   RH265_FN(mc_fetch)(ref, stride, pw, ph, x - 3, y - 3, patch, w + 7, h + 7);
   f = rh265_qpel_filt[mx];
   for (j = 0; j < h + 7; j++)
      for (i = 0; i < w; i++)
      {
         const RH265_PEL *p = patch + j * (w + 7) + i;
         tmp[j * w + i] = (f[0]*p[0] + f[1]*p[1] + f[2]*p[2]
               + f[3]*p[3] + f[4]*p[4] + f[5]*p[5]
               + f[6]*p[6] + f[7]*p[7]) >> (RH265_BD - 8);
      }
   f = rh265_qpel_filt[my];
   for (j = 0; j < h; j++)
      for (i = 0; i < w; i++)
      {
         const int *p = tmp + j * w + i;
         out[j * w + i] = (f[0]*p[0] + f[1]*p[w] + f[2]*p[2*w]
               + f[3]*p[3*w] + f[4]*p[4*w] + f[5]*p[5*w]
               + f[6]*p[6*w] + f[7]*p[7*w]) >> 6;
      }
}

/* Chroma eighth-pel interpolation, phases 0..7. */
static void RH265_FN(mc_chroma)(rh265_dec *d,
      const RH265_PEL *ref, int stride, int pw, int ph,
      int x, int y, int mx, int my, int *out, int w, int h)
{
   RH265_PEL *patch = (RH265_PEL*)d->mc_patch;   /* chroma fits in the luma scratch */
   int *tmp = d->mc_tmp;
   const int8_t *f;
   int i, j;
   if (!mx && !my)
   {
      RH265_FN(mc_fetch)(ref, stride, pw, ph, x, y, patch, w, h);
      for (j = 0; j < h; j++)
         for (i = 0; i < w; i++)
            out[j * w + i] = (int)patch[j * w + i]
                  << (14 - RH265_BD);
      return;
   }
   if (my == 0)
   {
      RH265_FN(mc_fetch)(ref, stride, pw, ph, x - 1, y, patch, w + 3, h);
      f = rh265_epel_filt[mx];
      for (j = 0; j < h; j++)
         for (i = 0; i < w; i++)
         {
            const RH265_PEL *p = patch + j * (w + 3) + i;
            out[j * w + i] = (f[0]*p[0] + f[1]*p[1]
                  + f[2]*p[2] + f[3]*p[3]) >> (RH265_BD - 8);
         }
      return;
   }
   if (mx == 0)
   {
      RH265_FN(mc_fetch)(ref, stride, pw, ph, x, y - 1, patch, w, h + 3);
      f = rh265_epel_filt[my];
      for (j = 0; j < h; j++)
         for (i = 0; i < w; i++)
         {
            const RH265_PEL *p = patch + j * w + i;
            out[j * w + i] = (f[0]*p[0] + f[1]*p[w] + f[2]*p[2*w]
                  + f[3]*p[3*w])
                  >> (RH265_BD - 8);
         }
      return;
   }
   RH265_FN(mc_fetch)(ref, stride, pw, ph, x - 1, y - 1, patch, w + 3, h + 3);
   f = rh265_epel_filt[mx];
   for (j = 0; j < h + 3; j++)
      for (i = 0; i < w; i++)
      {
         const RH265_PEL *p = patch + j * (w + 3) + i;
         tmp[j * w + i] = (f[0]*p[0] + f[1]*p[1]
               + f[2]*p[2] + f[3]*p[3]) >> (RH265_BD - 8);
      }
   f = rh265_epel_filt[my];
   for (j = 0; j < h; j++)
      for (i = 0; i < w; i++)
      {
         const int *p = tmp + j * w + i;
         out[j * w + i] = (f[0]*p[0] + f[1]*p[w] + f[2]*p[2*w]
               + f[3]*p[3*w]) >> 6;
      }
}

/* Write one predicted block into the current picture.  val0/val1 are
 * intermediate-domain blocks for the (up to two) hypotheses. */
static void RH265_FN(mc_write)(RH265_PEL *dst, int stride,
      const int *val0,
      const int *val1, int w, int h, int weighted, int denom,
      int w0, int o0, int w1, int o1)
{
   int i, j;
   if (!val1)
   {
      if (!weighted)
      {
         for (j = 0; j < h; j++)
            for (i = 0; i < w; i++)
               dst[j * stride + i] = RH265_FN(clip_pel)(
                     (val0[j * w + i] + (1 << (13 - RH265_BD)))
                     >> (14 - RH265_BD));
      }
      else
      {
         int shift  = denom + 14 - RH265_BD;
         int offset = 1 << (shift - 1);
         int oxs    = o0 << (RH265_BD - 8);
         for (j = 0; j < h; j++)
            for (i = 0; i < w; i++)
               dst[j * stride + i] = RH265_FN(clip_pel)(
                     ((val0[j * w + i] * w0 + offset) >> shift) + oxs);
      }
   }
   else
   {
      if (!weighted)
      {
         for (j = 0; j < h; j++)
            for (i = 0; i < w; i++)
               dst[j * stride + i] = RH265_FN(clip_pel)(
                     (val0[j * w + i] + val1[j * w + i]
                      + (1 << (14 - RH265_BD))) >> (15 - RH265_BD));
      }
      else
      {
         int log2wd = denom + 14 - RH265_BD;
         int oxp1   = (o0 << (RH265_BD - 8)) + (o1 << (RH265_BD - 8)) + 1;
         for (j = 0; j < h; j++)
            for (i = 0; i < w; i++)
               dst[j * stride + i] = RH265_FN(clip_pel)(
                     (val1[j * w + i] * w1 + val0[j * w + i] * w0
                      + (oxp1 << log2wd)) >> (log2wd + 1));
      }
   }
}

/* Run motion compensation for one prediction unit into the current
 * picture, in up to two hypotheses per plane. */
static void RH265_FN(mc_pu)(rh265_dec *d, int x0, int y0, int w, int h,
      const rh265_mvfield *mv)
{
   int *val0 = d->mc_val0;
   int *val1 = d->mc_val1;
   const rh265_shdr *sh = &d->sh;
   int weighted = (sh->slice_type == RH265_SLICE_P && d->pps->weighted_pred)
        || (sh->slice_type == RH265_SLICE_B && d->pps->weighted_bipred);
   int denom_l = weighted ? sh->luma_log2_denom : 0;
   int denom_c = weighted ? sh->chroma_log2_denom : 0;
   const rh265_pic *r0 = NULL, *r1 = NULL;
   int i0 = 0, i1 = 0;
   int c;

   if (mv->pred & RH265_PF_L0)
   {
      i0 = mv->ref_idx[0];
      r0 = d->ref_list[0][i0];
   }
   if (mv->pred & RH265_PF_L1)
   {
      i1 = mv->ref_idx[1];
      r1 = d->ref_list[1][i1];
   }
   if ((mv->pred & RH265_PF_L0) && !r0) return;
   if ((mv->pred & RH265_PF_L1) && !r1) return;

   /* luma */
   if (r0)
      RH265_FN(mc_luma)(d, (const RH265_PEL*)r0->pl[0], d->strd[0], d->pw[0], d->ph[0],
            x0 + (mv->mv[0].x >> 2), y0 + (mv->mv[0].y >> 2),
            mv->mv[0].x & 3, mv->mv[0].y & 3, val0, w, h);
   if (r1)
      RH265_FN(mc_luma)(d, (const RH265_PEL*)r1->pl[0], d->strd[0], d->pw[0], d->ph[0],
            x0 + (mv->mv[1].x >> 2), y0 + (mv->mv[1].y >> 2),
            mv->mv[1].x & 3, mv->mv[1].y & 3,
            r0 ? val1 : val0, w, h);
   if (mv->pred == RH265_PF_BI)
      RH265_FN(mc_write)((RH265_PEL*)d->pl[0]
            + y0 * d->strd[0] + x0, d->strd[0],
            val0, val1, w, h, weighted, denom_l,
            sh->luma_w[0][i0], sh->luma_o[0][i0],
            sh->luma_w[1][i1], sh->luma_o[1][i1]);
   else
   {
      int l = (mv->pred == RH265_PF_L1);
      int ri = l ? i1 : i0;
      RH265_FN(mc_write)((RH265_PEL*)d->pl[0]
            + y0 * d->strd[0] + x0, d->strd[0],
            val0, NULL, w, h, weighted, denom_l,
            sh->luma_w[l][ri], sh->luma_o[l][ri], 0, 0);
   }

   /* chroma */
   for (c = 1; c < 3; c++)
   {
      int xc = x0 >> 1, yc = y0 >> 1, wc = w >> 1, hc = h >> 1;
      if (!wc || !hc)
         continue;
      if (r0)
         RH265_FN(mc_chroma)(d, (const RH265_PEL*)r0->pl[c], d->strd[c], d->pw[c], d->ph[c],
               xc + (mv->mv[0].x >> 3), yc + (mv->mv[0].y >> 3),
               mv->mv[0].x & 7, mv->mv[0].y & 7, val0, wc, hc);
      if (r1)
         RH265_FN(mc_chroma)(d, (const RH265_PEL*)r1->pl[c], d->strd[c], d->pw[c], d->ph[c],
               xc + (mv->mv[1].x >> 3), yc + (mv->mv[1].y >> 3),
               mv->mv[1].x & 7, mv->mv[1].y & 7,
               r0 ? val1 : val0, wc, hc);
      if (mv->pred == RH265_PF_BI)
         RH265_FN(mc_write)((RH265_PEL*)d->pl[c]
               + yc * d->strd[c] + xc, d->strd[c],
               val0, val1, wc, hc, weighted, denom_c,
               sh->chroma_w[0][i0][c - 1], sh->chroma_o[0][i0][c - 1],
               sh->chroma_w[1][i1][c - 1], sh->chroma_o[1][i1][c - 1]);
      else
      {
         int l = (mv->pred == RH265_PF_L1);
         int ri = l ? i1 : i0;
         RH265_FN(mc_write)((RH265_PEL*)d->pl[c]
               + yc * d->strd[c] + xc, d->strd[c],
               val0, NULL, wc, hc, weighted, denom_c,
               sh->chroma_w[l][ri][c - 1], sh->chroma_o[l][ri][c - 1],
               0, 0);
      }
   }
}

static void RH265_FN(filter_luma_edge)(RH265_PEL *pix, int xs, int ys,
      int beta, int tc)
{
   int dp0, dq0, dp3, dq3, d0, d3;
   RH265_PEL *l0 = pix, *l3 = pix + 3 * ys;
   int j;
   if (!tc)
      return;
   dp0 = abs(l0[-3*xs] - 2*l0[-2*xs] + l0[-1*xs]);
   dq0 = abs(l0[ 2*xs] - 2*l0[ 1*xs] + l0[ 0]);
   dp3 = abs(l3[-3*xs] - 2*l3[-2*xs] + l3[-1*xs]);
   dq3 = abs(l3[ 2*xs] - 2*l3[ 1*xs] + l3[ 0]);
   d0 = dp0 + dq0;
   d3 = dp3 + dq3;
   if (d0 + d3 >= beta)
      return;
   if (abs(l0[-4*xs]-l0[-1*xs]) + abs(l0[3*xs]-l0[0]) < (beta >> 3) &&
       abs(l0[-1*xs]-l0[0]) < ((tc * 5 + 1) >> 1) &&
       abs(l3[-4*xs]-l3[-1*xs]) + abs(l3[3*xs]-l3[0]) < (beta >> 3) &&
       abs(l3[-1*xs]-l3[0]) < ((tc * 5 + 1) >> 1) &&
       (d0 << 1) < (beta >> 2) && (d3 << 1) < (beta >> 2))
   {
      /* strong */
      int tc2 = tc << 1;
      for (j = 0; j < 4; j++)
      {
         RH265_PEL *e = pix + j * ys;
         int p3 = e[-4*xs], p2 = e[-3*xs], p1 = e[-2*xs], p0 = e[-1*xs];
         int q0 = e[0], q1 = e[1*xs], q2 = e[2*xs], q3 = e[3*xs];
         e[-1*xs] = (RH265_PEL)rh265_clip3(p0-tc2, p0+tc2,
               (p2 + 2*p1 + 2*p0 + 2*q0 + q1 + 4) >> 3);
         e[-2*xs] = (RH265_PEL)rh265_clip3(p1-tc2, p1+tc2,
               (p2 + p1 + p0 + q0 + 2) >> 2);
         e[-3*xs] = (RH265_PEL)rh265_clip3(p2-tc2, p2+tc2,
               (2*p3 + 3*p2 + p1 + p0 + q0 + 4) >> 3);
         e[0]     = (RH265_PEL)rh265_clip3(q0-tc2, q0+tc2,
               (q2 + 2*q1 + 2*q0 + 2*p0 + p1 + 4) >> 3);
         e[1*xs]  = (RH265_PEL)rh265_clip3(q1-tc2, q1+tc2,
               (q2 + q1 + q0 + p0 + 2) >> 2);
         e[2*xs]  = (RH265_PEL)rh265_clip3(q2-tc2, q2+tc2,
               (2*q3 + 3*q2 + q1 + q0 + p0 + 4) >> 3);
      }
   }
   else
   {
      /* weak */
      int dEp = (dp0 + dp3 < ((beta + (beta >> 1)) >> 3));
      int dEq = (dq0 + dq3 < ((beta + (beta >> 1)) >> 3));
      for (j = 0; j < 4; j++)
      {
         RH265_PEL *e = pix + j * ys;
         int p2 = e[-3*xs], p1 = e[-2*xs], p0 = e[-1*xs];
         int q0 = e[0], q1 = e[1*xs], q2 = e[2*xs];
         int delta = (9 * (q0 - p0) - 3 * (q1 - p1) + 8) >> 4;
         if (abs(delta) < tc * 10)
         {
            delta = rh265_clip3(-tc, tc, delta);
            e[-1*xs] = RH265_FN(clip_pel)(p0 + delta);
            e[0]     = RH265_FN(clip_pel)(q0 - delta);
            if (dEp)
            {
               int d = rh265_clip3(-(tc >> 1), tc >> 1,
                     (((p2 + p0 + 1) >> 1) - p1 + delta) >> 1);
               e[-2*xs] = RH265_FN(clip_pel)(p1 + d);
            }
            if (dEq)
            {
               int d = rh265_clip3(-(tc >> 1), tc >> 1,
                     (((q2 + q0 + 1) >> 1) - q1 - delta) >> 1);
               e[1*xs] = RH265_FN(clip_pel)(q1 + d);
            }
         }
      }
   }
}

/* filter one 4-line chroma edge segment (8.7.2.5.8) */
static void RH265_FN(filter_chroma_edge)(RH265_PEL *pix,
      int xs, int ys, int tc)
{
   int j;
   if (!tc)
      return;
   for (j = 0; j < 4; j++)
   {
      RH265_PEL *e = pix + j * ys;
      int p1 = e[-2*xs], p0 = e[-1*xs], q0 = e[0], q1 = e[1*xs];
      int delta = rh265_clip3(-tc, tc,
            (((q0 - p0) * 4 + p1 - q1 + 4) >> 3));
      e[-1*xs] = RH265_FN(clip_pel)(p0 + delta);
      e[0]     = RH265_FN(clip_pel)(q0 - delta);
   }
}

static int RH265_FN(deblock_chroma_tc)(rh265_dec *d, int qp_y, int c_idx)
{
   int off = (c_idx == 1) ? d->pps->cb_qp_offset : d->pps->cr_qp_offset;
   /* deblocking's qPi floor is 0 at every depth (8.7.2.5.5), unlike
    * dequantisation's -QpBdOffset (8.6.1): pass bd 8 to keep the
    * clip3 floor at 0 */
   int qp = rh265_chroma_qp(qp_y, off, 8);
   /* bS is always 2 for intra: intra tc offset 2*(bS-1) = 2 */
   return rh265_tctable[rh265_clip3(0, 53, qp + 2
         + d->sh.tc_offset_div2 * 2)] << (RH265_BD - 8);
}

/* Deblock the whole picture: all marked TU/PU edges on the 8x8 luma grid
 * carry bS 2 in an intra picture.  Vertical edges over the full picture
 * first, then horizontal edges (8.7.2.1). */
static void RH265_FN(deblock_frame)(rh265_dec *d)
{
   const rh265_sps *sps = d->sps;
   /* the div2 offsets are se(v) values in -6..6; scale by
    * multiplication, not by a shift of a possibly negative value */
   int beta_off = d->sh.beta_offset_div2 * 2;
   int tc_off   = d->sh.tc_offset_div2 * 2;
   int x, y, bs;

   if (d->sh.deblocking_filter_disabled)
      return;

   /* vertical edges */
   for (x = 8; x < sps->width; x += 8)
      for (y = 0; y < sps->height; y += 4)
      {
         bs = d->vedge[(y >> 2) * d->w4 + (x >> 2)];
         if (!bs)
            continue;
         if (d->slice_seq)
         {
            /* 8.7.2: an edge on a slice boundary is left unfiltered
             * when the containing slice forbids filtering across it;
             * single-slice pictures (slice_seq 0) skip the lookups */
            int pc = ((y >> d->sps->log2_ctb) * d->sps->ctb_w)
                   + ((x - 1) >> d->sps->log2_ctb);
            int qc = ((y >> d->sps->log2_ctb) * d->sps->ctb_w)
                   + (x >> d->sps->log2_ctb);
            if (d->ctb_slice[pc] != d->ctb_slice[qc] &&
                !d->ctb_lf_across[qc])
               continue;
         }
         {
            int qp = (d->qpy[(y >> 3) * d->w8 + ((x - 1) >> 3)]
                    + d->qpy[(y >> 3) * d->w8 + (x >> 3)] + 1) >> 1;
            int beta = rh265_betatable[rh265_clip3(0, 51,
                  qp + beta_off)] << (RH265_BD - 8);
            int tc = rh265_tctable[rh265_clip3(0, 53,
                  qp + 2 * (bs - 1) + tc_off)] << (RH265_BD - 8);
            RH265_FN(filter_luma_edge)((RH265_PEL*)d->pl[0]
                  + y * d->strd[0] + x, 1, d->strd[0], beta, tc);
         }
         if (bs == 2 && (x & 15) == 0 && (y & 7) == 0 &&
             (y >> 1) + 4 <= d->ph[1])
         {
            int qp = (d->qpy[(y >> 3) * d->w8 + ((x - 1) >> 3)]
                    + d->qpy[(y >> 3) * d->w8 + (x >> 3)] + 1) >> 1;
            RH265_FN(filter_chroma_edge)(
                  (RH265_PEL*)d->pl[1] + (y >> 1) * d->strd[1] + (x >> 1),
                  1, d->strd[1], RH265_FN(deblock_chroma_tc)(d, qp, 1));
            RH265_FN(filter_chroma_edge)(
                  (RH265_PEL*)d->pl[2] + (y >> 1) * d->strd[2] + (x >> 1),
                  1, d->strd[2], RH265_FN(deblock_chroma_tc)(d, qp, 2));
         }
      }

   /* horizontal edges */
   for (y = 8; y < sps->height; y += 8)
      for (x = 0; x < sps->width; x += 4)
      {
         bs = d->hedge[(y >> 2) * d->w4 + (x >> 2)];
         if (!bs)
            continue;
         if (d->slice_seq)
         {
            int pc = (((y - 1) >> d->sps->log2_ctb) * d->sps->ctb_w)
                   + (x >> d->sps->log2_ctb);
            int qc = ((y >> d->sps->log2_ctb) * d->sps->ctb_w)
                   + (x >> d->sps->log2_ctb);
            if (d->ctb_slice[pc] != d->ctb_slice[qc] &&
                !d->ctb_lf_across[qc])
               continue;
         }
         {
            int qp = (d->qpy[((y - 1) >> 3) * d->w8 + (x >> 3)]
                    + d->qpy[(y >> 3) * d->w8 + (x >> 3)] + 1) >> 1;
            int beta = rh265_betatable[rh265_clip3(0, 51,
                  qp + beta_off)] << (RH265_BD - 8);
            int tc = rh265_tctable[rh265_clip3(0, 53,
                  qp + 2 * (bs - 1) + tc_off)] << (RH265_BD - 8);
            RH265_FN(filter_luma_edge)((RH265_PEL*)d->pl[0]
                  + y * d->strd[0] + x, d->strd[0], 1, beta, tc);
         }
         if (bs == 2 && (y & 15) == 0 && (x & 7) == 0 &&
             (x >> 1) + 4 <= d->pw[1])
         {
            int qp = (d->qpy[((y - 1) >> 3) * d->w8 + (x >> 3)]
                    + d->qpy[(y >> 3) * d->w8 + (x >> 3)] + 1) >> 1;
            RH265_FN(filter_chroma_edge)(
                  (RH265_PEL*)d->pl[1] + (y >> 1) * d->strd[1] + (x >> 1),
                  d->strd[1], 1, RH265_FN(deblock_chroma_tc)(d, qp, 1));
            RH265_FN(filter_chroma_edge)(
                  (RH265_PEL*)d->pl[2] + (y >> 1) * d->strd[2] + (x >> 1),
                  d->strd[2], 1, RH265_FN(deblock_chroma_tc)(d, qp, 2));
         }
      }
}

static void RH265_FN(sao_ctb)(rh265_dec *d, const RH265_PEL *src,
      int src_stride,
      int c_idx, int rx, int ry)
{
   const rh265_sao_params *sao = &d->sao[ry * d->sps->ctb_w + rx];
   int shift = c_idx ? 1 : 0;
   int ctb = 1 << (d->sps->log2_ctb - shift);
   int x0 = rx << (d->sps->log2_ctb - shift);
   int y0 = ry << (d->sps->log2_ctb - shift);
   int w = rh265_min(ctb, d->pw[c_idx] - x0);
   int h = rh265_min(ctb, d->ph[c_idx] - y0);
   RH265_PEL *dst = (RH265_PEL*)d->pl[c_idx]
         + y0 * d->strd[c_idx] + x0;
   const RH265_PEL *s = src + y0 * src_stride + x0;
   int x, y;

   if (sao->type_idx[c_idx] == 1)
   {
      /* band offset */
      int8_t band[32];
      int k;
      memset(band, 0, sizeof(band));
      for (k = 0; k < 4; k++)
         band[(sao->band_pos[c_idx] + k) & 31] = sao->off[c_idx][k];
      for (y = 0; y < h; y++)
         for (x = 0; x < w; x++)
            dst[y * d->strd[c_idx] + x] = RH265_FN(clip_pel)(
                  s[y * src_stride + x] + band[s[y * src_stride + x] >> (RH265_BD - 5)]);
   }
   else if (sao->type_idx[c_idx] == 2)
   {
      /* edge offset; samples whose neighbours leave the picture keep
       * their value (loop-filter-across-slices is moot with one slice) */
      int dx = rh265_sao_eo_dx[sao->eo_class[c_idx]];
      int dy = rh265_sao_eo_dy[sao->eo_class[c_idx]];
      for (y = 0; y < h; y++)
         for (x = 0; x < w; x++)
         {
            int px = x0 + x, py = y0 + y;
            int a, b, cval, edge;
            if (px + dx < 0 || px + dx >= d->pw[c_idx] ||
                py + dy < 0 || py + dy >= d->ph[c_idx] ||
                px - dx < 0 || px - dx >= d->pw[c_idx] ||
                py - dy < 0 || py - dy >= d->ph[c_idx])
               continue;
            if (d->slice_seq)
            {
               /* 8.7.3: neighbours across a slice boundary are
                * unavailable when the current slice forbids filtering
                * across it; the sample then keeps its value */
               int cc = ry * d->sps->ctb_w + rx;
               if (!d->ctb_lf_across[cc])
               {
                  int lshift = c_idx ? 1 : 0;
                  int na = (((py + dy) << lshift) >> d->sps->log2_ctb)
                         * d->sps->ctb_w
                         + (((px + dx) << lshift) >> d->sps->log2_ctb);
                  int nb = (((py - dy) << lshift) >> d->sps->log2_ctb)
                         * d->sps->ctb_w
                         + (((px - dx) << lshift) >> d->sps->log2_ctb);
                  if (d->ctb_slice[na] != d->ctb_slice[cc] ||
                      d->ctb_slice[nb] != d->ctb_slice[cc])
                     continue;
               }
            }
            cval = s[y * src_stride + x];
            a = s[(y + dy) * src_stride + (x + dx)];
            b = s[(y - dy) * src_stride + (x - dx)];
            edge = 2 + ((cval > a) - (cval < a)) + ((cval > b) - (cval < b));
            if (edge == 2)
               continue;
            if (edge < 2)
               edge++;             /* 0->1, 1->2 */
            /* offsets: index 1..4 -> off[0..3], 3 and 4 negated */
            {
               int v = sao->off[c_idx][edge - 1];
               dst[y * d->strd[c_idx] + x] = RH265_FN(clip_pel)(cval + v);
            }
         }
   }
}

static int RH265_FN(sao_frame)(rh265_dec *d)
{
   const rh265_sps *sps = d->sps;
   int c_idx, rx, ry;
   RH265_PEL *copy[3];
   int need = 0;
   for (ry = 0; ry < sps->ctb_h && !need; ry++)
      for (rx = 0; rx < sps->ctb_w && !need; rx++)
      {
         const rh265_sao_params *p = &d->sao[ry * sps->ctb_w + rx];
         if (p->type_idx[0] || p->type_idx[1] || p->type_idx[2])
            need = 1;
      }
   if (!need)
      return 0;
   for (c_idx = 0; c_idx < 3; c_idx++)
   {
      copy[c_idx] = (RH265_PEL*)malloc((size_t)d->strd[c_idx]
            * d->ph[c_idx] * sizeof(RH265_PEL));
      if (!copy[c_idx])
      {
         while (c_idx--) free(copy[c_idx]);
         return -1;
      }
      memcpy(copy[c_idx], d->pl[c_idx], (size_t)d->strd[c_idx]
            * d->ph[c_idx] * sizeof(RH265_PEL));
   }
   for (ry = 0; ry < sps->ctb_h; ry++)
      for (rx = 0; rx < sps->ctb_w; rx++)
         for (c_idx = 0; c_idx < 3; c_idx++)
            RH265_FN(sao_ctb)(d, copy[c_idx], d->strd[c_idx],
                  c_idx, rx, ry);
   for (c_idx = 0; c_idx < 3; c_idx++)
      free(copy[c_idx]);
   return 0;
}

#undef RH265_PIX_MAX
#undef RH265_HALF

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2021 - Chris Kennedy, Antonio Giner,
 *                            Alexandre Wodarczyk, Gil Delescluse
 *  Copyright (C) 2026 - The RetroArch team
 *
 *  RetroArch is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  RetroArch is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with RetroArch.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include <stdio.h>
#include <string.h>
#include <math.h>

#include "modeline_core.h"
#include "modeline_monitor.h"

#ifndef MODELINE_STANDALONE
#include "../../verbosity.h"
#endif

#define MODELINE_MAX(a, b) ((a) > (b) ? (a) : (b))
#define MODELINE_MIN(a, b) ((a) < (b) ? (a) : (b))

static int scale_into_range_i(int value, int lower_limit, int higher_limit)
{
   int scale = 1;
   while (value * scale < lower_limit)
      scale++;
   if (value * scale <= higher_limit)
      return scale;
   return 0;
}

static int scale_into_range_d(double value, double lower_limit,
      double higher_limit)
{
   int scale = 1;
   while (value * scale < lower_limit)
      scale++;
   if (value * scale <= higher_limit)
      return scale;
   return 0;
}

static int scale_into_aspect(int source_res, int tot_res,
      double original_monitor_aspect, double users_monitor_aspect,
      double *best_diff)
{
   int scale      = 1;
   int best_scale = 1;
   double diff    = 0;
   *best_diff     = 0;

   while (source_res * scale <= tot_res)
   {
      diff = fabs(1.0 - (users_monitor_aspect
               / ((double)tot_res / (double)(source_res * scale)
                  * original_monitor_aspect))) * 100.0;
      if (diff < *best_diff || *best_diff == 0)
      {
         *best_diff = diff;
         best_scale = scale;
      }
      scale++;
   }
   return best_scale;
}

static double max_vfreq_for_yres(int yres, video_modeline_range_t *range,
      double borders, double interlace)
{
   return range->hfreq_max / (yres / interlace
         + modeline_round_near(range->hfreq_max
            * (range->vertical_blank + borders)));
}

static int stretch_into_range(double vfreq, video_modeline_range_t *range,
      double borders, bool interlace_allowed, double *interlace)
{
   int yres, lower_limit;

   if (range->interlaced_lines_min && interlace_allowed)
   {
      yres        = range->interlaced_lines_max;
      lower_limit = range->interlaced_lines_min;
      *interlace  = 2;
   }
   else
   {
      yres        = range->progressive_lines_max;
      lower_limit = range->progressive_lines_min;
   }

   while (yres > lower_limit
         && max_vfreq_for_yres(yres, range, borders, *interlace) < vfreq)
      yres -= 8;

   return yres;
}

static int total_lines_for_yres(int yres, double vfreq,
      video_modeline_range_t *range, double borders, double interlace)
{
   int vvt = (int)MODELINE_MAX(yres / interlace
         + modeline_round_near(vfreq * yres
            / (interlace * (1.0 - vfreq * (range->vertical_blank + borders)))
            * (range->vertical_blank + borders)), 1);
   while ((vfreq * vvt < range->hfreq_min)
         && (vfreq * (vvt + 1) < range->hfreq_max))
      vvt++;
   return vvt;
}

static int get_line_params(video_modeline_t *mode,
      video_modeline_range_t *range, int char_size)
{
   int hhi, hhf, hht;
   int hh, hs, he, ht;
   double line_time, char_time, new_char_time;
   double hfront_porch_min, hsync_pulse_min, hback_porch_min;

   hfront_porch_min = range->hfront_porch * .90;
   hsync_pulse_min  = range->hsync_pulse  * .90;
   hback_porch_min  = range->hback_porch  * .90;

   line_time        = 1 / mode->hfreq * 1000000;

   hh               = mode->hactive / char_size;
   hs = he = ht     = 1;

   do
   {
      char_time = line_time / (hh + hs + he + ht);
      if (hs * char_time < hfront_porch_min ||
            fabs((hs + 1) * char_time - range->hfront_porch)
            < fabs(hs * char_time - range->hfront_porch))
         hs++;

      if (he * char_time < hsync_pulse_min ||
            fabs((he + 1) * char_time - range->hsync_pulse)
            < fabs(he * char_time - range->hsync_pulse))
         he++;

      if (ht * char_time < hback_porch_min ||
            fabs((ht + 1) * char_time - range->hback_porch)
            < fabs(ht * char_time - range->hback_porch))
         ht++;

      new_char_time = line_time / (hh + hs + he + ht);
   } while (new_char_time != char_time);

   hhi = (hh + hs) * char_size;
   hhf = (hh + hs + he) * char_size;
   hht = (hh + hs + he + ht) * char_size;

   mode->hbegin  = hhi;
   mode->hend    = hhf;
   mode->htotal  = hht;

   return 0;
}

int modeline_create(video_modeline_t *s_mode, video_modeline_t *t_mode,
      video_modeline_range_t *range, video_modeline_gen_t *cs)
{
   double vfreq_real     = 0;
   double interlace      = 1;
   double doublescan     = 1;
   double scan_factor    = 1;
   int x_scale           = 0;
   int y_scale           = 0;
   int v_scale           = 0;
   double x_fscale       = 0;
   double y_fscale       = 0;
   double v_fscale       = 0;
   double x_diff         = 0;
   double y_diff         = 0;
   double v_diff         = 0;
   double y_ratio        = 0;
   double borders        = 0;
   int rotation          = s_mode->type & MODELINE_ROTATED;
   double source_aspect  = rotation
      ? 1.0 / (MODELINE_STANDARD_CRT_ASPECT)
      : (MODELINE_STANDARD_CRT_ASPECT);
   t_mode->result.weight = 0;

   /* Vertical refresh: fit the vertical frequency into this range */
   v_scale = scale_into_range_d(t_mode->vfreq, range->vfreq_min,
         range->vfreq_max);

   if (!v_scale && (t_mode->type & MODELINE_V_FREQ_EDITABLE))
   {
      t_mode->vfreq = t_mode->vfreq < range->vfreq_min
         ? range->vfreq_min : range->vfreq_max;
      v_scale       = 1;
   }
   else if (v_scale != 1 && !(t_mode->type & MODELINE_V_FREQ_EDITABLE))
   {
      t_mode->result.weight |= MODELINE_R_OUT_OF_RANGE;
      return -1;
   }

   /* Vertical resolution: progressive range first */
   if (range->progressive_lines_min
         && (!t_mode->interlace || (t_mode->type & MODELINE_SCAN_EDITABLE)))
      y_scale = scale_into_range_i(t_mode->vactive,
            range->progressive_lines_min, range->progressive_lines_max);

   /* then the interlaced range, if any */
   if (!y_scale && range->interlaced_lines_min && cs->interlace
         && (t_mode->interlace || (t_mode->type & MODELINE_SCAN_EDITABLE)))
   {
      y_scale = scale_into_range_i(t_mode->vactive,
            range->interlaced_lines_min, range->interlaced_lines_max);
      interlace = 2;
   }

   /* integer scaling */
   if (y_scale == 1
         || (y_scale > 1 && (t_mode->type & MODELINE_Y_RES_EDITABLE)))
   {
      int y_source_scaled;

      if (cs->doublescan && y_scale % 2 == 0)
      {
         y_scale   /= 2;
         doublescan = 0.5;
      }
      scan_factor = interlace * doublescan;

      /* Top border for multi-standard consumer TVs */
      if (cs->v_shift_correct)
         borders = (range->progressive_lines_max
               - t_mode->vactive * y_scale / interlace)
            * (1.0 / range->hfreq_min) / 2;

      /* Expected achievable refresh for this height */
      vfreq_real = MODELINE_MIN(t_mode->vfreq * v_scale,
            max_vfreq_for_yres(t_mode->vactive * y_scale, range, borders,
               scan_factor));
      if (vfreq_real != t_mode->vfreq * v_scale
            && !(t_mode->type & MODELINE_V_FREQ_EDITABLE))
      {
         t_mode->result.weight |= MODELINE_R_OUT_OF_RANGE;
         return -1;
      }

      /* Ratio of the scaled height against the original height */
      y_ratio         = (double)t_mode->vactive * y_scale / s_mode->vactive;
      y_source_scaled = (int)(s_mode->vactive * floor(y_ratio));

      /* Original height does not fit the target: stretch */
      if (!y_source_scaled)
         t_mode->result.weight |= MODELINE_R_RES_STRETCH;
      else
      {
         /* LCD ranges are excluded from raw border computation */
         if ((t_mode->type & MODELINE_V_FREQ_EDITABLE)
               && range->progressive_lines_max - range->progressive_lines_min > 0)
         {
            /* Borders in physical lines rather than logical resolution */
            int tot_yres   = total_lines_for_yres(t_mode->vactive * y_scale,
                  vfreq_real, range, borders, scan_factor);
            int tot_source = total_lines_for_yres(y_source_scaled,
                  t_mode->vfreq * v_scale, range, borders, scan_factor);
            int y_min, tot_rest;
            y_diff = tot_yres > tot_source
               ? (double)(tot_yres % tot_source) / tot_yres * 100 : 0;

            /* Penalize the logical lines added to meet the lower limit */
            y_min    = interlace == 2
               ? range->interlaced_lines_min : range->progressive_lines_min;
            tot_rest = (y_min >= y_source_scaled / doublescan)
               ? y_min % (int)(y_source_scaled / doublescan) : 0;
            y_diff  += (double)tot_rest / tot_yres * 100;
         }
         else
            y_diff = (double)((t_mode->vactive * y_scale) % y_source_scaled)
               / (t_mode->vactive * y_scale) * 100;

         /* Integer ratio between source and target, used for prescaling */
         y_scale = (int)floor(y_ratio);

         /* Borders under 10%: integer scaling, else stretch */
         if (!(y_ratio >= 1.0 && y_ratio < 16.0 && y_diff < 10.0))
            t_mode->result.weight |= MODELINE_R_RES_STRETCH;
      }
   }
   /* fractional scaling allowed */
   else if (t_mode->type & MODELINE_Y_RES_EDITABLE)
      t_mode->result.weight |= MODELINE_R_RES_STRETCH;
   else
   {
      t_mode->result.weight |= MODELINE_R_OUT_OF_RANGE;
      return -1;
   }

   /* Horizontal resolution: the scaled case */
   if (!(t_mode->result.weight & MODELINE_R_RES_STRETCH))
   {
      if (t_mode->type & MODELINE_Y_RES_EDITABLE)
         t_mode->vactive *= y_scale;

      if (t_mode->type & MODELINE_X_RES_EDITABLE)
      {
         double aspect_corrector;
         x_scale          = cs->scale_proportional ? y_scale : 1;
         aspect_corrector = MODELINE_MAX(1.0,
               cs->monitor_aspect / source_aspect);
         t_mode->hactive  = modeline_normalize(
               (int)((double)t_mode->hactive * (double)x_scale
                  * aspect_corrector), cs->pixel_precision ? 1 : 8);
      }
      else
      {
         x_scale = t_mode->hactive / s_mode->hactive;
         if (x_scale)
         {
            x_scale = scale_into_aspect(s_mode->hactive, t_mode->hactive,
                  source_aspect, cs->monitor_aspect, &x_diff);
            if (x_diff > 15.0 && t_mode->hactive < cs->super_width)
               t_mode->result.weight |= MODELINE_R_RES_STRETCH;
         }
         else
            t_mode->result.weight |= MODELINE_R_RES_STRETCH;
      }
   }

   /* Fractional scaling in any of the previous steps */
   if (t_mode->result.weight & MODELINE_R_RES_STRETCH)
   {
      if (t_mode->type & MODELINE_Y_RES_EDITABLE)
      {
         /* The interlaced range first if it exists, for better resolution */
         t_mode->vactive = stretch_into_range(t_mode->vfreq * v_scale,
               range, borders, cs->interlace ? true : false, &interlace);

         vfreq_real = MODELINE_MIN(t_mode->vfreq * v_scale,
               max_vfreq_for_yres(t_mode->vactive, range, borders,
                  interlace));
      }

      if (t_mode->type & MODELINE_X_RES_EDITABLE)
         t_mode->hactive = MODELINE_MAX(t_mode->hactive,
               modeline_normalize(
                  (int)(MODELINE_STANDARD_CRT_ASPECT * t_mode->vactive),
                  cs->pixel_precision ? 1 : 8));

      x_scale = MODELINE_MAX(1, scale_into_aspect(s_mode->hactive,
               t_mode->hactive, source_aspect, cs->monitor_aspect, &x_diff));
      y_scale = MODELINE_MAX(1,
            (int)floor((double)t_mode->vactive / s_mode->vactive));

      scan_factor = interlace;
      doublescan  = 1;
   }

   x_fscale = (double)t_mode->hactive / s_mode->hactive
      * source_aspect / cs->monitor_aspect;
   y_fscale = (double)t_mode->vactive / s_mode->vactive;
   v_fscale = vfreq_real / s_mode->vfreq;
   v_diff   = (vfreq_real / v_scale) - s_mode->vfreq;
   if (fabs(v_diff) > cs->refresh_tolerance)
      t_mode->result.weight |= MODELINE_R_V_FREQ_OFF;

   /* Modeline generation */
   if (t_mode->type & MODELINE_V_FREQ_EDITABLE)
   {
      double margin         = 0;
      double vblank_lines   = 0;
      double vvt_ini        = 0;
      double v_front_porch;
      double interlace_incr = !cs->interlace_force_even && interlace == 2
         ? 0.5 : 0;
      int (*pf_round)(double);

      t_mode->vfreq = vfreq_real;

      vvt_ini       = total_lines_for_yres(t_mode->vactive, t_mode->vfreq,
            range, borders, scan_factor) + interlace_incr;

      t_mode->hfreq = t_mode->vfreq * vvt_ini;

horizontal_values:

      get_line_params(t_mode, range, cs->pixel_precision ? 1 : 8);

      t_mode->pclock = (uint64_t)(t_mode->htotal * t_mode->hfreq);
      if (t_mode->pclock <= cs->pclock_min)
      {
         if (t_mode->type & MODELINE_X_RES_EDITABLE)
         {
            x_scale         *= 2;
            x_fscale        *= 2;
            t_mode->hactive *= 2;
            goto horizontal_values;
         }
         t_mode->result.weight |= MODELINE_R_OUT_OF_RANGE;
         return -1;
      }

      /* Vertical blanking */
      t_mode->vtotal = (int)(vvt_ini * scan_factor);
      vblank_lines   = modeline_round_near(t_mode->hfreq
            * (range->vertical_blank + borders)) + interlace_incr;
      margin         = (t_mode->vtotal - t_mode->vactive
            - vblank_lines * scan_factor) / (cs->v_shift_correct ? 1 : 2);

      v_front_porch  = margin + t_mode->hfreq * range->vfront_porch
         * scan_factor + interlace_incr;
      pf_round       = interlace == 2
         ? (cs->interlace_force_even
               ? modeline_round_near_even : modeline_round_near_odd)
         : modeline_round_near;

      t_mode->vbegin = t_mode->vactive + MODELINE_MAX(pf_round(v_front_porch), 1);
      t_mode->vend   = t_mode->vbegin + MODELINE_MAX(modeline_round_near(
               t_mode->hfreq * range->vsync_pulse * scan_factor), 1);

      /* Final vfreq */
      t_mode->vfreq      = (t_mode->hfreq / t_mode->vtotal) * scan_factor;

      t_mode->hsync      = range->hsync_polarity;
      t_mode->vsync      = range->vsync_polarity;
      t_mode->interlace  = interlace == 2 ? 1 : 0;
      t_mode->doublescan = doublescan == 1 ? 0 : 1;
   }

   t_mode->result.scan_penalty = (s_mode->interlace != t_mode->interlace ? 1 : 0)
      + (s_mode->doublescan != t_mode->doublescan ? 1 : 0);
   t_mode->result.x_scale = ((t_mode->result.weight & MODELINE_R_RES_STRETCH)
         || t_mode->hactive >= cs->super_width) ? x_fscale : (double)x_scale;
   t_mode->result.y_scale = (t_mode->result.weight & MODELINE_R_RES_STRETCH)
      ? y_fscale : (double)y_scale;
   t_mode->result.v_scale = v_fscale;
   t_mode->result.x_diff  = x_diff;
   t_mode->result.y_diff  = y_diff;
   t_mode->result.v_diff  = v_diff;

   return 0;
}

/* Sum of two doubles rounded to a float with two decimals, the way
 * the scoring compares xy_diff: the float intermediate is part of
 * the tie-break behaviour. */
static double xy_diff_score(double x_diff, double y_diff)
{
   double v = (double)(float)((x_diff + y_diff) * 100);
   v        = v < 0.0 ? ceil(v - 0.5) : floor(v + 0.5);
   return (double)((float)v / 100.0f);
}

int modeline_compare(video_modeline_t *t, video_modeline_t *best)
{
   bool vector = (t->hactive == (int)t->result.x_scale);

   if (t->result.weight < best->result.weight)
      return 1;
   else if (t->result.weight <= best->result.weight)
   {
      double t_v_diff = fabs(t->result.v_diff);
      double b_v_diff = fabs(best->result.v_diff);

      if ((t->result.weight & MODELINE_R_RES_STRETCH) || vector)
      {
         double t_y_score = t->result.y_scale * (t->interlace ? (2.0 / 3.0) : 1.0);
         double b_y_score = best->result.y_scale * (best->interlace ? (2.0 / 3.0) : 1.0);

         if ((t_v_diff <  b_v_diff)
               || ((t_v_diff == b_v_diff) && (t_y_score > b_y_score))
               || ((t_v_diff == b_v_diff) && (t_y_score == b_y_score)
                  && (t->result.x_scale > best->result.x_scale)))
            return 1;
      }
      else
      {
         int t_y_score       = (int)(t->result.y_scale + t->result.scan_penalty);
         int b_y_score       = (int)(best->result.y_scale + best->result.scan_penalty);
         double xy_diff      = xy_diff_score(t->result.x_diff, t->result.y_diff);
         double best_xy_diff = xy_diff_score(best->result.x_diff, best->result.y_diff);

         if ((t_y_score < b_y_score)
               || ((t_y_score == b_y_score) && (xy_diff < best_xy_diff))
               || ((t_y_score == b_y_score) && (xy_diff == best_xy_diff)
                  && (t->result.x_scale < best->result.x_scale))
               || ((t_y_score == b_y_score) && (xy_diff == best_xy_diff)
                  && (t->result.x_scale == best->result.x_scale)
                  && (t_v_diff < b_v_diff)))
            return 1;
      }
   }
   return 0;
}

/* Based on the VESA GTF spreadsheet by Andy Morrish 1/5/97 */
int modeline_vesa_gtf(video_modeline_t *m)
{
   int C, M;
   int v_sync_lines, v_porch_lines_min, v_front_porch_lines;
   int v_back_porch_lines, v_sync_v_back_porch_lines, v_total_lines;
   int h_sync_width_percent, h_sync_width_pixels, h_blanking_pixels;
   int h_front_porch_pixels, h_total_pixels;
   double v_freq, v_freq_est, v_freq_real, v_sync_v_back_porch;
   double h_freq, h_period, h_period_real, h_ideal_blanking;
   double pixel_freq, interlace;

   /* Input vfreq is the field rate regardless of interlace */
   v_freq               = m->vfreq ? m->vfreq : (double)m->refresh;

   /* GTF defaults */
   v_sync_lines         = 3;
   v_porch_lines_min    = 1;
   v_front_porch_lines  = v_porch_lines_min;
   v_sync_v_back_porch  = 550;
   h_sync_width_percent = 8;
   M                    = (int)(128.0 / 256 * 600);
   C                    = (int)(((40 - 20) * 128.0 / 256) + 20);

   interlace            = m->interlace ? 0.5 : 0;
   h_period             = ((1.0 / v_freq) - (v_sync_v_back_porch / 1000000))
      / ((double)m->height + v_front_porch_lines + interlace) * 1000000;
   v_sync_v_back_porch_lines = modeline_round_near(v_sync_v_back_porch / h_period);
   v_back_porch_lines   = v_sync_v_back_porch_lines - v_sync_lines;
   v_total_lines        = m->height + v_front_porch_lines + v_sync_lines
      + v_back_porch_lines;
   v_freq_est           = (1.0 / h_period) / v_total_lines * 1000000;
   h_period_real        = h_period / (v_freq / v_freq_est);
   v_freq_real          = (1.0 / h_period_real) / v_total_lines * 1000000;
   h_ideal_blanking     = (double)(C - (M * h_period_real / 1000));
   h_blanking_pixels    = modeline_round_near(m->width * h_ideal_blanking
         / (100 - h_ideal_blanking) / (2 * 8)) * (2 * 8);
   h_total_pixels       = m->width + h_blanking_pixels;
   pixel_freq           = h_total_pixels / h_period_real * 1000000;
   h_freq               = 1000000 / h_period_real;
   h_sync_width_pixels  = modeline_round_near(
         h_sync_width_percent * h_total_pixels / 100 / 8) * 8;
   h_front_porch_pixels = (h_blanking_pixels / 2) - h_sync_width_pixels;

   m->hactive = m->width;
   m->hbegin  = m->hactive + h_front_porch_pixels;
   m->hend    = m->hbegin + h_sync_width_pixels;
   m->htotal  = h_total_pixels;
   m->vactive = m->height;
   m->vbegin  = m->vactive + v_front_porch_lines;
   m->vend    = m->vbegin + v_sync_lines;
   m->vtotal  = v_total_lines;
   m->hfreq   = h_freq;
   m->vfreq   = v_freq_real;
   m->pclock  = (uint64_t)pixel_freq;
   m->hsync   = 0;
   m->vsync   = 1;

   return 1;
}

int modeline_parse(const char *user_modeline, video_modeline_t *mode)
{
   char txt[256];
   double pclock;
   int e;
   const char *quote_start, *quote_end;

   if (!strcmp(user_modeline, "auto"))
      return 0;

   /* Strip a quoted label */
   quote_start = strstr(user_modeline, "\"");
   if (quote_start)
   {
      quote_start++;
      quote_end = strstr(quote_start, "\"");
      if (!quote_end || *quote_end++ == 0)
         return 0;
      user_modeline = quote_end;
   }

   mode->interlace  = strstr(user_modeline, "interlace") ? 1 : 0;
   mode->doublescan = strstr(user_modeline, "doublescan") ? 1 : 0;
   mode->hsync      = strstr(user_modeline, "+hsync") ? 1 : 0;
   mode->vsync      = strstr(user_modeline, "+vsync") ? 1 : 0;

   e = sscanf(user_modeline, " %lf %d %d %d %d %d %d %d %d",
         &pclock,
         &mode->hactive, &mode->hbegin, &mode->hend, &mode->htotal,
         &mode->vactive, &mode->vbegin, &mode->vend, &mode->vtotal);

   if (e != 9)
   {
      RARCH_ERR("[Modeline] Missing parameter in user modeline: %s\n",
            user_modeline);
      memset(mode, 0, sizeof(*mode));
      return 0;
   }

   mode->pclock  = (uint64_t)(pclock * 1000000.0);
   /* Whole hertz: the line rate label is an integer division */
   mode->hfreq   = (double)(mode->pclock / (uint64_t)mode->htotal);
   mode->vfreq   = mode->hfreq / mode->vtotal * (mode->interlace ? 2 : 1);
   mode->refresh = (int)mode->vfreq;
   mode->width   = mode->hactive;
   mode->height  = mode->vactive;
   RARCH_DBG("[Modeline] User modeline %s\n",
         modeline_print(mode, txt, sizeof(txt), MODELINE_PRINT_FULL));

   return 1;
}

int modeline_to_monitor_range(video_modeline_range_t *range,
      video_modeline_t *mode)
{
   double line_time, pixel_time, interlace_factor;

   /* An empty vfreq range is created around the provided vfreq */
   if (range->vfreq_min == 0.0f)
      range->vfreq_min = mode->vfreq - 0.2;
   if (range->vfreq_max == 0.0f)
      range->vfreq_max = mode->vfreq + 0.2;

   if (mode->vfreq < range->vfreq_min || mode->vfreq > range->vfreq_max)
      return 0;

   line_time        = 1 / mode->hfreq;
   pixel_time       = line_time / mode->htotal * 1000000;
   interlace_factor = mode->interlace ? 0.5 : 1.0;

   range->hfront_porch = pixel_time * (mode->hbegin - mode->hactive);
   range->hsync_pulse  = pixel_time * (mode->hend - mode->hbegin);
   range->hback_porch  = pixel_time * (mode->htotal - mode->hend);

   /* The vertical fields are floored so the half line of interlaced
    * modes is not counted twice; the generator adds it back. */
   range->vfront_porch = line_time * floor((mode->vbegin - mode->vactive) * interlace_factor);
   range->vsync_pulse  = line_time * floor((mode->vend - mode->vbegin) * interlace_factor);
   range->vback_porch  = line_time * floor((mode->vtotal - mode->vend) * interlace_factor);
   range->vertical_blank = range->vfront_porch + range->vsync_pulse + range->vback_porch;

   range->hsync_polarity = mode->hsync;
   range->vsync_polarity = mode->vsync;

   range->progressive_lines_min = mode->interlace ? 0 : mode->vactive;
   range->progressive_lines_max = mode->interlace ? 0 : mode->vactive;
   range->interlaced_lines_min  = mode->interlace ? mode->vactive : 0;
   range->interlaced_lines_max  = mode->interlace ? mode->vactive : 0;

   range->hfreq_min = range->vfreq_min * mode->vtotal * interlace_factor;
   range->hfreq_max = range->vfreq_max * mode->vtotal * interlace_factor;

   return 1;
}

int modeline_adjust(video_modeline_t *mode, double hfreq_max,
      video_modeline_gen_t *cs)
{
   /* Out-of-range inputs are clamped and left in cs for the caller */

   /* H size, valid 0.5-2.0 */
   if (cs->h_size != 1.0f)
   {
      video_modeline_range_t range;

      if (cs->h_size > 2.0f)
         cs->h_size = 2.0f;
      else if (cs->h_size < 0.5f)
         cs->h_size = 0.5f;

      memset(&range, 0, sizeof(range));
      modeline_to_monitor_range(&range, mode);

      range.hfront_porch /= cs->h_size;
      range.hback_porch  /= cs->h_size;

      modeline_create(mode, mode, &range, cs);
   }

   /* H shift, positive or negative */
   if (cs->h_shift != 0)
   {
      if (cs->h_shift >= mode->hbegin - mode->hactive)
         cs->h_shift = mode->hbegin - mode->hactive - 1;
      else if (cs->h_shift <= mode->hend - mode->htotal)
         cs->h_shift = mode->hend - mode->htotal + 1;

      mode->hbegin -= cs->h_shift;
      mode->hend   -= cs->h_shift;
   }

   /* V shift, positive or negative */
   if (cs->v_shift != 0)
   {
      int vactive = mode->vactive;
      int vbegin  = mode->vbegin;
      int vend    = mode->vend;
      int vtotal  = mode->vtotal;
      int v_front_porch, v_back_porch, max_vtotal, border, padding;

      if (mode->interlace)
      {
         vactive >>= 1;
         vbegin  >>= 1;
         vend    >>= 1;
         vtotal  >>= 1;
      }

      v_front_porch = vbegin - vactive;
      v_back_porch  = vend - vtotal;
      max_vtotal    = (int)(hfreq_max / mode->vfreq);
      border        = max_vtotal - vtotal;
      padding       = 0;

      if (cs->v_shift >= v_front_porch)
      {
         int v_front_porch_ex = v_front_porch + border;
         if (cs->v_shift >= v_front_porch_ex)
            cs->v_shift = v_front_porch_ex - 1;

         padding = cs->v_shift - v_front_porch + 1;
         vbegin += padding;
         vend   += padding;
         vtotal += padding;
      }
      else if (cs->v_shift <= v_back_porch + 1)
      {
         int v_back_porch_ex = v_back_porch - border;
         if (cs->v_shift <= v_back_porch_ex + 1)
            cs->v_shift = v_back_porch_ex + 2;

         padding = -(cs->v_shift - v_back_porch - 2);
         vtotal += padding;
      }

      vbegin -= cs->v_shift;
      vend   -= cs->v_shift;

      if (mode->interlace)
      {
         vbegin = (vbegin << 1) | (mode->vbegin & 1);
         vend   = (vend << 1)   | (mode->vend & 1);
         vtotal = (vtotal << 1) | (mode->vtotal & 1);
      }

      mode->vbegin = vbegin;
      mode->vend   = vend;
      mode->vtotal = vtotal;

      if (padding != 0)
      {
         video_modeline_range_t range;
         mode->hfreq = mode->vfreq * mode->vtotal / (mode->interlace ? 2.0 : 1.0);

         memset(&range, 0, sizeof(range));
         modeline_to_monitor_range(&range, mode);
         modeline_monitor_show_range(&range);
         modeline_create(mode, mode, &range, cs);
      }
   }

   return 0;
}

int modeline_is_different(const video_modeline_t *n,
      const video_modeline_t *p)
{
   return n->pclock     != p->pclock
      ||  n->hactive    != p->hactive
      ||  n->hbegin     != p->hbegin
      ||  n->hend       != p->hend
      ||  n->htotal     != p->htotal
      ||  n->vactive    != p->vactive
      ||  n->vbegin     != p->vbegin
      ||  n->vend       != p->vend
      ||  n->vtotal     != p->vtotal
      ||  n->interlace  != p->interlace
      ||  n->doublescan != p->doublescan
      ||  n->hsync      != p->hsync
      ||  n->vsync      != p->vsync;
}

void modeline_copy_timings(video_modeline_t *n, const video_modeline_t *p)
{
   n->pclock     = p->pclock;
   n->hactive    = p->hactive;
   n->hbegin     = p->hbegin;
   n->hend       = p->hend;
   n->htotal     = p->htotal;
   n->vactive    = p->vactive;
   n->vbegin     = p->vbegin;
   n->vend       = p->vend;
   n->vtotal     = p->vtotal;
   n->interlace  = p->interlace;
   n->doublescan = p->doublescan;
   n->hsync      = p->hsync;
   n->vsync      = p->vsync;
   n->vfreq      = p->vfreq;
   n->hfreq      = p->hfreq;
}

char *modeline_print(const video_modeline_t *mode, char *s, size_t len,
      int flags)
{
   size_t _len = 0;
   s[0]        = '\0';

   if (flags & MODELINE_PRINT_LABEL)
      _len = (size_t)snprintf(s, len, "\"%dx%d_%d%s %.6fKHz %.6fHz\"",
            mode->hactive, mode->vactive, mode->refresh,
            mode->interlace ? "i" : "", mode->hfreq / 1000, mode->vfreq);

   if (flags & MODELINE_PRINT_LABEL_SDL)
      _len = (size_t)snprintf(s, len, "\"%dx%d_%.6f\"",
            mode->hactive, mode->vactive, mode->vfreq);

   if ((flags & MODELINE_PRINT_PARAMS) && _len < len)
      snprintf(s + _len, len - _len,
            " %.6f %d %d %d %d %d %d %d %d %s %s %s %s",
            (double)mode->pclock / 1000000.0,
            mode->hactive, mode->hbegin, mode->hend, mode->htotal,
            mode->vactive, mode->vbegin, mode->vend, mode->vtotal,
            mode->interlace ? "interlace" : "",
            mode->doublescan ? "doublescan" : "",
            mode->hsync ? "+hsync" : "-hsync",
            mode->vsync ? "+vsync" : "-vsync");

   return s;
}

char *modeline_result(const video_modeline_t *mode, char *s, size_t len)
{
   if (mode->result.weight & MODELINE_R_OUT_OF_RANGE)
      snprintf(s, len, "rng(%d): out of range", mode->range);
   else
      snprintf(s, len,
            "rng(%d): %4d x%4d_%3.6f%s%s %3.6f [%s] scale(%.3f, %.3f, %.3f) diff(%.3f, %.3f, %.3f)",
            mode->range,
            mode->hactive, mode->vactive, mode->vfreq,
            mode->interlace ? "i" : "p", mode->doublescan ? "d" : "",
            mode->hfreq / 1000,
            (mode->result.weight & MODELINE_R_RES_STRETCH) ? "fract" : "integ",
            mode->result.x_scale, mode->result.y_scale, mode->result.v_scale,
            mode->result.x_diff, mode->result.y_diff, mode->result.v_diff);
   return s;
}

int modeline_round_near(double number)
{
   return (int)(number < 0.0 ? ceil(number - 0.5) : floor(number + 0.5));
}

int modeline_round_near_odd(double number)
{
   return (int)((int)ceil(number) % 2 == 0 ? floor(number) : ceil(number));
}

int modeline_round_near_even(double number)
{
   return (int)((int)ceil(number) % 2 == 1 ? floor(number) : ceil(number));
}

int modeline_normalize(int a, int b)
{
   int c = a % b;
   int d = a / b;
   if (c)
      d++;
   return d * b;
}

int modeline_real_res(int x)
{
   return (int)(x / 8) * 8;
}

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

#include <string/stdstring.h>

#include "modeline_monitor.h"

#ifndef MODELINE_STANDALONE
#include "../../verbosity.h"
#endif

#define HFREQ_MIN  14000
#define HFREQ_MAX  540672 /* 8192 * 1.1 * 60 */
#define VFREQ_MIN  40
#define VFREQ_MAX  200
#define PROGRESSIVE_LINES_MIN 128

/* One preset: up to six range lines, NULL-terminated. */
typedef struct
{
   const char *name;
   const char *alias;
   const char *lines[7];
} modeline_preset_t;

static const modeline_preset_t modeline_presets[] = {
   /* PAL TV - 50 Hz/625 */
   { "pal", NULL,
      {
         "15625.00-15625.00, 50.00-50.00, 1.500, 4.700, 5.800, 0.064, 0.160, 1.056, 0, 0, 192, 288, 448, 576",
         NULL
      }
   },
   /* NTSC TV - 60 Hz/525 */
   { "ntsc", NULL,
      {
         "15734.26-15734.26, 59.94-59.94, 1.500, 4.700, 4.700, 0.191, 0.191, 0.953, 0, 0, 192, 240, 448, 480",
         NULL
      }
   },
   /* Generic 15.7 kHz */
   { "generic_15", NULL,
      {
         "15625-15750, 49.50-65.00, 2.000, 4.700, 8.000, 0.064, 0.192, 1.024, 0, 0, 192, 288, 448, 576",
         NULL
      }
   },
   /* Arcade 15.7 kHz - standard resolution */
   { "arcade_15", NULL,
      {
         "15625-16200, 49.50-65.00, 2.000, 4.700, 8.000, 0.064, 0.192, 1.024, 0, 0, 192, 288, 448, 576",
         NULL
      }
   },
   /* Arcade 15.7-16.5 kHz - extended resolution */
   { "arcade_15ex", NULL,
      {
         "15625-16500, 49.50-65.00, 2.000, 4.700, 8.000, 0.064, 0.192, 1.024, 0, 0, 192, 288, 448, 576",
         NULL
      }
   },
   /* Arcade 25.0 kHz - medium resolution */
   { "arcade_25", NULL,
      {
         "24960-24960, 49.50-65.00, 0.800, 4.000, 3.200, 0.080, 0.200, 1.000, 0, 0, 384, 400, 768, 800",
         NULL
      }
   },
   /* Arcade 31.5 kHz - medium resolution */
   { "arcade_31", NULL,
      {
         "31400-31500, 49.50-65.00, 0.940, 3.770, 1.890, 0.349, 0.064, 1.017, 0, 0, 400, 512, 0, 0",
         NULL
      }
   },
   /* Arcade 15.7/25.0 kHz - dual-sync */
   { "arcade_15_25", NULL,
      {
         "15625-16200, 49.50-65.00, 2.000, 4.700, 8.000, 0.064, 0.192, 1.024, 0, 0, 192, 288, 448, 576",
         "24960-24960, 49.50-65.00, 0.800, 4.000, 3.200, 0.080, 0.200, 1.000, 0, 0, 384, 400, 768, 800",
         NULL
      }
   },
   /* Arcade 15.7/31.5 kHz - dual-sync */
   { "arcade_15_31", NULL,
      {
         "15625-16200, 49.50-65.00, 2.000, 4.700, 8.000, 0.064, 0.192, 1.024, 0, 0, 192, 288, 448, 576",
         "31400-31500, 49.50-65.00, 0.940, 3.770, 1.890, 0.349, 0.064, 1.017, 0, 0, 400, 512, 0, 0",
         NULL
      }
   },
   /* Arcade 15.7/25.0/31.5 kHz - tri-sync */
   { "arcade_15_25_31", NULL,
      {
         "15625-16200, 49.50-65.00, 2.000, 4.700, 8.000, 0.064, 0.192, 1.024, 0, 0, 192, 288, 448, 576",
         "24960-24960, 49.50-65.00, 0.800, 4.000, 3.200, 0.080, 0.200, 1.000, 0, 0, 384, 400, 768, 800",
         "31400-31500, 49.50-65.00, 0.940, 3.770, 1.890, 0.349, 0.064, 1.017, 0, 0, 400, 512, 0, 0",
         NULL
      }
   },
   /* Makvision 2929D */
   { "m2929", NULL,
      {
         "30000-40000, 47.00-90.00, 0.600, 2.500, 2.800, 0.032, 0.096, 0.448, 0, 0, 384, 640, 0, 0",
         NULL
      }
   },
   /* Wells Gardner D9800, D9400 */
   { "d9800", "d9400",
      {
         "15250-18000, 40-80, 2.187, 4.688, 6.719, 0.190, 0.191, 1.018, 0, 0, 224, 288, 448, 576",
         "18001-19000, 40-80, 2.187, 4.688, 6.719, 0.140, 0.191, 0.950, 0, 0, 288, 320, 0, 0",
         "20501-29000, 40-80, 2.910, 3.000, 4.440, 0.451, 0.164, 1.048, 0, 0, 320, 384, 0, 0",
         "29001-32000, 40-80, 0.636, 3.813, 1.906, 0.318, 0.064, 1.048, 0, 0, 384, 480, 0, 0",
         "32001-34000, 40-80, 0.636, 3.813, 1.906, 0.020, 0.106, 0.607, 0, 0, 480, 576, 0, 0",
         "34001-38000, 40-80, 1.000, 3.200, 2.200, 0.020, 0.106, 0.607, 0, 0, 576, 600, 0, 0",
         NULL
      }
   },
   /* Wells Gardner D9200 */
   { "d9200", NULL,
      {
         "15250-16500, 40-80, 2.187, 4.688, 6.719, 0.190, 0.191, 1.018, 0, 0, 224, 288, 448, 576",
         "23900-24420, 40-80, 2.910, 3.000, 4.440, 0.451, 0.164, 1.148, 0, 0, 384, 400, 0, 0",
         "31000-32000, 40-80, 0.636, 3.813, 1.906, 0.318, 0.064, 1.048, 0, 0, 400, 512, 0, 0",
         "37000-38000, 40-80, 1.000, 3.200, 2.200, 0.020, 0.106, 0.607, 0, 0, 512, 600, 0, 0",
         NULL
      }
   },
   /* Wells Gardner K7000 */
   { "k7000", NULL,
      {
         "15625-15800, 49.50-63.00, 2.000, 4.700, 8.000, 0.064, 0.160, 1.056, 0, 0, 192, 288, 448, 576",
         NULL
      }
   },
   /* Wells Gardner 25K7131 */
   { "k7131", NULL,
      {
         "15625-16670, 49.5-65, 2.000, 4.700, 8.000, 0.064, 0.160, 1.056, 0, 0, 192, 288, 448, 576",
         NULL
      }
   },
   /* Wei-Ya M3129 */
   { "m3129", NULL,
      {
         "15250-16500, 40-80, 2.187, 4.688, 6.719, 0.190, 0.191, 1.018, 1, 1, 192, 288, 448, 576",
         "23900-24420, 40-80, 2.910, 3.000, 4.440, 0.451, 0.164, 1.048, 1, 1, 384, 400, 0, 0",
         "31000-32000, 40-80, 0.636, 3.813, 1.906, 0.318, 0.064, 1.048, 1, 1, 400, 512, 0, 0",
         NULL
      }
   },
   /* Hantarex MTC 9110 */
   { "h9110", "polo",
      {
         "15625-16670, 49.5-65, 2.000, 4.700, 8.000, 0.064, 0.160, 1.056, 0, 0, 192, 288, 448, 576",
         NULL
      }
   },
   /* Hantarex Polostar 25 */
   { "pstar", NULL,
      {
         "15700-15800, 50-65, 1.800, 0.400, 7.400, 0.064, 0.160, 1.056, 0, 0, 192, 256, 0, 0",
         "16200-16300, 50-65, 0.200, 0.400, 8.000, 0.040, 0.040, 0.640, 0, 0, 256, 264, 512, 528",
         "25300-25400, 50-65, 0.200, 0.400, 8.000, 0.040, 0.040, 0.640, 0, 0, 384, 400, 768, 800",
         "31500-31600, 50-65, 0.170, 0.350, 5.500, 0.040, 0.040, 0.640, 0, 0, 400, 512, 0, 0",
         NULL
      }
   },
   /* Nanao MS-2930, MS-2931 */
   { "ms2930", NULL,
      {
         "15450-16050, 50-65, 3.190, 4.750, 6.450, 0.191, 0.191, 1.164, 0, 0, 192, 288, 448, 576",
         "23900-24900, 50-65, 2.870, 3.000, 4.440, 0.451, 0.164, 1.148, 0, 0, 384, 400, 0, 0",
         "31000-32000, 50-65, 0.330, 3.580, 1.750, 0.316, 0.063, 1.137, 0, 0, 480, 512, 0, 0",
         NULL
      }
   },
   /* Nanao MS9-29 */
   { "ms929", NULL,
      {
         "15450-16050, 50-65, 3.910, 4.700, 6.850, 0.190, 0.191, 1.018, 0, 0, 192, 288, 448, 576",
         "23900-24900, 50-65, 2.910, 3.000, 4.440, 0.451, 0.164, 1.048, 0, 0, 384, 400, 0, 0",
         NULL
      }
   },
   /* Rodotron 666B-29 */
   { "r666b", NULL,
      {
         "15450-16050, 50-65, 3.190, 4.750, 6.450, 0.191, 0.191, 1.164, 0, 0, 192, 288, 448, 576",
         "23900-24900, 50-65, 2.870, 3.000, 4.440, 0.451, 0.164, 1.148, 0, 0, 384, 400, 0, 0",
         "31000-32500, 50-65, 0.330, 3.580, 1.750, 0.316, 0.063, 1.137, 0, 0, 400, 512, 0, 0",
         NULL
      }
   },
   /* PC CRT 31kHz/120Hz */
   { "pc_31_120", NULL,
      {
         "31400-31600, 100-130, 0.671, 2.683, 3.353, 0.034, 0.101, 0.436, 0, 0, 200, 256, 0, 0",
         "31400-31600, 50-65, 0.671, 2.683, 3.353, 0.034, 0.101, 0.436, 0, 0, 400, 512, 0, 0",
         NULL
      }
   },
   /* PC CRT 70kHz/120Hz */
   { "pc_70_120", NULL,
      {
         "30000-70000, 100-130, 2.201, 0.275, 4.678, 0.063, 0.032, 0.633, 0, 0, 192, 320, 0, 0",
         "30000-70000, 50-65, 2.201, 0.275, 4.678, 0.063, 0.032, 0.633, 0, 0, 400, 1024, 0, 0",
         NULL
      }
   },
};

int modeline_monitor_fill_range(video_modeline_range_t *range,
      const char *specs_line)
{
   video_modeline_range_t new_range;

   if (string_is_empty(specs_line))
      return 0;

   if (strcmp(specs_line, "auto"))
   {
      int e = sscanf(specs_line,
            "%lf-%lf,%lf-%lf,%lf,%lf,%lf,%lf,%lf,%lf,%d,%d,%d,%d,%d,%d",
            &new_range.hfreq_min, &new_range.hfreq_max,
            &new_range.vfreq_min, &new_range.vfreq_max,
            &new_range.hfront_porch, &new_range.hsync_pulse, &new_range.hback_porch,
            &new_range.vfront_porch, &new_range.vsync_pulse, &new_range.vback_porch,
            &new_range.hsync_polarity, &new_range.vsync_polarity,
            &new_range.progressive_lines_min, &new_range.progressive_lines_max,
            &new_range.interlaced_lines_min, &new_range.interlaced_lines_max);

      if (e != 16)
      {
         RARCH_ERR("[Modeline] Error trying to fill monitor range with: %s\n",
               specs_line);
         return -1;
      }

      new_range.vfront_porch   /= 1000;
      new_range.vsync_pulse    /= 1000;
      new_range.vback_porch    /= 1000;
      new_range.vertical_blank  = (new_range.vfront_porch
            + new_range.vsync_pulse + new_range.vback_porch);

      if (modeline_monitor_evaluate_range(&new_range))
      {
         RARCH_ERR("[Modeline] Error in monitor range (ignoring): %s\n",
               specs_line);
         return -1;
      }

      memcpy(range, &new_range, sizeof(*range));
      modeline_monitor_show_range(range);
   }
   return 0;
}

int modeline_monitor_fill_lcd_range(video_modeline_range_t *range,
      const char *specs_line)
{
   if (string_is_empty(specs_line))
      return 0;

   if (strcmp(specs_line, "auto"))
   {
      if (sscanf(specs_line, "%lf-%lf", &range->vfreq_min, &range->vfreq_max) == 2)
      {
         RARCH_DBG("[Modeline] LCD vfreq range set by user as %f-%f\n",
               range->vfreq_min, range->vfreq_max);
         return 1;
      }
      RARCH_ERR("[Modeline] Error trying to fill LCD range with: %s\n",
            specs_line);
   }
   /* Defaults */
   range->vfreq_min = 59;
   range->vfreq_max = 61;
   RARCH_DBG("[Modeline] Using default vfreq range for LCD %f-%f\n",
         range->vfreq_min, range->vfreq_max);

   return 0;
}

int modeline_monitor_show_range(video_modeline_range_t *range)
{
   RARCH_DBG("[Modeline] Monitor range %.2f-%.2f,%.2f-%.2f,%.3f,%.3f,%.3f,%.3f,%.3f,%.3f,%d,%d,%d,%d,%d,%d\n",
         range->hfreq_min, range->hfreq_max,
         range->vfreq_min, range->vfreq_max,
         range->hfront_porch, range->hsync_pulse, range->hback_porch,
         range->vfront_porch * 1000, range->vsync_pulse * 1000, range->vback_porch * 1000,
         range->hsync_polarity, range->vsync_polarity,
         range->progressive_lines_min, range->progressive_lines_max,
         range->interlaced_lines_min, range->interlaced_lines_max);

   return 0;
}

int modeline_monitor_set_preset(const char *type,
      video_modeline_range_t *range)
{
   size_t i;

   for (i = 0; i < sizeof(modeline_presets) / sizeof(modeline_presets[0]); i++)
   {
      const modeline_preset_t *p = &modeline_presets[i];
      if (!strcmp(type, p->name) || (p->alias && !strcmp(type, p->alias)))
      {
         int n = 0;
         while (p->lines[n])
         {
            modeline_monitor_fill_range(&range[n], p->lines[n]);
            n++;
         }
         return n;
      }
   }

   /* VESA GTF */
   if (!strcmp(type, "vesa_480") || !strcmp(type, "vesa_600")
         || !strcmp(type, "vesa_768") || !strcmp(type, "vesa_1024"))
      return modeline_monitor_fill_vesa_gtf(&range[0], type);

   RARCH_ERR("[Modeline] Monitor type unknown: %s\n", type);
   return 0;
}

int modeline_monitor_fill_vesa_gtf(video_modeline_range_t *range,
      const char *max_lines)
{
   int lines = 0;
   int i     = 0;
   sscanf(max_lines, "vesa_%d", &lines);

   if (!lines)
      return 0;

   if (lines >= 480)
      i += modeline_monitor_fill_vesa_range(&range[i], 384, 480);
   if (lines >= 600)
      i += modeline_monitor_fill_vesa_range(&range[i], 480, 600);
   if (lines >= 768)
      i += modeline_monitor_fill_vesa_range(&range[i], 600, 768);
   if (lines >= 1024)
      i += modeline_monitor_fill_vesa_range(&range[i], 768, 1024);

   return i;
}

int modeline_monitor_fill_vesa_range(video_modeline_range_t *range,
      int lines_min, int lines_max)
{
   video_modeline_t mode;
   memset(&mode, 0, sizeof(mode));

   mode.width       = modeline_real_res((int)(MODELINE_STANDARD_CRT_ASPECT * lines_max));
   mode.height      = lines_max;
   mode.refresh     = 60;
   range->vfreq_min = 50;
   range->vfreq_max = 65;

   modeline_vesa_gtf(&mode);
   modeline_to_monitor_range(range, &mode);

   range->progressive_lines_min = lines_min;
   range->hfreq_min             = mode.hfreq - 500;
   range->hfreq_max             = mode.hfreq + 500;
   modeline_monitor_show_range(range);

   return 1;
}

int modeline_monitor_evaluate_range(video_modeline_range_t *range)
{
   double line_time, frame_time;

   /* Frequency bands */
   if (range->hfreq_min < HFREQ_MIN || range->hfreq_min > HFREQ_MAX)
   {
      RARCH_ERR("[Modeline] hfreq_min %.2f out of range\n", range->hfreq_min);
      return 1;
   }
   if (range->hfreq_max < HFREQ_MIN || range->hfreq_max < range->hfreq_min
         || range->hfreq_max > HFREQ_MAX)
   {
      RARCH_ERR("[Modeline] hfreq_max %.2f out of range\n", range->hfreq_max);
      return 1;
   }
   if (range->vfreq_min < VFREQ_MIN || range->vfreq_min > VFREQ_MAX)
   {
      RARCH_ERR("[Modeline] vfreq_min %.2f out of range\n", range->vfreq_min);
      return 1;
   }
   if (range->vfreq_max < VFREQ_MIN || range->vfreq_max < range->vfreq_min
         || range->vfreq_max > VFREQ_MAX)
   {
      RARCH_ERR("[Modeline] vfreq_max %.2f out of range\n", range->vfreq_max);
      return 1;
   }

   /* No horizontal value may be longer than a whole line (us) */
   line_time = 1 / range->hfreq_max * 1000000;

   if (range->hfront_porch <= 0 || range->hfront_porch > line_time)
   {
      RARCH_ERR("[Modeline] hfront_porch %.3f out of range\n", range->hfront_porch);
      return 1;
   }
   if (range->hsync_pulse <= 0 || range->hsync_pulse > line_time)
   {
      RARCH_ERR("[Modeline] hsync_pulse %.3f out of range\n", range->hsync_pulse);
      return 1;
   }
   if (range->hback_porch <= 0 || range->hback_porch > line_time)
   {
      RARCH_ERR("[Modeline] hback_porch %.3f out of range\n", range->hback_porch);
      return 1;
   }

   /* No vertical value may be longer than a whole frame (ms) */
   frame_time = 1 / range->vfreq_max * 1000;

   if (range->vfront_porch <= 0 || range->vfront_porch > frame_time)
   {
      RARCH_ERR("[Modeline] vfront_porch %.3f out of range\n", range->vfront_porch);
      return 1;
   }
   if (range->vsync_pulse <= 0 || range->vsync_pulse > frame_time)
   {
      RARCH_ERR("[Modeline] vsync_pulse %.3f out of range\n", range->vsync_pulse);
      return 1;
   }
   if (range->vback_porch <= 0 || range->vback_porch > frame_time)
   {
      RARCH_ERR("[Modeline] vback_porch %.3f out of range\n", range->vback_porch);
      return 1;
   }

   /* Polarities */
   if (range->hsync_polarity != 0 && range->hsync_polarity != 1)
   {
      RARCH_ERR("[Modeline] Hsync polarity can be only 0 or 1\n");
      return 1;
   }
   if (range->vsync_polarity != 0 && range->vsync_polarity != 1)
   {
      RARCH_ERR("[Modeline] Vsync polarity can be only 0 or 1\n");
      return 1;
   }

   /* Progressive line limits */
   if (range->progressive_lines_min > 0
         && range->progressive_lines_min < PROGRESSIVE_LINES_MIN)
   {
      RARCH_ERR("[Modeline] progressive_lines_min must be greater than %d\n",
            PROGRESSIVE_LINES_MIN);
      return 1;
   }
   if ((range->progressive_lines_min + range->hfreq_max * range->vertical_blank)
         * range->vfreq_min > range->hfreq_max)
   {
      RARCH_ERR("[Modeline] progressive_lines_min %d out of range\n",
            range->progressive_lines_min);
      return 1;
   }
   if (range->progressive_lines_max < range->progressive_lines_min)
   {
      RARCH_ERR("[Modeline] progressive_lines_max must greater than progressive_lines_min\n");
      return 1;
   }
   if ((range->progressive_lines_max + range->hfreq_max * range->vertical_blank)
         * range->vfreq_min > range->hfreq_max)
   {
      RARCH_ERR("[Modeline] progressive_lines_max %d out of range\n",
            range->progressive_lines_max);
      return 1;
   }

   /* Interlaced line limits */
   if (range->interlaced_lines_min != 0)
   {
      if (range->interlaced_lines_min < range->progressive_lines_max)
      {
         RARCH_ERR("[Modeline] interlaced_lines_min must greater than progressive_lines_max\n");
         return 1;
      }
      if (range->interlaced_lines_min < PROGRESSIVE_LINES_MIN * 2)
      {
         RARCH_ERR("[Modeline] interlaced_lines_min must be greater than %d\n",
               PROGRESSIVE_LINES_MIN * 2);
         return 1;
      }
      if ((range->interlaced_lines_min / 2 + range->hfreq_max * range->vertical_blank)
            * range->vfreq_min > range->hfreq_max)
      {
         RARCH_ERR("[Modeline] interlaced_lines_min %d out of range\n",
               range->interlaced_lines_min);
         return 1;
      }
      if (range->interlaced_lines_max < range->interlaced_lines_min)
      {
         RARCH_ERR("[Modeline] interlaced_lines_max must greater than interlaced_lines_min\n");
         return 1;
      }
      if ((range->interlaced_lines_max / 2 + range->hfreq_max * range->vertical_blank)
            * range->vfreq_min > range->hfreq_max)
      {
         RARCH_ERR("[Modeline] interlaced_lines_max %d out of range\n",
               range->interlaced_lines_max);
         return 1;
      }
   }
   else if (range->interlaced_lines_max != 0)
   {
      RARCH_ERR("[Modeline] interlaced_lines_max must be zero if interlaced_lines_min is not defined\n");
      return 1;
   }
   return 0;
}

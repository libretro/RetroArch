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

#include "modeline_edid.h"
#include "modeline_list.h"

/* Layout after the EDID 1.3 base block; the fixed bytes describe a
 * 48x36 cm analog RGB display with separate syncs and no established
 * or standard timings, so the only mode on offer is the detailed one. */
bool modeline_edid_build(const video_modeline_t *mode,
      const video_modeline_range_t *range, const char *name,
      uint8_t out[MODELINE_EDID_SIZE])
{
   int i;
   unsigned checksum;
   unsigned pclock_10k;
   int h_active, h_blank, h_offset, h_pulse;
   int v_active, v_blank, v_offset, v_pulse;
   static const uint8_t fixed_header[54] = {
      /* header */
      0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00,
      /* manufacturer "RAR", product, serial, week, year (2026) */
      0x48, 0x32, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 5, 2026 - 1990,
      /* EDID 1.3, analog input, 48x36 cm, gamma 2.2, features */
      1, 3, 0x6d, 48, 36, 120, 0x0a,
      /* chromaticity */
      0x5e, 0xc0, 0xa4, 0x59, 0x4a, 0x98, 0x25, 0x20, 0x50, 0x54,
      /* established timings: none */
      0x00, 0x00, 0x00,
      /* standard timings: none */
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01,
      0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01, 0x01
   };

   if (!mode || !range || !out || mode->pclock == 0 || mode->htotal == 0)
      return false;

   memset(out, 0, MODELINE_EDID_SIZE);
   memcpy(out, fixed_header, sizeof(fixed_header));

   /* Detailed timing: the preferred mode */
   pclock_10k = (unsigned)(mode->pclock / 10000);
   h_active   = mode->hactive;
   h_blank    = mode->htotal - mode->hactive;
   h_offset   = mode->hbegin - mode->hactive;
   h_pulse    = mode->hend - mode->hbegin;
   v_active   = mode->vactive;
   v_blank    = mode->vtotal - mode->vactive;
   v_offset   = mode->vbegin - mode->vactive;
   v_pulse    = mode->vend - mode->vbegin;

   out[54] = pclock_10k & 0xff;
   out[55] = (pclock_10k >> 8) & 0xff;
   out[56] = h_active & 0xff;
   out[57] = h_blank & 0xff;
   out[58] = (((h_active >> 8) & 0x0f) << 4) | ((h_blank >> 8) & 0x0f);
   out[59] = v_active & 0xff;
   out[60] = v_blank & 0xff;
   out[61] = (((v_active >> 8) & 0x0f) << 4) | ((v_blank >> 8) & 0x0f);
   out[62] = h_offset & 0xff;
   out[63] = h_pulse & 0xff;
   out[64] = ((v_offset & 0x0f) << 4) | (v_pulse & 0x0f);
   out[65] = (((h_offset >> 8) & 0x03) << 6)
           | (((h_pulse >> 8) & 0x03) << 4)
           | (((v_offset >> 8) & 0x03) << 2)
           |  ((v_pulse >> 8) & 0x03);
   /* Image size 485x364 mm */
   out[66] = 485 & 0xff;
   out[67] = 364 & 0xff;
   out[68] = (((485 >> 8) & 0x0f) << 4) | ((364 >> 8) & 0x0f);
   out[69] = 0;
   out[70] = 0;
   /* Interlace, separate syncs (bits 4-3), vsync polarity bit 2,
    * hsync polarity bit 1 */
   out[71] = ((mode->interlace & 0x01) << 7) | 0x18
           | ((mode->vsync & 0x01) << 2) | ((mode->hsync & 0x01) << 1);

   /* Descriptor: serial number string */
   out[75] = 0xff;
   memcpy(out + 77, "RetroArch\n", 10);
   for (i = 87; i < 90; i++)
      out[i] = 0x20;

   /* Descriptor: monitor range limits, Hz and kHz */
   out[93]  = 0xfd;
   out[95]  = ((int)range->vfreq_min) & 0xff;
   out[96]  = ((int)range->vfreq_max) & 0xff;
   out[97]  = ((int)range->hfreq_min / 1000) & 0xff;
   out[98]  = ((int)range->hfreq_max / 1000) & 0xff;
   out[99]  = 0xff; /* max pixel clock: not specified */
   out[100] = 0;    /* no secondary GTF */
   out[101] = 0x0a;
   for (i = 102; i < 108; i++)
      out[i] = 0x20;

   /* Descriptor: monitor name, 13 characters, newline terminated */
   out[111] = 0xfc;
   for (i = 0; i < 13; i++)
   {
      char c = name ? name[i] : '\0';
      if (!c)
         break;
      out[113 + i] = (uint8_t)c;
   }
   if (i < 13)
      out[113 + i++] = 0x0a;
   for (; i < 13; i++)
      out[113 + i] = 0x20;

   /* No extension blocks; checksum makes the block sum to zero */
   out[126] = 0;
   checksum = 0;
   for (i = 0; i < 127; i++)
      checksum += out[i];
   out[127] = (uint8_t)((256 - (checksum & 0xff)) & 0xff);
   return true;
}

bool modeline_edid_for_gen(video_modeline_gen_t *gen,
      uint8_t out[MODELINE_EDID_SIZE])
{
   int i;
   video_modeline_t *mode;
   video_modeline_ops_t ops;

   if (!gen)
      return false;

   for (i = 0; i < MODELINE_MAX_RANGES; i++)
      if (gen->range[i].hfreq_min != 0)
         break;
   if (i == MODELINE_MAX_RANGES)
      return false;

   /* A generation-only list: the preset's 320x240@60 as a new mode */
   memset(&ops, 0, sizeof(ops));
   ops.name = "edid";
   modeline_list_init(gen, &ops);
   mode = modeline_get(gen, &ops, 320, 240, 60.0, 0);
   if (!mode)
      return false;
   return modeline_edid_build(mode, &gen->range[mode->range], gen->monitor, out);
}

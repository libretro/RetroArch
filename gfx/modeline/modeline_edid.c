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

#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <compat/strl.h>

#include "modeline_edid.h"
#include "modeline_list.h"
#include "modeline_monitor.h"

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

/* ---- Reading ---- */

static bool edid_checksum_ok(const uint8_t *block)
{
   int i;
   unsigned sum = 0;
   for (i = 0; i < MODELINE_EDID_SIZE; i++)
      sum += block[i];
   return (sum & 0xff) == 0;
}

/* 18-byte detailed timing descriptor -> timing; false when the
 * descriptor is not a timing (pixel clock 0) */
static bool edid_parse_dtd(const uint8_t *d, uint8_t src,
      video_edid_timing_t *t)
{
   unsigned pclock_10k = d[0] | (d[1] << 8);
   if (pclock_10k == 0)
      return false;
   memset(t, 0, sizeof(*t));
   t->src       = src;
   t->pclock    = pclock_10k * 10000;
   t->hactive   = d[2] | ((d[4] & 0xf0) << 4);
   t->hblank    = d[3] | ((d[4] & 0x0f) << 8);
   t->vactive   = d[5] | ((d[7] & 0xf0) << 4);
   t->vblank    = d[6] | ((d[7] & 0x0f) << 8);
   t->hfront    = d[8]  | ((d[11] & 0xc0) << 2);
   t->hsync     = d[9]  | ((d[11] & 0x30) << 4);
   t->vfront    = (d[10] >> 4)  | ((d[11] & 0x0c) << 2);
   t->vsync     = (d[10] & 0x0f) | ((d[11] & 0x03) << 4);
   t->hsize_mm  = d[12] | ((d[14] & 0xf0) << 4);
   t->vsize_mm  = d[13] | ((d[14] & 0x0f) << 8);
   t->hborder   = d[15];
   t->vborder   = d[16];
   t->interlace = (d[17] & 0x80) != 0;
   t->stereo    = (d[17] >> 5) & 0x03;
   t->sync_type = (d[17] >> 3) & 0x03;
   /* Polarity bits only mean anything for separate syncs; analog
    * modes use bits 2-1 for serration and sync-on-green instead */
   if (t->sync_type == 3)
   {
      t->vsync_pos = (d[17] & 0x04) != 0;
      t->hsync_pos = (d[17] & 0x02) != 0;
   }
   return true;
}

/* DisplayID type I (v1.x, tag 0x03) and type VII (v2.0, tag 0x22)
 * share the 20-byte layout */
static bool edid_parse_did_timing(const uint8_t *d, video_edid_timing_t *t)
{
   unsigned pclock_10k = d[0] | (d[1] << 8) | (d[2] << 16);
   if (pclock_10k == 0)
      return false;
   memset(t, 0, sizeof(*t));
   t->src       = MODELINE_EDID_SRC_DISPLAYID;
   t->pclock    = (pclock_10k + 1) * 10000;
   t->preferred = (d[3] & 0x80) != 0;
   t->interlace = (d[3] & 0x10) != 0;
   t->stereo    = (d[3] >> 5) & 0x03;
   t->sync_type = 3;
   t->hactive   = (d[4]  | (d[5]  << 8)) + 1;
   t->hblank    = (d[6]  | (d[7]  << 8)) + 1;
   t->hfront    = (d[8]  | ((d[9]  & 0x7f) << 8)) + 1;
   t->hsync_pos = (d[9]  & 0x80) != 0;
   t->hsync     = (d[10] | (d[11] << 8)) + 1;
   t->vactive   = (d[12] | (d[13] << 8)) + 1;
   t->vblank    = (d[14] | (d[15] << 8)) + 1;
   t->vfront    = (d[16] | ((d[17] & 0x7f) << 8)) + 1;
   t->vsync_pos = (d[17] & 0x80) != 0;
   t->vsync     = (d[18] | (d[19] << 8)) + 1;
   return true;
}

static void edid_add_timing(video_edid_info_t *info,
      const video_edid_timing_t *t)
{
   if (info->n_timings < MODELINE_EDID_MAX_TIMINGS)
      info->timing[info->n_timings++] = *t;
}

static void edid_copy_text(char *out, const uint8_t *d)
{
   int i;
   for (i = 0; i < 13; i++)
   {
      if (d[i] == 0x0a || d[i] == 0)
         break;
      /* the spec pads with spaces and forbids control characters */
      out[i] = (d[i] < 0x20 || d[i] > 0x7e) ? ' ' : (char)d[i];
   }
   while (i > 0 && out[i - 1] == ' ')
      i--;
   out[i] = '\0';
}

static void edid_add_std(video_edid_info_t *info, uint8_t b1, uint8_t b2,
      bool pre13)
{
   video_edid_std_timing_t *s;
   unsigned w;
   if ((b1 == 0x01 && b2 == 0x01) || b1 == 0
         || info->n_std >= MODELINE_EDID_MAX_STD)
      return;
   s  = &info->std[info->n_std];
   w  = (b1 + 31) * 8;
   s->width   = w;
   s->refresh = (b2 & 0x3f) + 60;
   switch (b2 >> 6)
   {
      case 0:  s->height = pre13 ? w : w * 10 / 16; break;
      case 1:  s->height = w * 3 / 4;  break;
      case 2:  s->height = w * 4 / 5;  break;
      default: s->height = w * 9 / 16; break;
   }
   info->n_std++;
}

static void edid_parse_descriptor(video_edid_info_t *info,
      const uint8_t *d, bool pre13)
{
   video_edid_timing_t t;
   int i;

   if (d[0] != 0 || d[1] != 0 || d[2] != 0)
   {
      if (edid_parse_dtd(d, MODELINE_EDID_SRC_BASE, &t))
         edid_add_timing(info, &t);
      return;
   }

   switch (d[3])
   {
      case 0xff:
         edid_copy_text(info->serial_text, d + 5);
         break;
      case 0xfe:
         if (!info->text[0])
            edid_copy_text(info->text, d + 5);
         break;
      case 0xfc:
         edid_copy_text(info->name, d + 5);
         break;
      case 0xfd:
         info->has_range   = true;
         info->vfreq_min   = d[5];
         info->vfreq_max   = d[6];
         info->hfreq_min   = d[7];
         info->hfreq_max   = d[8];
         /* 1.4: byte 4 flags add 255 to the max, or to both */
         if (!pre13)
         {
            if (d[4] & 0x02) info->vfreq_max += 255;
            if (d[4] & 0x01) { info->vfreq_min += 255; info->vfreq_max += 255; }
            if (d[4] & 0x08) info->hfreq_max += 255;
            if (d[4] & 0x04) { info->hfreq_min += 255; info->hfreq_max += 255; }
         }
         info->pclock_max = (d[9] == 0xff) ? 0 : d[9] * 10;
         info->range_type = d[10];
         break;
      case 0xfa:
         for (i = 0; i < 6; i++)
            edid_add_std(info, d[5 + i * 2], d[6 + i * 2], pre13);
         break;
      default:
         break;
   }
}

static void edid_parse_cta(video_edid_ext_t *e, const uint8_t *b,
      video_edid_info_t *info)
{
   unsigned pos, dtd_off, end;
   video_edid_timing_t t;

   e->revision = b[1];
   dtd_off     = b[2];
   if (e->revision >= 2)
   {
      e->underscan   = (b[3] & 0x80) != 0;
      e->basic_audio = (b[3] & 0x40) != 0;
      e->ycbcr444    = (b[3] & 0x20) != 0;
      e->ycbcr422    = (b[3] & 0x10) != 0;
      e->native_dtds = b[3] & 0x0f;
   }

   /* Data block collection, revision 3 and up */
   end = dtd_off;
   if (end > MODELINE_EDID_SIZE - 1)
      end = MODELINE_EDID_SIZE - 1;
   pos = 4;
   while (e->revision >= 3 && dtd_off >= 4 && pos + 1 <= end)
   {
      unsigned tag = b[pos] >> 5;
      unsigned n   = b[pos] & 0x1f;
      const uint8_t *p = b + pos + 1;
      unsigned i;
      if (pos + 1 + n > end)
         break;
      switch (tag)
      {
         case 1:
            e->n_audio += n / 3;
            break;
         case 2:
            for (i = 0; i < n && e->n_vics < MODELINE_EDID_MAX_VICS; i++)
               e->vic[e->n_vics++] = p[i];
            break;
         case 3:
            if (n >= 5 && p[0] == 0x03 && p[1] == 0x0c && p[2] == 0x00)
            {
               e->hdmi         = true;
               e->hdmi_phys[0] = p[3];
               e->hdmi_phys[1] = p[4];
               if (n >= 7)
                  e->hdmi_max_tmds = p[6] * 5;
            }
            else if (n >= 5 && p[0] == 0xd8 && p[1] == 0x5d && p[2] == 0xc4)
            {
               e->hdmi_forum  = true;
               e->hf_max_tmds = p[4] * 5;
            }
            break;
         case 7:
            if (n >= 1)
            {
               switch (p[0])
               {
                  case 0x05:
                     e->colorimetry = true;
                     if (n >= 2)
                        e->colorimetry_flags = p[1];
                     break;
                  case 0x06:
                     e->hdr = true;
                     if (n >= 2) e->hdr_eotf    = p[1];
                     if (n >= 4) e->hdr_max_lum = p[3];
                     if (n >= 5) e->hdr_max_fal = p[4];
                     if (n >= 6) e->hdr_min_lum = p[5];
                     break;
                  case 0x0e:
                  case 0x0f:
                     e->ycbcr420 = true;
                     break;
                  default:
                     break;
               }
            }
            break;
         default:
            break;
      }
      pos += 1 + n;
   }

   /* Detailed timings from dtd_off to the checksum */
   if (dtd_off >= 4)
   {
      pos = dtd_off;
      while (pos + 18 <= MODELINE_EDID_SIZE - 1)
      {
         if (!edid_parse_dtd(b + pos, MODELINE_EDID_SRC_CTA, &t))
            break;
         edid_add_timing(info, &t);
         pos += 18;
      }
   }
}

static void edid_parse_displayid(video_edid_ext_t *e, const uint8_t *b,
      video_edid_info_t *info)
{
   unsigned pos, end;
   video_edid_timing_t t;
   /* byte 1: DisplayID version, byte 2: bytes in the section, byte 3:
    * product type, byte 4: extension count; sections follow */
   e->revision    = b[1];
   e->did_product = b[3];
   end            = 5 + b[2];
   if (end > MODELINE_EDID_SIZE - 1)
      end = MODELINE_EDID_SIZE - 1;
   pos = 5;
   while (pos + 3 <= end)
   {
      uint8_t  tag = b[pos];
      unsigned n   = b[pos + 2];
      unsigned i;
      bool timing_block;
      if (pos + 3 + n > end)
         break;
      if (e->n_sections < MODELINE_EDID_MAX_SECTIONS)
         e->section[e->n_sections++] = tag;
      timing_block = (e->revision >= 0x20) ? (tag == 0x22) : (tag == 0x03);
      if (timing_block)
         for (i = 0; i + 20 <= n; i += 20)
            if (edid_parse_did_timing(b + pos + 3 + i, &t))
               edid_add_timing(info, &t);
      pos += 3 + n;
   }
}

bool modeline_edid_parse(const uint8_t *data, size_t len,
      video_edid_info_t *info)
{
   static const uint8_t header[8] =
      { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
   int i;
   bool pre13;
   unsigned mfr;

   if (!info)
      return false;
   memset(info, 0, sizeof(*info));
   if (!data || len < MODELINE_EDID_SIZE)
      return false;
   if (len > MODELINE_EDID_MAX_LEN)
      len = MODELINE_EDID_MAX_LEN;

   info->len         = len;
   info->n_blocks    = (uint8_t)(len / MODELINE_EDID_SIZE);
   info->header_ok   = memcmp(data, header, 8) == 0;
   if (!info->header_ok)
      return false;
   info->checksum_ok = edid_checksum_ok(data);

   /* Vendor and product */
   mfr = (data[8] << 8) | data[9];
   info->manufacturer[0] = (char)('A' - 1 + ((mfr >> 10) & 0x1f));
   info->manufacturer[1] = (char)('A' - 1 + ((mfr >>  5) & 0x1f));
   info->manufacturer[2] = (char)('A' - 1 + ( mfr        & 0x1f));
   info->manufacturer[3] = '\0';
   for (i = 0; i < 3; i++)
      if (info->manufacturer[i] < 'A' || info->manufacturer[i] > 'Z')
         info->manufacturer[i] = '?';
   info->product   = data[10] | (data[11] << 8);
   info->serial    = data[12] | (data[13] << 8) | (data[14] << 16)
                   | ((uint32_t)data[15] << 24);
   info->ver_major = data[18];
   info->ver_minor = data[19];
   pre13           = (info->ver_major == 1 && info->ver_minor < 3);
   info->week      = data[16];
   if (data[17] != 0)
      info->year   = 1990 + data[17];
   /* 1.4: week 0xff makes byte 17 a model year; 1.3 reserves it */
   if (info->week == 0xff)
   {
      info->model_year = !pre13 && info->ver_minor >= 4;
      info->week       = 0;
   }
   else if (info->week > 54)
      info->week = 0;

   /* Video input */
   info->digital = (data[20] & 0x80) != 0;
   if (info->digital)
   {
      if (info->ver_major == 1 && info->ver_minor >= 4)
      {
         unsigned bpc = (data[20] >> 4) & 0x07;
         info->bit_depth = (bpc >= 1 && bpc <= 6) ? (uint8_t)(4 + bpc * 2) : 0;
         info->interface = data[20] & 0x0f;
      }
      else
         info->dfp1x = (data[20] & 0x01) != 0;
   }
   else
   {
      info->analog_level = (data[20] >> 5) & 0x03;
      info->analog_sync  = data[20] & 0x1f;
   }

   info->width_cm   = data[21];
   info->height_cm  = data[22];
   info->gamma_x100 = (data[23] == 0xff) ? 0 : 100 + data[23];
   info->features   = data[24];

   /* Chromaticity, 10 bits each, in thousandths */
   info->red_x   = (((data[27] << 2) | ((data[25] >> 6) & 3)) * 1000 + 512) >> 10;
   info->red_y   = (((data[28] << 2) | ((data[25] >> 4) & 3)) * 1000 + 512) >> 10;
   info->green_x = (((data[29] << 2) | ((data[25] >> 2) & 3)) * 1000 + 512) >> 10;
   info->green_y = (((data[30] << 2) | ( data[25]       & 3)) * 1000 + 512) >> 10;
   info->blue_x  = (((data[31] << 2) | ((data[26] >> 6) & 3)) * 1000 + 512) >> 10;
   info->blue_y  = (((data[32] << 2) | ((data[26] >> 4) & 3)) * 1000 + 512) >> 10;
   info->white_x = (((data[33] << 2) | ((data[26] >> 2) & 3)) * 1000 + 512) >> 10;
   info->white_y = (((data[34] << 2) | ( data[26]       & 3)) * 1000 + 512) >> 10;

   /* Established timings: byte 35 bit 7 = 720x400@70 ... byte 37
    * bit 7 = 1152x870@75; kept in that reading order */
   info->established = 0;
   for (i = 0; i < 8; i++)
   {
      if (data[35] & (0x80 >> i)) info->established |= 1u << i;
      if (data[36] & (0x80 >> i)) info->established |= 1u << (8 + i);
   }
   if (data[37] & 0x80)
      info->established |= 1u << 16;
   info->est_mfr = data[37] & 0x7f;

   for (i = 0; i < 8; i++)
      edid_add_std(info, data[38 + i * 2], data[39 + i * 2], pre13);

   /* Descriptors; the first is the preferred timing when the feature
    * bit says so (always, from 1.3 on) */
   for (i = 0; i < 4; i++)
   {
      uint8_t before = info->n_timings;
      edid_parse_descriptor(info, data + 54 + i * 18, pre13);
      if (i == 0 && info->n_timings > before
            && (!pre13 || (info->features & 0x02)))
         info->timing[0].preferred = true;
   }

   /* Extensions */
   info->n_ext_declared = data[126];
   info->truncated      = (info->n_blocks < 1u + info->n_ext_declared);
   for (i = 1; i < (int)info->n_blocks && i <= 1 + (int)info->n_ext_declared
         && info->n_ext < MODELINE_EDID_MAX_BLOCKS - 1; i++)
   {
      const uint8_t *b   = data + i * MODELINE_EDID_SIZE;
      video_edid_ext_t *e = &info->ext[info->n_ext++];
      e->tag         = b[0];
      e->checksum_ok = edid_checksum_ok(b);
      switch (e->tag)
      {
         case MODELINE_EDID_EXT_CTA:
            edid_parse_cta(e, b, info);
            break;
         case MODELINE_EDID_EXT_DISPLAYID:
            edid_parse_displayid(e, b, info);
            break;
         default:
            e->revision = b[1];
            break;
      }
   }
   return true;
}

/* ---- Formatting ---- */

size_t modeline_edid_timing_str(const video_edid_timing_t *t,
      char *s, size_t len)
{
   double htotal, vtotal, hfreq, vfreq;
   if (!t || !s || !len)
      return 0;
   htotal = (double)(t->hactive + t->hblank);
   vtotal = (double)(t->vactive + t->vblank);
   hfreq  = htotal > 0 ? (double)t->pclock / htotal : 0.0;
   vfreq  = vtotal > 0 ? hfreq / vtotal * (t->interlace ? 2.0 : 1.0) : 0.0;
   return (size_t)snprintf(s, len, "%ux%u%c %.2f Hz, %.2f MHz, %.2f kHz",
         t->hactive, t->vactive, t->interlace ? 'i' : 'p',
         vfreq, (double)t->pclock / 1000000.0, hfreq / 1000.0);
}

size_t modeline_edid_modeline_str(const video_edid_timing_t *t,
      char *s, size_t len)
{
   if (!t || !s || !len)
      return 0;
   return (size_t)snprintf(s, len,
         "Modeline \"%ux%u\" %.3f %u %u %u %u %u %u %u %u %chsync %cvsync%s",
         t->hactive, t->vactive, (double)t->pclock / 1000000.0,
         t->hactive, t->hactive + t->hfront,
         t->hactive + t->hfront + t->hsync, t->hactive + t->hblank,
         t->vactive, t->vactive + t->vfront,
         t->vactive + t->vfront + t->vsync, t->vactive + t->vblank,
         t->hsync_pos ? '+' : '-', t->vsync_pos ? '+' : '-',
         t->interlace ? " interlace" : "");
}

/* The area a set of primaries encloses in the xy plane */
static double edid_gamut_area(const double *p)
{
   return fabs((p[2] - p[0]) * (p[5] - p[1])
             - (p[4] - p[0]) * (p[3] - p[1])) / 2.0;
}

size_t modeline_edid_gamut_str(const video_edid_info_t *info,
      char *s, size_t len)
{
   /* The standards a display's primaries are likely to sit on, red,
    * green then blue. sRGB and BT.709 share theirs, as do DCI-P3 and
    * Display P3 - the difference there is the white point, so the
    * two are told apart by that. */
   static const struct
   {
      const char *name;
      double p[6];
      /* Only where two standards share primaries and the white point
       * is what separates them; 0 elsewhere, so a panel calibrated to
       * another white still gets its primaries named. */
      double white_x, white_y;
   } gamuts[] = {
      { "sRGB / BT.709",  { 0.640, 0.330, 0.300, 0.600, 0.150, 0.060 }, 0.0, 0.0 },
      { "Display P3",     { 0.680, 0.320, 0.265, 0.690, 0.150, 0.060 }, 0.3127, 0.3290 },
      { "DCI-P3",         { 0.680, 0.320, 0.265, 0.690, 0.150, 0.060 }, 0.3140, 0.3510 },
      { "Adobe RGB",      { 0.640, 0.330, 0.210, 0.710, 0.150, 0.060 }, 0.0, 0.0 },
      { "BT.2020",        { 0.708, 0.292, 0.170, 0.797, 0.131, 0.046 }, 0.0, 0.0 },
      { "BT.601 (SMPTE C)", { 0.630, 0.340, 0.310, 0.595, 0.155, 0.070 }, 0.0, 0.0 }
   };
   static const struct
   {
      const char *name;
      double x, y;
   } whites[] = {
      { "D65", 0.3127, 0.3290 },
      { "D50", 0.3457, 0.3585 },
      { "DCI", 0.3140, 0.3510 },
      { "D75", 0.2990, 0.3150 },
      { "D93", 0.2831, 0.2971 }
   };
   static const double srgb[6] =
      { 0.640, 0.330, 0.300, 0.600, 0.150, 0.060 };
   double p[6], wx, wy, best = 0.0, area;
   const char *gamut = NULL;
   const char *white = NULL;
   size_t _len;
   int i;

   if (!info || !s || !len)
      return 0;
   if (!info->red_x && !info->green_x && !info->blue_x && !info->white_x)
      return 0;

   p[0] = info->red_x   / 1000.0; p[1] = info->red_y   / 1000.0;
   p[2] = info->green_x / 1000.0; p[3] = info->green_y / 1000.0;
   p[4] = info->blue_x  / 1000.0; p[5] = info->blue_y  / 1000.0;
   wx   = info->white_x / 1000.0; wy   = info->white_y / 1000.0;

   /* A standard's name only when every primary lands on it. EDID
    * stores each coordinate in ten bits, about a thousandth, and
    * panels are binned rather than exact, so the window is 0.015 -
    * wide enough for a real display, far too tight to confuse the
    * gamuts above with each other. */
   for (i = 0; i < (int)(sizeof(gamuts) / sizeof(gamuts[0])); i++)
   {
      double worst = 0.0;
      int j;
      for (j = 0; j < 6; j++)
      {
         double d = p[j] - gamuts[i].p[j];
         if (d < 0.0)
            d = -d;
         if (d > worst)
            worst = d;
      }
      if (worst > 0.015)
         continue;
      /* Same primaries, different white: DCI-P3 against Display P3.
       * Everything else takes its name from the primaries alone. */
      if (gamuts[i].white_x && (wx || wy))
      {
         double dw = fabs(wx - gamuts[i].white_x) + fabs(wy - gamuts[i].white_y);
         if (dw > 0.010)
            continue;
      }
      gamut = gamuts[i].name;
      break;
   }

   for (i = 0; i < (int)(sizeof(whites) / sizeof(whites[0])); i++)
   {
      double dw = fabs(wx - whites[i].x) + fabs(wy - whites[i].y);
      if (dw <= 0.006)
      {
         white = whites[i].name;
         break;
      }
   }

   /* The area against sRGB's: not a coverage figure, which needs the
    * two triangles intersected, but the size of what the display can
    * show against the size of what sRGB asks for. */
   area = edid_gamut_area(srgb);
   best = area > 0.0 ? edid_gamut_area(p) / area * 100.0 : 0.0;

   _len = strlcpy(s, gamut ? gamut : "custom primaries", len);
   if (white && _len < len)
      _len += snprintf(s + _len, len - _len, ", %s", white);
   if (best > 0.0 && _len < len)
      _len += snprintf(s + _len, len - _len, ", %.0f%% of sRGB area", best);
   return _len;
}

const char *modeline_edid_established_name(unsigned bit)
{
   static const char *names[17] = {
      "720x400 @ 70 Hz", "720x400 @ 88 Hz", "640x480 @ 60 Hz",
      "640x480 @ 67 Hz", "640x480 @ 72 Hz", "640x480 @ 75 Hz",
      "800x600 @ 56 Hz", "800x600 @ 60 Hz",
      "800x600 @ 72 Hz", "800x600 @ 75 Hz", "832x624 @ 75 Hz",
      "1024x768i @ 87 Hz", "1024x768 @ 60 Hz", "1024x768 @ 70 Hz",
      "1024x768 @ 75 Hz", "1280x1024 @ 75 Hz",
      "1152x870 @ 75 Hz"
   };
   return bit < 17 ? names[bit] : "";
}

/* CTA-861-H short video descriptor codes 1-107: width, height,
 * refresh, interlaced */
typedef struct
{
   uint16_t w;
   uint16_t h;
   uint8_t  hz;
   uint8_t  i;
} edid_vic_t;

static const edid_vic_t edid_vics[] = {
   {  640,  480,  60, 0 }, {  720,  480,  60, 0 }, {  720,  480,  60, 0 },
   { 1280,  720,  60, 0 }, { 1920, 1080,  60, 1 }, {  720,  480,  60, 1 },
   {  720,  480,  60, 1 }, {  720,  240,  60, 0 }, {  720,  240,  60, 0 },
   { 2880,  480,  60, 1 }, { 2880,  480,  60, 1 }, { 2880,  240,  60, 0 },
   { 2880,  240,  60, 0 }, { 1440,  480,  60, 0 }, { 1440,  480,  60, 0 },
   { 1920, 1080,  60, 0 }, {  720,  576,  50, 0 }, {  720,  576,  50, 0 },
   { 1280,  720,  50, 0 }, { 1920, 1080,  50, 1 }, {  720,  576,  50, 1 },
   {  720,  576,  50, 1 }, {  720,  288,  50, 0 }, {  720,  288,  50, 0 },
   { 2880,  576,  50, 1 }, { 2880,  576,  50, 1 }, { 2880,  288,  50, 0 },
   { 2880,  288,  50, 0 }, { 1440,  576,  50, 0 }, { 1440,  576,  50, 0 },
   { 1920, 1080,  50, 0 }, { 1920, 1080,  24, 0 }, { 1920, 1080,  25, 0 },
   { 1920, 1080,  30, 0 }, { 2880,  480,  60, 0 }, { 2880,  480,  60, 0 },
   { 2880,  576,  50, 0 }, { 2880,  576,  50, 0 }, { 1920, 1080,  50, 1 },
   { 1920, 1080, 100, 1 }, { 1280,  720, 100, 0 }, {  720,  576, 100, 0 },
   {  720,  576, 100, 0 }, {  720,  576, 100, 1 }, {  720,  576, 100, 1 },
   { 1920, 1080, 120, 1 }, { 1280,  720, 120, 0 }, {  720,  480, 120, 0 },
   {  720,  480, 120, 0 }, {  720,  480, 120, 1 }, {  720,  480, 120, 1 },
   {  720,  576, 200, 0 }, {  720,  576, 200, 0 }, {  720,  576, 200, 1 },
   {  720,  576, 200, 1 }, {  720,  480, 240, 0 }, {  720,  480, 240, 0 },
   {  720,  480, 240, 1 }, {  720,  480, 240, 1 }, { 1280,  720,  24, 0 },
   { 1280,  720,  25, 0 }, { 1280,  720,  30, 0 }, { 1920, 1080, 120, 0 },
   { 1920, 1080, 100, 0 }, { 1280,  720,  24, 0 }, { 1280,  720,  25, 0 },
   { 1280,  720,  30, 0 }, { 1280,  720,  50, 0 }, { 1280,  720,  60, 0 },
   { 1280,  720, 100, 0 }, { 1280,  720, 120, 0 }, { 1920, 1080,  24, 0 },
   { 1920, 1080,  25, 0 }, { 1920, 1080,  30, 0 }, { 1920, 1080,  50, 0 },
   { 1920, 1080,  60, 0 }, { 1920, 1080, 100, 0 }, { 1920, 1080, 120, 0 },
   { 1680,  720,  24, 0 }, { 1680,  720,  25, 0 }, { 1680,  720,  30, 0 },
   { 1680,  720,  50, 0 }, { 1680,  720,  60, 0 }, { 1680,  720, 100, 0 },
   { 1680,  720, 120, 0 }, { 2560, 1080,  24, 0 }, { 2560, 1080,  25, 0 },
   { 2560, 1080,  30, 0 }, { 2560, 1080,  50, 0 }, { 2560, 1080,  60, 0 },
   { 2560, 1080, 100, 0 }, { 2560, 1080, 120, 0 }, { 3840, 2160,  24, 0 },
   { 3840, 2160,  25, 0 }, { 3840, 2160,  30, 0 }, { 3840, 2160,  50, 0 },
   { 3840, 2160,  60, 0 }, { 4096, 2160,  24, 0 }, { 4096, 2160,  25, 0 },
   { 4096, 2160,  30, 0 }, { 4096, 2160,  50, 0 }, { 4096, 2160,  60, 0 },
   { 3840, 2160,  24, 0 }, { 3840, 2160,  25, 0 }, { 3840, 2160,  30, 0 },
   { 3840, 2160,  50, 0 }, { 3840, 2160,  60, 0 }
};

size_t modeline_edid_vic_str(unsigned vic, char *s, size_t len)
{
   const edid_vic_t *v;
   if (!s || !len)
      return 0;
   if (vic == 0 || vic > sizeof(edid_vics) / sizeof(edid_vics[0]))
      return (size_t)snprintf(s, len, "VIC %u", vic);
   v = &edid_vics[vic - 1];
   return (size_t)snprintf(s, len, "%ux%u%c%u", v->w, v->h,
         v->i ? 'i' : 'p', v->hz);
}

const char *modeline_edid_ext_name(uint8_t tag)
{
   switch (tag)
   {
      case MODELINE_EDID_EXT_CTA:       return "CTA-861";
      case MODELINE_EDID_EXT_VTB:       return "VTB-EXT";
      case MODELINE_EDID_EXT_DI:        return "DI-EXT";
      case MODELINE_EDID_EXT_LS:        return "LS-EXT";
      case MODELINE_EDID_EXT_DPVL:      return "DPVL-EXT";
      case MODELINE_EDID_EXT_DISPLAYID: return "DisplayID";
      case MODELINE_EDID_EXT_BLOCKMAP:  return "Block Map";
      case MODELINE_EDID_EXT_VENDOR:    return "Manufacturer";
      default:                          return "Unknown";
   }
}

const char *modeline_edid_did_section_name(uint8_t version, uint8_t tag)
{
   if (version >= 0x20)
   {
      switch (tag)
      {
         case 0x20: return "Product Identification";
         case 0x21: return "Display Parameters";
         case 0x22: return "Type VII Timing";
         case 0x23: return "Type VIII Timing";
         case 0x24: return "Type IX Timing";
         case 0x25: return "Dynamic Video Timing Range";
         case 0x26: return "Display Interface Features";
         case 0x27: return "Stereo Display Interface";
         case 0x28: return "Tiled Display Topology";
         case 0x29: return "Container ID";
         case 0x2a: return "Type X Timing";
         case 0x2b: return "Adaptive Sync";
         case 0x32: return "Brightness Luminance Range";
         case 0x7e: return "Vendor Specific";
         case 0x7f: return "CTA DisplayID";
         default:   return "Unknown";
      }
   }
   switch (tag)
   {
      case 0x00: return "Product Identification";
      case 0x01: return "Display Parameters";
      case 0x02: return "Color Characteristics";
      case 0x03: return "Type I Timing";
      case 0x04: return "Type II Timing";
      case 0x05: return "Type III Timing";
      case 0x06: return "Type IV Timing";
      case 0x07: return "VESA Timing";
      case 0x08: return "CEA Timing";
      case 0x09: return "Video Timing Range";
      case 0x0a: return "Product Serial Number";
      case 0x0b: return "General Purpose ASCII String";
      case 0x0c: return "Display Device Data";
      case 0x0d: return "Interface Power Sequencing";
      case 0x0e: return "Transfer Characteristics";
      case 0x0f: return "Display Interface";
      case 0x10: return "Stereo Display Interface";
      case 0x11: return "Type V Timing";
      case 0x12: return "Tiled Display Topology";
      case 0x13: return "Type VI Timing";
      case 0x7f: return "Vendor Specific";
      default:   return "Unknown";
   }
}

/* ---- Ranges from the range limits descriptor ---- */

int modeline_edid_fill_ranges(const video_edid_info_t *info,
      video_modeline_range_t *range, int max)
{
   /* Each band: its limits in Hz and the preset whose first range
    * lends the blanking; NULL means VESA GTF */
   static const struct
   {
      double lo, hi;
      const char *tmpl;
   } bands[] = {
      { 14000.0, 20000.0, "arcade_15" },
      { 20000.0, 28000.0, "arcade_25" },
      { 28000.0, 40000.0, "arcade_31" },
      { 40000.0, 540000.0, NULL }
   };
   double hmin, hmax, vmin, vmax;
   int b, n = 0;

   if (!info || !range || max <= 0 || !info->has_range
         || !info->hfreq_min || !info->hfreq_max || !info->vfreq_min || !info->vfreq_max)
      return 0;

   /* The descriptor holds whole kHz and Hz, rounded by the monitor's
    * maker: a 15.734 kHz set that says 15-16 must still admit 15.734,
    * and one that says 15-15 must too, so the maxima extend to the
    * next whole unit */
   hmin = (double)info->hfreq_min * 1000.0;
   hmax = (double)info->hfreq_max * 1000.0 + 999.0;
   vmin = (double)info->vfreq_min;
   vmax = (double)info->vfreq_max + 0.99;
   if (vmin < 40.0)
      vmin = 40.0;
   if (vmax > 200.0)
      vmax = 200.0;
   if (hmax <= hmin || vmax <= vmin)
      return 0;

   for (b = 0; b < (int)(sizeof(bands) / sizeof(bands[0])) && n < max; b++)
   {
      double lo = hmin > bands[b].lo ? hmin : bands[b].lo;
      double hi = hmax < bands[b].hi ? hmax : bands[b].hi;
      video_modeline_range_t r;
      if (hi <= lo)
         continue;
      memset(&r, 0, sizeof(r));
      if (bands[b].tmpl)
      {
         video_modeline_range_t tmpl[MODELINE_MAX_RANGES];
         memset(tmpl, 0, sizeof(tmpl));
         if (modeline_monitor_set_preset(bands[b].tmpl, tmpl) < 1)
            continue;
         r = tmpl[0];
      }
      else
      {
         /* GTF blanking for the tallest picture the band's top rate
          * scans at 60 Hz with 5% vertical blanking, bounded to what
          * the generator lists */
         int lines_max = (int)(hi / (60.0 * 1.05));
         if (lines_max > 2048)
            lines_max = 2048;
         if (lines_max < 480)
            lines_max = 480;
         modeline_monitor_fill_vesa_range(&r, 480, lines_max);
      }
      r.hfreq_min = lo;
      r.hfreq_max = hi;
      r.vfreq_min = vmin;
      r.vfreq_max = vmax;
      if (modeline_monitor_evaluate_range(&r))
         continue;
      range[n++] = r;
   }
   return n;
}

int modeline_edid_super_width(const uint8_t *data, size_t len,
      double hfreq_max, int want)
{
   static const int widths[] = { 3840, 2560, 1920 };
   video_edid_info_t *info;
   double max_hz;
   int i, w = want;

   if (!data || len < MODELINE_EDID_SIZE || want < 1920 || hfreq_max <= 0.0)
      return want;
   info = (video_edid_info_t*)calloc(1, sizeof(*info));
   if (!info)
      return want;
   max_hz = (modeline_edid_parse(data, len, info) && info->has_range)
      ? (double)info->pclock_max * 1000000.0 : 0.0;
   free(info);
   if (max_hz <= 0.0)
      return want;

   /* A width needs width / 0.75 pixels per line at hfreq_max lines
    * per second; the first listed width at or under the wanted one
    * whose clock fits is the answer */
   for (i = 0; i < (int)(sizeof(widths) / sizeof(widths[0])); i++)
   {
      if (widths[i] > want)
         continue;
      w = widths[i];
      if ((double)w / 0.75 * hfreq_max <= max_hz)
         return w;
   }
   return w;
}

/* ---- Synthesis ---- */

/* One 18-byte detailed timing descriptor */
static void edid_write_dtd(uint8_t *d, const video_edid_timing_t *t,
      unsigned width_mm, unsigned height_mm)
{
   unsigned pclock_10k = (t->pclock + 5000) / 10000;
   unsigned hblank     = t->hblank;
   unsigned vblank     = t->vblank;

   if (pclock_10k > 0xffff)
      pclock_10k = 0xffff;
   d[0]  = (uint8_t)(pclock_10k & 0xff);
   d[1]  = (uint8_t)(pclock_10k >> 8);
   d[2]  = (uint8_t)(t->hactive & 0xff);
   d[3]  = (uint8_t)(hblank & 0xff);
   d[4]  = (uint8_t)((((t->hactive >> 8) & 0x0f) << 4) | ((hblank >> 8) & 0x0f));
   d[5]  = (uint8_t)(t->vactive & 0xff);
   d[6]  = (uint8_t)(vblank & 0xff);
   d[7]  = (uint8_t)((((t->vactive >> 8) & 0x0f) << 4) | ((vblank >> 8) & 0x0f));
   d[8]  = (uint8_t)(t->hfront & 0xff);
   d[9]  = (uint8_t)(t->hsync & 0xff);
   d[10] = (uint8_t)(((t->vfront & 0x0f) << 4) | (t->vsync & 0x0f));
   d[11] = (uint8_t)((((t->hfront >> 8) & 0x03) << 6)
                   | (((t->hsync  >> 8) & 0x03) << 4)
                   | (((t->vfront >> 4) & 0x03) << 2)
                   |  ((t->vsync  >> 4) & 0x03));
   d[12] = (uint8_t)(width_mm & 0xff);
   d[13] = (uint8_t)(height_mm & 0xff);
   d[14] = (uint8_t)((((width_mm >> 8) & 0x0f) << 4) | ((height_mm >> 8) & 0x0f));
   d[15] = 0;
   d[16] = 0;
   /* interlace, digital separate sync, polarities */
   d[17] = (uint8_t)((t->interlace ? 0x80 : 0x00) | 0x18
         | (t->vsync_pos ? 0x04 : 0x00) | (t->hsync_pos ? 0x02 : 0x00));
}

/* A 0xFC / 0xFE / 0xFF string descriptor, newline-terminated and
 * space-padded the way the spec asks */
static void edid_write_text(uint8_t *d, uint8_t tag, const char *s)
{
   int i;
   memset(d, 0, 18);
   d[3] = tag;
   for (i = 0; i < 13 && s && s[i]; i++)
      d[5 + i] = (uint8_t)s[i];
   if (i < 13)
      d[5 + i++] = 0x0a;
   for (; i < 13; i++)
      d[5 + i] = 0x20;
}

/* A 10-bit fraction of 1.0, which is how the chromaticity bytes hold
 * a coordinate */
static unsigned edid_chroma10(unsigned thousandths)
{
   return (thousandths * 1024u + 500u) / 1000u;
}

size_t modeline_edid_synthesize(const video_edid_synth_t *in,
      uint8_t *out, size_t max)
{
   static const uint8_t header[8] =
      { 0x00, 0xff, 0xff, 0xff, 0xff, 0xff, 0xff, 0x00 };
   unsigned checksum = 0;
   unsigned vmin, vmax, hmin_khz, hmax_khz;
   int i, slot;

   if (!in || !out || max < MODELINE_EDID_SIZE || in->n_timings < 1)
      return 0;

   memset(out, 0, MODELINE_EDID_SIZE);
   memcpy(out, header, sizeof(header));

   /* Vendor and product */
   out[8]  = (uint8_t)((in->vendor >> 8) & 0xff);
   out[9]  = (uint8_t)(in->vendor & 0xff);
   out[10] = (uint8_t)(in->product & 0xff);
   out[11] = (uint8_t)((in->product >> 8) & 0xff);
   out[12] = (uint8_t)(in->serial & 0xff);
   out[13] = (uint8_t)((in->serial >> 8) & 0xff);
   out[14] = (uint8_t)((in->serial >> 16) & 0xff);
   out[15] = (uint8_t)((in->serial >> 24) & 0xff);
   out[16] = 0;  /* week: not reported */
   out[17] = (in->year >= 1990 && in->year < 2245)
      ? (uint8_t)(in->year - 1990) : 0;
   out[18] = 1;
   out[19] = 4;  /* the 1.4 encodings are what this fills */

   /* Digital input, bit depth and interface where they are known */
   out[20] = 0x80;
   if (in->bit_depth >= 6 && in->bit_depth <= 16)
      out[20] |= (uint8_t)((((in->bit_depth - 4) / 2) & 0x07) << 4);
   out[20] |= (uint8_t)(in->interface & 0x0f);

   /* Physical size, in whole centimetres as the block stores it */
   out[21] = (uint8_t)((in->width_mm  + 5) / 10);
   out[22] = (uint8_t)((in->height_mm + 5) / 10);
   /* Gamma, when the profile gave one; 0xff is "undefined" */
   out[23] = (in->gamma_x100 >= 100 && in->gamma_x100 <= 355)
      ? (uint8_t)(in->gamma_x100 - 100) : 0xff;
   /* Preferred timing is the first descriptor, continuous frequency,
    * plus whatever colour formats the source reported. No
    * power-management claims and no sRGB claim: the source said
    * nothing about either. */
   out[24] = 0x02 | 0x01;
   if (in->ycbcr444)
      out[24] |= 0x08;
   if (in->ycbcr422)
      out[24] |= 0x10;

   /* Chromaticity, when the display's colour profile supplied the
    * primaries; all zero otherwise, the encoding for "not stated".
    * Inventing primaries here would read as measured data. */
   if (in->white_x || in->red_x)
   {
      unsigned c[8];
      c[0] = edid_chroma10(in->red_x);   c[1] = edid_chroma10(in->red_y);
      c[2] = edid_chroma10(in->green_x); c[3] = edid_chroma10(in->green_y);
      c[4] = edid_chroma10(in->blue_x);  c[5] = edid_chroma10(in->blue_y);
      c[6] = edid_chroma10(in->white_x); c[7] = edid_chroma10(in->white_y);
      /* the low two bits of each live packed in bytes 25-26 */
      out[25] = (uint8_t)(((c[0] & 3) << 6) | ((c[1] & 3) << 4)
                        | ((c[2] & 3) << 2) |  (c[3] & 3));
      out[26] = (uint8_t)(((c[4] & 3) << 6) | ((c[5] & 3) << 4)
                        | ((c[6] & 3) << 2) |  (c[7] & 3));
      for (i = 0; i < 8; i++)
         out[27 + i] = (uint8_t)(c[i] >> 2);
   }

   /* No established or standard timings: the source reports its modes
    * as detailed timings, and a guessed DMT bitmap would be a claim
    * nothing made. 0x01 is the unused standard-timing encoding. */
   for (i = 0; i < 8; i++)
      out[38 + i * 2] = out[39 + i * 2] = 0x01;

   /* Descriptor 1: the preferred timing */
   edid_write_dtd(out + 54, &in->timing[0], in->width_mm, in->height_mm);

   /* Descriptors 2-4, filled in order of usefulness until the four
    * slots run out: a second timing, the range limits, the monitor
    * name, then the origin note */
   vmin     = in->vfreq_min;
   vmax     = in->vfreq_max;
   hmin_khz = (in->hfreq_min + 999) / 1000;
   hmax_khz = (in->hfreq_max + 999) / 1000;
   slot     = 1;

   if (in->n_timings > 1 && slot < 4)
      edid_write_dtd(out + 54 + slot++ * 18, &in->timing[1],
            in->width_mm, in->height_mm);

   if (vmin && vmax && hmin_khz && hmax_khz
         && vmax < 256 && hmax_khz < 256 && slot < 4)
   {
      uint8_t *d = out + 54 + slot++ * 18;
      memset(d, 0, 18);
      d[3]  = 0xfd;
      d[5]  = (uint8_t)vmin;
      d[6]  = (uint8_t)vmax;
      d[7]  = (uint8_t)hmin_khz;
      d[8]  = (uint8_t)hmax_khz;
      d[9]  = in->pclock_max
         ? (uint8_t)((in->pclock_max / 1000000 + 9) / 10) : 0xff;
      d[10] = 0x01; /* limits only: no GTF or CVT formula is implied */
      d[11] = 0x0a;
      for (i = 12; i < 18; i++)
         d[i] = 0x20;
   }

   if (in->name[0] && slot < 4)
      edid_write_text(out + 54 + slot++ * 18, 0xfc, in->name);
   if (in->text[0] && slot < 4)
      edid_write_text(out + 54 + slot++ * 18, 0xfe, in->text);

   out[126] = in->cta ? 1 : 0;
   for (i = 0; i < 127; i++)
      checksum += out[i];
   out[127] = (uint8_t)((256 - (checksum & 0xff)) & 0xff);

   /* A CTA-861 extension carrying only what was reported: no video
    * data block, since a native panel timing has no VIC to name, and
    * no audio data block, since a panel that cannot make a sound has
    * no descriptor to give. Revision 3, one native detailed timing,
    * no basic audio. */
   if (in->cta && max >= MODELINE_EDID_SIZE * 2)
   {
      uint8_t *e  = out + MODELINE_EDID_SIZE;
      unsigned p  = 4;
      memset(e, 0, MODELINE_EDID_SIZE);
      e[0] = MODELINE_EDID_EXT_CTA;
      e[1] = 3;
      /* No native detailed timings in this block: the base block
       * already carries the panel's timing and repeating it here
       * would read as a second mode. */
      e[3] = (uint8_t)((in->ycbcr444 ? 0x20 : 0)
                     | (in->ycbcr422 ? 0x10 : 0));
      if (in->cta_colorimetry)
      {
         e[p++] = (uint8_t)((7 << 5) | 3);   /* extended tag, 3 bytes */
         e[p++] = 0x05;                      /* colorimetry data block */
         e[p++] = in->cta_colorimetry;
         e[p++] = 0x00;                      /* no gamut metadata profiles */
      }
      if (in->cta_hdr_eotf)
      {
         unsigned len = in->cta_hdr_max_lum ? 4 : 2;
         e[p++] = (uint8_t)((7 << 5) | (len + 1));
         e[p++] = 0x06;                      /* HDR static metadata */
         e[p++] = in->cta_hdr_eotf;
         e[p++] = 0x01;                      /* static metadata type 1 */
         if (in->cta_hdr_max_lum)
         {
            e[p++] = in->cta_hdr_max_lum;
            e[p++] = 0x00;                   /* frame average: not reported */
         }
      }
      /* Byte 2 points past the data blocks, where detailed timings
       * would start; the zeros there say there are none */
      e[2] = (uint8_t)p;
      checksum = 0;
      for (i = 0; i < 127; i++)
         checksum += e[i];
      e[127] = (uint8_t)((256 - (checksum & 0xff)) & 0xff);
      return MODELINE_EDID_SIZE * 2;
   }
   return MODELINE_EDID_SIZE;
}

/*  RetroArch - A frontend for libretro.
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

/* Tests for the EDID reader in gfx/modeline/modeline_edid.c.
 *
 * 1. Round trip: the block the writer produces for a CRT preset parses
 *    back to the same timing, range and name.
 * 2. A hand-built EDID 1.4 with a CTA-861 extension (video data block,
 *    HDMI and HDMI Forum vendor blocks, HDR static metadata, a DTD) and
 *    a DisplayID 2.0 extension with a type VII timing decodes every
 *    field the menu shows.
 * 3. Defects: short input, wrong header, bad checksums, a declared but
 *    missing extension, and CTA data block lengths that run past the
 *    block end all come back as flags rather than reads past the
 *    buffer. A deterministic byte-noise sweep runs the same parser
 *    under the sanitizers.
 *
 * Build and run: make SANITIZER=address,undefined && ./modeline_edid_parse_test */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define MODELINE_STANDALONE
#define RARCH_LOG(...)  do { } while (0)
#define RARCH_DBG(...)  do { } while (0)
#define RARCH_ERR(...)  do { } while (0)
#define RARCH_WARN(...) do { } while (0)

#include "../../../gfx/modeline/modeline_core.c"
#include "../../../gfx/modeline/modeline_monitor.c"
#include "../../../gfx/modeline/modeline_list.c"
#include "../../../gfx/modeline/modeline_ini.c"
#include "../../../gfx/modeline/modeline_edid.c"

static int failures = 0;

#define CHECK(cond, ...) do { \
   if (!(cond)) { \
      fprintf(stderr, "FAIL %s:%d: ", __FILE__, __LINE__); \
      fprintf(stderr, __VA_ARGS__); \
      fprintf(stderr, "\n"); \
      failures++; \
   } } while (0)

static void seal(uint8_t *block)
{
   int i;
   unsigned sum = 0;
   for (i = 0; i < 127; i++)
      sum += block[i];
   block[127] = (uint8_t)((256 - (sum & 0xff)) & 0xff);
}

/* ---- 1. writer -> reader round trip ---- */

static void test_round_trip(void)
{
   uint8_t block[MODELINE_EDID_SIZE];
   video_edid_info_t info;
   video_modeline_t mode;
   video_modeline_range_t range;
   char line[128];

   memset(&mode, 0, sizeof(mode));
   mode.pclock    = 6700000;      /* 320x240 @ 60 on a 15 kHz preset */
   mode.hactive   = 320;
   mode.hbegin    = 336;
   mode.hend      = 368;
   mode.htotal    = 426;
   mode.vactive   = 240;
   mode.vbegin    = 244;
   mode.vend      = 247;
   mode.vtotal    = 262;
   mode.hsync     = 0;
   mode.vsync     = 0;
   mode.interlace = 0;

   memset(&range, 0, sizeof(range));
   range.hfreq_min = 15250;
   range.hfreq_max = 15750;
   range.vfreq_min = 49;
   range.vfreq_max = 65;

   CHECK(modeline_edid_build(&mode, &range, "arcade_15", block), "build");
   CHECK(modeline_edid_parse(block, sizeof(block), &info), "parse");
   CHECK(info.header_ok && info.checksum_ok && !info.truncated, "flags");
   CHECK(info.ver_major == 1 && info.ver_minor == 3, "version %u.%u",
         info.ver_major, info.ver_minor);
   CHECK(!strcmp(info.manufacturer, "RAR"), "manufacturer %s", info.manufacturer);
   CHECK(info.width_cm == 48 && info.height_cm == 36, "size");
   CHECK(info.gamma_x100 == 220, "gamma %u", info.gamma_x100);
   CHECK(!info.digital, "analog input");
   CHECK(info.n_timings == 1, "timings %u", info.n_timings);
   CHECK(info.timing[0].preferred, "preferred");
   CHECK(info.timing[0].pclock == 6700000, "pclock %u", info.timing[0].pclock);
   CHECK(info.timing[0].hactive == 320 && info.timing[0].hblank == 106,
         "h %u/%u", info.timing[0].hactive, info.timing[0].hblank);
   CHECK(info.timing[0].hfront == 16 && info.timing[0].hsync == 32,
         "hfront/hsync %u/%u", info.timing[0].hfront, info.timing[0].hsync);
   CHECK(info.timing[0].vactive == 240 && info.timing[0].vblank == 22,
         "v %u/%u", info.timing[0].vactive, info.timing[0].vblank);
   CHECK(info.timing[0].vfront == 4 && info.timing[0].vsync == 3,
         "vfront/vsync %u/%u", info.timing[0].vfront, info.timing[0].vsync);
   CHECK(info.timing[0].sync_type == 3 && !info.timing[0].hsync_pos
         && !info.timing[0].vsync_pos, "sync flags");
   CHECK(info.has_range, "range");
   CHECK(info.vfreq_min == 49 && info.vfreq_max == 65, "vfreq %u-%u",
         info.vfreq_min, info.vfreq_max);
   CHECK(info.hfreq_min == 15 && info.hfreq_max == 15, "hfreq %u-%u",
         info.hfreq_min, info.hfreq_max);
   CHECK(info.pclock_max == 0, "pclock max %u", info.pclock_max);
   CHECK(!strcmp(info.name, "arcade_15"), "name '%s'", info.name);
   CHECK(!strcmp(info.serial_text, "RetroArch"), "serial '%s'", info.serial_text);
   CHECK(info.n_ext == 0 && info.n_ext_declared == 0, "extensions");

   modeline_edid_modeline_str(&info.timing[0], line, sizeof(line));
   CHECK(!strcmp(line,
         "Modeline \"320x240\" 6.700 320 336 368 426 240 244 247 262 -hsync -vsync"),
         "modeline '%s'", line);
   modeline_edid_timing_str(&info.timing[0], line, sizeof(line));
   CHECK(!strncmp(line, "320x240p 60.0", 13), "timing '%s'", line);
}

/* ---- 2. a modern display: EDID 1.4 + CTA-861 + DisplayID 2.0 ---- */

static void put_dtd(uint8_t *d, unsigned pclk_10k,
      unsigned ha, unsigned hb, unsigned hf, unsigned hs,
      unsigned va, unsigned vb, unsigned vf, unsigned vs,
      unsigned wmm, unsigned hmm, uint8_t flags)
{
   d[0]  = pclk_10k & 0xff;
   d[1]  = pclk_10k >> 8;
   d[2]  = ha & 0xff;
   d[3]  = hb & 0xff;
   d[4]  = ((ha >> 8) << 4) | (hb >> 8);
   d[5]  = va & 0xff;
   d[6]  = vb & 0xff;
   d[7]  = ((va >> 8) << 4) | (vb >> 8);
   d[8]  = hf & 0xff;
   d[9]  = hs & 0xff;
   d[10] = ((vf & 0x0f) << 4) | (vs & 0x0f);
   d[11] = ((hf >> 8) << 6) | ((hs >> 8) << 4) | ((vf >> 4) << 2) | (vs >> 4);
   d[12] = wmm & 0xff;
   d[13] = hmm & 0xff;
   d[14] = ((wmm >> 8) << 4) | (hmm >> 8);
   d[17] = flags;
}

static void build_modern(uint8_t *e)
{
   uint8_t *base = e;
   uint8_t *cta  = e + 128;
   uint8_t *did  = e + 256;
   unsigned p;

   memset(e, 0, 384);

   /* Base block, EDID 1.4 */
   base[1] = base[2] = base[3] = base[4] = base[5] = base[6] = 0xff;
   base[8]  = 0x10; base[9]  = 0xac;  /* "DEL" */
   base[10] = 0x34; base[11] = 0x12;  /* product 0x1234 */
   base[12] = 0x78; base[13] = 0x56; base[14] = 0x34; base[15] = 0x12;
   base[16] = 12;   base[17] = 2022 - 1990;
   base[18] = 1;    base[19] = 4;
   base[20] = 0x80 | (3 << 4) | 0x05;  /* digital, 10 bpc, DisplayPort */
   base[21] = 60;   base[22] = 34;
   base[23] = 120;                      /* gamma 2.2 */
   base[24] = 0xea;                     /* standby/suspend/off, RGB 4:4:4 + YCrCb, sRGB, preferred, continuous */
   /* chromaticity: sRGB primaries */
   base[25] = 0xee; base[26] = 0x91; base[27] = 0xa3; base[28] = 0x54;
   base[29] = 0x4c; base[30] = 0x99; base[31] = 0x26; base[32] = 0x0f;
   base[33] = 0x50; base[34] = 0x54;
   base[35] = 0x21;                     /* 640x480@60, 800x600@60 */
   base[36] = 0x08;                     /* 1024x768@60 */
   base[37] = 0x80;                     /* 1152x870@75 */
   base[38] = 0xd1; base[39] = 0xc0;    /* 1920x1080 @ 60 (16:9) */
   base[40] = 0x81; base[41] = 0x80;    /* 1280x1024 @ 60 (5:4) */
   base[42] = 0xa9; base[43] = 0x40;    /* 1600x1200 @ 60 (4:3) */
   base[44] = 0xb3; base[45] = 0x00;    /* 1680x1050 @ 60 (16:10) */
   for (p = 46; p < 54; p++)
      base[p] = 0x01;
   /* Descriptor 1: 1920x1080p60 preferred */
   put_dtd(base + 54, 14850, 1920, 280, 88, 44, 1080, 45, 4, 5, 598, 336, 0x1e);
   /* Descriptor 2: range limits 1.4 with the +255 flag on hmax */
   base[75] = 0xfd; base[76] = 0x08;
   base[77] = 48; base[78] = 144; base[79] = 30; base[80] = 10;
   base[81] = 0x21; base[82] = 0x04;    /* 330 MHz, CVT */
   base[83] = 0x0a;
   /* Descriptor 3: monitor name */
   base[93] = 0xfc;
   memcpy(base + 95, "DELL U2723QE\n", 13);
   /* Descriptor 4: serial string with trailing padding */
   base[111] = 0xff;
   memcpy(base + 113, "ABC123\n      ", 13);
   base[126] = 2;
   seal(base);

   /* CTA-861 revision 3 */
   cta[0] = 0x02; cta[1] = 0x03;
   cta[3] = 0xf1;                       /* underscan, audio, 444, 422, 1 native DTD */
   p = 4;
   /* Video data block: 5 SVDs, VIC 16 native */
   cta[p++] = (2 << 5) | 5;
   cta[p++] = 16 | 0x80; cta[p++] = 4; cta[p++] = 5; cta[p++] = 97; cta[p++] = 107;
   /* Audio data block: LPCM 2ch and 8ch */
   cta[p++] = (1 << 5) | 6;
   cta[p++] = 0x09; cta[p++] = 0x07; cta[p++] = 0x07;
   cta[p++] = 0x0f; cta[p++] = 0x07; cta[p++] = 0x07;
   /* HDMI VSDB: OUI 00-0C-03, physical address 1.0.0.0, flags, 340 MHz */
   cta[p++] = (3 << 5) | 7;
   cta[p++] = 0x03; cta[p++] = 0x0c; cta[p++] = 0x00;
   cta[p++] = 0x10; cta[p++] = 0x00; cta[p++] = 0x80; cta[p++] = 68;
   /* HDMI Forum VSDB: OUI C4-5D-D8, version 1, 600 MHz */
   cta[p++] = (3 << 5) | 5;
   cta[p++] = 0xd8; cta[p++] = 0x5d; cta[p++] = 0xc4;
   cta[p++] = 0x01; cta[p++] = 120;
   /* HDR static metadata: SDR/HDR/PQ, type 1, max 1000 nits code */
   cta[p++] = (7 << 5) | 6;
   cta[p++] = 0x06; cta[p++] = 0x07; cta[p++] = 0x01;
   cta[p++] = 0x9a; cta[p++] = 0x80; cta[p++] = 0x2a;
   /* Colorimetry: BT.2020 RGB + YCC */
   cta[p++] = (7 << 5) | 3;
   cta[p++] = 0x05; cta[p++] = 0xc0; cta[p++] = 0x00;
   /* YCbCr 4:2:0 capability map */
   cta[p++] = (7 << 5) | 2;
   cta[p++] = 0x0f; cta[p++] = 0x18;
   cta[2] = (uint8_t)p;
   /* One DTD: 3840x2160p30 */
   put_dtd(cta + p, 29700, 3840, 560, 176, 88, 2160, 90, 8, 10, 598, 336, 0x1e);
   seal(cta);

   /* DisplayID 2.0 extension: one type VII timing, 2560x1440 @ 165 */
   did[0] = 0x70; did[1] = 0x20; did[3] = 0x02;
   p = 5;
   did[p++] = 0x22; did[p++] = 0x00; did[p++] = 20;
   {
      unsigned pclk = 60000 - 1;       /* 600.00 MHz */
      uint8_t *t = did + p;
      t[0] = pclk & 0xff; t[1] = (pclk >> 8) & 0xff; t[2] = pclk >> 16;
      t[3] = 0x80;                     /* preferred, progressive */
      t[4] = (2560 - 1) & 0xff; t[5] = (2560 - 1) >> 8;
      t[6] = (160 - 1);  t[7] = 0;
      t[8] = (48 - 1);   t[9] = 0x80;  /* hfront 48, +hsync */
      t[10] = (32 - 1);  t[11] = 0;
      t[12] = (1440 - 1) & 0xff; t[13] = (1440 - 1) >> 8;
      t[14] = (77 - 1);  t[15] = 0;
      t[16] = (3 - 1);   t[17] = 0x80; /* vfront 3, +vsync */
      t[18] = (5 - 1);   t[19] = 0;
   }
   p += 20;
   did[2] = (uint8_t)(p - 5);
   seal(did);
}

static void test_modern(void)
{
   uint8_t e[384];
   video_edid_info_t info;
   char line[128];
   const video_edid_ext_t *cta;
   const video_edid_ext_t *did;

   build_modern(e);
   CHECK(modeline_edid_parse(e, sizeof(e), &info), "parse");
   CHECK(info.header_ok && info.checksum_ok && !info.truncated, "flags");
   CHECK(info.n_blocks == 3 && info.n_ext == 2 && info.n_ext_declared == 2,
         "blocks %u ext %u", info.n_blocks, info.n_ext);
   CHECK(info.ver_major == 1 && info.ver_minor == 4, "version");
   CHECK(!strcmp(info.manufacturer, "DEL"), "manufacturer %s", info.manufacturer);
   CHECK(info.product == 0x1234, "product %x", info.product);
   CHECK(info.serial == 0x12345678, "serial %x", (unsigned)info.serial);
   CHECK(info.week == 12 && info.year == 2022 && !info.model_year, "date");
   CHECK(info.digital && info.bit_depth == 10 && info.interface == 5,
         "input %d %u %u", info.digital, info.bit_depth, info.interface);
   CHECK(info.width_cm == 60 && info.height_cm == 34, "size");
   CHECK(info.features == 0xea, "features");
   CHECK(info.red_x == 640 && info.red_y == 330, "red %u %u", info.red_x, info.red_y);
   CHECK(info.green_x == 300 && info.green_y == 600, "green %u %u", info.green_x, info.green_y);
   CHECK(info.blue_x == 150 && info.blue_y == 60, "blue %u %u", info.blue_x, info.blue_y);
   CHECK(info.white_x == 313 && info.white_y == 329, "white %u %u", info.white_x, info.white_y);
   CHECK(info.established == ((1u << 2) | (1u << 7) | (1u << 12) | (1u << 16)),
         "established %x", info.established);
   CHECK(info.n_std == 4, "std %u", info.n_std);
   CHECK(info.std[0].width == 1920 && info.std[0].height == 1080 && info.std[0].refresh == 60,
         "std0 %ux%u@%u", info.std[0].width, info.std[0].height, info.std[0].refresh);
   CHECK(info.std[1].width == 1280 && info.std[1].height == 1024, "std1");
   CHECK(info.std[2].width == 1600 && info.std[2].height == 1200, "std2");
   CHECK(info.std[3].width == 1680 && info.std[3].height == 1050, "std3 %ux%u",
         info.std[3].width, info.std[3].height);
   CHECK(info.has_range && info.vfreq_min == 48 && info.vfreq_max == 144
         && info.hfreq_min == 30 && info.hfreq_max == 265
         && info.pclock_max == 330 && info.range_type == 0x04,
         "range %u-%u %u-%u %u", info.vfreq_min, info.vfreq_max,
         info.hfreq_min, info.hfreq_max, info.pclock_max);
   CHECK(!strcmp(info.name, "DELL U2723QE"), "name '%s'", info.name);
   CHECK(!strcmp(info.serial_text, "ABC123"), "serial '%s'", info.serial_text);

   CHECK(info.n_timings == 3, "timings %u", info.n_timings);
   CHECK(info.timing[0].preferred && info.timing[0].src == MODELINE_EDID_SRC_BASE,
         "t0 origin");
   modeline_edid_timing_str(&info.timing[0], line, sizeof(line));
   CHECK(!strcmp(line, "1920x1080p 60.00 Hz, 148.50 MHz, 67.50 kHz"), "t0 '%s'", line);
   CHECK(info.timing[0].hsync_pos && info.timing[0].vsync_pos, "t0 polarity");
   CHECK(info.timing[0].hsize_mm == 598 && info.timing[0].vsize_mm == 336, "t0 size");
   CHECK(info.timing[1].src == MODELINE_EDID_SRC_CTA && !info.timing[1].preferred, "t1 origin");
   modeline_edid_timing_str(&info.timing[1], line, sizeof(line));
   CHECK(!strcmp(line, "3840x2160p 30.00 Hz, 297.00 MHz, 67.50 kHz"), "t1 '%s'", line);
   CHECK(info.timing[2].src == MODELINE_EDID_SRC_DISPLAYID && info.timing[2].preferred,
         "t2 origin");
   modeline_edid_modeline_str(&info.timing[2], line, sizeof(line));
   CHECK(!strcmp(line,
         "Modeline \"2560x1440\" 600.000 2560 2608 2640 2720 1440 1443 1448 1517 +hsync +vsync"),
         "t2 '%s'", line);

   cta = &info.ext[0];
   CHECK(cta->tag == MODELINE_EDID_EXT_CTA && cta->revision == 3 && cta->checksum_ok,
         "cta header");
   CHECK(cta->underscan && cta->basic_audio && cta->ycbcr444 && cta->ycbcr422
         && cta->native_dtds == 1, "cta flags");
   CHECK(cta->n_vics == 5 && (cta->vic[0] & 0x7f) == 16 && (cta->vic[0] & 0x80)
         && cta->vic[4] == 107, "vics %u", cta->n_vics);
   CHECK(cta->n_audio == 2, "audio %u", cta->n_audio);
   CHECK(cta->hdmi && cta->hdmi_phys[0] == 0x10 && cta->hdmi_phys[1] == 0x00
         && cta->hdmi_max_tmds == 340, "hdmi %u", cta->hdmi_max_tmds);
   CHECK(cta->hdmi_forum && cta->hf_max_tmds == 600, "hdmi forum %u", cta->hf_max_tmds);
   CHECK(cta->hdr && cta->hdr_eotf == 0x07 && cta->hdr_max_lum == 0x9a
         && cta->hdr_max_fal == 0x80 && cta->hdr_min_lum == 0x2a, "hdr");
   CHECK(cta->colorimetry && cta->colorimetry_flags == 0xc0, "colorimetry");
   CHECK(cta->ycbcr420, "420");
   modeline_edid_vic_str(cta->vic[0] & 0x7f, line, sizeof(line));
   CHECK(!strcmp(line, "1920x1080p60"), "vic16 '%s'", line);
   modeline_edid_vic_str(97, line, sizeof(line));
   CHECK(!strcmp(line, "3840x2160p60"), "vic97 '%s'", line);
   modeline_edid_vic_str(5, line, sizeof(line));
   CHECK(!strcmp(line, "1920x1080i60"), "vic5 '%s'", line);
   modeline_edid_vic_str(200, line, sizeof(line));
   CHECK(!strcmp(line, "VIC 200"), "vic200 '%s'", line);

   did = &info.ext[1];
   CHECK(did->tag == MODELINE_EDID_EXT_DISPLAYID && did->revision == 0x20
         && did->checksum_ok && did->did_product == 2, "displayid header");
   CHECK(did->n_sections == 1 && did->section[0] == 0x22, "sections");
   CHECK(!strcmp(modeline_edid_did_section_name(0x20, 0x22), "Type VII Timing"),
         "section name");
   CHECK(!strcmp(modeline_edid_ext_name(0x70), "DisplayID"), "ext name");
   CHECK(!strcmp(modeline_edid_established_name(16), "1152x870 @ 75 Hz"), "est name");
   CHECK(!*modeline_edid_established_name(17), "est past end");
}

/* ---- 3. defects ---- */

static void test_defects(void)
{
   uint8_t e[384];
   video_edid_info_t info;
   uint32_t seed = 0x2545f491;
   int i;

   build_modern(e);

   CHECK(!modeline_edid_parse(NULL, 384, &info), "NULL data");
   CHECK(!modeline_edid_parse(e, 127, &info), "short");
   CHECK(!modeline_edid_parse(e, 384, NULL), "NULL info");

   e[0] = 0x01;
   CHECK(!modeline_edid_parse(e, 384, &info) && !info.header_ok, "bad header");
   e[0] = 0x00;

   /* One extension short of what byte 126 promises */
   CHECK(modeline_edid_parse(e, 256, &info), "parse 2 of 3");
   CHECK(info.truncated && info.n_ext == 1 && info.n_ext_declared == 2, "truncated");

   /* Odd trailing bytes are ignored, not read */
   CHECK(modeline_edid_parse(e, 300, &info), "parse 300");
   CHECK(info.n_blocks == 2 && info.n_ext == 1, "partial block dropped");

   /* Bad checksums are flagged per block, nothing else changes */
   e[127] ^= 0x01;
   e[255] ^= 0x01;
   CHECK(modeline_edid_parse(e, 384, &info), "parse bad sums");
   CHECK(!info.checksum_ok && !info.ext[0].checksum_ok && info.ext[1].checksum_ok,
         "checksum flags");
   CHECK(info.n_timings == 3, "content survives a bad checksum");
   e[127] ^= 0x01;
   e[255] ^= 0x01;

   /* A CTA data block whose length runs past the data block area */
   e[128 + 4] = (2 << 5) | 31;
   e[128 + 2] = 20;
   CHECK(modeline_edid_parse(e, 384, &info), "parse overlong block");
   CHECK(info.ext[0].n_vics == 0, "overlong block dropped");
   /* and a DTD offset past the end */
   e[128 + 2] = 200;
   CHECK(modeline_edid_parse(e, 384, &info), "parse dtd offset past end");
   build_modern(e);

   /* A DisplayID section running past its extension */
   e[256 + 7] = 200;
   CHECK(modeline_edid_parse(e, 384, &info), "parse overlong section");
   CHECK(info.n_timings == 2, "overlong section dropped: %u", info.n_timings);
   build_modern(e);

   /* Every VIC slot full, then one more */
   {
      unsigned p = 4;
      memset(e + 128 + 4, 0, 123);
      e[128 + p++] = (2 << 5) | 31;
      for (i = 0; i < 31; i++) e[128 + p++] = (uint8_t)(i + 1);
      e[128 + p++] = (2 << 5) | 31;
      for (i = 0; i < 31; i++) e[128 + p++] = (uint8_t)(i + 32);
      e[128 + p++] = (2 << 5) | 4;
      for (i = 0; i < 4; i++) e[128 + p++] = (uint8_t)(i + 63);
      e[128 + 2] = (uint8_t)p;
      seal(e + 128);
      CHECK(modeline_edid_parse(e, 384, &info), "parse full vics");
      CHECK(info.ext[0].n_vics == MODELINE_EDID_MAX_VICS, "vic cap %u", info.ext[0].n_vics);
   }
   build_modern(e);

   /* Byte noise behind a valid header: the parser must never read
    * outside the buffer, whatever the lengths and offsets say */
   for (i = 0; i < 4000; i++)
   {
      size_t n, len;
      for (n = 8; n < sizeof(e); n++)
      {
         seed = seed * 1664525u + 1013904223u;
         e[n] = (uint8_t)(seed >> 24);
      }
      /* keep a recognisable extension tag on the second block half
       * the time, so the CTA and DisplayID walkers get exercised */
      if (i & 1)
         e[128] = (i & 2) ? 0x02 : 0x70;
      seed = seed * 1664525u + 1013904223u;
      len  = 128 + (seed >> 24) % 257;
      modeline_edid_parse(e, len, &info);
      build_modern(e);
   }
}

int main(void)
{
   test_round_trip();
   test_modern();
   test_defects();
   if (failures)
   {
      fprintf(stderr, "%d failure(s)\n", failures);
      return 1;
   }
   printf("[pass] modeline_edid_parse_test\n");
   return 0;
}

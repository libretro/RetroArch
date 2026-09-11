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

#ifndef __VIDEO_MODELINE_EDID_H
#define __VIDEO_MODELINE_EDID_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>
#include <retro_common_api.h>

#include "modeline_core.h"

RETRO_BEGIN_DECLS

#define MODELINE_EDID_SIZE 128

/* One EDID 1.3 base block describing a monitor that syncs the given
 * range, with the modeline as its preferred detailed timing and the
 * name (13 characters at most) as the monitor name. This is what a
 * display without DDC - a 15 kHz CRT on a VGA or SCART adapter -
 * cannot tell the kernel or the driver itself: loaded as a firmware
 * EDID (Linux, drm.edid_firmware=<connector>:edid/<file>) or an EDID
 * override, it makes the connector report as connected, gives the
 * driver the sync limits so the CRT timings are not pruned, and puts
 * the system on a scanrate the tube can show before anything else
 * runs. The generator only writes bytes; installing the block is a
 * deliberate, separate step for the user. */
bool modeline_edid_build(const video_modeline_t *mode,
      const video_modeline_range_t *range, const char *name,
      uint8_t out[MODELINE_EDID_SIZE]);

/* The block for a generator's current preset: the preferred timing is
 * the preset's 320x240@60 (the canonical CRT mode, doublescanned on
 * a 31 kHz preset) and the limits are the first live range. */
bool modeline_edid_for_gen(video_modeline_gen_t *gen,
      uint8_t out[MODELINE_EDID_SIZE]);

/* ---- Reading ----
 *
 * The other direction: a block the display server pulled off a real
 * monitor (KMS connector property, XRandR output property, the Win32
 * PnP registry) decoded into what the menu shows under System
 * Information > Display > EDID. The base block layout is the same
 * from EDID 1.0 through 1.4 (1.4 reinterprets a few bytes, which the
 * parser handles by version), so one parser covers every E-EDID; the
 * extension blocks that a modern display appends - CTA-861 and
 * DisplayID - are decoded for their timings and capability bits.
 *
 * Everything is bounds-checked against the length handed in; a
 * truncated or corrupt block yields whatever could be read plus the
 * checksum and truncation flags, never a failure to parse. */

/* The most an OS reader copies: a base block plus seven extensions.
 * More than that is legal (E-EDID allows 255) but nothing shipped
 * uses it; the parser flags the truncation. */
#define MODELINE_EDID_MAX_BLOCKS   8
#define MODELINE_EDID_MAX_LEN      (MODELINE_EDID_SIZE * MODELINE_EDID_MAX_BLOCKS)

#define MODELINE_EDID_MAX_TIMINGS  16   /* detailed timings, all blocks */
#define MODELINE_EDID_MAX_STD      14   /* 8 base + 6 from a 0xFA descriptor */
#define MODELINE_EDID_MAX_VICS     64   /* CTA short video descriptors */
#define MODELINE_EDID_MAX_SECTIONS 16   /* DisplayID section tags */
#define MODELINE_EDID_TEXT_LEN     14   /* 13 characters plus NUL */

/* Where a detailed timing came from */
enum modeline_edid_timing_src
{
   MODELINE_EDID_SRC_BASE = 0,   /* base block descriptor */
   MODELINE_EDID_SRC_CTA,        /* CTA-861 extension DTD */
   MODELINE_EDID_SRC_DISPLAYID   /* DisplayID type I / VII */
};

typedef struct video_edid_timing
{
   unsigned pclock;         /* Hz */
   unsigned hactive, hblank, hfront, hsync, hborder;
   unsigned vactive, vblank, vfront, vsync, vborder;
   unsigned hsize_mm, vsize_mm;
   uint8_t  src;            /* modeline_edid_timing_src */
   uint8_t  sync_type;      /* 0 analog composite, 1 bipolar analog,
                               2 digital composite, 3 digital separate */
   uint8_t  stereo;         /* bits 6-5 of flags, 0 = none */
   bool     interlace;
   bool     hsync_pos;
   bool     vsync_pos;
   bool     preferred;
} video_edid_timing_t;

typedef struct video_edid_std_timing
{
   unsigned width, height, refresh;
} video_edid_std_timing_t;

/* Extension block tags (byte 0 of the block) */
#define MODELINE_EDID_EXT_CTA       0x02
#define MODELINE_EDID_EXT_VTB       0x10
#define MODELINE_EDID_EXT_DI        0x40
#define MODELINE_EDID_EXT_LS        0x50
#define MODELINE_EDID_EXT_DPVL      0x60
#define MODELINE_EDID_EXT_DISPLAYID 0x70
#define MODELINE_EDID_EXT_BLOCKMAP  0xf0
#define MODELINE_EDID_EXT_VENDOR    0xff

typedef struct video_edid_ext
{
   uint8_t  tag;
   uint8_t  revision;       /* CTA revision or DisplayID version byte */
   bool     checksum_ok;
   /* CTA-861 */
   bool     underscan;
   bool     basic_audio;
   bool     ycbcr444;
   bool     ycbcr422;
   uint8_t  native_dtds;
   uint8_t  n_vics;
   uint8_t  vic[MODELINE_EDID_MAX_VICS];
   uint8_t  n_audio;        /* short audio descriptors */
   bool     hdmi;           /* HDMI VSDB present */
   uint8_t  hdmi_phys[2];   /* physical address, 4 nibbles */
   unsigned hdmi_max_tmds;  /* MHz, 0 = unstated */
   bool     hdmi_forum;     /* HDMI Forum VSDB present (HDMI 2.x) */
   unsigned hf_max_tmds;    /* MHz, 0 = unstated */
   bool     hdr;            /* HDR static metadata block present */
   uint8_t  hdr_eotf;       /* bit 0 SDR, 1 HDR, 2 PQ, 3 HLG */
   uint8_t  hdr_max_lum;    /* coded, 0 = unstated */
   uint8_t  hdr_max_fal;
   uint8_t  hdr_min_lum;
   bool     colorimetry;
   uint8_t  colorimetry_flags;  /* byte 2 of the block */
   bool     ycbcr420;       /* a 4:2:0 video or capability block */
   /* DisplayID */
   uint8_t  did_product;    /* product type code */
   uint8_t  n_sections;
   uint8_t  section[MODELINE_EDID_MAX_SECTIONS];
} video_edid_ext_t;

typedef struct video_edid_info
{
   size_t   len;            /* bytes parsed */
   uint8_t  n_blocks;       /* blocks present in the data */
   uint8_t  n_ext_declared; /* byte 126: what the base block promises */
   bool     header_ok;
   bool     checksum_ok;    /* base block */
   bool     truncated;      /* fewer blocks than declared */

   uint8_t  ver_major, ver_minor;
   char     manufacturer[4];
   unsigned product;
   uint32_t serial;
   uint8_t  week;
   unsigned year;           /* 0 = unknown */
   bool     model_year;     /* 1.4: week 0xff means the year is a model year */

   bool     digital;
   uint8_t  bit_depth;      /* 1.4 digital: bits per colour, 0 = undefined */
   uint8_t  interface;      /* 1.4 digital: 1 DVI 2 HDMI-a 3 HDMI-b 4 MDDI 5 DP */
   uint8_t  analog_level;   /* analog: video level code 0-3 */
   uint8_t  analog_sync;    /* analog: bits 4-0 of byte 20 */
   bool     dfp1x;          /* pre-1.4 digital: DFP 1.x compatible */

   uint8_t  width_cm, height_cm;   /* 0 = unknown / aspect only */
   unsigned gamma_x100;     /* 0 = undefined */
   uint8_t  features;       /* byte 24 verbatim */

   unsigned red_x, red_y, green_x, green_y, blue_x, blue_y, white_x, white_y; /* x1000 */

   unsigned established;    /* 17 bits, MODELINE_EDID_EST_* order */
   uint8_t  est_mfr;        /* byte 37 bits 6-0, manufacturer timings */

   uint8_t  n_std;
   video_edid_std_timing_t std[MODELINE_EDID_MAX_STD];

   uint8_t  n_timings;
   video_edid_timing_t timing[MODELINE_EDID_MAX_TIMINGS];

   char     name[MODELINE_EDID_TEXT_LEN];
   char     serial_text[MODELINE_EDID_TEXT_LEN];
   char     text[MODELINE_EDID_TEXT_LEN];

   bool     has_range;
   unsigned vfreq_min, vfreq_max;   /* Hz */
   unsigned hfreq_min, hfreq_max;   /* kHz */
   unsigned pclock_max;             /* MHz, 0 = unstated */
   uint8_t  range_type;             /* byte 10: 0 GTF, 1 limits only, 2 secondary GTF, 4 CVT */

   uint8_t  n_ext;
   video_edid_ext_t ext[MODELINE_EDID_MAX_BLOCKS - 1];
} video_edid_info_t;

/* Monitor ranges from a display's own range limits, for the "edid"
 * preset: the EDID's horizontal and vertical bands are split at the
 * 15 / 25 / 31 kHz arcade boundaries and each piece takes its
 * blanking template from the matching arcade preset, with everything
 * above 40 kHz on VESA GTF blanking. Returns the number of ranges
 * written (0 when the EDID carries no usable range descriptor); range
 * has room for max entries. */
int modeline_edid_fill_ranges(const video_edid_info_t *info,
      video_modeline_range_t *range, int max);

/* Decode len bytes of EDID into info. Returns false only when data is
 * NULL, len is under one block, or the 8-byte header is wrong; every
 * other defect is reported in the flags. */
bool modeline_edid_parse(const uint8_t *data, size_t len,
      video_edid_info_t *info);

/* Formatting helpers for the menu and the harness; each writes into s
 * and returns the length. */

/* "1920x1080p 60.00 Hz, 148.50 MHz, 67.50 kHz" */
size_t modeline_edid_timing_str(const video_edid_timing_t *t,
      char *s, size_t len);
/* The same timing as an xorg-style modeline */
size_t modeline_edid_modeline_str(const video_edid_timing_t *t,
      char *s, size_t len);
/* Name of an established timing bit (0-16), "" past the end */
const char *modeline_edid_established_name(unsigned bit);
/* CTA-861 VIC as "1920x1080p60", or "VIC n" when unlisted */
size_t modeline_edid_vic_str(unsigned vic, char *s, size_t len);
/* Extension tag name */
const char *modeline_edid_ext_name(uint8_t tag);
/* DisplayID section tag name for the given DisplayID version byte */
const char *modeline_edid_did_section_name(uint8_t version, uint8_t tag);

RETRO_END_DECLS

#endif

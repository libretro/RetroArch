/*  RetroArch - A frontend for libretro.
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

/* The GameCube/Wii video mode table and the rules that turn one of
 * its entries into what the VI is actually programmed with. Nothing
 * here touches libogc: the TV standard comes in as a
 * gx_vi_standard_t (dispserv_gx.c fills it from the console), so the
 * display server's mode list and the video driver's mode switch are
 * worked out by the same code, and a host build can test it. */

#ifndef __DISPSERV_GX_MODES_H
#define __DISPSERV_GX_MODES_H

#include <stddef.h>

#include <retro_common_api.h>
#include <boolean.h>

#include "../video_display_server.h"

RETRO_BEGIN_DECLS

/* Id 0 is "let the console choose" (VIDEO_GetPreferredMode); ids
 * 1..gx_modes_count()-1 are the fixed entries. The ids are what
 * current_resolution_id stores in the config, so the table's order
 * must never change - new entries go at the end. */
#define GX_MODE_ID_DEFAULT 0

typedef struct gx_vi_standard
{
   unsigned max_width;   /* VI_MAX_WIDTH_* of the TV standard */
   unsigned max_height;  /* VI_MAX_HEIGHT_*: 480, or 576 for PAL */
   unsigned pref_width;  /* the console's preferred mode, fbWidth */
   unsigned pref_height; /* and xfbHeight */
   bool     fifty_hz;    /* VI_PAL: 50 Hz fields */
   bool     progressive; /* component cable with progressive scan on */
} gx_vi_standard_t;

typedef struct gx_vi_mode
{
   unsigned width;       /* framebuffer width, clamped to the standard */
   unsigned lines;       /* framebuffer lines, clamped to the standard */
   float    hz;
   bool     double_strike; /* VI_NON_INTERLACE: 240p/288p */
   bool     interlaced;    /* VI_INTERLACE: 480i/576i */
} gx_vi_mode_t;

/* Number of ids, the default included. */
unsigned gx_modes_count(void);

/* The table entry for an id as VIDEO_SCALE_PACKed dims; 0 for the
 * default and for an id out of range. */
unsigned gx_modes_dims(unsigned id);

/* An id from the config made safe: out of range becomes the default. */
unsigned gx_modes_clamp_id(unsigned id);

/* What the VI runs for dims (0 = the preferred mode) under std. */
void gx_modes_resolve(const gx_vi_standard_t *std, unsigned dims,
      gx_vi_mode_t *out);

/* The id that selects exactly dims under std, or -1. The preferred
 * mode's dims select the default when no fixed entry has them. */
int gx_modes_find(const gx_vi_standard_t *std, unsigned dims);

/* The modes std can show, for the display server's resolution list:
 * the preferred mode first when no fixed entry has its dims, then the
 * fixed entries that fit, with the one current_id resolves to marked
 * current. Writes at most max entries to out and returns how many
 * there are in total; out may be NULL to count. */
unsigned gx_modes_list(const gx_vi_standard_t *std, unsigned current_id,
      video_display_config_t *out, unsigned max);

RETRO_END_DECLS

#endif

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

#include <string.h>

#include "dispserv_gx_modes.h"

/* Width, lines. Index = id; index 0 is the default and stays 0x0.
 * Order is the config's current_resolution_id - append only. */
static const unsigned short gx_mode_table[][2] = {
   {   0,   0 },
   { 512, 192 }, { 598, 200 }, { 640, 200 },
   { 384, 224 }, { 448, 224 }, { 480, 224 }, { 512, 224 },
   { 576, 224 }, { 608, 224 }, { 640, 224 },
   { 340, 232 }, { 512, 232 }, { 512, 236 },
   { 336, 240 }, { 352, 240 }, { 384, 240 }, { 512, 240 },
   { 530, 240 }, { 608, 240 }, { 640, 240 },
   { 512, 384 }, { 598, 400 }, { 640, 400 },
   { 384, 448 }, { 448, 448 }, { 480, 448 }, { 512, 448 },
   { 576, 448 }, { 608, 448 }, { 640, 448 },
   { 340, 464 }, { 512, 464 }, { 512, 472 },
   { 352, 480 }, { 384, 480 }, { 512, 480 }, { 530, 480 },
   { 608, 480 }, { 640, 480 }
};

#define GX_MODE_TABLE_COUNT (sizeof(gx_mode_table) / sizeof(gx_mode_table[0]))

unsigned gx_modes_count(void)
{
   return (unsigned)GX_MODE_TABLE_COUNT;
}

unsigned gx_modes_dims(unsigned id)
{
   if (id >= GX_MODE_TABLE_COUNT)
      return 0;
   return VIDEO_SCALE_PACK(gx_mode_table[id][0], gx_mode_table[id][1]);
}

unsigned gx_modes_clamp_id(unsigned id)
{
   return (id < GX_MODE_TABLE_COUNT) ? id : GX_MODE_ID_DEFAULT;
}

void gx_modes_resolve(const gx_vi_standard_t *std, unsigned dims,
      gx_vi_mode_t *out)
{
   unsigned width = VIDEO_SCALE_W(dims);
   unsigned lines = VIDEO_SCALE_H(dims);

   if (!width || !lines)
   {
      width = std->pref_width;
      lines = std->pref_height;
   }

   /* Up to half the standard's lines is one field per frame, the
    * same field every time (double strike); above that the VI
    * interlaces, or scans progressively on a component cable */
   out->double_strike = (lines <= std->max_height / 2);
   out->interlaced    = !out->double_strike && !std->progressive;

   if (lines > std->max_height)
      lines = std->max_height;
   if (width > std->max_width)
      width = std->max_width;

   out->width = width;
   out->lines = lines;

   if (std->fifty_hz)
      out->hz = out->double_strike ? 50.0801f : 50.0f;
   else
      out->hz = out->double_strike ? 59.8261f : 59.94f;
}

/* A fixed entry the standard shows as it is, unclamped */
static bool gx_modes_fits(const gx_vi_standard_t *std, unsigned id)
{
   return gx_mode_table[id][0] <= std->max_width
       && gx_mode_table[id][1] <= std->max_height;
}

int gx_modes_find(const gx_vi_standard_t *std, unsigned dims)
{
   unsigned id;
   gx_vi_mode_t pref;

   if (!VIDEO_SCALE_W(dims) || !VIDEO_SCALE_H(dims))
      return -1;

   for (id = 1; id < GX_MODE_TABLE_COUNT; id++)
      if (     gx_modes_fits(std, id)
            && gx_modes_dims(id) == dims)
         return (int)id;

   gx_modes_resolve(std, 0, &pref);
   if (VIDEO_SCALE_PACK(pref.width, pref.lines) == dims)
      return GX_MODE_ID_DEFAULT;

   return -1;
}

static void gx_modes_fill(video_display_config_t *cfg,
      const gx_vi_mode_t *mode, unsigned id, unsigned current_dims)
{
   unsigned dims          = VIDEO_SCALE_PACK(mode->width, mode->lines);
   memset(cfg, 0, sizeof(*cfg));
   cfg->dims              = dims;
   cfg->bpp               = 16;
   cfg->refreshrate       = (unsigned)mode->hz;
   cfg->refreshrate_float = mode->hz;
   cfg->idx               = id;
   cfg->interlaced        = mode->interlaced;
   cfg->current           = (dims == current_dims);
}

unsigned gx_modes_list(const gx_vi_standard_t *std, unsigned current_id,
      video_display_config_t *out, unsigned max)
{
   unsigned id, current_dims;
   unsigned count = 0;
   gx_vi_mode_t mode;

   /* Whatever the stored id ends up running is the current entry,
    * the default included */
   gx_modes_resolve(std, gx_modes_dims(gx_modes_clamp_id(current_id)),
         &mode);
   current_dims = VIDEO_SCALE_PACK(mode.width, mode.lines);

   gx_modes_resolve(std, 0, &mode);
   if (gx_modes_find(std, VIDEO_SCALE_PACK(mode.width, mode.lines))
         == GX_MODE_ID_DEFAULT)
   {
      if (out && count < max)
         gx_modes_fill(&out[count], &mode, GX_MODE_ID_DEFAULT,
               current_dims);
      count++;
   }

   for (id = 1; id < GX_MODE_TABLE_COUNT; id++)
   {
      if (!gx_modes_fits(std, id))
         continue;
      if (out && count < max)
      {
         gx_modes_resolve(std, gx_modes_dims(id), &mode);
         gx_modes_fill(&out[count], &mode, id, current_dims);
      }
      count++;
   }

   return count;
}

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2026 - Libretro team
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

#include "gfx_instrument.h"

#ifdef HAVE_GFX_INSTRUMENT

retro_atomic_int_t gfx_instrument_counters[GFX_INSTR_COUNT];

/* Index-matched to enum gfx_instrument_counter. */
static const char *gfx_instrument_names[GFX_INSTR_COUNT] = {
   "tex_load",
   "tex_load_async",
   "tex_update",
   "tex_update_refused",
   "tex_unload",
   "async_post",
   "async_post_alloc",
   "async_done",
   "wrapper_cmd",
   "surface_new",
   "surface_bytes",
   "surface_free",
   "submit_done",
   "submit_queued",
   "submit_busy",
   "submit_failed",
   "submit_copy",
   "anim_frame",
   "anim_direct",
   "anim_copy",
   "anim_swizzle",
   "overlay_upload",
   "overlay_pixel_kib",
   "overlay_page",
   "overlay_page_load",
   "overlay_draw",
   "overlay_draw_alloc"
};

int gfx_instrument_get(enum gfx_instrument_counter c)
{
   if ((unsigned)c >= GFX_INSTR_COUNT)
      return 0;
   return retro_atomic_load_relaxed_int(&gfx_instrument_counters[c]);
}

void gfx_instrument_reset(void)
{
   unsigned i;
   for (i = 0; i < GFX_INSTR_COUNT; i++)
      retro_atomic_store_relaxed_int(&gfx_instrument_counters[i], 0);
}

const char *gfx_instrument_name(enum gfx_instrument_counter c)
{
   if ((unsigned)c >= GFX_INSTR_COUNT)
      return "?";
   return gfx_instrument_names[c];
}

void gfx_instrument_report(void (*write)(void *user, const char *line),
      void *user)
{
   char line[128];
   unsigned i;

   if (!write)
      return;
   for (i = 0; i < GFX_INSTR_COUNT; i++)
   {
      int v = retro_atomic_load_relaxed_int(&gfx_instrument_counters[i]);
      if (!v)
         continue;
      snprintf(line, sizeof(line), "%-20s %d", gfx_instrument_names[i], v);
      write(user, line);
   }
}

#endif

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - m4xw
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
#include <time.h>

#include <queues/message_queue.h>
#include <retro_miscellaneous.h>

#include "../../retroarch.h"

#include "../gfx_display.h"

static void gfx_display_switch_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height) { }

static const float *gfx_display_switch_get_default_vertices(void)
{
   static float dummy[16] = {0.0f};
   return &dummy[0];
}

static const float *gfx_display_switch_get_default_tex_coords(void)
{
   static float dummy[16] = {0.0f};
   return &dummy[0];
}

gfx_display_ctx_driver_t gfx_display_ctx_switch = {
   gfx_display_switch_draw,
   NULL,                                        /* draw_pipeline   */
   NULL,                                        /* blend_begin     */
   NULL,                                        /* blend_end       */
   NULL,                                        /* get_default_mvp */
   gfx_display_switch_get_default_vertices,
   gfx_display_switch_get_default_tex_coords,
   FONT_DRIVER_RENDER_SWITCH,
   GFX_VIDEO_DRIVER_SWITCH,
   "switch",
   false,
   NULL,                                         /* scissor_begin */
   NULL                                          /* scissor_end   */
};

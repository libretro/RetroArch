/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2019 - Francisco Javier Trujillo Mata
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
#include <stdlib.h>
#include <malloc.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>

#include "../font_driver.h"

#define FONTM_TEXTURE_COLOR         GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00)
#define FONTM_TEXTURE_SCALED        0.5f
#define FONTM_TEXTURE_LEFT_MARGIN   0
#define FONTM_TEXTURE_BOTTOM_MARGIN 15
#define FONTM_TEXTURE_ZPOSITION     3

typedef struct ps2_font_info
{
   ps2_video_t *ps2_video;
   GSFONTM *gsFontM;
} ps2_font_info_t;

static void *ps2_font_init_font(void *gl_data, const char *font_path,
      float font_size, bool is_threaded)
{
   ps2_font_info_t *ps2 = (ps2_font_info_t*)calloc(1, sizeof(ps2_font_info_t));
   ps2->ps2_video = (ps2_video_t *)gl_data;
   ps2->gsFontM = gsKit_init_fontm();

   gsKit_fontm_upload(ps2->ps2_video->gsGlobal, ps2->gsFontM);

   return ps2;
}

static void ps2_font_free_font(void *data, bool is_threaded)
{
   ps2_font_info_t *ps2 = (ps2_font_info_t *)data;
   gsKit_free_fontm(ps2->ps2_video->gsGlobal, ps2->gsFontM);
   ps2->ps2_video = NULL;
   
   free(ps2);
   ps2 = NULL;
}

static void ps2_font_render_msg(
      void *userdata,
      void *data, const char *msg,
      const struct font_params *params)
{
   ps2_font_info_t *ps2 = (ps2_font_info_t *)data;

   if (ps2) 
   {
      int x = FONTM_TEXTURE_LEFT_MARGIN;
      int y = ps2->ps2_video->gsGlobal->Height - FONTM_TEXTURE_BOTTOM_MARGIN;
      gsKit_fontm_print_scaled(
            ps2->ps2_video->gsGlobal, 
            ps2->gsFontM, x, y, FONTM_TEXTURE_ZPOSITION,
            FONTM_TEXTURE_SCALED , FONTM_TEXTURE_COLOR, msg);
   }
}

font_renderer_t ps2_font = {
   ps2_font_init_font,
   ps2_font_free_font,
   ps2_font_render_msg,
   "PS2 font",
   NULL,                      /* get_glyph */
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   NULL,                      /* get_message_width */
   NULL                       /* get_line_metrics */
};

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

#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>

#include "../font_driver.h"

#define FONTM_VRAM_SIZE 4096
#define FONTM_TEXTURE_COLOR GS_SETREG_RGBAQ(0x80,0x80,0x80,0x80,0x00)
#define FONTM_TEXTURE_WIDTH 52
#define FONTM_TEXTURE_HEIGHT 832
#define FONTM_TEXTURE_SPACING 1.0f
#define FONTM_TEXTURE_SCALED 0.5f
#define FONTM_TEXTURE_LEFT_MARGIN 0
#define FONTM_TEXTURE_BOTTOM_MARGIN 15
#define FONTM_TEXTURE_ZPOSITION 3

typedef struct ps2_font_info
{
   ps2_video_t *ps2_video;
   GSFONTM *gsFontM;
} ps2_font_info_t;

/* Copied from GSKIT FONTM CLUT
   FONTM Textures are GS_PSM_T4, and need a 16x16 CLUT
   This is a greyscale ramp CLUT, with linear alpha. */
static u32 gsKit_fontm_clut[16] = {	0x00000000, 0x11111111, 0x22222222, 0x33333333, \
					0x44444444, 0x55555555, 0x66666666, 0x77777777, \
					0x80888888, 0x80999999, 0x80AAAAAA, 0x80BBBBBB, \
					0x80CCCCCC, 0x80DDDDDD, 0x80EEEEEE, 0x80FFFFFF };

static void deinit_texture(GSTEXTURE *texture)
{
   if (texture->Mem!= NULL) {
      free(texture->Mem);
      texture->Mem = NULL;
   }

   if (texture->Mem!= NULL) {
      free(texture->Clut);
      texture->Clut = NULL;
   }
}

static void deinit_gsfont(GSFONTM *gsFontM)
{
   deinit_texture(gsFontM->Texture);
   free(gsFontM->TexBase);
   gsFontM->TexBase = NULL;
   free(gsFontM);
}

static void ps2_prepare_font(GSGLOBAL *gsGlobal, GSFONTM *gsFontM)
{  
   if(gsKit_fontm_unpack(gsFontM) == 0) {
      gsFontM->Texture->Width = FONTM_TEXTURE_WIDTH;
      gsFontM->Texture->Height = FONTM_TEXTURE_HEIGHT;
      gsFontM->Texture->PSM = GS_PSM_T4;
      gsFontM->Texture->ClutPSM = GS_PSM_CT32;
      gsFontM->Texture->Filter = GS_FILTER_LINEAR;
      gsKit_setup_tbw(gsFontM->Texture);
   }
}

static void ps2_upload_font(GSGLOBAL *gsGlobal, GSFONTM *gsFontM)
{
	int pgindx;
   int TexSize = gsKit_texture_size(gsFontM->Texture->Width, gsFontM->Texture->Height, gsFontM->Texture->PSM);

   gsFontM->Texture->VramClut = gsKit_vram_alloc(gsGlobal, FONTM_VRAM_SIZE, GSKIT_ALLOC_USERBUFFER);

   for (pgindx = 0; pgindx < GS_FONTM_PAGE_COUNT; ++pgindx) {
      gsFontM->Vram[pgindx] = gsKit_vram_alloc(gsGlobal, TexSize, GSKIT_ALLOC_USERBUFFER);
      gsFontM->LastPage[pgindx] = (u32) -1;
   }

   gsFontM->Texture->Vram = gsFontM->Vram[0];
   gsFontM->VramIdx = 0;
   gsFontM->Spacing = FONTM_TEXTURE_SPACING;
   gsFontM->Align = GSKIT_FALIGN_LEFT;

	gsFontM->Texture->Clut = memalign(GS_VRAM_TBWALIGN_CLUT, GS_VRAM_TBWALIGN);
	memcpy(gsFontM->Texture->Clut, gsKit_fontm_clut, GS_VRAM_TBWALIGN);
	gsKit_texture_send(gsFontM->Texture->Clut, 8,  2, gsFontM->Texture->VramClut, gsFontM->Texture->ClutPSM, 1, GS_CLUT_PALLETE);
	free(gsFontM->Texture->Clut);
}

static void *ps2_font_init_font(void *gl_data, const char *font_path,
      float font_size, bool is_threaded)
{
   ps2_font_info_t *ps2 = (ps2_font_info_t*)calloc(1, sizeof(ps2_font_info_t));
   ps2->ps2_video = (ps2_video_t *)gl_data;
   ps2->gsFontM = gsKit_init_fontm();

   ps2_prepare_font(ps2->ps2_video->gsGlobal, ps2->gsFontM);
   ps2_upload_font(ps2->ps2_video->gsGlobal, ps2->gsFontM);

   return ps2;
}

static void ps2_font_free_font(void *data, bool is_threaded)
{
   ps2_font_info_t *ps2 = (ps2_font_info_t *)data;
   deinit_gsfont(ps2->gsFontM);
   ps2->ps2_video = NULL;
   ps2 = NULL;
}

static void ps2_font_render_msg(
      video_frame_info_t *video_info,
      void *data, const char *msg,
      const struct font_params *params)
{
   ps2_font_info_t *ps2 = (ps2_font_info_t *)data;

   if (ps2) {
      int x = FONTM_TEXTURE_LEFT_MARGIN;
      int y = ps2->ps2_video->gsGlobal->Height - FONTM_TEXTURE_BOTTOM_MARGIN;
      if (ps2->ps2_video->clearVRAM_font) {
         ps2_upload_font(ps2->ps2_video->gsGlobal, ps2->gsFontM);
         ps2->ps2_video->clearVRAM_font = false;
      }
      gsKit_fontm_print_scaled(ps2->ps2_video->gsGlobal, ps2->gsFontM, x, y, FONTM_TEXTURE_ZPOSITION,
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
};

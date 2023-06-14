/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018 - Francisco Javier Trujillo Mata - fjtrujy
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

#include <stdint.h>
#include <stdlib.h>
#include <math.h>
#include <malloc.h>

#include <kernel.h>
#include <gsKit.h>
#include <dmaKit.h>
#include <gsToolkit.h>
#include <gsInline.h>

#include <encodings/utf.h>
#include <libretro_gskit_ps2.h>

#include "../../driver.h"
#include "../../retroarch.h"
#include "../../verbosity.h"

#include "../gfx_display.h"
#include "../common/ps2_defines.h"

/* Generic tint color */
#define GS_TEXT GS_SETREG_RGBA(0x80, 0x80, 0x80, 0x80)
/* turn black GS Screen */
#define GS_BLACK GS_SETREG_RGBA(0x00, 0x00, 0x00, 0x80)

#define NUM_RM_VMODES 7
#define PS2_RESOLUTION_LAST NUM_RM_VMODES - 1
#define RM_VMODE_AUTO 0

#define A_COLOR_SOURCE 0
#define A_COLOR_DEST 1
#define A_COLOR_NULL 2
#define A_ALPHA_SOURCE 0
#define A_ALPHA_DEST 1
#define A_ALPHA_FIX 2

/* RM Vmode -> GS Vmode conversion table */
struct rm_mode
{
   char mode;
   short int width;
   short int height;
   short int VCK;
   short int interlace;
   short int field;
   short int PAR1; /* Pixel Aspect Ratio 1 (For video modes with non-square pixels, like PAL/NTSC) */
   short int PAR2; /* Pixel Aspect Ratio 2 (For video modes with non-square pixels, like PAL/NTSC) */
   char *desc;
};

typedef struct
{
   GSTEXTURE *texture;
   const font_renderer_driver_t* font_driver;
   void* font_data;
} ps2_font_t;

static struct rm_mode rm_mode_table[NUM_RM_VMODES] = {
    /* SDTV modes */
    {-1, 704, -1, 4, GS_INTERLACED, GS_FIELD, -1, 11, "AUTO"},
    {GS_MODE_PAL, 704, 576, 4, GS_INTERLACED, GS_FIELD, 12, 11, "PAL@50Hz"},
    {GS_MODE_NTSC, 704, 480, 4, GS_INTERLACED, GS_FIELD, 10, 11, "NTSC@60Hz"},
    /* SDTV special modes */
    {GS_MODE_PAL, 704, 288, 4, GS_NONINTERLACED, GS_FRAME, 12, 22, "PAL@50Hz-288p"},
    {GS_MODE_NTSC, 704, 240, 4, GS_NONINTERLACED, GS_FRAME, 10, 22, "NTSC@60Hz-240p"},
    /* EDTV modes (component cable!) */
    {GS_MODE_DTV_480P, 704, 480, 2, GS_NONINTERLACED, GS_FRAME, 10, 11, "DTV480P@60Hz"},
    {GS_MODE_DTV_576P, 704, 576, 2, GS_NONINTERLACED, GS_FRAME, 12, 11, "DTV576P@50Hz"},
};

static int vsync_sema_id;

/*
 * FONT DRIVER
 */
static void* ps2_font_init(void* data, const char* font_path,
      float font_size, bool is_threaded)
{
   uint32_t j;
   int text_size, clut_size;
   uint8_t *tex8;
   uint32_t *clut32;
   const struct font_atlas* atlas = NULL;
   ps2_font_t* font = (ps2_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
      free(font);
      return NULL;
   }

   atlas                  = font->font_driver->get_atlas(font->font_data);
   font->texture          = (GSTEXTURE*)calloc(1, sizeof(GSTEXTURE));
   font->texture->Width   = atlas->width;
   font->texture->Height  = atlas->height;
   font->texture->PSM     = GS_PSM_T8;
   font->texture->ClutPSM = GS_PSM_CT32;
   font->texture->Filter  = GS_FILTER_NEAREST;

   /* Convert to 8bit texture */
   text_size           = gsKit_texture_size_ee(atlas->width, atlas->height, GS_PSM_T8);
   tex8                = (uint8_t*)malloc(text_size);
   for (j = 0; j <  atlas->width * atlas->height; j++ )
      tex8[j]          = atlas->buffer[j] & 0x000000FF;
   font->texture->Mem  = (u32 *)tex8;

   /* Create 8bit CLUT */
   clut_size           = gsKit_texture_size_ee(16, 16, GS_PSM_CT32);
   clut32              = (uint32_t*)malloc(clut_size);
   for (j = 0; j < 256; j++)
      clut32[j]        = 0x01010101 * j;
   font->texture->Clut = (u32 *)clut32;

   return font;
}

static void ps2_font_free(void* data, bool is_threaded)
{
   ps2_font_t* font = (ps2_font_t*)data;

   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   if (font->texture->Clut)
      free(font->texture->Clut);

   if (font->texture->Mem)
      free(font->texture->Mem);

   if (font->texture)
      free(font->texture);
}

static int ps2_font_get_message_width(void* data, const char* msg,
      size_t msg_len, float scale)
{
   int i;
   const struct font_glyph* glyph_q = NULL;
   int delta_x      = 0;
   ps2_font_t* font = (ps2_font_t*)data;

   if (!font)
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph* glyph;
      const char* msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph =
         font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void ps2_font_render_line(
      ps2_video_t *ps2,
      ps2_font_t* font, const char* msg, size_t msg_len,
      float scale, const unsigned int color, float pos_x,
      float pos_y,
      unsigned width, unsigned height, unsigned text_align)
{
   int i;
   const struct font_glyph* glyph_q = NULL;
   int x            = roundf(pos_x * width);
   int y            = roundf((1.0f - pos_y) * height);
   int delta_x      = 0;
   int delta_y      = 0;
   /* We need to >> 1, because GS_SETREG_RGBAQ expects 0x80 as max color */
   int color_a      = (int)(((color & 0xFF000000) >> 24) >> 2);
   int color_b      = (int)(((color & 0x00FF0000) >> 16) >> 1);
   int color_g      = (int)(((color & 0x0000FF00) >> 8)  >> 1);
   int color_r      = (int)(((color & 0x000000FF) >> 0)  >> 1);

   /* Enable Alpha for font */
   gsKit_set_primalpha(ps2->gsGlobal, GS_SETREG_ALPHA(0, 1, 0, 1, 0), 0);
   ps2->gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
   gsKit_set_test(ps2->gsGlobal, GS_ATEST_ON);

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= ps2_font_get_message_width(font, msg, msg_len, scale);
         break;

      case TEXT_ALIGN_CENTER:
         x -= ps2_font_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph* glyph;
      int off_x, off_y, tex_x, tex_y, width, height;
      float x1, y1, u1, v1, x2, y2, u2, v2;
      const char* msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      /* Do something smarter here ... */
      if (!(glyph =
               font->font_driver->get_glyph(font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      off_x  = glyph->draw_offset_x;
      off_y  = glyph->draw_offset_y;
      tex_x  = glyph->atlas_offset_x;
      tex_y  = glyph->atlas_offset_y;
      width  = glyph->width;
      height = glyph->height;

      /* The -0.5 is needed to achieve pixel perfect. 
       * More info here (PS2 GSKit uses same logic as Direct3D9)
       * https://docs.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-coordinates
      */
      x1     = -0.5f + x + (off_x + delta_x) * scale;
      y1     = -0.5f + y + (off_y + delta_y) * scale;
      u1     = tex_x;
      v1     = tex_y;
      x2     = x1 + width * scale;
      y2     = y1 + height * scale;
      u2     = u1 + width;
      v2     = v1 + height;

      gsKit_prim_sprite_texture(ps2->gsGlobal, font->texture,
            x1,                /* X1 */
            y1,                /* Y1 */
            u1,                /* U1 */
            v1,                /* V1 */
            x2,                /* X2 */
            y2,                /* Y2 */
            u2,                /* U2 */
            v2,                /* V2 */
            5,                 /* Z  */
            GS_SETREG_RGBAQ(color_r, color_g, color_b, color_a, 0x00));

      delta_x += glyph->advance_x;
      delta_y += glyph->advance_y;
   }
}

static void ps2_font_render_message(
      ps2_video_t *ps2,
      ps2_font_t* font, const char* msg, float scale,
      const unsigned int color, float pos_x, float pos_y,
      unsigned width, unsigned height, unsigned text_align)
{
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = (float)line_metrics->height * scale / (float)height;

   for (;;)
   {
      const char* delim = strchr(msg, '\n');
      size_t msg_len    = delim ? (delim - msg) : strlen(msg);

      /* Draw the line */
      ps2_font_render_line(ps2, font, msg, msg_len,
            scale, color, pos_x, pos_y - (float)lines * line_height,
            width, height, text_align);
      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void ps2_font_render_msg(
      void *userdata,
      void* data, const char* msg,
      const struct font_params *params)
{
   float x, y, scale, drop_mod, drop_alpha;
   int drop_x, drop_y;
   enum text_alignment text_align;
   unsigned color, r, g, b, alpha;
   ps2_font_t                * font = (ps2_font_t*)data;
   ps2_video_t                *ps2  = (ps2_video_t*)userdata;
   unsigned width                   = ps2->vp.full_width;
   unsigned height                  = ps2->vp.full_height;

   if (!font || !msg || !*msg)
      return;

   if (params)
   {
      x                       = params->x;
      y                       = params->y;
      scale                   = params->scale;
      text_align              = params->text_align;
      drop_x                  = params->drop_x;
      drop_y                  = params->drop_y;
      drop_mod                = params->drop_mod;
      drop_alpha              = params->drop_alpha;

      r                       = FONT_COLOR_GET_RED(params->color);
      g                       = FONT_COLOR_GET_GREEN(params->color);
      b                       = FONT_COLOR_GET_BLUE(params->color);
      alpha                   = FONT_COLOR_GET_ALPHA(params->color);

      color                   = COLOR_ABGR(r, g, b, alpha);
   }
   else
   {
      settings_t *settings    = config_get_ptr();
      float video_msg_pos_x   = settings->floats.video_msg_pos_x;
      float video_msg_pos_y   = settings->floats.video_msg_pos_y;
      float video_msg_color_r = settings->floats.video_msg_color_r;
      float video_msg_color_g = settings->floats.video_msg_color_g;
      float video_msg_color_b = settings->floats.video_msg_color_b;
      x                       = video_msg_pos_x;
      y                       = video_msg_pos_y;
      scale                   = 1.0f;
      text_align              = TEXT_ALIGN_LEFT;

      r                       = (video_msg_color_r * 255);
      g                       = (video_msg_color_g * 255);
      b                       = (video_msg_color_b * 255);
      alpha                   = 255;
      color                   = COLOR_ABGR(r, g, b, alpha);

      drop_x                  = 1;
      drop_y                  = -1;
      drop_mod                = 0.0f;
      drop_alpha              = 0.75f;
   }

   gsKit_TexManager_bind(ps2->gsGlobal, font->texture);

   if (drop_x || drop_y)
   {
      unsigned r_dark         = r * drop_mod;
      unsigned g_dark         = g * drop_mod;
      unsigned b_dark         = b * drop_mod;
      unsigned alpha_dark     = alpha * drop_alpha;
      unsigned color_dark     = COLOR_ABGR(r_dark, g_dark, b_dark, alpha_dark);
      ps2_font_render_message(ps2, font, msg, scale, color_dark,
                              x + scale * drop_x / width, y +
                              scale * drop_y / height,
                              width, height, text_align);
   }

   ps2_font_render_message(ps2, font, msg, scale,
                           color, x, y,
                           width, height, text_align);
}

static const struct font_glyph* ps2_font_get_glyph(
   void* data, uint32_t code)
{
   ps2_font_t* font = (ps2_font_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static bool ps2_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   ps2_font_t* font = (ps2_font_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t ps2_font = {
   ps2_font_init,
   ps2_font_free,
   ps2_font_render_msg,
   "ps2",
   ps2_font_get_glyph,
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   ps2_font_get_message_width,
   ps2_font_get_line_metrics
};

/*
 * VIDEO DRIVER
 */

/* PRIVATE METHODS */
static int vsync_handler(void)
{
   iSignalSema(vsync_sema_id);

   ExitHandler();
   return 0;
}

static void rmEnd(ps2_video_t *ps2)
{
   gsKit_deinit_global(ps2->gsGlobal);
   gsKit_remove_vsync_handler(ps2->vsync_callback_id);
}

static void ps2_update_offsets_if_needed(ps2_video_t *ps2)
{
   settings_t *settings       = config_get_ptr();
   int video_window_offset_x  = settings->ints.video_window_offset_x;
   int video_window_offset_y  = settings->ints.video_window_offset_y;

   if (     (video_window_offset_x != ps2->video_window_offset_x)
         || (video_window_offset_y != ps2->video_window_offset_y))
   {
      ps2->video_window_offset_x = video_window_offset_x;
      ps2->video_window_offset_y = video_window_offset_y;

      gsKit_set_display_offset(ps2->gsGlobal, ps2->video_window_offset_x * rm_mode_table[ps2->vmode].VCK, ps2->video_window_offset_y);
      RARCH_LOG("PS2_GFX Change offset: %d, %d\n", ps2->video_window_offset_x, ps2->video_window_offset_y);
   }
}

/* Copy of gsKit_sync_flip, but without the 'flip' */
static void gsKit_sync(GSGLOBAL *gsGlobal)
{
   if (!gsGlobal->FirstFrame)
      WaitSema(vsync_sema_id);

   while (PollSema(vsync_sema_id) >= 0);
}

/* Copy of gsKit_sync_flip, but without the 'sync' */
static void gsKit_flip(GSGLOBAL *gsGlobal)
{
   if (!gsGlobal->FirstFrame)
   {
      if (gsGlobal->DoubleBuffering == GS_SETTING_ON)
      {
         GS_SET_DISPFB2(gsGlobal->ScreenBuffer[gsGlobal->ActiveBuffer & 1] / 8192,
                        gsGlobal->Width / 64, gsGlobal->PSM, 0, 0);

         gsGlobal->ActiveBuffer ^= 1;
      }
   }

   gsKit_setactive(gsGlobal);
}

static void rmSetMode(ps2_video_t *ps2, int force)
{
   struct rm_mode *mode;
   global_t *global = global_get_ptr();

   /* we don't want to set the vmode without a reason... */
   if (ps2->vmode == global->console.screen.resolutions.current.id && force == 0)
      return;

   /* Cleanup previous gsKit instance */
   if (ps2->vmode >= 0)
   {
      if (ps2->gsGlobal)
         rmEnd(ps2);
      /* Set new mode */
      global->console.screen.resolutions.current.id = ps2->vmode;
   }
   else
      /* first driver init */
      ps2->vmode                  = global->console.screen.resolutions.current.id;

   mode                           = &rm_mode_table[ps2->vmode];

   /* Invalidate scaling state */
   ps2->iTextureWidth             = 0;
   ps2->iTextureHeight            = 0;

   ps2->gsGlobal                  = gsKit_init_global();
   gsKit_TexManager_setmode(ps2->gsGlobal, ETM_DIRECT);
   ps2->vsync_callback_id         = gsKit_add_vsync_handler(vsync_handler);
   ps2->gsGlobal->Mode            = mode->mode;
   ps2->gsGlobal->Width           = mode->width;
   ps2->gsGlobal->Height          = mode->height;
   ps2->gsGlobal->Interlace       = mode->interlace;
   ps2->gsGlobal->Field           = mode->field;
   ps2->gsGlobal->PSM             = GS_PSM_CT16;
   ps2->gsGlobal->PSMZ            = GS_PSMZ_16S;
   ps2->gsGlobal->DoubleBuffering = GS_SETTING_ON;
   ps2->gsGlobal->ZBuffering      = GS_SETTING_OFF;
   ps2->gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;

   if ((ps2->gsGlobal->Interlace == GS_INTERLACED) && (ps2->gsGlobal->Field == GS_FRAME))
      ps2->gsGlobal->Height /= 2;

   /* Coordinate space ranges from 0 to 4096 pixels
   * Center the buffer in the coordinate space */
   ps2->gsGlobal->OffsetX = ((4096 - ps2->gsGlobal->Width)  / 2) * 16;
   ps2->gsGlobal->OffsetY = ((4096 - ps2->gsGlobal->Height) / 2) * 16;

   gsKit_init_screen(ps2->gsGlobal);
   gsKit_mode_switch(ps2->gsGlobal, GS_ONESHOT);

   gsKit_set_test(ps2->gsGlobal, GS_ZTEST_OFF);
   gsKit_set_test(ps2->gsGlobal, GS_ATEST_OFF);

   /* reset the contents of the screen to avoid garbage being displayed */
   gsKit_clear(ps2->gsGlobal, GS_BLACK);
   gsKit_sync(ps2->gsGlobal);
   gsKit_flip(ps2->gsGlobal);

   RARCH_LOG("PS2_GFX New vmode: %d, %d x %d\n", ps2->vmode, ps2->gsGlobal->Width, ps2->gsGlobal->Height);

   ps2_update_offsets_if_needed(ps2);
}

static void rmInit(ps2_video_t *ps2)
{
   ee_sema_t sema;
   short int mode;

   sema.init_count = 0;
   sema.max_count  = 1;
   sema.option     = 0;

   vsync_sema_id   = CreateSema(&sema);
   mode            = gsKit_check_rom();

   rm_mode_table[RM_VMODE_AUTO].mode   = mode;
   rm_mode_table[RM_VMODE_AUTO].height = (mode == GS_MODE_PAL) ? 576 : 480;
   rm_mode_table[RM_VMODE_AUTO].PAR1   = (mode == GS_MODE_PAL) ? 12  : 10;

   dmaKit_init(D_CTRL_RELE_OFF, D_CTRL_MFD_OFF, D_CTRL_STS_UNSPEC,
               D_CTRL_STD_OFF, D_CTRL_RCYC_8, 1 << DMA_CHANNEL_GIF);

   /* Initialize the DMAC */
   dmaKit_chan_init(DMA_CHANNEL_GIF);

   rmSetMode(ps2, 1);
}

static void init_ps2_video(ps2_video_t *ps2)
{
   ps2->vmode                   = -1;
   rmInit(ps2);

   ps2->vp.x                    = 0;
   ps2->vp.y                    = 0;
   ps2->vp.width                = ps2->gsGlobal->Width;
   ps2->vp.height               = ps2->gsGlobal->Height;
   ps2->vp.full_width           = ps2->gsGlobal->Width;
   ps2->vp.full_height          = ps2->gsGlobal->Height;

   ps2->menuTexture             = (GSTEXTURE*)calloc(1, sizeof(GSTEXTURE));
   ps2->coreTexture             = (GSTEXTURE*)calloc(1, sizeof(GSTEXTURE));

   ps2->video_window_offset_x   = 0;
   ps2->video_window_offset_y   = 0;

   /* Used for cores that supports palette */
   ps2->iface.interface_type    = RETRO_HW_RENDER_INTERFACE_GSKIT_PS2;
   ps2->iface.interface_version = RETRO_HW_RENDER_INTERFACE_GSKIT_PS2_VERSION;
   ps2->iface.coreTexture       = ps2->coreTexture;
}

static void ps2_deinit_texture(GSTEXTURE *texture)
{
   texture->Mem  = NULL;
   texture->Clut = NULL;
}

static void set_texture(GSTEXTURE *texture, const void *frame,
      int width, int height, int PSM, int filter)
{
   texture->Width  = width;
   texture->Height = height;
   texture->PSM    = PSM;
   texture->Filter = filter;
   texture->Mem    = (void *)frame;
}

static INLINE int ABS(int v)
{
   return (v >= 0) ? v : -v;
}

static void setupScalingMode(ps2_video_t *ps2, int iWidth, int iHeight, float fDAR, bool bScaleInteger)
{
   GSGLOBAL *gsGlobal       = ps2->gsGlobal;
   struct rm_mode *currMode = &rm_mode_table[ps2->vmode];
   int iBestFBWidth         = currMode->width;
   int iBestMagH            = currMode->VCK - 1;
   /* Calculate the pixel aspect ratio (PAR) */
   float fPAR               = (float)currMode->PAR2 / (float)currMode->PAR1;

   if (bScaleInteger)
   {
      /* Best match the framebuffer width/height to a multiple of the texture
       * Width, rounded down so it always fits */
      int iHeightScale = MAX(1, currMode->height / iHeight);
      ps2->iDisplayHeight = iHeight * iHeightScale;
      /* Height, rounded */
      int iWidthScale = MAX(1, (int)((float)ps2->iDisplayHeight * fDAR * fPAR + (float)(iWidth / 2)) / iWidth);
      ps2->iDisplayWidth = iWidth * iWidthScale;

#if defined(DEBUG)
      printf("Integer scaling:\n");
      printf("- Width  = %d x %d = %d\n", iWidth, iWidthScale, ps2->iDisplayWidth);
      printf("- Height = %d x %d = %d\n", iHeight, iHeightScale, ps2->iDisplayHeight);
#endif

      if (currMode->VCK > 1 && ps2->iDisplayWidth < currMode->width)
      {
         /* We try to best match the number of "VCK" units, for the best output
          * For 576i/480i: 1 pixel = 4 VCK (4x super-resolution)
          * For 576p/480p: 1 pixel = 2 VCK (2x super-resolution) */
         int iTargetVCK = (int)((float)ps2->iDisplayHeight * fDAR * fPAR * (float)currMode->VCK + 0.5f);
         int iBestVCK = ps2->iDisplayWidth * currMode->VCK;
         /* Try all possible framebuffer widths */
#if defined(DEBUG)
         printf("Find match for %d * SCALE * MagH = %d VCK (current = %d VCK)\n", iWidth, iTargetVCK, iBestVCK);
#endif
         int iFBWidth;
         for (iFBWidth = 64; iFBWidth <= currMode->width; iFBWidth += 64)
         {
            /* Ignore too small framebuffers */
            if (iFBWidth < iWidth)
               continue;

            iWidthScale = iFBWidth / iWidth;

            /* Try all possible magnifications */
            int iMagH;
            for (iMagH = 0; iMagH < 15; iMagH++)
            {
               int iVCK = iWidth * iWidthScale * (iMagH + 1);
               if (ABS(iTargetVCK - iVCK) < ABS(iTargetVCK - iBestVCK))
               {
#if defined(DEBUG)
                  printf("- found %d * %d * %d = %d\n", iWidth, iWidthScale, iMagH + 1, iVCK);
#endif

                  /* Better match */
                  iBestVCK = iVCK;
                  iBestFBWidth = iFBWidth;
                  iBestMagH = iMagH;
                  ps2->iDisplayWidth = iWidth * iWidthScale;
               }
            }
         }
      }
   }
   else
   {
      /* Assume black bars left/right */
      ps2->iDisplayHeight = currMode->height;
      ps2->iDisplayWidth = (int)((float)ps2->iDisplayHeight * fDAR * fPAR + 0.5f);
      if (ps2->iDisplayWidth > currMode->width)
      {
         /* Really wide screen, black bars top/bottom */
         ps2->iDisplayWidth = currMode->width;
         ps2->iDisplayHeight = (int)((float)ps2->iDisplayWidth / (fDAR * fPAR) + 0.5f);
      }
   }

   if ((gsGlobal->Interlace == GS_INTERLACED) && (gsGlobal->Field == GS_FRAME))
      ps2->iDisplayHeight /= 2;

#if defined(DEBUG)
   printf("Texture resolution:\n");
   printf("- Width  = %d x %.2f = %d\n", iWidth, (float)ps2->iDisplayWidth / (float)iWidth, ps2->iDisplayWidth);
   printf("- Height = %d x %.2f = %d\n", iHeight, (float)ps2->iDisplayHeight / (float)iHeight, ps2->iDisplayHeight);
   printf("Setting custom framebuffer:\n");
   printf("- Width  = %d x %d / %d = %d\n", iBestFBWidth, iBestMagH + 1, currMode->VCK, iBestFBWidth * (iBestMagH + 1) / currMode->VCK);
   printf("- Height = %d x %d     = %d\n", currMode->height, gsGlobal->MagV + 1, currMode->height * (gsGlobal->MagV + 1));
#endif

   /* Center on screen by adding the difference (in VCK units). */
   gsGlobal->StartX += (gsGlobal->Width * (gsGlobal->MagH + 1) - iBestFBWidth * (iBestMagH + 1)) / 2;
   /* Calculate the actual display width and height, again */
   gsGlobal->DW = (iBestMagH + 1) * iBestFBWidth;
   /* Override magh */
   gsGlobal->MagH = iBestMagH;

   /* Reset VRAM allocation */
   gsGlobal->CurrentPointer = 0;
   /* Allocate new framebuffer(s) */
   gsGlobal->ScreenBuffer[0] = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(iBestFBWidth, currMode->height, gsGlobal->PSM), GSKIT_ALLOC_SYSBUFFER);
   if (gsGlobal->DoubleBuffering == GS_SETTING_ON)
      gsGlobal->ScreenBuffer[1] = gsKit_vram_alloc(gsGlobal, gsKit_texture_size(iBestFBWidth, currMode->height, gsGlobal->PSM), GSKIT_ALLOC_SYSBUFFER);
   /* Set the new framebuffer as display buffer */
   gsGlobal->Width = iBestFBWidth;
   gsKit_display_buffer(gsGlobal);

   /* Update DISPLAY1/2 register (code from gsKit) */
   GS_SET_DISPLAY1(
       gsGlobal->StartX + gsGlobal->StartXOffset,
       gsGlobal->StartY + gsGlobal->StartYOffset,
       gsGlobal->MagH,
       gsGlobal->MagV,
       gsGlobal->DW - 1,
       gsGlobal->DH - 1);
   GS_SET_DISPLAY2(
       gsGlobal->StartX + gsGlobal->StartXOffset,
       gsGlobal->StartY + gsGlobal->StartYOffset,
       gsGlobal->MagH,
       gsGlobal->MagV,
       gsGlobal->DW - 1,
       gsGlobal->DH - 1);
}

static void *ps2_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   void *ps2input   = NULL;
   ps2_video_t *ps2 = (ps2_video_t *)calloc(1, sizeof(ps2_video_t));

   *input_data      = NULL;

   if (!ps2)
      return NULL;

   init_ps2_video(ps2);
   if (video->font_enable)
      font_driver_init_osd(ps2,
            video, false,
            video->is_threaded,
            FONT_DRIVER_RENDER_PS2);

   ps2->PSM          = (video->rgb32 ? GS_PSM_CT32 : GS_PSM_CT16);
   ps2->tex_filter   = video->smooth ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;
   ps2->force_aspect = video->force_aspect;
   ps2->vsync        = video->vsync;

   if (input && input_data)
   {
      settings_t *settings = config_get_ptr();
      ps2input = input_driver_init_wrap(&input_ps2,
                                        settings->arrays.input_joypad_driver);
      *input = ps2input ? &input_ps2 : NULL;
      *input_data = ps2input;
   }

   return ps2;
}

static bool ps2_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   ps2_video_t *ps2               = (ps2_video_t *)data;
   GSGLOBAL *gsGlobal             = ps2->gsGlobal;
   struct font_params *osd_params = (struct font_params *)&video_info->osd_stat_params;
   bool statistics_show           = video_info->statistics_show;
   settings_t *settings           = config_get_ptr();
   GSTEXTURE *tex                 = ps2->coreTexture;

   if (!width || !height)
      return false;

#if defined(DEBUG)
   if (frame_count % 180 == 0)
      printf("ps2_frame %llu\n", frame_count);
#endif

   /* Check if user change offset values */
   ps2_update_offsets_if_needed(ps2);

   if (frame)
   {
      /* New frame from core, update */
      float fDAR = ps2->force_aspect ? video_driver_get_aspect_ratio() : 0;
      bool bScaleInteger = settings->bools.video_scale_integer;

      /* Checking if the transfer is done in the core */
      if (frame != RETRO_HW_FRAME_BUFFER_VALID)
      {
         /* SW rendered texture */
         int shifh_per_bytes = (ps2->PSM == GS_PSM_CT32) ? 2 : 1;
         int real_width = pitch >> shifh_per_bytes;
         set_texture(tex, frame, real_width, height, ps2->PSM, ps2->tex_filter);

         /* Padding */
         ps2->padding = empty_ps2_insets;
         ps2->padding.right = real_width - width;
      }
      else
      {
         /* "HW" rendered texture */
         /* Set current filter mode */
         tex->Filter = ps2->tex_filter;

         /* Padding */
         ps2->padding = ps2->iface.padding;
      }

      /* Texture dimensions */
      int iTextureWidth = tex->Width - ps2->padding.left - ps2->padding.right;
      int iTextureHeight = tex->Height - ps2->padding.top - ps2->padding.bottom;
      if (ps2->iTextureWidth != iTextureWidth ||
          ps2->iTextureHeight != iTextureHeight ||
          ps2->fDAR != fDAR ||
          ps2->bScaleInteger != bScaleInteger)
      {
         /* Scaling changed, try to find best matching output mode */
         ps2->iTextureWidth = iTextureWidth;
         ps2->iTextureHeight = iTextureHeight;
         ps2->fDAR = fDAR;
         ps2->bScaleInteger = bScaleInteger;
         setupScalingMode(ps2, iTextureWidth, iTextureHeight, fDAR, bScaleInteger);
      }

      gsKit_TexManager_invalidate(ps2->gsGlobal, tex);
   }

   /* Center texture on framebuffer */
   float fDisplayOffsetX = (gsGlobal->Width - ps2->iDisplayWidth + 1) / 2 - 0.5f;
   float fDisplayOffsetY = (gsGlobal->Height - ps2->iDisplayHeight + 1) / 2 - 0.5f;
   /* Draw */
   gsGlobal->PrimAlphaEnable = GS_SETTING_OFF;
   gsKit_TexManager_bind(gsGlobal, tex);
   gsKit_prim_sprite_texture(gsGlobal, tex,
                             fDisplayOffsetX,                        /* X1 */
                             fDisplayOffsetY,                        /* Y1 */
                             ps2->padding.left,                      /* U1 */
                             ps2->padding.top,                       /* V1 */
                             fDisplayOffsetX + ps2->iDisplayWidth,   /* X2 */
                             fDisplayOffsetY + ps2->iDisplayHeight,  /* Y2 */
                             ps2->padding.left + ps2->iTextureWidth, /* U2 */
                             ps2->padding.top + ps2->iTextureHeight, /* V2 */
                             1,
                             GS_TEXT);

   if (ps2->menuVisible)
   {
      bool texture_empty = !ps2->menuTexture->Width || !ps2->menuTexture->Height;
      if (!texture_empty)
      {
         /* (A - B) * C + D */
         gsKit_set_primalpha(gsGlobal, GS_SETREG_ALPHA(A_COLOR_DEST, A_COLOR_NULL, A_ALPHA_FIX, A_COLOR_SOURCE, 0x20), 0);
         gsGlobal->PrimAlphaEnable = GS_SETTING_ON;
         gsKit_prim_sprite_texture(gsGlobal, ps2->menuTexture,
                                   -0.5f,                           /* X1 */
                                   -0.5f,                           /* Y1 */
                                   0,                               /* U1 */
                                   0,                               /* V1 */
                                   -0.5f + (float)gsGlobal->Width,  /* X2 */
                                   -0.5f + (float)gsGlobal->Height, /* Y2 */
                                   ps2->menuTexture->Width,         /* U2 */
                                   ps2->menuTexture->Height,        /* V2 */
                                   2,
                                   GS_TEXT);
      }
   }
   else if (statistics_show)
   {
      if (osd_params)
         font_driver_render_msg(ps2, video_info->stat_text, osd_params, NULL);
   }

   if (!string_is_empty(msg))
      font_driver_render_msg(ps2, msg, NULL, NULL);

   if (gsGlobal->DoubleBuffering == GS_SETTING_OFF)
   {
      /* Without double buffering:
       * - Wait for VSync
       * - Draw to front buffer (during VSync, so it's not visible to the user) */
      if (ps2->vsync)
         gsKit_sync(gsGlobal);
      gsKit_queue_exec(gsGlobal);
   }
   else
   {
      /* With double buffering:
       * - Draw to back buffer (invisible to user)
       * - Make sure drawing is completed (gsKit_finish)
       * - Wait for VSync
       * - Flip (back and front) buffers */
      gsKit_queue_exec(gsGlobal);
      gsKit_finish();
      if (ps2->vsync)
         gsKit_sync(gsGlobal);
      gsKit_flip(gsGlobal);
   }

   gsKit_TexManager_nextFrame(gsGlobal);
   gsKit_clear(gsGlobal, GS_BLACK);

   return true;
}

static void ps2_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;
   if (ps2)
      ps2->vsync = !toggle;
}

static bool ps2_alive(void *data) { return true; }
static bool ps2_focus(void *data) { return true; }
static bool ps2_suppress_screensaver(void *a, bool b) { return false; }
static bool ps2_has_windowed(void *data) { return false; }

static void ps2_free(void *data)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;

   gsKit_clear(ps2->gsGlobal, GS_BLACK);
   gsKit_vram_clear(ps2->gsGlobal);

   font_driver_free_osd();

   ps2_deinit_texture(ps2->menuTexture);
   ps2_deinit_texture(ps2->coreTexture);

   free(ps2->menuTexture);
   free(ps2->coreTexture);

   if (ps2->gsGlobal)
      rmEnd(ps2);

   if (vsync_sema_id >= 0)
      DeleteSema(vsync_sema_id);

   free(data);
}

static bool ps2_set_shader(void *data,
      enum rarch_shader_type type, const char *path) { return false; }

static void ps2_set_video_mode(void *data, unsigned fbWidth, unsigned lines,
      bool fullscreen)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;
   if (!ps2)
      return;

   rmSetMode(ps2, 0);
}

static void ps2_set_filtering(void *data, unsigned index, bool smooth, bool ctx_scaling)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;

   ps2->menu_filter = smooth ? GS_FILTER_LINEAR : GS_FILTER_NEAREST;
}

static void ps2_get_video_output_size(void *data,
      unsigned *width, unsigned *height, char *desc, size_t desc_len)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;
   if (!ps2)
      return;

   /* If the current index is out of bound default it to zero */
   if (ps2->vmode > PS2_RESOLUTION_LAST || ps2->vmode < 0)
      ps2->vmode = 0;

   *width  = rm_mode_table[ps2->vmode].width;
   *height = rm_mode_table[ps2->vmode].height;

   strlcpy(desc, rm_mode_table[ps2->vmode].desc, desc_len);
}

static void ps2_get_video_output_prev(void *data)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;
   if (!ps2)
      return;

   if (ps2->vmode == 0)
      ps2->vmode = PS2_RESOLUTION_LAST;
   else
      ps2->vmode--;
}

static void ps2_get_video_output_next(void *data)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;
   if (!ps2)
      return;

   if (ps2->vmode >= PS2_RESOLUTION_LAST)
      ps2->vmode = 0;
   else
      ps2->vmode++;
}

static void ps2_set_texture_frame(void *data, const void *frame, bool rgb32,
                                  unsigned width, unsigned height, float alpha)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;

   int PSM          = (rgb32 ? GS_PSM_CT32 : GS_PSM_CT16);

   set_texture(ps2->menuTexture, frame, width, height, PSM, ps2->menu_filter);
   gsKit_TexManager_invalidate(ps2->gsGlobal, ps2->menuTexture);
   gsKit_TexManager_bind(ps2->gsGlobal, ps2->menuTexture);
}

static void ps2_set_texture_enable(void *data, bool enable, bool fullscreen)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;

   ps2->menuVisible = enable;
}

static void ps2_set_osd_msg(void *data, const char *msg,
      const struct font_params *params, void *font)
{
   ps2_video_t *ps2 = (ps2_video_t *)data;

   if (ps2)
      font_driver_render_msg(data, msg, params, font);
}

static bool ps2_get_hw_render_interface(void *data,
      const struct retro_hw_render_interface **iface)
{
   ps2_video_t *ps2   = (ps2_video_t *)data;
   ps2->iface.padding = empty_ps2_insets;
   *iface =
       (const struct retro_hw_render_interface *)&ps2->iface;
   return true;
}

static const video_poke_interface_t ps2_poke_interface = {
   NULL, /* get_flags  */
   NULL, /* load_texture */
   NULL, /* unload_texture */
   ps2_set_video_mode,
   NULL, /* get_refresh_rate */
   ps2_set_filtering,
   ps2_get_video_output_size,
   ps2_get_video_output_prev,
   ps2_get_video_output_next,
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   NULL, /* set_aspect_ratio */
   NULL, /* apply_state_changes */
   ps2_set_texture_frame,
   ps2_set_texture_enable,
   ps2_set_osd_msg,
   NULL, /* show_mouse  */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   ps2_get_hw_render_interface,
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
};

static void ps2_get_poke_interface(void *data,
      const video_poke_interface_t **iface)
{
   *iface = &ps2_poke_interface;
}

video_driver_t video_ps2 = {
   ps2_init,
   ps2_frame,
   ps2_set_nonblock_state,
   ps2_alive,
   ps2_focus,
   ps2_suppress_screensaver,
   ps2_has_windowed,
   ps2_set_shader,
   ps2_free,
   "ps2",
   NULL, /* set_viewport */
   NULL, /* set_rotation */
   NULL, /* viewport_info */
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   NULL, /* overlay_interface */
#endif
   ps2_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   NULL  /* gfx_widgets_enabled */
#endif
};

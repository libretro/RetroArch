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

typedef struct
{
   GSTEXTURE *texture;
   const font_renderer_driver_t* font_driver;
   void* font_data;
} ps2_font_t;

static void* ps2_font_init_font(void* data, const char* font_path,
      float font_size, bool is_threaded)
{
   const struct font_atlas* atlas = NULL;
   uint32_t j;
   ps2_font_t* font = (ps2_font_t*)calloc(1, sizeof(*font));
   ps2_video_t* ps2 = (ps2_video_t*)data;

   if (!font)
      return NULL;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
      RARCH_WARN("Couldn't initialize font renderer.\n");
      free(font);
      return NULL;
   }

   atlas = font->font_driver->get_atlas(font->font_data);
   font->texture = (GSTEXTURE*)calloc(1, sizeof(GSTEXTURE));
   font->texture->Width = atlas->width;
   font->texture->Height = atlas->height;
   font->texture->PSM = GS_PSM_T8;
   font->texture->ClutPSM = GS_PSM_CT32;
   font->texture->Filter = GS_FILTER_NEAREST;

   // Convert to 8bit texture
   int textSize = gsKit_texture_size_ee(atlas->width, atlas->height, GS_PSM_T8);
   uint8_t *tex8 = malloc(textSize);
   for (j = 0; j <  atlas->width * atlas->height; j++ )
      tex8[j] = atlas->buffer[j] & 0x000000FF;
   font->texture->Mem = (u32 *)tex8;

   // Create 8bit CLUT
   int clutSize = gsKit_texture_size_ee(16, 16, GS_PSM_CT32);
   uint32_t *clut32 = malloc(clutSize);
   for (j = 0; j < 256; j++ )
      clut32[j] = 0x01010101 * j;
   font->texture->Clut = (u32 *)clut32;

   return font;
}

static void ps2_font_free_font(void* data, bool is_threaded)
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
                                      unsigned msg_len, float scale)
{
   ps2_font_t* font = (ps2_font_t*)data;

   unsigned i;
   int delta_x = 0;

   if (!font)
      return 0;

   for (i = 0; i < msg_len; i++)
   {
      const char* msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      const struct font_glyph* glyph =
         font->font_driver->get_glyph(font->font_data, code);

      if (!glyph) /* Do something smarter here ... */
         glyph = font->font_driver->get_glyph(font->font_data, '?');

      if (!glyph)
         continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void ps2_font_render_line(
      ps2_video_t *ps2,
      ps2_font_t* font, const char* msg, unsigned msg_len,
      float scale, const unsigned int color, float pos_x,
      float pos_y,
      unsigned width, unsigned height, unsigned text_align)
{
   unsigned i;

   int x            = roundf(pos_x * width);
   int y            = roundf((1.0f - pos_y) * height);
   int delta_x      = 0;
   int delta_y      = 0;
   int colorR, colorG, colorB, colorA;

   if (!ps2)
      return;
   
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

   /* We need to >> 1, because GS_SETREG_RGBAQ expect 0x80 as max color */
   colorA = (int)(((color & 0xFF000000) >> 24) >> 2);
   colorB = (int)(((color & 0x00FF0000) >> 16) >> 1);
   colorG = (int)(((color & 0x0000FF00) >> 8) >> 1);
   colorR = (int)(((color & 0x000000FF) >> 0) >> 1);

   for (i = 0; i < msg_len; i++)
   {
      int off_x, off_y, tex_x, tex_y, width, height;
      float x1, y1, u1, v1, x2, y2, u2, v2;
      const char* msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      const struct font_glyph* glyph =
         font->font_driver->get_glyph(font->font_data, code);

      if (!glyph) /* Do something smarter here ... */
         glyph = font->font_driver->get_glyph(font->font_data, '?');

      if (!glyph)
         continue;

      off_x  = glyph->draw_offset_x;
      off_y  = glyph->draw_offset_y;
      tex_x  = glyph->atlas_offset_x;
      tex_y  = glyph->atlas_offset_y;
      width  = glyph->width;
      height = glyph->height;

      /* The -0.5 is needed to achieve pixel perfect. More info here (PS2 uses same logic than Directx 9)
      *  https://docs.microsoft.com/en-us/windows/win32/direct3d10/d3d10-graphics-programming-guide-resources-coordinates
      */
      x1 = -0.5f + x + (off_x + delta_x) * scale;
      y1 = -0.5f + y + (off_y + delta_y) * scale;
      u1 = tex_x;
      v1 = tex_y;
      x2 = x1 + width * scale;
      y2 = y1 + height * scale;
      u2 = u1 + width;
      v2 = v1 + height;

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
            GS_SETREG_RGBAQ(colorR,colorG,colorB,colorA,0x00));

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
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   float line_height;

   if (!msg || !*msg)
      return;

   /* If font line metrics are not supported just draw as usual */
   if (!font->font_driver->get_line_metrics ||
       !font->font_driver->get_line_metrics(font->font_data, &line_metrics))
   {
      ps2_font_render_line(ps2, font, msg, strlen(msg),
                           scale, color, pos_x, pos_y,
                           width, height, text_align);
      return;
   }

   line_height = (float)line_metrics->height * scale / (float)height;

   for (;;)
   {
      const char* delim = strchr(msg, '\n');
      unsigned msg_len  = delim ?
         (unsigned)(delim - msg) : strlen(msg);

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
   unsigned max_glyphs;
   enum text_alignment text_align;
   unsigned color, color_dark, r, g, b,
            alpha, r_dark, g_dark, b_dark, alpha_dark;
   ps2_font_t                * font = (ps2_font_t*)data;
   ps2_video_t                *ps2  = (ps2_video_t*)userdata;
   unsigned width                   = ps2->vp.full_width;
   unsigned height                  = ps2->vp.full_height;
   settings_t *settings             = config_get_ptr();
   float video_msg_pos_x            = settings->floats.video_msg_pos_x;
   float video_msg_pos_y            = settings->floats.video_msg_pos_y;
   float video_msg_color_r          = settings->floats.video_msg_color_r;
   float video_msg_color_g          = settings->floats.video_msg_color_g;
   float video_msg_color_b          = settings->floats.video_msg_color_b;

   if (!font || !msg || !*msg)
      return;

   if (params)
   {
      x                    = params->x;
      y                    = params->y;
      scale                = params->scale;
      text_align           = params->text_align;
      drop_x               = params->drop_x;
      drop_y               = params->drop_y;
      drop_mod             = params->drop_mod;
      drop_alpha           = params->drop_alpha;

      r                    = FONT_COLOR_GET_RED(params->color);
      g                    = FONT_COLOR_GET_GREEN(params->color);
      b                    = FONT_COLOR_GET_BLUE(params->color);
      alpha                = FONT_COLOR_GET_ALPHA(params->color);

      color                = COLOR_ABGR(r, g, b, alpha);
   }
   else
   {
      x              = video_msg_pos_x;
      y              = video_msg_pos_y;
      scale          = 1.0f;
      text_align     = TEXT_ALIGN_LEFT;

      r              = (video_msg_color_r * 255);
      g              = (video_msg_color_g * 255);
      b              = (video_msg_color_b * 255);
      alpha          = 255;
      color          = COLOR_ABGR(r, g, b, alpha);

      drop_x         = 1;
      drop_y         = -1;
      drop_mod       = 0.0f;
      drop_alpha     = 0.75f;
   }

   max_glyphs        = strlen(msg);

   if (drop_x || drop_y)
      max_glyphs    *= 2;

   gsKit_TexManager_bind(ps2->gsGlobal, font->texture);

   if (drop_x || drop_y)
   {
      r_dark         = r * drop_mod;
      g_dark         = g * drop_mod;
      b_dark         = b * drop_mod;
      alpha_dark     = alpha * drop_alpha;
      color_dark     = COLOR_ABGR(r_dark, g_dark, b_dark, alpha_dark);

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

   if (!font || !font->font_driver)
      return NULL;

   if (!font->font_driver->ident)
      return NULL;

   return font->font_driver->get_glyph((void*)font->font_driver, code);
}

static bool ps2_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   ps2_font_t* font = (ps2_font_t*)data;

   if (!font || !font->font_driver || !font->font_data)
      return -1;

   return font->font_driver->get_line_metrics(font->font_data, metrics);
}

font_renderer_t ps2_font = {
   ps2_font_init_font,
   ps2_font_free_font,
   ps2_font_render_msg,
   "PS2 font",
   ps2_font_get_glyph,
   NULL,                      /* bind_block */
   NULL,                      /* flush */
   ps2_font_get_message_width,
   ps2_font_get_line_metrics
};

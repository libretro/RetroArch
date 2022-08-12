/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2017 - Daniel De Matteis
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
#include <malloc.h>
#include <math.h>
#include <encodings/utf.h>
#include <3ds.h>

#include <retro_math.h>

#include "../font_driver.h"
#include "../common/ctr_common.h"
#include "../drivers/ctr_gu.h"
#include "../../ctr/gpu_old.h"

#include "../../configuration.h"
#include "../../verbosity.h"

typedef struct
{
   ctr_texture_t texture;
   ctr_scale_vector_t scale_vector_top;
   ctr_scale_vector_t scale_vector_bottom;
   const font_renderer_driver_t* font_driver;
   void* font_data;
} ctr_font_t;

static void* ctr_font_init(void* data, const char* font_path,
      float font_size, bool is_threaded)
{
   int i, j;
   const uint8_t*     src         = NULL;
   uint8_t* tmp                   = NULL;
   const struct font_atlas* atlas = NULL;
   ctr_font_t* font = (ctr_font_t*)calloc(1, sizeof(*font));
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (!font)
      return NULL;

   font_size        = 10;
   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
      RARCH_WARN("Couldn't initialize font renderer.\n");
      free(font);
      return NULL;
   }

   atlas                = font->font_driver->get_atlas(font->font_data);

   font->texture.width  = next_pow2(atlas->width);
   font->texture.height = next_pow2(atlas->height);
#if FONT_TEXTURE_IN_VRAM
   font->texture.data   = vramAlloc(font->texture.width * font->texture.height);
   tmp                  = linearAlloc(font->texture.width * font->texture.height);
#else
   font->texture.data   = linearAlloc(font->texture.width * font->texture.height);
   tmp                  = font->texture.data;
#endif

   src                  = atlas->buffer;

   for (j = 0; (j < atlas->height) && (j < font->texture.height); j++)
      for (i = 0; (i < atlas->width) && (i < font->texture.width); i++)
         tmp[ctrgu_swizzle_coords(i, j, font->texture.width)] = src[i + j * atlas->width];

   GSPGPU_FlushDataCache(tmp, font->texture.width * font->texture.height);

#if FONT_TEXTURE_IN_VRAM
   ctrGuCopyImage(true, tmp, font->texture.width >> 2, font->texture.height, CTRGU_RGBA8, true,
                  font->texture.data, font->texture.width >> 2, CTRGU_RGBA8,  true);

   linearFree(tmp);
#endif

   ctr_set_scale_vector(&font->scale_vector_top, 
      CTR_TOP_FRAMEBUFFER_WIDTH, 
      CTR_TOP_FRAMEBUFFER_HEIGHT,
      font->texture.width, font->texture.height);

   ctr_set_scale_vector(&font->scale_vector_bottom, 
      CTR_BOTTOM_FRAMEBUFFER_WIDTH, 
      CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
      font->texture.width, font->texture.height);

   return font;
}

static void ctr_font_free(void* data, bool is_threaded)
{
   ctr_font_t* font = (ctr_font_t*)data;

   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

#ifdef FONT_TEXTURE_IN_VRAM
   vramFree(font->texture.data);
#else
   linearFree(font->texture.data);
#endif
   free(font);
}

static int ctr_font_get_message_width(void* data, const char* msg,
                                      unsigned msg_len, float scale)
{
   unsigned i;
   int delta_x = 0;
   const struct font_glyph* glyph_q = NULL;
   ctr_font_t* font                 = (ctr_font_t*)data;

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

static void ctr_font_render_line(
      ctr_video_t *ctr,
      ctr_font_t* font, const char* msg, unsigned msg_len,
      float scale, const unsigned int color, float pos_x,
      float pos_y,
      unsigned width, unsigned height, unsigned text_align)
{
   unsigned i;
   const struct font_glyph* glyph_q = NULL;
   ctr_vertex_t* v  = NULL;
   int delta_x      = 0;
   int delta_y      = 0;
   int x            = roundf(pos_x * width);
   int y            = roundf((1.0f - pos_y) * height);

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x += width - ctr_font_get_message_width(font, msg, msg_len, scale);
         break;

      case TEXT_ALIGN_CENTER:
         x += width / 2 - 
            ctr_font_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   if ((ctr->vertex_cache.size - (ctr->vertex_cache.current - ctr->vertex_cache.buffer)) < msg_len)
      ctr->vertex_cache.current = ctr->vertex_cache.buffer;

   v       = ctr->vertex_cache.current;
   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph* glyph;
      int off_x, off_y, tex_x, tex_y, width, height;
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

      off_x    = glyph->draw_offset_x;
      off_y    = glyph->draw_offset_y;
      tex_x    = glyph->atlas_offset_x;
      tex_y    = glyph->atlas_offset_y;
      width    = glyph->width;
      height   = glyph->height;

      v->x0    = x + (off_x + delta_x) * scale;
      v->y0    = y + (off_y + delta_y) * scale;
      v->u0    = tex_x;
      v->v0    = tex_y;
      v->x1    = v->x0 + width * scale;
      v->y1    = v->y0 + height * scale;
      v->u1    = v->u0 + width;
      v->v1    = v->v0 + height;

      v++;
      delta_x += glyph->advance_x;
      delta_y += glyph->advance_y;
   }

   if (v == ctr->vertex_cache.current)
      return;

   GPUCMD_AddWrite(GPUREG_GSH_BOOLUNIFORM, 0);
   if (!ctr->render_font_bottom)
      ctrGuSetVertexShaderFloatUniform(0, (float*)&font->scale_vector_top, 1);
   else
      ctrGuSetVertexShaderFloatUniform(0, (float*)&font->scale_vector_bottom, 1);

   GSPGPU_FlushDataCache(ctr->vertex_cache.current,
         (v - ctr->vertex_cache.current) * sizeof(ctr_vertex_t));
   ctrGuSetAttributeBuffers(2,
                            VIRT_TO_PHYS(ctr->vertex_cache.current),
                            CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 0 |
                            CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 4,
                            sizeof(ctr_vertex_t));

   GPU_SetTexEnv(0,
                 GPU_TEVSOURCES(GPU_TEXTURE0, GPU_CONSTANT, 0),
                 GPU_TEVSOURCES(GPU_TEXTURE0, GPU_CONSTANT, 0),
                 0,
                 GPU_TEVOPERANDS(GPU_TEVOP_RGB_SRC_ALPHA, 0, 0),
                 GPU_MODULATE, GPU_MODULATE,
                 color);

   ctrGuSetTexture(GPU_TEXUNIT0, VIRT_TO_PHYS(font->texture.data),
         font->texture.width, font->texture.height,
         GPU_TEXTURE_MAG_FILTER(GPU_NEAREST)  | GPU_TEXTURE_MIN_FILTER(GPU_NEAREST) |
         GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
         GPU_L8);

   if (!ctr->render_font_bottom)
   {
      GPU_SetViewport(NULL,
            VIRT_TO_PHYS(ctr->drawbuffers.top.left),
            0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
            ctr->video_mode == CTR_VIDEO_MODE_2D_800X240
            ? CTR_TOP_FRAMEBUFFER_WIDTH * 2 : CTR_TOP_FRAMEBUFFER_WIDTH);

      GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, v - ctr->vertex_cache.current);

      if (ctr->video_mode == CTR_VIDEO_MODE_3D)
      {
         GPU_SetViewport(NULL,
               VIRT_TO_PHYS(ctr->drawbuffers.top.right),
               0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
               CTR_TOP_FRAMEBUFFER_WIDTH);
         GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, v - ctr->vertex_cache.current);
      }
   }
   else
   {
      GPU_SetViewport(NULL,
            VIRT_TO_PHYS(ctr->drawbuffers.bottom),
            0, 0, CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
            CTR_BOTTOM_FRAMEBUFFER_WIDTH);
      GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, v - ctr->vertex_cache.current);
   }

   GPU_SetTexEnv(0, GPU_TEXTURE0, GPU_TEXTURE0, 0, 0, GPU_REPLACE, GPU_REPLACE, 0);

   ctr->vertex_cache.current = v;
}

static void ctr_font_render_message(
      ctr_video_t *ctr,
      ctr_font_t* font, const char* msg, float scale,
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
      unsigned msg_len = strlen(msg);
      ctr_font_render_line(ctr, font, msg, msg_len,
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
      ctr_font_render_line(ctr, font, msg, msg_len,
            scale, color, pos_x, pos_y - (float)lines * line_height,
            width, height, text_align);
      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void ctr_font_render_msg(
      void *userdata,
      void* data, const char* msg,
      const struct font_params *params)
{
   float x, y, scale, drop_mod, drop_alpha;
   int drop_x, drop_y;
   enum text_alignment text_align;
   unsigned color, r, g, b, alpha;
   ctr_font_t                * font = (ctr_font_t*)data;
   ctr_video_t                *ctr  = (ctr_video_t*)userdata;
   unsigned width                   = ctr->render_font_bottom ?
      CTR_BOTTOM_FRAMEBUFFER_WIDTH : CTR_TOP_FRAMEBUFFER_WIDTH;
   unsigned height                  = ctr->render_font_bottom ?
      CTR_BOTTOM_FRAMEBUFFER_HEIGHT : CTR_TOP_FRAMEBUFFER_HEIGHT;

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

   if (drop_x || drop_y)
   {
      unsigned r_dark         = r * drop_mod;
      unsigned g_dark         = g * drop_mod;
      unsigned b_dark         = b * drop_mod;
      unsigned alpha_dark     = alpha * drop_alpha;
      unsigned color_dark     = COLOR_ABGR(r_dark, g_dark,
            b_dark, alpha_dark);
      ctr_font_render_message(ctr, font, msg, scale, color_dark,
                              x + scale * drop_x / width, y +
                              scale * drop_y / height,
                              width, height, text_align);
   }

   ctr_font_render_message(ctr, font, msg, scale,
                           color, x, y,
                           width, height, text_align);
}

static const struct font_glyph* ctr_font_get_glyph(
   void* data, uint32_t code)
{
   ctr_font_t* font = (ctr_font_t*)data;
   if (font && font->font_driver && font->font_driver->ident)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static bool ctr_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   ctr_font_t* font = (ctr_font_t*)data;
   if (font && font->font_driver && font->font_data)
      return font->font_driver->get_line_metrics(font->font_data, metrics);
   return false;
}

font_renderer_t ctr_font =
{
   ctr_font_init,
   ctr_font_free,
   ctr_font_render_msg,
   "ctr_font",
   ctr_font_get_glyph,
   NULL,                         /* bind_block */
   NULL,                         /* flush_block */
   ctr_font_get_message_width,
   ctr_font_get_line_metrics
};

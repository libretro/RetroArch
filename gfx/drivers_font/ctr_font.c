/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#include "../font_driver.h"
#include "../video_driver.h"
#include "../common/ctr_common.h"
#include "../drivers/ctr_gu.h"
#include "../../ctr/gpu_old.h"

#include "../../configuration.h"
#include "../../verbosity.h"

/* FIXME: this is just a workaround to avoid
 * using ctrGuCopyImage, since it seems to cause
 * a freeze/blackscreen when used here. */
//#define FONT_TEXTURE_IN_VRAM

typedef struct
{
   ctr_texture_t texture;
   ctr_scale_vector_t scale_vector;
   const font_t* font;
   void* font_data;
} ctr_font_t;

static void* ctr_font_init_font(void* data, const font_t* font)
{
   const struct font_atlas* atlas = NULL;
   ctr_font_t* self= (ctr_font_t*)calloc(1, sizeof(*self));
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (!self)
      return NULL;

   self->font = font_ref(font);

   atlas = font_get_atlas(font);

   self->texture.width = next_pow2(atlas->width);
   self->texture.height = next_pow2(atlas->height);
#if FONT_TEXTURE_IN_VRAM
   font->texture.data = vramAlloc(font->texture.width * font->texture.height);
   uint8_t* tmp = linearAlloc(font->texture.width * font->texture.height);
#else
   self->texture.data = linearAlloc(self->texture.width * self->texture.height);
   uint8_t* tmp = self->texture.data;
#endif

   int i, j;
   const uint8_t*     src = atlas->buffer;

   for (j = 0; (j < atlas->height) && (j < self->texture.height); j++)
      for (i = 0; (i < atlas->width) && (i < self->texture.width); i++)
         tmp[ctrgu_swizzle_coords(i, j, self->texture.width)] = src[i + j * atlas->width];

   GSPGPU_FlushDataCache(tmp, self->texture.width * self->texture.height);

#if FONT_TEXTURE_IN_VRAM
   ctrGuCopyImage(true, tmp, font->texture.width >> 2, font->texture.height, CTRGU_RGBA8, true,
                  font->texture.data, font->texture.width >> 2, CTRGU_RGBA8,  true);

   linearFree(tmp);
#endif

   ctr_set_scale_vector(&self->scale_vector, 400, 240, self->texture.width, self->texture.height);

   return self;
}

static void ctr_font_free_font(void* data)
{
   ctr_font_t* self = (ctr_font_t*)data;

   if (!self)
      return;

   font_unref(self->font);

#ifdef FONT_TEXTURE_IN_VRAM
   vramFree(font->texture.data);
#else
   linearFree(self->texture.data);
#endif
   free(self);
}

static int ctr_font_get_message_width(void* data, const char* msg,
                                      unsigned msg_len, float scale)
{
   ctr_font_t* self = (ctr_font_t*)data;

   unsigned i;
   int delta_x = 0;

   if (!self)
      return 0;

   for (i = 0; i < msg_len; i++)
   {
      const char* msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      const struct font_glyph* glyph = font_get_glyph(self->font, code);

      if (glyph)
         delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void ctr_font_render_line(
   ctr_font_t* self, const char* msg, unsigned msg_len,
   float scale, const unsigned int color, float pos_x,
   float pos_y, unsigned text_align)
{
   int x, y, delta_x, delta_y;
   unsigned width, height;
   unsigned i;

   ctr_video_t* ctr = (ctr_video_t*)video_driver_get_ptr(false);
   ctr_vertex_t* v;
   video_driver_get_size(&width, &height);

   x       = roundf(pos_x * width);
   y       = roundf((1.0f - pos_y) * height);
   delta_x = 0;
   delta_y = 0;


   switch (text_align)
   {
   case TEXT_ALIGN_RIGHT:
      x -= ctr_font_get_message_width(self, msg, msg_len, scale);
      break;

   case TEXT_ALIGN_CENTER:
      x -= ctr_font_get_message_width(self, msg, msg_len, scale) / 2;
      break;
   }

   if ((ctr->vertex_cache.size - (ctr->vertex_cache.current - ctr->vertex_cache.buffer)) < msg_len)
      ctr->vertex_cache.current = ctr->vertex_cache.buffer;

   v = ctr->vertex_cache.current;

   for (i = 0; i < msg_len; i++)
   {
      int off_x, off_y, tex_x, tex_y, width, height;
      const char* msg_tmp            = &msg[i];
      unsigned code                  = utf8_walk(&msg_tmp);
      unsigned skip                  = msg_tmp - &msg[i];

      if (skip > 1)
         i += skip - 1;

      const struct font_glyph* glyph = font_get_glyph(self->font, code);

      if (!glyph)
         continue;

      off_x  = glyph->draw_offset_x;
      off_y  = glyph->draw_offset_y;
      tex_x  = glyph->atlas_offset_x;
      tex_y  = glyph->atlas_offset_y;
      width  = glyph->width;
      height = glyph->height;


      v->x0 = x + off_x + delta_x * scale;
      v->y0 = y + off_y + delta_y * scale;
      v->u0 = tex_x;
      v->v0 = tex_y;
      v->x1 = v->x0 + width * scale;
      v->y1 = v->y0 + height * scale;
      v->u1 = v->u0 + width;
      v->v1 = v->v0 + height;

      v++;
      delta_x += glyph->advance_x;
      delta_y += glyph->advance_y;
   }

   if (v == ctr->vertex_cache.current)
      return;

   ctrGuSetVertexShaderFloatUniform(0, (float*)&self->scale_vector, 1);
   GSPGPU_FlushDataCache(ctr->vertex_cache.current, (v - ctr->vertex_cache.current) * sizeof(ctr_vertex_t));
   ctrGuSetAttributeBuffers(2,
                            VIRT_TO_PHYS(ctr->vertex_cache.current),
                            CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 0 |
                            CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 4,
                            sizeof(ctr_vertex_t));

   GPU_SetTexEnv(0,
                 GPU_TEVSOURCES(GPU_TEXTURE0, GPU_CONSTANT, 0),
                 GPU_TEVSOURCES(GPU_TEXTURE0, GPU_CONSTANT, 0),
                 0,
                 GPU_TEVOPERANDS(GPU_TEVOP_RGB_SRC_R, GPU_TEVOP_RGB_SRC_ALPHA, 0),
                 GPU_MODULATE, GPU_MODULATE,
                 color);
//   printf("%s\n", msg);
//   DEBUG_VAR(color);
//   GPU_SetTexEnv(0, GPU_TEXTURE0, GPU_TEXTURE0, 0, GPU_TEVOPERANDS(GPU_TEVOP_RGB_SRC_R, 0, 0), GPU_REPLACE, GPU_REPLACE, 0);
   ctrGuSetTexture(GPU_TEXUNIT0, VIRT_TO_PHYS(self->texture.data), self->texture.width, self->texture.height,
                   GPU_TEXTURE_MAG_FILTER(GPU_NEAREST)  | GPU_TEXTURE_MIN_FILTER(GPU_NEAREST) |
                   GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
                   GPU_L8);

   GPU_SetViewport(NULL,
                   VIRT_TO_PHYS(ctr->drawbuffers.top.left),
                   0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
                   ctr->video_mode == CTR_VIDEO_MODE_800x240 ? CTR_TOP_FRAMEBUFFER_WIDTH * 2 : CTR_TOP_FRAMEBUFFER_WIDTH);

   GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, v - ctr->vertex_cache.current);

   if (ctr->video_mode == CTR_VIDEO_MODE_3D)
   {
      GPU_SetViewport(NULL,
                      VIRT_TO_PHYS(ctr->drawbuffers.top.right),
                      0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
                      CTR_TOP_FRAMEBUFFER_WIDTH);
      GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, v - ctr->vertex_cache.current);
   }




//   v = font->vertices;
//   v->x0 = 0;
//   v->y0 = 0;
//   v->u0 = 0;
//   v->v0 = 0;
//   v->x1 = font->texture.width;
//   v->y1 = font->texture.height;
//   v->u1 = font->texture.width;
//   v->v1 = font->texture.height;
//   GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);

   GPU_SetTexEnv(0, GPU_TEXTURE0, GPU_TEXTURE0, 0, 0, GPU_REPLACE, GPU_REPLACE, 0);
   //   DEBUG_VAR(v - font->vertices);
   //   v = font->vertices;
   //   printf("OSDMSG: %s\n", msg);
   //   printf("vertex : (%i,%i,%i,%i) - (%i,%i,%i,%i)\n",
   //          v->x0, v->y0, v->x1, v->y1,
   //          v->u0, v->v0, v->u1, v->v1);

//   printf("%s\n", msg);
   ctr->vertex_cache.current = v;
}

static void ctr_font_render_message(
   ctr_font_t* self, const char* msg, float scale,
   const unsigned int color, float pos_x, float pos_y,
   unsigned text_align)
{
   int lines = 0;
   float line_height;

   if (!msg || !*msg)
      return;

   /* If the font height is not supported just draw as usual */
   if (font_get_line_height(self->font) <= 0)
   {
      ctr_font_render_line(self, msg, strlen(msg),
                           scale, color, pos_x, pos_y, text_align);
      return;
   }

   line_height = scale / font_get_line_height(self->font);

   for (;;)
   {
      const char* delim = strchr(msg, '\n');

      /* Draw the line */
      if (delim)
      {
         unsigned msg_len = delim - msg;
         ctr_font_render_line(self, msg, msg_len,
                              scale, color, pos_x, pos_y - (float)lines * line_height,
                              text_align);
         msg += msg_len + 1;
         lines++;
      }
      else
      {
         unsigned msg_len = strlen(msg);
         ctr_font_render_line(self, msg, msg_len,
                              scale, color, pos_x, pos_y - (float)lines * line_height,
                              text_align);
         break;
      }
   }
}

static void ctr_font_render_msg(void* data, const char* msg,
                                const void* userdata)
{
   float x, y, scale, drop_mod, drop_alpha;
   unsigned color, color_dark, r, g, b, alpha, r_dark, g_dark, b_dark, alpha_dark;
   unsigned width, height;
   int drop_x, drop_y;
   unsigned max_glyphs;
   enum text_alignment text_align;
   settings_t* settings = config_get_ptr();
   ctr_font_t* font = (ctr_font_t*)data;
   const struct font_params* params = (const struct font_params*)userdata;

   if (!font || !msg || !*msg)
      return;

   video_driver_get_size(&width, &height);

   if (params)
   {
//      printf("%s\n", msg);
      x           = params->x;
      y           = params->y;
      scale       = params->scale;
      text_align  = params->text_align;
      drop_x      = params->drop_x;
      drop_y      = params->drop_y;
      drop_mod    = params->drop_mod;
      drop_alpha  = params->drop_alpha;
      r              = FONT_COLOR_GET_RED(params->color);
      g              = FONT_COLOR_GET_GREEN(params->color);
      b              = FONT_COLOR_GET_BLUE(params->color);
      alpha          = FONT_COLOR_GET_ALPHA(params->color);
      color          = params->color;
//      color          = COLOR_ABGR(r, g, b, alpha);
   }
   else
   {
      x           = settings->video.msg_pos_x;
      y           = settings->video.msg_pos_y;
      scale       = 1.0f;
      text_align  = TEXT_ALIGN_LEFT;

      r           = (settings->video.msg_color_r * 255);
      g           = (settings->video.msg_color_g * 255);
      b           = (settings->video.msg_color_b * 255);
      alpha          = 255;
      color          = COLOR_ABGR(r, g, b, alpha);

      drop_x = -2;
      drop_y = -2;
      drop_mod = 0.3f;
      drop_alpha = 1.0f;
   }

   max_glyphs = strlen(msg);

   if (drop_x || drop_y)
      max_glyphs *= 2;

   if (drop_x || drop_y)
   {
      r_dark        = r * drop_mod;
      g_dark        = g * drop_mod;
      b_dark        = b * drop_mod;
      alpha_dark     = alpha * drop_alpha;
      color_dark = COLOR_ABGR(r_dark, g_dark, b_dark, alpha_dark);

      ctr_font_render_message(font, msg, scale, color_dark,
                              x + scale * drop_x / width, y +
                              scale * drop_y / height, text_align);
   }

   ctr_font_render_message(font, msg, scale,
                           color, x, y, text_align);
}

static const struct font_glyph* ctr_font_get_glyph(
   void* data, uint32_t code)
{
   ctr_font_t* self = (ctr_font_t*)data;

   if (!self)
      return NULL;

   return font_get_glyph(self->font, code);
}

static void ctr_font_flush_block(void* data)
{
   (void)data;
}

static void ctr_font_bind_block(void* data, void* userdata)
{
   (void)data;
}


font_renderer_t ctr_font =
{
   ctr_font_init_font,
   ctr_font_free_font,
   ctr_font_render_msg,
   "ctrfont",
   ctr_font_get_glyph,
   ctr_font_bind_block,
   ctr_font_flush_block,
   ctr_font_get_message_width,
};

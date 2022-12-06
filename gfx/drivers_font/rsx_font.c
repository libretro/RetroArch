/*  RetroArch - A frontend for libretro.
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

#include <defines/ps3_defines.h>
#include <rsx/rsx.h>
#include <rsx/nv40.h>
#include <ppu-types.h>

#include <encodings/utf.h>

#include "../common/rsx_common.h"

#include "../font_driver.h"

#include "../../configuration.h"

#define RSX_FONT_EMIT(c, vx, vy) \
   font_vertex[     2 * (6 * i + c) + 0] = (x + (delta_x + off_x + vx * width) * scale) * inv_win_width; \
   font_vertex[     2 * (6 * i + c) + 1] = (y + (delta_y - off_y - vy * height) * scale) * inv_win_height; \
   font_tex_coords[ 2 * (6 * i + c) + 0] = (tex_x + vx * width) * inv_tex_size_x; \
   font_tex_coords[ 2 * (6 * i + c) + 1] = (tex_y + vy * height) * inv_tex_size_y; \
   font_color[      4 * (6 * i + c) + 0] = color[0]; \
   font_color[      4 * (6 * i + c) + 1] = color[1]; \
   font_color[      4 * (6 * i + c) + 2] = color[2]; \
   font_color[      4 * (6 * i + c) + 3] = color[3]

#define MAX_MSG_LEN_CHUNK 64

typedef struct
{
   rsx_t *rsx;
   rsx_vertex_t *vertices;
   rsx_texture_t texture;
   u32 tex_width;
   u32 tex_height;
   rsxProgramAttrib *proj_matrix;
   rsxProgramAttrib *pos_index;
   rsxProgramAttrib *uv_index;
   rsxProgramAttrib *col_index;
   rsxProgramAttrib *tex_unit;
   rsxVertexProgram* vpo;
   rsxFragmentProgram* fpo;
   u32 fp_offset;
   u32 pos_offset;
   u32 uv_offset;
   u32 col_offset;
   void *vp_ucode;
   void *fp_ucode;
   const font_renderer_driver_t *font_driver;
   void *font_data;
   struct font_atlas *atlas;
   video_font_raster_block_t *block;
} rsx_font_t;

static void rsx_font_free(void *data,
      bool is_threaded)
{
   rsx_font_t *font = (rsx_font_t*)data;
   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   if (is_threaded)
   {
      if (
            font->rsx &&
            font->rsx->ctx_driver &&
            font->rsx->ctx_driver->make_current)
         font->rsx->ctx_driver->make_current(true);
   }

   rsxClearSurface(font->rsx->context, GCM_CLEAR_Z);
   gcmSetWaitFlip(font->rsx->context);
   if (font->texture.data)
      rsxFree(font->texture.data);
   if (font->vertices)
      rsxFree(font->vertices);

   free(font);
}

static bool rsx_font_upload_atlas(rsx_font_t *font)
{
   u8 *texbuffer = (u8 *)font->texture.data;
	 const u8 *atlas_data = (u8 *)font->atlas->buffer;
   memcpy(texbuffer, atlas_data, font->atlas->height * font->atlas->width);

	 font->texture.tex.format = GCM_TEXTURE_FORMAT_B8 | GCM_TEXTURE_FORMAT_LIN;
	 font->texture.tex.mipmap = 1;
	 font->texture.tex.dimension = GCM_TEXTURE_DIMS_2D;
	 font->texture.tex.cubemap = GCM_FALSE;
   font->texture.tex.remap = ((GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT) |
                              (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT) |
                              (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT) |
                              (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT) |
                              (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_B_SHIFT) |
                              (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_G_SHIFT) |
                              (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_R_SHIFT) |
                              (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_A_SHIFT));
   font->texture.tex.width = font->tex_width;
   font->texture.tex.height = font->tex_height;
   font->texture.tex.depth = 1;
   font->texture.tex.pitch = font->tex_width;
   font->texture.tex.location = GCM_LOCATION_RSX;
   font->texture.tex.offset = font->texture.offset;
   font->texture.wrap_s = GCM_TEXTURE_CLAMP_TO_EDGE;
   font->texture.wrap_t = GCM_TEXTURE_CLAMP_TO_EDGE;
   font->texture.min_filter = GCM_TEXTURE_LINEAR;
   font->texture.mag_filter = GCM_TEXTURE_LINEAR;

   rsxInvalidateTextureCache(font->rsx->context, GCM_INVALIDATE_TEXTURE);
   rsxLoadTexture(font->rsx->context, font->tex_unit->index, &font->texture.tex);
   rsxTextureControl(font->rsx->context, font->tex_unit->index, GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
   rsxTextureFilter(font->rsx->context, font->tex_unit->index, 0, font->texture.min_filter, font->texture.mag_filter, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
   rsxTextureWrapMode(font->rsx->context, font->tex_unit->index, font->texture.wrap_s, font->texture.wrap_t, GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);

   return true;
}

static void *rsx_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   rsx_font_t *font = (rsx_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->rsx = (rsx_t *)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
      free(font);
      return NULL;
   }

   if (is_threaded)
      if (
            font->rsx &&
            font->rsx->ctx_driver &&
            font->rsx->ctx_driver->make_current)
         font->rsx->ctx_driver->make_current(false);

   font->atlas        = font->font_driver->get_atlas(font->font_data);

   font->vpo          = font->rsx->vpo;
   font->fpo          = font->rsx->fpo;
   font->fp_ucode     = font->rsx->fp_ucode;
   font->vp_ucode     = font->rsx->vp_ucode;
   font->fp_offset    = font->rsx->fp_offset;

   font->proj_matrix  = font->rsx->proj_matrix;
   font->pos_index    = font->rsx->pos_index;
   font->uv_index     = font->rsx->uv_index;
   font->col_index    = font->rsx->col_index;
   font->tex_unit     = font->rsx->tex_unit;

   font->vertices     = (rsx_vertex_t *)rsxMemalign(128, MAX_MSG_LEN_CHUNK*sizeof(rsx_vertex_t)*6);

   rsxAddressToOffset(&font->vertices[0].x, &font->pos_offset);
   rsxAddressToOffset(&font->vertices[0].u, &font->uv_offset);
   rsxAddressToOffset(&font->vertices[0].r, &font->col_offset);

   font->tex_width    = font->atlas->width;
   font->tex_height   = font->atlas->height;
   font->texture.data = (u8 *)rsxMemalign(128, (font->tex_height * font->tex_width));
   rsxAddressToOffset(font->texture.data, &font->texture.offset);

   if (!font->texture.data)
      goto error;

   if (!rsx_font_upload_atlas(font))
      goto error;

   font->atlas->dirty = false;

   rsxTextureControl(font->rsx->context, font->tex_unit->index, GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
   return font;

error:
   rsx_font_free(font, is_threaded);
   return NULL;
}

static int rsx_font_get_message_width(void *data, const char *msg,
      size_t msg_len, float scale)
{
   const struct font_glyph* glyph_q = NULL;
   rsx_font_t *font   = (rsx_font_t*)data;
   const char* msg_end = msg + msg_len;
   int delta_x         = 0;

   if (     !font
         || !font->font_driver
         || !font->font_driver->get_glyph
         || !font->font_data )
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   while (msg < msg_end)
   {
      const struct font_glyph *glyph;
      unsigned code                  = utf8_walk(&msg);

      /* Do something smarter here ... */
      if (!(glyph = font->font_driver->get_glyph(
                  font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void rsx_font_draw_vertices(rsx_font_t *font,
      const video_coords_t *coords)
{
   int i;
   const float *vertex              = coords->vertex;
   const float *tex_coord           = coords->tex_coord;
   const float *color               = coords->color;

   if (font->atlas->dirty)
   {
      rsx_font_upload_atlas(font);
      font->atlas->dirty   = false;
   }

   for (i = 0; i < coords->vertices; i++)
   {
      font->vertices[i].x = *vertex++;
      font->vertices[i].y = *vertex++;
      font->vertices[i].z = 0.0f;
      font->vertices[i].u = *tex_coord++;
      font->vertices[i].v = *tex_coord++;
      font->vertices[i].r = *color++;
      font->vertices[i].g = *color++;
      font->vertices[i].b = *color++;
      font->vertices[i].a = *color++;
   }

   rsxBindVertexArrayAttrib(font->rsx->context, font->pos_index->index, 0, font->pos_offset, sizeof(rsx_vertex_t), 3, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(font->rsx->context, font->uv_index->index, 0, font->uv_offset, sizeof(rsx_vertex_t), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(font->rsx->context, font->col_index->index, 0, font->col_offset, sizeof(rsx_vertex_t), 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

   rsxSetVertexProgramParameter(font->rsx->context, font->vpo, font->proj_matrix, (float*)&font->rsx->mvp_no_rot);
   rsxClearSurface(font->rsx->context, GCM_CLEAR_Z);
   rsxDrawVertexArray(font->rsx->context, GCM_TYPE_TRIANGLES, 0, coords->vertices);
}

static void rsx_font_render_line(
      rsx_font_t *font, const char *msg, size_t msg_len,
      float scale, const float color[4], float pos_x,
      float pos_y,unsigned text_align)
{
   int i;
   struct video_coords coords;
   const struct font_glyph* glyph_q = NULL;
   float font_tex_coords[2 * 6 * MAX_MSG_LEN_CHUNK];
   float font_vertex[2 * 6 * MAX_MSG_LEN_CHUNK];
   float font_color[4 * 6 * MAX_MSG_LEN_CHUNK];
   rsx_t      *rsx      = font->rsx;
   const char* msg_end  = msg + msg_len;
   int x                = roundf(pos_x * rsx->vp.width);
   int y                = roundf(pos_y * rsx->vp.height);
   int delta_x          = 0;
   int delta_y          = 0;
   float inv_tex_size_x = 1.0f / font->tex_width;
   float inv_tex_size_y = 1.0f / font->tex_height;
   float inv_win_width  = 1.0f / font->rsx->vp.width;
   float inv_win_height = 1.0f / font->rsx->vp.height;

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= rsx_font_get_message_width(font, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x -= rsx_font_get_message_width(font, msg, msg_len, scale) / 2.0;
         break;
   }

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   while (msg < msg_end)
   {
      i = 0;
      while ((i < MAX_MSG_LEN_CHUNK) && (msg < msg_end))
      {
         const struct font_glyph *glyph;
         int off_x, off_y, tex_x, tex_y, width, height;
         unsigned                  code = utf8_walk(&msg);

         /* Do something smarter here ... */
         if (!(glyph = font->font_driver->get_glyph(
               font->font_data, code)))
            if (!(glyph = glyph_q))
               continue;

         off_x  = glyph->draw_offset_x;
         off_y  = glyph->draw_offset_y;
         tex_x  = glyph->atlas_offset_x;
         tex_y  = glyph->atlas_offset_y;
         width  = glyph->width;
         height = glyph->height;

         RSX_FONT_EMIT(0, 0, 1); /* Bottom-left */
         RSX_FONT_EMIT(1, 1, 1); /* Bottom-right */
         RSX_FONT_EMIT(2, 0, 0); /* Top-left */

         RSX_FONT_EMIT(3, 1, 0); /* Top-right */
         RSX_FONT_EMIT(4, 0, 0); /* Top-left */
         RSX_FONT_EMIT(5, 1, 1); /* Bottom-right */

         i++;

         delta_x += glyph->advance_x;
         delta_y -= glyph->advance_y;
      }

      coords.tex_coord     = font_tex_coords;
      coords.vertex        = font_vertex;
      coords.color         = font_color;
      coords.vertices      = i * 6;
      coords.lut_tex_coord = font_tex_coords;

      if (font->block)
         video_coord_array_append(&font->block->carr, &coords, coords.vertices);
      else
         rsx_font_draw_vertices(font, &coords);
   }
}

static void rsx_font_render_message(
      rsx_font_t *font, const char *msg, float scale,
      const float color[4], float pos_x, float pos_y,
      unsigned text_align)
{
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   float line_height;

   /* If font line metrics are not supported just draw as usual */
   if (!font->font_driver->get_line_metrics ||
       !font->font_driver->get_line_metrics(font->font_data, &line_metrics))
   {
      rsx_font_render_line(font,
            msg, strlen(msg), scale, color, pos_x,
            pos_y, text_align);
      return;
   }

   line_height = line_metrics->height * scale / font->rsx->vp.height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');
      size_t msg_len    = delim
         ? (delim - msg) : strlen(msg);

      /* Draw the line */
      rsx_font_render_line(font,
            msg, msg_len, scale, color, pos_x,
            pos_y - (float)lines*line_height, text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void rsx_font_setup_viewport(unsigned width, unsigned height,
      rsx_font_t *font, bool full_screen)
{
   video_driver_set_viewport(width, height, full_screen, false);

  if (font->rsx)
  {
     rsxSetBlendFunc(font->rsx->context, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
     rsxSetBlendEquation(font->rsx->context, GCM_FUNC_ADD, GCM_FUNC_ADD);
     rsxSetBlendEnable(font->rsx->context, GCM_TRUE);

     rsxLoadVertexProgram(font->rsx->context, font->vpo, font->vp_ucode);
     rsxLoadFragmentProgramLocation(font->rsx->context, font->fpo, font->fp_offset, GCM_LOCATION_RSX);

     rsxLoadTexture(font->rsx->context, font->tex_unit->index, &font->texture.tex);
     rsxTextureControl(font->rsx->context, font->tex_unit->index, GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
  }
}

static void rsx_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   float color[4];
   int drop_x, drop_y;
   float x, y, scale, drop_mod, drop_alpha;
   enum text_alignment text_align   = TEXT_ALIGN_LEFT;
   bool full_screen                 = false ;
   rsx_font_t *font                 = (rsx_font_t*)data;
   unsigned width                   = font->rsx->width;
   unsigned height                  = font->rsx->height;
   settings_t *settings             = config_get_ptr();
   float video_msg_pos_x            = settings->floats.video_msg_pos_x;
   float video_msg_pos_y            = settings->floats.video_msg_pos_y;
   float video_msg_color_r          = settings->floats.video_msg_color_r;
   float video_msg_color_g          = settings->floats.video_msg_color_g;
   float video_msg_color_b          = settings->floats.video_msg_color_b;

   if (!font || string_is_empty(msg))
      return;

   if (params)
   {
      x           = params->x;
      y           = params->y;
      scale       = params->scale;
      full_screen = params->full_screen;
      text_align  = params->text_align;
      drop_x      = params->drop_x;
      drop_y      = params->drop_y;
      drop_mod    = params->drop_mod;
      drop_alpha  = params->drop_alpha;

      color[0]    = FONT_COLOR_GET_RED(params->color)   / 255.0f;
      color[1]    = FONT_COLOR_GET_GREEN(params->color) / 255.0f;
      color[2]    = FONT_COLOR_GET_BLUE(params->color)  / 255.0f;
      color[3]    = FONT_COLOR_GET_ALPHA(params->color) / 255.0f;

      /* If alpha is 0.0f, turn it into default 1.0f */
      if (color[3] <= 0.0f)
         color[3] = 1.0f;
   }
   else
   {
      x                    = video_msg_pos_x;
      y                    = video_msg_pos_y;
      scale                = 1.0f;
      full_screen          = true;
      text_align           = TEXT_ALIGN_LEFT;

      color[0]             = video_msg_color_r;
      color[1]             = video_msg_color_g;
      color[2]             = video_msg_color_b;
      color[3]             = 1.0f;

      drop_x               = -2;
      drop_y               = -2;
      drop_mod             = 0.3f;
      drop_alpha           = 1.0f;
   }

   if (font->block)
      font->block->fullscreen = full_screen;
   else
      rsx_font_setup_viewport(width, height, font, full_screen);

   if (font->rsx)
   {
      if (!string_is_empty(msg)
            && font->font_data  && font->font_driver)
      {
         if (drop_x || drop_y)
         {
            float color_dark[4];

            color_dark[0] = color[0] * drop_mod;
            color_dark[1] = color[1] * drop_mod;
            color_dark[2] = color[2] * drop_mod;
            color_dark[3] = color[3] * drop_alpha;

            rsx_font_render_message(font, msg, scale, color_dark,
                  x + scale * drop_x / font->rsx->vp.width, y +
                      scale * drop_y / font->rsx->vp.height, text_align);
         }

         rsx_font_render_message(font, msg, scale, color,
               x, y, text_align);
      }

      if (!font->block)
      {
         rsxTextureControl(font->rsx->context, font->rsx->tex_unit->index, GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
         rsxSetBlendEnable(font->rsx->context, GCM_FALSE);
         video_driver_set_viewport(width, height, false, true);
      }
   }
}

static const struct font_glyph *rsx_font_get_glyph(
      void *data, uint32_t code)
{
   rsx_font_t *font = (rsx_font_t*)data;
   if (font && font->font_driver && font->font_driver->ident)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static void rsx_font_flush_block(unsigned width, unsigned height,
      void *data)
{
   rsx_font_t          *font       = (rsx_font_t*)data;
   video_font_raster_block_t *block = font ? font->block : NULL;

   if (!font || !block || !block->carr.coords.vertices)
      return;

   rsx_font_setup_viewport(width, height, font, block->fullscreen);
   rsx_font_draw_vertices(font, (video_coords_t*)&block->carr.coords);

   if (font->rsx)
   {
      rsxTextureControl(font->rsx->context, font->rsx->tex_unit->index, GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
      rsxSetBlendEnable(font->rsx->context, GCM_FALSE);
      video_driver_set_viewport(width, height, block->fullscreen, true);
   }
}

static void rsx_font_bind_block(void *data, void *userdata)
{
   rsx_font_t                *font = (rsx_font_t*)data;
   video_font_raster_block_t *block = (video_font_raster_block_t*)userdata;

   if (font)
      font->block = block;
}

static bool rsx_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   rsx_font_t *font = (rsx_font_t*)data;
   if (font && font->font_driver && font->font_data)
      return font->font_driver->get_line_metrics(font->font_data, metrics);
   return false;
}

font_renderer_t rsx_font = {
   rsx_font_init,
   rsx_font_free,
   rsx_font_render_msg,
   "rsx_font",
   rsx_font_get_glyph,
   rsx_font_bind_block,
   rsx_font_flush_block,
   rsx_font_get_message_width,
   rsx_font_get_line_metrics
};

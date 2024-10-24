/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2020 Google
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
#include <unistd.h>

#include <time.h>
#include <math.h>

#include <retro_inline.h>
#include <retro_math.h>
#include <encodings/utf.h>
#include <formats/image.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif

#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../common/rsx_defines.h"
#include "../font_driver.h"

#include "../../configuration.h"
#include "../../command.h"
#include "../../driver.h"

#include "../../retroarch.h"
#include "../../runloop.h"
#include "../../verbosity.h"

#ifndef HAVE_THREADS
#include "../../tasks/tasks_internal.h"
#endif

#include <defines/ps3_defines.h>

#define CB_SIZE		0x100000
#define HOST_SIZE	(32*1024*1024)

#ifdef __PSL1GHT__
#include <rsx/rsx.h>
#include <rsx/nv40.h>
#include <ppu-types.h>
#include <ppu-lv2.h>
#include <sysutil/video.h>
#include <rsx/gcm_sys.h>
#include <rsx/rsx.h>
#include <io/pad.h>
#endif

#define rsx_context_bind_hw_render(rsx, enable) \
   if (rsx->shared_context_use) \
      rsx->ctx_driver->bind_hw_render(rsx->ctx_data, enable)

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
   rsxProgramConst *proj_matrix;
   rsxProgramAttrib *pos_index;
   rsxProgramAttrib *uv_index;
   rsxProgramAttrib *col_index;
   rsxProgramAttrib *tex_unit;
   rsxVertexProgram* vpo;
   rsxFragmentProgram* fpo;
   void *vp_ucode;
   void *fp_ucode;
   const font_renderer_driver_t *font_driver;
   void *font_data;
   struct font_atlas *atlas;
   video_font_raster_block_t *block;
   u32 tex_width;
   u32 tex_height;
   u32 fp_offset;
   u32 pos_offset;
   u32 uv_offset;
   u32 col_offset;
} rsx_font_t;

static const float rsx_vertexes[8] = {
   0, 0,
   1, 0,
   0, 1,
   1, 1
};

static const float rsx_tex_coords[8] = {
   0, 1,
   1, 1,
   0, 0,
   1, 0
};


/*
 * FORWARD DECLARATIONS
 */

static void rsx_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate);

/*
 * DISPLAY DRIVER
 */

static const float *gfx_display_rsx_get_default_vertices(void)
{
   return &rsx_vertexes[0];
}

static const float *gfx_display_rsx_get_default_tex_coords(void)
{
   return &rsx_tex_coords[0];
}

static void *gfx_display_rsx_get_default_mvp(void *data)
{
   rsx_t *rsx = (rsx_t*)data;

   if (!rsx)
      return NULL;

   return &rsx->mvp_no_rot;
}

static void gfx_display_rsx_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   unsigned i;
   rsx_viewport_t vp;
   int end_vert_idx;
   rsx_vertex_t *vertices   = NULL;
   rsx_texture_t *texture   = NULL;
   const float *vertex      = NULL;
   const float *tex_coord   = NULL;
   const float *color       = NULL;
   rsx_t             *rsx   = (rsx_t*)data;

   if (!rsx || !draw)
      return;

   texture                  = (rsx_texture_t *)draw->texture;
   vertex                   = draw->coords->vertex;
   tex_coord                = draw->coords->tex_coord;
   color                    = draw->coords->color;

   if (!vertex)
      vertex                = &rsx_vertexes[0];
   if (!tex_coord)
      tex_coord             = &rsx_tex_coords[0];
   if (!draw->coords->lut_tex_coord)
      draw->coords->lut_tex_coord   = &rsx_tex_coords[0];
   if (!draw->texture)
      return;

   vp.x                     = fabs(draw->x);
   vp.y                     = fabs(rsx->height - draw->y - draw->height);
   vp.w                     = MIN(draw->width, rsx->width);
   vp.h                     = MIN(draw->height, rsx->height);
   vp.min                   = 0.0f;
   vp.max                   = 1.0f;
   vp.scale[0]              = vp.w *  0.5f;
   vp.scale[1]              = vp.h * -0.5f;
   vp.scale[2]              = (vp.max - vp.min) * 0.5f;
   vp.scale[3]              = 0.0f;
   vp.offset[0]             = vp.x + vp.w * 0.5f;
   vp.offset[1]             = vp.y + vp.h * 0.5f;
   vp.offset[2]             = (vp.max + vp.min) * 0.5f;
   vp.offset[3]             = 0.0f;

   rsxSetViewport(rsx->context, vp.x, vp.y, vp.w, vp.h, vp.min, vp.max, vp.scale, vp.offset);

   rsxInvalidateTextureCache(rsx->context, GCM_INVALIDATE_TEXTURE);
   rsxLoadTexture(rsx->context, rsx->tex_unit[RSX_SHADER_STOCK_BLEND]->index, &texture->tex);
   rsxTextureControl(rsx->context, rsx->tex_unit[RSX_SHADER_STOCK_BLEND]->index,
         GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
   rsxTextureFilter(rsx->context, rsx->tex_unit[RSX_SHADER_STOCK_BLEND]->index, 0,
         texture->min_filter, texture->mag_filter, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
   rsxTextureWrapMode(rsx->context, rsx->tex_unit[RSX_SHADER_STOCK_BLEND]->index, texture->wrap_s,
         texture->wrap_t, GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);

#if RSX_MAX_TEXTURE_VERTICES > 0
   /* Using preallocated texture vertices uses better memory management but may cause more flickering */
   end_vert_idx             = rsx->texture_vert_idx + draw->coords->vertices;
   if (end_vert_idx > RSX_MAX_TEXTURE_VERTICES)
   {
      rsx->texture_vert_idx = 0;
      end_vert_idx          = rsx->texture_vert_idx + draw->coords->vertices;
   }
   vertices                 = &rsx->texture_vertices[rsx->texture_vert_idx];
#else
   /* Smoother gfx at the cost of unmanaged rsx memory */
   rsx->texture_vert_idx    = 0;
   end_vert_idx             = draw->coords->vertices;
   vertices                 = (rsx_vertex_t *)rsxMemalign(128, sizeof(rsx_vertex_t) * draw->coords->vertices);
#endif
   for (i = rsx->texture_vert_idx; i < end_vert_idx; i++)
   {
      vertices[i].x         = *vertex++;
      vertices[i].y         = *vertex++;
      vertices[i].u         = *tex_coord++;
      vertices[i].v         = *tex_coord++;
      vertices[i].r         = *color++;
      vertices[i].g         = *color++;
      vertices[i].b         = *color++;
      vertices[i].a         = *color++;
   }
   rsxAddressToOffset(&vertices[rsx->texture_vert_idx].x,
         &rsx->pos_offset[RSX_SHADER_STOCK_BLEND]);
   rsxAddressToOffset(&vertices[rsx->texture_vert_idx].u,
         &rsx->uv_offset[RSX_SHADER_STOCK_BLEND]);
   rsxAddressToOffset(&vertices[rsx->texture_vert_idx].r,
         &rsx->col_offset[RSX_SHADER_STOCK_BLEND]);
   rsx->texture_vert_idx  = end_vert_idx;

   rsxBindVertexArrayAttrib(rsx->context, rsx->pos_index[RSX_SHADER_STOCK_BLEND]->index, 0,
         rsx->pos_offset[RSX_SHADER_STOCK_BLEND], sizeof(rsx_vertex_t), 2,
         GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, rsx->uv_index[RSX_SHADER_STOCK_BLEND]->index, 0,
         rsx->uv_offset[RSX_SHADER_STOCK_BLEND], sizeof(rsx_vertex_t), 2,
         GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, rsx->col_index[RSX_SHADER_STOCK_BLEND]->index, 0,
         rsx->col_offset[RSX_SHADER_STOCK_BLEND], sizeof(rsx_vertex_t), 4,
         GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

   rsxLoadVertexProgram(rsx->context, rsx->vpo[RSX_SHADER_STOCK_BLEND],
         rsx->vp_ucode[RSX_SHADER_STOCK_BLEND]);
   rsxSetVertexProgramParameter(rsx->context,
         rsx->vpo[RSX_SHADER_STOCK_BLEND], rsx->proj_matrix[RSX_SHADER_STOCK_BLEND],
         (float *)&rsx->mvp_no_rot);
   rsxLoadFragmentProgramLocation(rsx->context,
         rsx->fpo[RSX_SHADER_STOCK_BLEND], rsx->fp_offset[RSX_SHADER_STOCK_BLEND],
         GCM_LOCATION_RSX);

   rsxClearSurface(rsx->context, GCM_CLEAR_Z);
   rsxDrawVertexArray(rsx->context, GCM_TYPE_TRIANGLE_STRIP, 0, draw->coords->vertices);
}

static void gfx_display_rsx_scissor_begin(void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y,
      unsigned width, unsigned height)
{
   rsx_t *rsx = (rsx_t *)data;
   rsxSetScissor(rsx->context, x, video_height - y - height, width, height);
}

static void gfx_display_rsx_scissor_end(
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   rsx_t *rsx = (rsx_t *)data;
   rsxSetScissor(rsx->context, 0, 0, video_width, video_height);
}

static void gfx_display_rsx_blend_begin(void *data)
{
   rsx_t *rsx = (rsx_t *)data;
   rsxSetBlendEnable(rsx->context,     GCM_TRUE);
   rsxSetBlendFunc(rsx->context,       GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
   rsxSetBlendEquation(rsx->context,   GCM_FUNC_ADD,  GCM_FUNC_ADD);
#if 0
   rsxSetBlendEnableMrt(rsx->context,  GCM_TRUE, GCM_TRUE, GCM_TRUE);
   rsxSetDepthFunc(rsx->context,       GCM_LESS);
   rsxSetDepthTestEnable(rsx->context, GCM_FALSE);
   rsxSetAlphaFunc(rsx->context,       GCM_ALWAYS, 0);
   rsxSetAlphaTestEnable(rsx->context, GCM_TRUE);
#endif
}

static void gfx_display_rsx_blend_end(void *data)
{
   rsx_t *rsx = (rsx_t *)data;
   rsxSetBlendEnable(rsx->context, GCM_FALSE);
#if 0
   rsxSetBlendEnableMrt(rsx->context, GCM_FALSE, GCM_FALSE, GCM_FALSE);
#endif
}

gfx_display_ctx_driver_t gfx_display_ctx_rsx = {
   gfx_display_rsx_draw,
   NULL,                                        /* draw_pipeline */
   gfx_display_rsx_blend_begin,
   gfx_display_rsx_blend_end,
   gfx_display_rsx_get_default_mvp,
   gfx_display_rsx_get_default_vertices,
   gfx_display_rsx_get_default_tex_coords,
   FONT_DRIVER_RENDER_RSX,
   GFX_VIDEO_DRIVER_RSX,
   "rsx",
   true,
   gfx_display_rsx_scissor_begin,
   gfx_display_rsx_scissor_end
};

/*
 * FONT DRIVER
 */

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
#if 0
   /* TODO fix crash on loading core */
   if (font->texture.data)
      rsxFree(font->texture.data);
   if (font->vertices)
      rsxFree(font->vertices);
#endif
   free(font);
}

static bool rsx_font_upload_atlas(rsx_t *rsx, rsx_font_t *font)
{
   u8 *texbuffer               = (u8 *)font->texture.data;
   const u8 *atlas_data        = (u8 *)font->atlas->buffer;
   memcpy(texbuffer, atlas_data, font->atlas->height * font->atlas->width);

   font->texture.tex.format    = GCM_TEXTURE_FORMAT_B8 | GCM_TEXTURE_FORMAT_LIN;
   font->texture.tex.mipmap    = 1;
   font->texture.tex.dimension = GCM_TEXTURE_DIMS_2D;
   font->texture.tex.cubemap   = GCM_FALSE;
   font->texture.tex.remap     = (
           (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT)
         | (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT)
         | (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT)
         | (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT)
         | (GCM_TEXTURE_REMAP_COLOR_B    << GCM_TEXTURE_REMAP_COLOR_B_SHIFT)
         | (GCM_TEXTURE_REMAP_COLOR_B    << GCM_TEXTURE_REMAP_COLOR_G_SHIFT)
         | (GCM_TEXTURE_REMAP_COLOR_B    << GCM_TEXTURE_REMAP_COLOR_R_SHIFT)
         | (GCM_TEXTURE_REMAP_COLOR_B    << GCM_TEXTURE_REMAP_COLOR_A_SHIFT));
   font->texture.tex.width     = font->tex_width;
   font->texture.tex.height    = font->tex_height;
   font->texture.tex.depth     = 1;
   font->texture.tex.pitch     = font->tex_width;
   font->texture.tex.location  = GCM_LOCATION_RSX;
   font->texture.tex.offset    = font->texture.offset;
   font->texture.wrap_s        = GCM_TEXTURE_CLAMP_TO_EDGE;
   font->texture.wrap_t        = GCM_TEXTURE_CLAMP_TO_EDGE;
   font->texture.min_filter    = GCM_TEXTURE_LINEAR;
   font->texture.mag_filter    = GCM_TEXTURE_LINEAR;

   rsxInvalidateTextureCache(rsx->context, GCM_INVALIDATE_TEXTURE);
   rsxLoadTexture(rsx->context, font->tex_unit->index, &font->texture.tex);
   rsxTextureControl(rsx->context, font->tex_unit->index,
         GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
   rsxTextureFilter(rsx->context, font->tex_unit->index,
         0, font->texture.min_filter,
         font->texture.mag_filter, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
   rsxTextureWrapMode(rsx->context, font->tex_unit->index,
         font->texture.wrap_s, font->texture.wrap_t,
         GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);

   return true;
}

static void *rsx_font_init(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   rsx_font_t *font         = (rsx_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   font->rsx                = (rsx_t *)data;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
      free(font);
      return NULL;
   }

   if (is_threaded)
      if (
               font->rsx
            && font->rsx->ctx_driver
            && font->rsx->ctx_driver->make_current)
         font->rsx->ctx_driver->make_current(false);

   font->atlas              = font->font_driver->get_atlas(font->font_data);

   font->vpo                = font->rsx->vpo[RSX_SHADER_STOCK_BLEND];
   font->fpo                = font->rsx->fpo[RSX_SHADER_STOCK_BLEND];
   font->fp_ucode           = font->rsx->fp_ucode[RSX_SHADER_STOCK_BLEND];
   font->vp_ucode           = font->rsx->vp_ucode[RSX_SHADER_STOCK_BLEND];
   font->fp_offset          = font->rsx->fp_offset[RSX_SHADER_STOCK_BLEND];

   font->proj_matrix        = font->rsx->proj_matrix[RSX_SHADER_STOCK_BLEND];
   font->pos_index          = font->rsx->pos_index[RSX_SHADER_STOCK_BLEND];
   font->uv_index           = font->rsx->uv_index[RSX_SHADER_STOCK_BLEND];
   font->col_index          = font->rsx->col_index[RSX_SHADER_STOCK_BLEND];
   font->tex_unit           = font->rsx->tex_unit[RSX_SHADER_STOCK_BLEND];

   font->vertices           = (rsx_vertex_t *)rsxMemalign(128,
         sizeof(rsx_vertex_t) * RSX_MAX_FONT_VERTICES);
   font->rsx->font_vert_idx = 0;

   font->tex_width          = font->atlas->width;
   font->tex_height         = font->atlas->height;
   font->texture.data       = (u32*)rsxMemalign(128, (font->tex_height * font->tex_width));
   rsxAddressToOffset(font->texture.data, &font->texture.offset);

   if (!font->texture.data)
      goto error;

   if (!rsx_font_upload_atlas(font->rsx, font))
      goto error;

   font->atlas->dirty = false;

   rsxTextureControl(font->rsx->context, font->tex_unit->index,
         GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
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
         || !font->font_data )
      return 0;

   glyph_q = font->font_driver->get_glyph(font->font_data, '?');

   while (msg < msg_end)
   {
      const struct font_glyph *glyph;
      unsigned code = utf8_walk(&msg);

      /* Do something smarter here ... */
      if (!(glyph = font->font_driver->get_glyph(
                  font->font_data, code)))
         if (!(glyph = glyph_q))
            continue;

      delta_x += glyph->advance_x;
   }

   return delta_x * scale;
}

static void rsx_font_draw_vertices(
      rsx_t *rsx,
      rsx_font_t *font,
      const video_coords_t *coords)
{
   int i, end_vert_idx;
   rsx_vertex_t *vertices           = NULL;
   const float *vertex              = coords->vertex;
   const float *tex_coord           = coords->tex_coord;
   const float *color               = coords->color;

   if (font->atlas->dirty)
   {
      rsx_font_upload_atlas(rsx, font);
      font->atlas->dirty            = false;
   }

   end_vert_idx                     = rsx->font_vert_idx + coords->vertices;
   if (end_vert_idx > RSX_MAX_FONT_VERTICES)
   {
      rsx->font_vert_idx      = 0;
      end_vert_idx                  = rsx->font_vert_idx + coords->vertices;
   }

   vertices = &font->vertices[rsx->font_vert_idx];

   for (i = rsx->font_vert_idx; i < end_vert_idx; i++)
   {
      vertices[i].x = *vertex++;
      vertices[i].y = *vertex++;
      vertices[i].u = *tex_coord++;
      vertices[i].v = *tex_coord++;
      vertices[i].r = *color++;
      vertices[i].g = *color++;
      vertices[i].b = *color++;
      vertices[i].a = *color++;
   }

   rsxAddressToOffset(&vertices[rsx->font_vert_idx].x, &font->pos_offset);
   rsxAddressToOffset(&vertices[rsx->font_vert_idx].u, &font->uv_offset);
   rsxAddressToOffset(&vertices[rsx->font_vert_idx].r, &font->col_offset);
   rsx->font_vert_idx = end_vert_idx;

   rsxBindVertexArrayAttrib(rsx->context, font->pos_index->index, 0,
         font->pos_offset, sizeof(rsx_vertex_t), 2,
         GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, font->uv_index->index, 0,
         font->uv_offset, sizeof(rsx_vertex_t), 2,
         GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, font->col_index->index, 0,
         font->col_offset, sizeof(rsx_vertex_t), 4,
         GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

   rsxLoadVertexProgram(rsx->context, font->vpo, font->vp_ucode);
   rsxSetVertexProgramParameter(rsx->context, font->vpo,
         font->proj_matrix, (float *)&rsx->mvp_no_rot);
   rsxLoadFragmentProgramLocation(rsx->context, font->fpo,
         font->fp_offset, GCM_LOCATION_RSX);

   rsxClearSurface(rsx->context, GCM_CLEAR_Z);
   rsxDrawVertexArray(rsx->context, GCM_TYPE_TRIANGLES, 0, coords->vertices);
}

static void rsx_font_render_line(rsx_t *rsx,
      rsx_font_t *font,
      const struct font_glyph* glyph_q,
      const char *msg,
      size_t msg_len,
      float scale,
      const float color[4],
      float pos_x,
      float pos_y,
      int pre_x,
      float inv_tex_size_x,
      float inv_tex_size_y,
      float inv_win_width,
      float inv_win_height,
      unsigned text_align)
{
   int i;
   struct video_coords coords;
   float font_tex_coords[2 * 6 * MAX_MSG_LEN_CHUNK];
   float font_vertex    [2 * 6 * MAX_MSG_LEN_CHUNK];
   float font_color     [4 * 6 * MAX_MSG_LEN_CHUNK];
   const char* msg_end  = msg + msg_len;
   int x                = pre_x;
   int y                = roundf(pos_y * rsx->vp.height);
   int delta_x          = 0;
   int delta_y          = 0;

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x             -= rsx_font_get_message_width(font, msg, msg_len, scale);
         break;
      case TEXT_ALIGN_CENTER:
         x             -= rsx_font_get_message_width(font, msg, msg_len, scale) / 2.0;
         break;
   }

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
         video_coord_array_append(
               &font->block->carr, &coords, coords.vertices);
      else
         rsx_font_draw_vertices(rsx, font, &coords);
   }
}

static void rsx_font_render_message(rsx_t *rsx,
      rsx_font_t *font, const char *msg, float scale,
      const float color[4], float pos_x, float pos_y,
      unsigned text_align)
{
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   const struct font_glyph* glyph_q       = font->font_driver->get_glyph(font->font_data, '?');
   int lines                              = 0;
   int x                                  = roundf(pos_x * rsx->vp.width);
   float inv_tex_size_x                   = 1.0f / font->tex_width;
   float inv_tex_size_y                   = 1.0f / font->tex_height;
   float inv_win_width                    = 1.0f / rsx->vp.width;
   float inv_win_height                   = 1.0f / rsx->vp.height;
   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = line_metrics->height * scale / rsx->vp.height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');
      size_t msg_len    = delim ? (delim - msg) : strlen(msg);

      /* Draw the line */
      rsx_font_render_line(rsx, font, glyph_q,
            msg, msg_len, scale, color, pos_x,
            pos_y - (float)lines*line_height,
            x,
            inv_tex_size_x,
            inv_tex_size_y,
            inv_win_width,
            inv_win_height,
            text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void rsx_font_setup_viewport(
      rsx_t *rsx, rsx_font_t *font,
      unsigned width, unsigned height,
      bool full_screen)
{
   rsx_set_viewport(rsx, width, height, full_screen, false);

   rsxSetBlendEnable(rsx->context, GCM_TRUE);
   rsxSetBlendFunc(rsx->context, GCM_SRC_ALPHA,
         GCM_ONE_MINUS_SRC_ALPHA, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
   rsxSetBlendEquation(rsx->context, GCM_FUNC_ADD, GCM_FUNC_ADD);

   rsxLoadTexture(rsx->context, font->tex_unit->index, &font->texture.tex);
   rsxTextureControl(rsx->context, font->tex_unit->index,
         GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
}

static void rsx_font_render_msg(
      void *userdata,
      void *data,
      const char *msg,
      const struct font_params *params)
{
   float color[4];
   int drop_x, drop_y;
   unsigned width, height;
   float x, y, scale, drop_mod, drop_alpha;
   enum text_alignment text_align   = TEXT_ALIGN_LEFT;
   bool full_screen                 = false;
   rsx_font_t *font                 = (rsx_font_t*)data;
   settings_t *settings             = config_get_ptr();
   float video_msg_pos_x            = settings->floats.video_msg_pos_x;
   float video_msg_pos_y            = settings->floats.video_msg_pos_y;
   float video_msg_color_r          = settings->floats.video_msg_color_r;
   float video_msg_color_g          = settings->floats.video_msg_color_g;
   float video_msg_color_b          = settings->floats.video_msg_color_b;
   rsx_t *rsx                       = (rsx_t*)userdata;

   if (!font || string_is_empty(msg) || !rsx)
      return;

   width                            = rsx->width;
   height                           = rsx->height;

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
      rsx_font_setup_viewport(rsx, font, width, height, full_screen);

   if (    !string_is_empty(msg)
         && font->font_data
         && font->font_driver)
   {
      if (drop_x || drop_y)
      {
         float color_dark[4];

         color_dark[0] = color[0] * drop_mod;
         color_dark[1] = color[1] * drop_mod;
         color_dark[2] = color[2] * drop_mod;
         color_dark[3] = color[3] * drop_alpha;

         rsx_font_render_message(rsx, font, msg, scale, color_dark,
               x + scale * drop_x / rsx->vp.width, y +
               scale * drop_y / rsx->vp.height, text_align);
      }

      rsx_font_render_message(rsx, font, msg, scale, color,
            x, y, text_align);
   }

   if (!font->block)
   {
      /* Restore viewport */
      rsxTextureControl(rsx->context, font->tex_unit->index,
            GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
      rsxSetBlendEnable(rsx->context, GCM_FALSE);
      rsx_set_viewport(rsx, width, height, false, true);
   }
   rsx->font_vert_idx = 0;
}

static const struct font_glyph *rsx_font_get_glyph(
      void *data, uint32_t code)
{
   rsx_font_t *font = (rsx_font_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static void rsx_font_flush_block(unsigned width, unsigned height,
      void *data)
{
   rsx_font_t          *font        = (rsx_font_t*)data;
   video_font_raster_block_t *block = font ? font->block : NULL;
   rsx_t *rsx                       = font ? font->rsx   : NULL;

   if (!font || !block || !block->carr.coords.vertices || !rsx)
      return;

   rsx_font_setup_viewport(rsx, font, width, height, block->fullscreen);
   rsx_font_draw_vertices (rsx, font, (video_coords_t*)&block->carr.coords);

   /* Restore viewport */
   rsxTextureControl(rsx->context, font->tex_unit->index,
         GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
   rsxSetBlendEnable(rsx->context, GCM_FALSE);
   rsx_set_viewport(rsx, width, height, block->fullscreen, true);
   font->rsx->font_vert_idx = 0;
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
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t rsx_font = {
   rsx_font_init,
   rsx_font_free,
   rsx_font_render_msg,
   "rsx",
   rsx_font_get_glyph,
   rsx_font_bind_block,
   rsx_font_flush_block,
   rsx_font_get_message_width,
   rsx_font_get_line_metrics
};

/*
 * VIDEO DRIVER
 */

static void rsx_load_texture_data(rsx_t* rsx, rsx_texture_t *texture,
      const void *frame, unsigned width, unsigned height, unsigned pitch,
      bool rgb32, bool menu, enum texture_filter_type filter_type)
{
   u8 *texbuffer;
   u32 mag_filter, min_filter;
   const u8 *data         = (u8*)frame;

   if (!texture->data)
   {
      texture->data       = (u32*)rsxMemalign(128, texture->height * pitch);
      rsxAddressToOffset(texture->data, &texture->offset);
   }

   texbuffer              = (u8*)texture->data;
   memcpy(texbuffer, data, height * pitch);

   texture->tex.format    = (rgb32
                            ? GCM_TEXTURE_FORMAT_A8R8G8B8 :
                            (menu)
                            ? GCM_TEXTURE_FORMAT_A4R4G4B4
                            : GCM_TEXTURE_FORMAT_R5G6B5)
                            | GCM_TEXTURE_FORMAT_LIN;
   texture->tex.mipmap    = 1;
   texture->tex.dimension = GCM_TEXTURE_DIMS_2D;
   texture->tex.cubemap   = GCM_FALSE;
   texture->tex.remap     =  ((GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_B_SHIFT)
                            | (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_G_SHIFT)
                            | (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_R_SHIFT)
                            | (GCM_TEXTURE_REMAP_TYPE_REMAP << GCM_TEXTURE_REMAP_TYPE_A_SHIFT)
                            | (GCM_TEXTURE_REMAP_COLOR_B << GCM_TEXTURE_REMAP_COLOR_B_SHIFT)
                            | (GCM_TEXTURE_REMAP_COLOR_G << GCM_TEXTURE_REMAP_COLOR_G_SHIFT)
                            | (GCM_TEXTURE_REMAP_COLOR_R << GCM_TEXTURE_REMAP_COLOR_R_SHIFT)
                            | (GCM_TEXTURE_REMAP_COLOR_A << GCM_TEXTURE_REMAP_COLOR_A_SHIFT));
   texture->tex.width     = width;
   texture->tex.height    = height;
   texture->tex.depth     = 1;
   texture->tex.location  = GCM_LOCATION_RSX;
   texture->tex.pitch     = pitch;
   texture->tex.offset    = texture->offset;

   switch (filter_type)
   {
      case TEXTURE_FILTER_MIPMAP_NEAREST:
      case TEXTURE_FILTER_NEAREST:
         min_filter       = GCM_TEXTURE_NEAREST;
         mag_filter       = GCM_TEXTURE_NEAREST;
         break;
      case TEXTURE_FILTER_MIPMAP_LINEAR:
      case TEXTURE_FILTER_LINEAR:
         default:
         min_filter       = GCM_TEXTURE_LINEAR;
         mag_filter       = GCM_TEXTURE_LINEAR;
         break;
   }
   texture->min_filter    = min_filter;
   texture->mag_filter    = mag_filter;
   texture->wrap_s        = GCM_TEXTURE_CLAMP_TO_EDGE;
   texture->wrap_t        = GCM_TEXTURE_CLAMP_TO_EDGE;
}

static void rsx_set_projection(rsx_t *rsx,
      struct video_ortho *ortho, bool allow_rotate)
{
   static math_matrix_4x4 rot     = {
      { 0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    1.0f }
   };
   float radians, cosine, sine;

   /* Calculate projection. */
   matrix_4x4_ortho(rsx->mvp_no_rot, ortho->left, ortho->right,
         ortho->bottom, ortho->top, ortho->znear, ortho->zfar);

   if (!allow_rotate)
   {
      rsx->mvp             = rsx->mvp_no_rot;
      return;
   }

   radians                 = M_PI * rsx->rotation / 180.0f;
   cosine                  = cosf(radians);
   sine                    = sinf(radians);
   MAT_ELEM_4X4(rot, 0, 0) = cosine;
   MAT_ELEM_4X4(rot, 0, 1) = -sine;
   MAT_ELEM_4X4(rot, 1, 0) = sine;
   MAT_ELEM_4X4(rot, 1, 1) = cosine;
   matrix_4x4_multiply(rsx->mvp, rot, rsx->mvp_no_rot);
}


static void rsx_set_viewport(void *data, unsigned viewport_width,
      unsigned viewport_height, bool force_full, bool allow_rotate)
{
	int i;
   rsx_viewport_t vp;
   struct video_ortho ortho  = {0, 1, 0, 1, -1, 1};
   settings_t *settings      = config_get_ptr();
   rsx_t *rsx                = (rsx_t*)data;
   bool video_scale_integer  = settings->bools.video_scale_integer;

   if (video_scale_integer && !force_full)
   {
      video_viewport_get_scaled_integer(&rsx->vp,
            viewport_width, viewport_height,
            video_driver_get_aspect_ratio(), rsx->keep_aspect,
            true);
      viewport_width          = rsx->vp.width;
      viewport_height         = rsx->vp.height;
   }
   else if (rsx->keep_aspect && !force_full)
   {
      video_viewport_get_scaled_aspect(&rsx->vp, viewport_width, viewport_height, true);
      viewport_width          = rsx->vp.width;
      viewport_height         = rsx->vp.height;
   }
   else
   {
      rsx->vp.x               = 0;
      rsx->vp.y               = 0;
      rsx->vp.width           = viewport_width;
      rsx->vp.height          = viewport_height;
   }

   vp.min                     = 0.0f;
   vp.max                     = 1.0f;
   vp.x                       = rsx->vp.x;
   vp.y                       = rsx->height - rsx->vp.y - rsx->vp.height;
   vp.w                       = rsx->vp.width;
   vp.h                       = rsx->vp.height;
   vp.scale[0]                = vp.w *  0.5f;
   vp.scale[1]                = vp.h * -0.5f;
   vp.scale[2]                = (vp.max - vp.min) * 0.5f;
   vp.scale[3]                = 0.0f;
   vp.offset[0]               = vp.x + vp.w * 0.5f;
   vp.offset[1]               = vp.y + vp.h * 0.5f;
   vp.offset[2]               = (vp.max + vp.min) * 0.5f;
   vp.offset[3]               = 0.0f;

   rsxSetViewport(rsx->context, vp.x, vp.y, vp.w, vp.h, vp.min, vp.max, vp.scale, vp.offset);
   for (i = 0; i < 8; i++)
      rsxSetViewportClip(rsx->context, i, rsx->width, rsx->height);
   rsxSetScissor(rsx->context, vp.x, vp.y, vp.w, vp.h);

   rsx_set_projection(rsx, &ortho, allow_rotate);

   /* Set last backbuffer viewport. */
   if (!force_full)
   {
      rsx->vp.width           = viewport_width;
      rsx->vp.height          = viewport_height;
   }
}

static const gfx_ctx_driver_t* rsx_get_context(rsx_t* rsx)
{
   const gfx_ctx_driver_t* gfx_ctx      = NULL;
   void* ctx_data                       = NULL;
   settings_t* settings                 = config_get_ptr();
   struct retro_hw_render_callback* hwr = video_driver_get_hw_context();
   bool video_shared_context            = settings->bools.video_shared_context;
   enum gfx_ctx_api api                 = GFX_CTX_RSX_API;

   rsx->shared_context_use              = (video_shared_context && (hwr->context_type != RETRO_HW_CONTEXT_NONE));

   if ((runloop_get_flags() & RUNLOOP_FLAG_CORE_SET_SHARED_CONTEXT)
      && (hwr->context_type != RETRO_HW_CONTEXT_NONE))
      rsx->shared_context_use           = true;

   gfx_ctx = video_context_driver_init_first(rsx,
      settings->arrays.video_context_driver,
      api, 1, 0, rsx->shared_context_use, &ctx_data);

   if (ctx_data)
      rsx->ctx_data                     = ctx_data;

   return gfx_ctx;
}

#ifndef HAVE_THREADS
static bool rsx_tasks_finder(retro_task_t *task,void *userdata) { return task; }
task_finder_data_t rsx_tasks_finder_data = {rsx_tasks_finder, NULL};
#endif

static int rsx_make_buffer(rsxBuffer * buffer, u16 width, u16 height, int id)
{
   int depth         = sizeof(u32);
   int pitch         = depth * width;
   int size          = depth * width * height;
   if (!(buffer->ptr = (uint32_t*)rsxMemalign (64, size)))
      goto error;

   if (rsxAddressToOffset(buffer->ptr, &buffer->offset) != 0)
      goto error;

   /* Register the display buffer with the RSX */
   if (gcmSetDisplayBuffer(id, buffer->offset, pitch, width, height) != 0)
      goto error;

   buffer->width  = width;
   buffer->height = height;
   buffer->id     = id;

   return 1;

error:
   if (buffer->ptr)
      rsxFree (buffer->ptr);
   return 0;
}

static int rsx_flip(gcmContextData *context, s32 buffer)
{
   if (gcmSetFlip(context, buffer) == 0)
   {
      rsxFlushBuffer (context);
      /* Prevent the RSX from continuing until the flip has finished. */
      gcmSetWaitFlip (context);
      return 1;
   }
   return 0;
}

#define GCM_LABEL_INDEX		255

static void rsx_wait_finish(gcmContextData *context, u32 sLabelVal)
{
  rsxSetWriteBackendLabel(context, GCM_LABEL_INDEX, sLabelVal);

  rsxFlushBuffer(context);

  while(*(vu32 *)gcmGetLabelAddress(GCM_LABEL_INDEX) != sLabelVal)
    usleep(30);

  sLabelVal++;
}

static void rsx_wait_rsx_idle(gcmContextData *context)
{
  u32 sLabelVal = 1;

  rsxSetWriteBackendLabel(context, GCM_LABEL_INDEX, sLabelVal);
  rsxSetWaitLabel(context, GCM_LABEL_INDEX, sLabelVal);

  sLabelVal++;

  rsx_wait_finish(context, sLabelVal);
}

static void rsx_wait_flip(void)
{
  while(gcmGetFlipStatus() != 0)
    usleep(200);  /* Sleep, to not stress the cpu. */
  gcmResetFlipStatus();
}

static gcmContextData *rsx_init_screen(rsx_t* gcm)
{
   videoState state;
   videoConfiguration vconfig;
   videoResolution res; /* Screen Resolution */
   /* Context to keep track of the RSX buffer. */
   gcmContextData              *context = NULL;
   static gcmContextData *saved_context = NULL;

   if (!saved_context)
   {
      /* Allocate a 1MB buffer, aligned to a 1MB boundary
       * to be our shared I/O memory with the RSX. */
      void *host_addr = memalign(1024*1024, HOST_SIZE);

      if (!host_addr)
         goto error;

      /* Initialise Reality, which sets up the
       * command buffer and shared I/O memory */
#ifdef NV40TCL_RENDER_ENABLE
      /* There was an API breakage on 2020-07-10, let's
       * workaround this by using one of the new defines */
      rsxInit(&context, CB_SIZE, HOST_SIZE, host_addr);
#else
      context = rsxInit(CB_SIZE, HOST_SIZE, host_addr);
#endif
      if (!context)
         goto error;
      saved_context = context;
   }
   else
      context = saved_context;

   /* Get the state of the display */
   if (videoGetState(0, 0, &state) != 0)
      goto error;

   /* Make sure display is enabled */
   if (state.state != 0)
      goto error;

   /* Get the current resolution */
   if (videoGetResolution(state.displayMode.resolution, &res) != 0)
      goto error;

   /* Configure the buffer format to xRGB */
   memset(&vconfig, 0, sizeof(videoConfiguration));
   vconfig.resolution = state.displayMode.resolution;
   vconfig.format     = VIDEO_BUFFER_FORMAT_XRGB;
   vconfig.pitch      = res.width * sizeof(u32);
   vconfig.aspect     = state.displayMode.aspect;

   gcm->width         = res.width;
   gcm->height        = res.height;

   rsx_wait_rsx_idle(context);

   if (videoConfigure(0, &vconfig, NULL, 0) != 0)
      goto error;

   if (videoGetState(0, 0, &state) != 0)
      goto error;

   gcmSetFlipMode(GCM_FLIP_VSYNC); /* Wait for VSYNC to flip */

   gcm->depth_pitch  = res.width * sizeof(u32);
   gcm->depth_buffer = (u32 *)rsxMemalign(64, (res.height * gcm->depth_pitch));  /* Beware, if was (res.height * gcm->depth_pitch) * 2 */

   rsxAddressToOffset(gcm->depth_buffer, &gcm->depth_offset);

   gcmResetFlipStatus();

   return context;

error:
#if 0
   if (context)
      rsxFinish(context, 0);

   if (gcm->host_addr)
      free(gcm->host_addr);
#endif

   return NULL;
}

static void rsx_init_render_target(rsx_t *rsx, rsxBuffer * buffer, int id)
{
   u32 i;
   memset(&rsx->surface[id], 0, sizeof(gcmSurface));
   rsx->surface[id].colorFormat		      = GCM_SURFACE_X8R8G8B8;
   rsx->surface[id].colorTarget		      = GCM_SURFACE_TARGET_0;
   rsx->surface[id].colorLocation[0]	   = GCM_LOCATION_RSX;
   rsx->surface[id].colorOffset[0]	      = buffer->offset;
   rsx->surface[id].colorPitch[0]	      = rsx->width * 4;
   for (i = 1; i < GCM_MAX_MRT_COUNT; i++)
   {
      rsx->surface[id].colorLocation[i]	= GCM_LOCATION_RSX;
      rsx->surface[id].colorOffset[i]		= buffer->offset;
      rsx->surface[id].colorPitch[i]		= 64;
   }
   rsx->surface[id].depthFormat		      = GCM_SURFACE_ZETA_Z24S8;
   rsx->surface[id].depthLocation	      = GCM_LOCATION_RSX;
   rsx->surface[id].depthOffset		      = rsx->depth_offset;
   rsx->surface[id].depthPitch		      = rsx->width * 4;
   rsx->surface[id].type			         = GCM_SURFACE_TYPE_LINEAR;
   rsx->surface[id].antiAlias		         = GCM_SURFACE_CENTER_1;
   rsx->surface[id].width			         = rsx->width;
   rsx->surface[id].height			         = rsx->height;
   rsx->surface[id].x				         = 0;
   rsx->surface[id].y				         = 0;
}

static void rsx_init_vertices(rsx_t *rsx)
{
   rsx->vertices         = (rsx_vertex_t *)rsxMemalign(128, sizeof(rsx_vertex_t) * RSX_MAX_VERTICES); /* vertices for menu and core */
   rsx->vert_idx         = 0;

   rsx->vertices[0].x    = 0.0f;
   rsx->vertices[0].y    = 0.0f;
   rsx->vertices[0].u    = 0.0f;
   rsx->vertices[0].v    = 1.0f;
   rsx->vertices[0].r    = 1.0f;
   rsx->vertices[0].g    = 1.0f;
   rsx->vertices[0].b    = 1.0f;
   rsx->vertices[0].a    = 1.0f;

   rsx->vertices[1].x    = 1.0f;
   rsx->vertices[1].y    = 0.0f;
   rsx->vertices[1].u    = 1.0f;
   rsx->vertices[1].v    = 1.0f;
   rsx->vertices[1].r    = 1.0f;
   rsx->vertices[1].g    = 1.0f;
   rsx->vertices[1].b    = 1.0f;
   rsx->vertices[1].a    = 1.0f;

   rsx->vertices[2].x    = 0.0f;
   rsx->vertices[2].y    = 1.0f;
   rsx->vertices[2].u    = 0.0f;
   rsx->vertices[2].v    = 0.0f;
   rsx->vertices[2].r    = 1.0f;
   rsx->vertices[2].g    = 1.0f;
   rsx->vertices[2].b    = 1.0f;
   rsx->vertices[2].a    = 1.0f;

   rsx->vertices[3].x    = 1.0f;
   rsx->vertices[3].y    = 1.0f;
   rsx->vertices[3].u    = 1.0f;
   rsx->vertices[3].v    = 0.0f;
   rsx->vertices[3].r    = 1.0f;
   rsx->vertices[3].g    = 1.0f;
   rsx->vertices[3].b    = 1.0f;
   rsx->vertices[3].a    = 1.0f;

#if RSX_MAX_TEXTURE_VERTICES > 0
   /* Using preallocated texture vertices */
   rsx->texture_vertices = (rsx_vertex_t *)rsxMemalign(128, sizeof(rsx_vertex_t) * RSX_MAX_TEXTURE_VERTICES);
   rsx->texture_vert_idx = 0;
#endif
}

static void rsx_init_shader(rsx_t *rsx)
{
   u32 fpsize                               = 0;
   u32 vpsize                               = 0;
   rsx->vp_ucode[RSX_SHADER_MENU]           = NULL;
   rsx->fp_ucode[RSX_SHADER_MENU]           = NULL;
   rsx->vpo[RSX_SHADER_MENU]                = (rsxVertexProgram*)modern_opaque_vpo;
   rsx->fpo[RSX_SHADER_MENU]                = (rsxFragmentProgram*)modern_opaque_fpo;
   rsxVertexProgramGetUCode(  rsx->vpo[RSX_SHADER_MENU], &rsx->vp_ucode[RSX_SHADER_MENU], &vpsize);
   rsxFragmentProgramGetUCode(rsx->fpo[RSX_SHADER_MENU], &rsx->fp_ucode[RSX_SHADER_MENU], &fpsize);
   rsx->fp_buffer[RSX_SHADER_MENU]          = (u32*)rsxMemalign(64, fpsize);
   if (!rsx->fp_buffer[RSX_SHADER_MENU])
   {
      RARCH_ERR("failed to allocate fp_buffer\n");
      return;
   }
   memcpy(rsx->fp_buffer[RSX_SHADER_MENU], rsx->fp_ucode[RSX_SHADER_MENU], fpsize);
   rsxAddressToOffset(rsx->fp_buffer[RSX_SHADER_MENU], &rsx->fp_offset[RSX_SHADER_MENU]);
   rsx->proj_matrix[RSX_SHADER_MENU]        = rsxVertexProgramGetConst(rsx->vpo[RSX_SHADER_MENU], "modelViewProj");
   rsx->pos_index[RSX_SHADER_MENU]          = rsxVertexProgramGetAttrib(rsx->vpo[RSX_SHADER_MENU], "position");
   rsx->col_index[RSX_SHADER_MENU]          = rsxVertexProgramGetAttrib(rsx->vpo[RSX_SHADER_MENU], "color");
   rsx->uv_index[RSX_SHADER_MENU]           = rsxVertexProgramGetAttrib(rsx->vpo[RSX_SHADER_MENU], "texcoord");
   rsx->tex_unit[RSX_SHADER_MENU]           = rsxFragmentProgramGetAttrib(rsx->fpo[RSX_SHADER_MENU], "texture");

   rsx->vp_ucode[RSX_SHADER_STOCK_BLEND]    = NULL;
   rsx->fp_ucode[RSX_SHADER_STOCK_BLEND]    = NULL;
   rsx->vpo[RSX_SHADER_STOCK_BLEND]         = (rsxVertexProgram *)modern_alpha_blend_vpo;
   rsx->fpo[RSX_SHADER_STOCK_BLEND]         = (rsxFragmentProgram *)modern_alpha_blend_fpo;
   rsxVertexProgramGetUCode(rsx->vpo[RSX_SHADER_STOCK_BLEND], &rsx->vp_ucode[RSX_SHADER_STOCK_BLEND], &vpsize);
   rsxFragmentProgramGetUCode(rsx->fpo[RSX_SHADER_STOCK_BLEND], &rsx->fp_ucode[RSX_SHADER_STOCK_BLEND], &fpsize);
   rsx->fp_buffer[RSX_SHADER_STOCK_BLEND]   = (u32 *)rsxMemalign(64, fpsize);
   if (!rsx->fp_buffer[RSX_SHADER_STOCK_BLEND])
   {
      RARCH_ERR("failed to allocate fp_buffer\n");
      return;
   }
   memcpy(rsx->fp_buffer[RSX_SHADER_STOCK_BLEND], rsx->fp_ucode[RSX_SHADER_STOCK_BLEND], fpsize);
   rsxAddressToOffset(rsx->fp_buffer[RSX_SHADER_STOCK_BLEND], &rsx->fp_offset[RSX_SHADER_STOCK_BLEND]);
   rsx->proj_matrix[RSX_SHADER_STOCK_BLEND] = rsxVertexProgramGetConst(rsx->vpo[RSX_SHADER_STOCK_BLEND], "modelViewProj");
   rsx->pos_index[RSX_SHADER_STOCK_BLEND]   = rsxVertexProgramGetAttrib(rsx->vpo[RSX_SHADER_STOCK_BLEND], "position");
   rsx->col_index[RSX_SHADER_STOCK_BLEND]   = rsxVertexProgramGetAttrib(rsx->vpo[RSX_SHADER_STOCK_BLEND], "color");
   rsx->uv_index[RSX_SHADER_STOCK_BLEND]    = rsxVertexProgramGetAttrib(rsx->vpo[RSX_SHADER_STOCK_BLEND], "texcoord");
   rsx->tex_unit[RSX_SHADER_STOCK_BLEND]    = rsxFragmentProgramGetAttrib(rsx->fpo[RSX_SHADER_STOCK_BLEND], "texture");
   rsx->bgcolor[RSX_SHADER_STOCK_BLEND]     = rsxFragmentProgramGetConst(rsx->fpo[RSX_SHADER_STOCK_BLEND], "bgcolor");
}

static void* rsx_init(const video_info_t* video,
      input_driver_t** input, void** input_data)
{
   int i;
   const gfx_ctx_driver_t* ctx_driver = NULL;
   rsx_t* rsx                         = (rsx_t*)malloc(sizeof(rsx_t));

   if (!rsx)
      return NULL;

   memset(rsx, 0, sizeof(rsx_t));

   rsx->context = rsx_init_screen(rsx);

   if (!(ctx_driver = rsx_get_context(rsx)))
   {
      free(rsx);
      return NULL;
   }

   video_context_driver_set((const gfx_ctx_driver_t*)ctx_driver);
   rsx->ctx_driver = ctx_driver;
   rsx->video_info = *video;

   for (i = 0; i < RSX_MAX_BUFFERS; i++)
   {
      rsx_make_buffer(&rsx->buffers[i], rsx->width, rsx->height, i);
      rsx_init_render_target(rsx, &rsx->buffers[i], i);
   }

#if defined(HAVE_MENU_BUFFER)
   for (i = 0; i < RSX_MAX_MENU_BUFFERS; i++)
   {
      rsx_make_buffer(&rsx->menuBuffers[i], rsx->width, rsx->height, i+RSX_MAX_BUFFERS);
      rsx_init_render_target(rsx, &rsx->menuBuffers[i], i+RSX_MAX_BUFFERS);
   }
#endif

   for (i = 0; i < RSX_MAX_TEXTURES; i++)
   {
      rsx->texture[i].data   = NULL;
      rsx->texture[i].height = rsx->height;
      rsx->texture[i].width  = rsx->width;
   }
   rsx->menu_texture.data    = NULL;
   rsx->menu_texture.height  = rsx->height;
   rsx->menu_texture.width   = rsx->width;

   rsx_init_shader(rsx);
   rsx_init_vertices(rsx);

   rsx_flip(rsx->context, RSX_MAX_BUFFERS - 1);

   rsx->vp.x                 = 0;
   rsx->vp.y                 = 0;
   rsx->vp.width             = rsx->width;
   rsx->vp.height            = rsx->height;
   rsx->vp.full_width        = rsx->width;
   rsx->vp.full_height       = rsx->height;
   rsx->rgb32                = video->rgb32;
   video_driver_set_size(rsx->vp.width, rsx->vp.height);
   rsx_set_viewport(rsx, rsx->vp.width, rsx->vp.height, false, true);

   if (input && input_data)
   {
      void *ps3input         = input_driver_init_wrap(&input_ps3, ps3_joypad.ident);
      *input                 = ps3input ? &input_ps3 : NULL;
      *input_data            = ps3input;
   }

   rsx_context_bind_hw_render(rsx, true);

   if (video->font_enable)
   {
      font_driver_init_osd(rsx,
            video,
            false,
            video->is_threaded,
            FONT_DRIVER_RENDER_RSX);
      rsx->msg_rendering_enabled = true;
   }

   return rsx;
}

static void rsx_update_viewport(rsx_t* rsx)
{
   int x                     = 0;
   int y                     = 0;
   unsigned viewport_width   = rsx->width;
   unsigned viewport_height  = rsx->height;
   float device_aspect       = ((float)viewport_width) / viewport_height;
   settings_t *settings      = config_get_ptr();
   bool video_scale_integer  = settings->bools.video_scale_integer;
   unsigned aspect_ratio_idx = settings->uints.video_aspect_ratio_idx;

   if (video_scale_integer)
   {
      video_viewport_get_scaled_integer(&rsx->vp, viewport_width,
            viewport_height, video_driver_get_aspect_ratio(), rsx->keep_aspect,
            true);
      viewport_width         = rsx->vp.width;
      viewport_height        = rsx->vp.height;
   }
   else if (rsx->keep_aspect)
   {
      float desired_aspect   = video_driver_get_aspect_ratio();

#if defined(HAVE_MENU)
      if (aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         video_viewport_t *custom_vp = &settings->video_viewport_custom;
         /* RSX/libgcm has top-left origin viewport. */
         x                           = custom_vp->x;
         y                           = custom_vp->y;
         viewport_width              = custom_vp->width;
         viewport_height             = custom_vp->height;
      }
      else
#endif
      {
         float delta;

         if ((fabsf(device_aspect - desired_aspect) < 0.0001f))
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            float viewport_bias = settings->floats.video_viewport_bias_x;
            delta           = (desired_aspect / device_aspect - 1.0f)
               / 2.0f + 0.5f;
            x               = (int)roundf(viewport_width * ((0.5f - delta) * (viewport_bias * 2.0f)));
            viewport_width  = (unsigned)roundf(2.0f * viewport_width * delta);
         }
         else
         {
            float viewport_bias = 1.0 - settings->floats.video_viewport_bias_y;
            delta           = (device_aspect / desired_aspect - 1.0f)
               / 2.0f + 0.5f;
            y               = (int)roundf(viewport_height * ((0.5f - delta) * (viewport_bias * 2.0f)));
            viewport_height = (unsigned)roundf(2.0f * viewport_height * delta);
         }
      }

      rsx->vp.x             = x;
      rsx->vp.y             = y;
      rsx->vp.width         = viewport_width;
      rsx->vp.height        = viewport_height;
   }
   else
   {
      rsx->vp.x             = 0;
      rsx->vp.y             = 0;
      rsx->vp.width         = viewport_width;
      rsx->vp.height        = viewport_height;
   }

   rsx->should_resize       = false;
}

static unsigned rsx_wrap_type_to_enum(enum gfx_wrap_type type)
{
   switch (type)
   {
      case RARCH_WRAP_BORDER:
      case RARCH_WRAP_EDGE:
         return GCM_TEXTURE_CLAMP_TO_EDGE;
      case RARCH_WRAP_REPEAT:
         return GCM_TEXTURE_REPEAT;
      case RARCH_WRAP_MIRRORED_REPEAT:
         return GCM_TEXTURE_MIRRORED_REPEAT;
      default:
	       break;
   }

   return 0;
}

static uintptr_t rsx_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   rsx_t *rsx                     = (rsx_t *)video_data;
   struct texture_image *image    = (struct texture_image*)data;
   rsx_texture_t *texture         = (rsx_texture_t *)malloc(sizeof(rsx_texture_t));
   texture->width                 = image->width;
   texture->height                = image->height;
   texture->data                  = (u32*)rsxMemalign(128, (image->height * image->width*4));
   rsxAddressToOffset(texture->data, &texture->offset);
   rsx_load_texture_data(rsx, texture, image->pixels, image->width, image->height, image->width*4, true, false, filter_type);

   return (uintptr_t)texture;;
}

static void rsx_unload_texture(void *data,
      bool threaded, uintptr_t handle)
{
   rsx_texture_t *texture = (rsx_texture_t *)handle;
   if (texture)
   {
#if 0
      /* TODO fix crash on loading core */
      if (texture->data)
         rsxFree(texture->data);
#endif
      free(texture);
   }
}

#if 0
/* TODO/FIXME - commenting this code out for now until it gets used */
static void rsx_fill_black(uint32_t *dst, uint32_t *dst_end, size_t sz)
{
  if (sz > dst_end - dst)
    sz = dst_end - dst;
  memset(dst, 0, sz * 4);
}

static void rsx_blit_buffer(
      rsxBuffer *buffer, const void *frame, unsigned width,
      unsigned height, unsigned pitch, int rgb32, bool do_scaling)
{
   int i;
   uint32_t *dst;
   uint32_t *dst_end;
   int pre_clean;
   int scale = 1, xofs = 0, yofs = 0;
   if (width > buffer->width)
      width = buffer->width;
   if (height > buffer->height)
      height = buffer->height;

   if (do_scaling)
   {
      scale    = buffer->width / width;
      if (scale > buffer->height / height)
         scale = buffer->height / height;
      if (scale >= 10)
         scale = 10;
      if (scale >= 1)
      {
         xofs  = (buffer->width - width * scale)   / 2;
         yofs  = (buffer->height - height * scale) / 2;
      }
      else
         scale = 1;
   }

   /* TODO/FIXME: let RSX do the copy */
   pre_clean   = xofs + buffer->width * yofs;
   dst         = buffer->ptr;
   dst_end     = buffer->ptr + buffer->width * buffer->height;

   memset(dst, 0, pre_clean * 4);
   dst        += pre_clean;

   if (scale == 1)
   {
      if (rgb32)
      {
         const uint8_t *src = frame;
         for (i = 0; i < height; i++)
         {
            memcpy(dst, src, width * 4);
            rsx_fill_black(dst + width, dst_end, buffer->width - width);
            dst += buffer->width;
            src += pitch;
         }
      }
      else
      {
         const uint16_t *src = frame;
         for (i = 0; i < height; i++)
         {
            int j;
            for (j = 0; j < width; j++, src++, dst++)
            {
               u16 rgb565 = *src;
               u8 r       = ((rgb565 >> 8) & 0xf8);
               u8 g       = ((rgb565 >> 3) & 0xfc);
               u8 b       = ((rgb565 << 3) & 0xfc);
               *dst       = (r<<16) | (g<<8) | b;
            }
            rsx_fill_black(dst, dst_end, buffer->width - width);

            dst += buffer->width - width;
            src += pitch / 2 - width;
         }
      }
   }
   else
   {
      if (rgb32)
      {
         const uint32_t *src = frame;
         for (i = 0; i < height; i++)
         {
            int j, l;
            for (j = 0; j < width; j++, src++)
            {
               int k;
               u32 c = *src;
               for (k = 0; k < scale; k++, dst++)
                  for (l = 0; l < scale; l++)
                     dst[l * buffer->width] = c;
            }
            for (l = 0; l < scale; l++)
               rsx_fill_black(dst + l * buffer->width, dst_end, buffer->width - width * scale);

            dst += buffer->width * scale - width * scale;
            src += pitch / 4 - width;
         }
      }
      else
      {
         const uint16_t *src = frame;
         for (i = 0; i < height; i++)
         {
            int j, l;
            for (j = 0; j < width; j++, src++)
            {
               int k;
               u16 rgb565 = *src;
               u8 r       = ((rgb565 >> 8) & 0xf8);
               u8 g       = ((rgb565 >> 3) & 0xfc);
               u8 b       = ((rgb565 << 3) & 0xfc);
               u32 c      = (r<<16) | (g<<8) | b;
               for (k = 0; k < scale; k++, dst++)
                  for (l = 0; l < scale; l++)
                     dst[l * buffer->width] = c;
            }
            for (l = 0; l < scale; l++)
               rsx_fill_black(dst + l * buffer->width, dst_end, buffer->width - width * scale);

            dst += buffer->width * scale - width * scale;
            src += pitch / 2 - width;
         }
      }
   }

   if (dst < dst_end)
      memset(dst, 0, 4 * (dst_end - dst));
}
#endif

static void rsx_set_texture(rsx_t* rsx, rsx_texture_t *texture)
{
   rsxInvalidateTextureCache(rsx->context, GCM_INVALIDATE_TEXTURE);
   rsxLoadTexture(rsx->context, rsx->tex_unit[RSX_SHADER_MENU]->index, &texture->tex);
   rsxTextureControl(rsx->context, rsx->tex_unit[RSX_SHADER_MENU]->index, GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
   rsxTextureFilter(rsx->context, rsx->tex_unit[RSX_SHADER_MENU]->index, 0, texture->min_filter, texture->mag_filter, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
   rsxTextureWrapMode(rsx->context, rsx->tex_unit[RSX_SHADER_MENU]->index, texture->wrap_s, texture->wrap_t, GCM_TEXTURE_CLAMP_TO_EDGE,
         0, GCM_TEXTURE_ZFUNC_LESS, 0);
}

static void rsx_set_menu_texture(rsx_t* rsx, rsx_texture_t *texture)
{
   rsxInvalidateTextureCache(rsx->context, GCM_INVALIDATE_TEXTURE);
   rsxLoadTexture(rsx->context, rsx->tex_unit[RSX_SHADER_STOCK_BLEND]->index, &texture->tex);
   rsxTextureControl(rsx->context, rsx->tex_unit[RSX_SHADER_STOCK_BLEND]->index, GCM_TRUE, 0 << 8, 12 << 8, GCM_TEXTURE_MAX_ANISO_1);
   rsxTextureFilter(rsx->context, rsx->tex_unit[RSX_SHADER_STOCK_BLEND]->index, 0, texture->min_filter,
         texture->mag_filter, GCM_TEXTURE_CONVOLUTION_QUINCUNX);
   rsxTextureWrapMode(rsx->context, rsx->tex_unit[RSX_SHADER_STOCK_BLEND]->index, texture->wrap_s,
         texture->wrap_t, GCM_TEXTURE_CLAMP_TO_EDGE, 0, GCM_TEXTURE_ZFUNC_LESS, 0);
}

static void rsx_clear_surface(rsx_t* rsx)
{
   rsxSetColorMask(rsx->context,
                 GCM_COLOR_MASK_R
               | GCM_COLOR_MASK_G
               | GCM_COLOR_MASK_B
               | GCM_COLOR_MASK_A);

   rsxSetColorMaskMrt(rsx->context, 0);

   rsxSetUserClipPlaneControl(rsx->context,
               GCM_USER_CLIP_PLANE_DISABLE,
               GCM_USER_CLIP_PLANE_DISABLE,
               GCM_USER_CLIP_PLANE_DISABLE,
               GCM_USER_CLIP_PLANE_DISABLE,
               GCM_USER_CLIP_PLANE_DISABLE,
               GCM_USER_CLIP_PLANE_DISABLE);

   rsxSetClearColor(rsx->context, 0);
   rsxSetClearDepthStencil(rsx->context, 0xffffff00);
   rsxClearSurface(rsx->context,
                    GCM_CLEAR_R
                  | GCM_CLEAR_G
                  | GCM_CLEAR_B
                  | GCM_CLEAR_A
                  | GCM_CLEAR_S
                  | GCM_CLEAR_Z);
   rsxSetZMinMaxControl(rsx->context, 0, 1, 1);
}

static void rsx_draw_vertices(rsx_t* rsx)
{
   rsx_vertex_t *vertices      = NULL;
   int end_vert_idx            = rsx->vert_idx + 4;
   if (end_vert_idx > RSX_MAX_VERTICES)
   {
      rsx->vert_idx            = 0;
      end_vert_idx             = rsx->vert_idx + 4;
   }
   vertices                    = &rsx->vertices[rsx->vert_idx];

   vertices[rsx->vert_idx+0].x = 0.0f;
   vertices[rsx->vert_idx+0].y = 0.0f;
   vertices[rsx->vert_idx+0].u = 0.0f;
   vertices[rsx->vert_idx+0].v = 1.0f;
   vertices[rsx->vert_idx+0].r = 1.0f;
   vertices[rsx->vert_idx+0].g = 1.0f;
   vertices[rsx->vert_idx+0].b = 1.0f;
   vertices[rsx->vert_idx+0].a = 1.0f;

   vertices[rsx->vert_idx+1].x = 1.0f;
   vertices[rsx->vert_idx+1].y = 0.0f;
   vertices[rsx->vert_idx+1].u = 1.0f;
   vertices[rsx->vert_idx+1].v = 1.0f;
   vertices[rsx->vert_idx+1].r = 1.0f;
   vertices[rsx->vert_idx+1].g = 1.0f;
   vertices[rsx->vert_idx+1].b = 1.0f;
   vertices[rsx->vert_idx+1].a = 1.0f;

   vertices[rsx->vert_idx+2].x = 0.0f;
   vertices[rsx->vert_idx+2].y = 1.0f;
   vertices[rsx->vert_idx+2].u = 0.0f;
   vertices[rsx->vert_idx+2].v = 0.0f;
   vertices[rsx->vert_idx+2].r = 1.0f;
   vertices[rsx->vert_idx+2].g = 1.0f;
   vertices[rsx->vert_idx+2].b = 1.0f;
   vertices[rsx->vert_idx+2].a = 1.0f;

   vertices[rsx->vert_idx+3].x = 1.0f;
   vertices[rsx->vert_idx+3].y = 1.0f;
   vertices[rsx->vert_idx+3].u = 1.0f;
   vertices[rsx->vert_idx+3].v = 0.0f;
   vertices[rsx->vert_idx+3].r = 1.0f;
   vertices[rsx->vert_idx+3].g = 1.0f;
   vertices[rsx->vert_idx+3].b = 1.0f;
   vertices[rsx->vert_idx+3].a = 1.0f;

   rsxAddressToOffset(&vertices[rsx->vert_idx].x, &rsx->pos_offset[RSX_SHADER_MENU]);
   rsxAddressToOffset(&vertices[rsx->vert_idx].u, &rsx->uv_offset[RSX_SHADER_MENU]);
   rsxAddressToOffset(&vertices[rsx->vert_idx].r, &rsx->col_offset[RSX_SHADER_MENU]);
   rsx->vert_idx               = end_vert_idx;

   rsxBindVertexArrayAttrib(rsx->context, rsx->pos_index[RSX_SHADER_MENU]->index, 0,
         rsx->pos_offset[RSX_SHADER_MENU], sizeof(rsx_vertex_t), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, rsx->uv_index[RSX_SHADER_MENU]->index, 0,
         rsx->uv_offset[RSX_SHADER_MENU], sizeof(rsx_vertex_t), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, rsx->col_index[RSX_SHADER_MENU]->index, 0,
         rsx->col_offset[RSX_SHADER_MENU], sizeof(rsx_vertex_t), 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

   rsxLoadVertexProgram(rsx->context, rsx->vpo[RSX_SHADER_MENU], rsx->vp_ucode[RSX_SHADER_MENU]);
   rsxSetVertexProgramParameter(rsx->context, rsx->vpo[RSX_SHADER_MENU], rsx->proj_matrix[RSX_SHADER_MENU], (float *)&rsx->mvp);
   rsxLoadFragmentProgramLocation(rsx->context, rsx->fpo[RSX_SHADER_MENU], rsx->fp_offset[RSX_SHADER_MENU], GCM_LOCATION_RSX);

   rsxClearSurface(rsx->context, GCM_CLEAR_Z);
   rsxDrawVertexArray(rsx->context, GCM_TYPE_TRIANGLE_STRIP, 0, 4);
}

#if defined(HAVE_MENU)
static void rsx_draw_menu_vertices(rsx_t* rsx)
{
   rsx_vertex_t *vertices      = NULL;
   int end_vert_idx            = rsx->vert_idx + 4;
   if (end_vert_idx > RSX_MAX_VERTICES)
   {
      rsx->vert_idx            = 0;
      end_vert_idx             = rsx->vert_idx + 4;
   }
   vertices                    = &rsx->vertices[rsx->vert_idx];

   vertices[rsx->vert_idx+0].x = 0.0f;
   vertices[rsx->vert_idx+0].y = 0.0f;
   vertices[rsx->vert_idx+0].u = 0.0f;
   vertices[rsx->vert_idx+0].v = 1.0f;
   vertices[rsx->vert_idx+0].r = 1.0f;
   vertices[rsx->vert_idx+0].g = 1.0f;
   vertices[rsx->vert_idx+0].b = 1.0f;
   vertices[rsx->vert_idx+0].a = rsx->menu_texture_alpha;

   vertices[rsx->vert_idx+1].x = 1.0f;
   vertices[rsx->vert_idx+1].y = 0.0f;
   vertices[rsx->vert_idx+1].u = 1.0f;
   vertices[rsx->vert_idx+1].v = 1.0f;
   vertices[rsx->vert_idx+1].r = 1.0f;
   vertices[rsx->vert_idx+1].g = 1.0f;
   vertices[rsx->vert_idx+1].b = 1.0f;
   vertices[rsx->vert_idx+1].a = rsx->menu_texture_alpha;

   vertices[rsx->vert_idx+2].x = 0.0f;
   vertices[rsx->vert_idx+2].y = 1.0f;
   vertices[rsx->vert_idx+2].u = 0.0f;
   vertices[rsx->vert_idx+2].v = 0.0f;
   vertices[rsx->vert_idx+2].r = 1.0f;
   vertices[rsx->vert_idx+2].g = 1.0f;
   vertices[rsx->vert_idx+2].b = 1.0f;
   vertices[rsx->vert_idx+2].a = rsx->menu_texture_alpha;

   vertices[rsx->vert_idx+3].x = 1.0f;
   vertices[rsx->vert_idx+3].y = 1.0f;
   vertices[rsx->vert_idx+3].u = 1.0f;
   vertices[rsx->vert_idx+3].v = 0.0f;
   vertices[rsx->vert_idx+3].r = 1.0f;
   vertices[rsx->vert_idx+3].g = 1.0f;
   vertices[rsx->vert_idx+3].b = 1.0f;
   vertices[rsx->vert_idx+3].a = rsx->menu_texture_alpha;

   rsxAddressToOffset(&vertices[rsx->vert_idx].x, &rsx->pos_offset[RSX_SHADER_STOCK_BLEND]);
   rsxAddressToOffset(&vertices[rsx->vert_idx].u, &rsx->uv_offset[RSX_SHADER_STOCK_BLEND]);
   rsxAddressToOffset(&vertices[rsx->vert_idx].r, &rsx->col_offset[RSX_SHADER_STOCK_BLEND]);
   rsx->vert_idx               = end_vert_idx;

   rsxBindVertexArrayAttrib(rsx->context, rsx->pos_index[RSX_SHADER_STOCK_BLEND]->index, 0,
         rsx->pos_offset[RSX_SHADER_STOCK_BLEND], sizeof(rsx_vertex_t), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, rsx->uv_index[RSX_SHADER_STOCK_BLEND]->index, 0,
         rsx->uv_offset[RSX_SHADER_STOCK_BLEND], sizeof(rsx_vertex_t), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, rsx->col_index[RSX_SHADER_STOCK_BLEND]->index, 0,
         rsx->col_offset[RSX_SHADER_STOCK_BLEND], sizeof(rsx_vertex_t), 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

   rsxLoadVertexProgram(rsx->context, rsx->vpo[RSX_SHADER_STOCK_BLEND], rsx->vp_ucode[RSX_SHADER_STOCK_BLEND]);
   rsxSetVertexProgramParameter(rsx->context, rsx->vpo[RSX_SHADER_STOCK_BLEND], rsx->proj_matrix[RSX_SHADER_STOCK_BLEND], (float *)&rsx->mvp_no_rot);
   rsxLoadFragmentProgramLocation(rsx->context, rsx->fpo[RSX_SHADER_STOCK_BLEND], rsx->fp_offset[RSX_SHADER_STOCK_BLEND], GCM_LOCATION_RSX);

   rsxSetBlendEnable(rsx->context, GCM_TRUE);
   rsxSetBlendFunc(rsx->context, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
   rsxSetBlendEquation(rsx->context, GCM_FUNC_ADD, GCM_FUNC_ADD);

   rsxClearSurface(rsx->context, GCM_CLEAR_Z);
   rsxDrawVertexArray(rsx->context, GCM_TYPE_TRIANGLE_STRIP, 0, 4);

   rsxSetBlendEnable(rsx->context, GCM_FALSE);
}
#endif

#ifdef HAVE_OVERLAY
static void rsx_draw_overlay_vertices(rsx_t* rsx, unsigned image)
{
   rsx_vertex_t *vertices = &rsx->overlay[image].vertices[0];

   rsxAddressToOffset(&vertices[0].x, &rsx->pos_offset[RSX_SHADER_STOCK_BLEND]);
   rsxAddressToOffset(&vertices[0].u, &rsx->uv_offset[RSX_SHADER_STOCK_BLEND]);
   rsxAddressToOffset(&vertices[0].r, &rsx->col_offset[RSX_SHADER_STOCK_BLEND]);

   rsxBindVertexArrayAttrib(rsx->context, rsx->pos_index[RSX_SHADER_STOCK_BLEND]->index, 0,
         rsx->pos_offset[RSX_SHADER_STOCK_BLEND], sizeof(rsx_vertex_t), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, rsx->uv_index[RSX_SHADER_STOCK_BLEND]->index, 0,
         rsx->uv_offset[RSX_SHADER_STOCK_BLEND], sizeof(rsx_vertex_t), 2, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);
   rsxBindVertexArrayAttrib(rsx->context, rsx->col_index[RSX_SHADER_STOCK_BLEND]->index, 0,
         rsx->col_offset[RSX_SHADER_STOCK_BLEND], sizeof(rsx_vertex_t), 4, GCM_VERTEX_DATA_TYPE_F32, GCM_LOCATION_RSX);

   rsxLoadVertexProgram(rsx->context, rsx->vpo[RSX_SHADER_STOCK_BLEND], rsx->vp_ucode[RSX_SHADER_STOCK_BLEND]);
   rsxSetVertexProgramParameter(rsx->context, rsx->vpo[RSX_SHADER_STOCK_BLEND], rsx->proj_matrix[RSX_SHADER_STOCK_BLEND], (float *)&rsx->mvp_no_rot);
   rsxLoadFragmentProgramLocation(rsx->context, rsx->fpo[RSX_SHADER_STOCK_BLEND], rsx->fp_offset[RSX_SHADER_STOCK_BLEND], GCM_LOCATION_RSX);

   rsxSetBlendEnable(rsx->context, GCM_TRUE);
   rsxSetBlendFunc(rsx->context, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA, GCM_SRC_ALPHA, GCM_ONE_MINUS_SRC_ALPHA);
   rsxSetBlendEquation(rsx->context, GCM_FUNC_ADD, GCM_FUNC_ADD);

   rsxClearSurface(rsx->context, GCM_CLEAR_Z);
   rsxDrawVertexArray(rsx->context, GCM_TYPE_TRIANGLE_STRIP, 0, 4);

   rsxSetBlendEnable(rsx->context, GCM_FALSE);
}

static void rsx_overlay_vertex_geom(void *data,
      unsigned image,
      float x, float y,
      float w, float h)
{
   rsx_t              *rsx = (rsx_t *)data;
   rsx_overlay_t *o = NULL;

   if (rsx)
      o = (rsx_overlay_t *)&rsx->overlay[image];

   if (!o)
      return;

   /* Flipped, so we preserve top-down semantics. */
   y                      = 1.0f - y;
   h                      = -h;

   o->vertices[0].x       = x;
   o->vertices[0].y       = y;
   o->vertices[1].x       = x + w;
   o->vertices[1].y       = y;
   o->vertices[2].x       = x;
   o->vertices[2].y       = y + h;
   o->vertices[3].x       = x + w;
   o->vertices[3].y       = y + h;
}

static void rsx_overlay_tex_geom(void *data,
      unsigned image,
      float x, float y,
      float w, float h)
{
   rsx_t              *rsx = (rsx_t *)data;
   rsx_overlay_t *o = NULL;

   if (rsx)
      o = (rsx_overlay_t *)&rsx->overlay[image];

   if (!o)
      return;

   o->vertices[0].u       = x;
   o->vertices[0].v       = y;
   o->vertices[1].u       = x + w;
   o->vertices[1].v       = y;
   o->vertices[2].u       = x;
   o->vertices[2].v       = y + h;
   o->vertices[3].u       = x + w;
   o->vertices[3].v       = y + h;
}

static void rsx_free_overlay(rsx_t *rsx)
{
   unsigned i;

   for (i = 0; i < rsx->overlays; i++)
   {
      if (rsx->overlay[i].texture.data)
         rsxFree(rsx->overlay[i].texture.data);
      if (rsx->overlay[i].vertices)
         rsxFree(rsx->overlay[i].vertices);
   }

   free(rsx->overlay);
   rsx->overlay = NULL;
   rsx->overlays = 0;
}

static bool rsx_overlay_load(void *data,
      const void *image_data, unsigned num_images)
{
   unsigned i, j;
   rsx_t *rsx = (rsx_t *)data;
   const struct texture_image *images = (const struct texture_image *)image_data;

   rsx_free_overlay(rsx);
   rsx->overlay = (rsx_overlay_t *)calloc(num_images, sizeof(rsx_overlay_t));

   if (!rsx->overlay)
      return false;

   rsx->overlays = num_images;

   for (i = 0; i < num_images; i++)
   {
      rsx_overlay_t *o = (rsx_overlay_t *)&rsx->overlay[i];
      o->vertices = (rsx_vertex_t *)rsxMemalign(128, sizeof(rsx_vertex_t) * RSX_MAX_VERTICES);

      /* Default. Stretch to whole screen. */
      rsx_overlay_tex_geom(rsx, i, 0, 0, 1, 1);
      rsx_overlay_vertex_geom(rsx, i, 0, 0, 1, 1);
      for (j = 0; j < RSX_MAX_VERTICES; j++)
      {
         o->vertices[j].r = 1.0f;
         o->vertices[j].g = 1.0f;
         o->vertices[j].b = 1.0f;
         o->vertices[j].a = 1.0f;
      }
      o->texture.data = NULL;
      o->texture.height = images[i].height;
      o->texture.width = images[i].width;
      rsx_load_texture_data(rsx, &o->texture, images[i].pixels, images[i].width, images[i].height, images[i].width*4,
                            true, false, TEXTURE_FILTER_LINEAR);
   }

   return true;
}

static void rsx_overlay_enable(void *data, bool state)
{
   rsx_t *rsx = (rsx_t *)data;
   rsx->overlay_enable = state;
}

static void rsx_overlay_full_screen(void *data, bool enable)
{
   rsx_t *rsx = (rsx_t *)data;
   rsx->overlay_full_screen = enable;
}

static void rsx_overlay_set_alpha(void *data, unsigned image, float mod)
{
   rsx_t *rsx = (rsx_t *)data;

   if (rsx)
   {
      rsx->overlay[image].vertices[0].a = mod;
      rsx->overlay[image].vertices[1].a = mod;
      rsx->overlay[image].vertices[2].a = mod;
      rsx->overlay[image].vertices[3].a = mod;
   }
}

static void rsx_render_overlay(void *data)
{
   unsigned i;

   rsx_t *rsx = (rsx_t *)data;

   rsx_set_viewport(rsx, rsx->width, rsx->height, true, true);

   for (i = 0; i < rsx->overlays; i++)
   {
      rsx_set_texture(rsx, &rsx->overlay[i].texture);
      rsx_draw_overlay_vertices(rsx, i);
   }
}

static const video_overlay_interface_t rsx_overlay_interface =
{
   rsx_overlay_enable,
   rsx_overlay_load,
   rsx_overlay_tex_geom,
   rsx_overlay_vertex_geom,
   rsx_overlay_full_screen,
   rsx_overlay_set_alpha,
};

static void rsx_get_overlay_interface(void *data,
      const video_overlay_interface_t **iface) {*iface = &rsx_overlay_interface;}
#endif

static void rsx_update_screen(rsx_t* gcm)
{
   rsxBuffer *buffer     = NULL;
#if defined(HAVE_MENU_BUFFER)
   if (gcm->menu_frame_enable)
   {
      buffer             = &gcm->menuBuffers[gcm->menuBuffer];
      gcm->menuBuffer    = (gcm->menuBuffer + 1) % RSX_MAX_MENU_BUFFERS;
      gcm->nextBuffer    = RSX_MAX_BUFFERS + gcm->menuBuffer;
   }
   else
#endif
   {
      buffer             = &gcm->buffers[gcm->currentBuffer];
      gcm->currentBuffer = (gcm->currentBuffer + 1) % RSX_MAX_BUFFERS;
      gcm->nextBuffer    = gcm->currentBuffer;
   }

   rsx_flip(gcm->context, buffer->id);
   if (gcm->vsync)
      rsx_wait_flip();
   rsxSetSurface(gcm->context, &gcm->surface[gcm->nextBuffer]);
#ifdef HAVE_SYSUTILS
   cellSysutilCheckCallback();
#endif
}

static bool rsx_frame(void* data, const void* frame,
      unsigned width, unsigned height,
      uint64_t frame_count,
      unsigned pitch, const char* msg, video_frame_info_t *video_info)
{
   rsx_viewport_t vp;
   rsx_t *gcm                       = (rsx_t*)data;
#ifdef HAVE_MENU
   bool statistics_show             = video_info->statistics_show;
   struct font_params *osd_params   = (struct font_params*)
      &video_info->osd_stat_params;
   bool menu_is_alive               = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active              = video_info->widgets_active;
#endif
   bool draw                        = false;

   if (gcm->should_resize)
      rsx_update_viewport(gcm);

   vp.min                           = 0.0f;
   vp.max                           = 1.0f;
   vp.x                             = gcm->vp.x;
   vp.y                             = gcm->height - gcm->vp.y - gcm->vp.height;
   vp.w                             = gcm->vp.width;
   vp.h                             = gcm->vp.height;
   vp.scale[0]                      = vp.w *  0.5f;
   vp.scale[1]                      = vp.h * -0.5f;
   vp.scale[2]                      = (vp.max - vp.min) * 0.5f;
   vp.scale[3]                      = 0.0f;
   vp.offset[0]                     = vp.x + vp.w * 0.5f;
   vp.offset[1]                     = vp.y + vp.h * 0.5f;
   vp.offset[2]                     = (vp.max + vp.min) * 0.5f;
   vp.offset[3]                     = 0.0f;
   rsxSetViewport(gcm->context, vp.x, vp.y, vp.w, vp.h, vp.min, vp.max, vp.scale, vp.offset);

   if (frame && width && height)
   {
      gcm->tex_index  = ((gcm->tex_index + 1) % RSX_MAX_TEXTURES);
      rsx_load_texture_data(gcm,
            &gcm->texture[gcm->tex_index],
            frame, width, height, pitch, gcm->rgb32, false,
            gcm->smooth
            ? TEXTURE_FILTER_LINEAR
            : TEXTURE_FILTER_NEAREST);
      /* TODO/FIXME - pipeline ID being used here is RSX_SHADER_MENU,
       * shouldn't this be RSX_SHADER_STOCK_BLEND instead? */
      rsx_set_texture(gcm, &gcm->texture[gcm->tex_index]);
      rsx_draw_vertices(gcm);
      draw = true;
   }

#ifdef HAVE_MENU
   if (gcm->menu_frame_enable && menu_is_alive)
   {
      menu_driver_frame(menu_is_alive, video_info);
      if (gcm->menu_texture.data)
      {
         /* TODO/FIXME - pipeline ID being used here
          * is RSX_SHADER_STOCK_BLEND, shouldn't
          * this be RSX_SHADER_MENU instead? */
         rsx_set_menu_texture(gcm, &gcm->menu_texture);
         rsx_draw_menu_vertices(gcm);
         draw = true;
      }
   };

   if (statistics_show)
      if (osd_params)
         font_driver_render_msg(gcm,
               video_info->stat_text,
               osd_params, NULL);
#endif

#ifdef HAVE_OVERLAY
   if (gcm->overlay_enable)
      rsx_render_overlay(gcm);
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

   if (msg)
      font_driver_render_msg(gcm, msg, NULL, NULL);

#if 0
   /* TODO: translucid menu */
#endif
   if (draw)
   {
      /* Only update when we draw to prevent flickering when core frame duping is enabled */
      rsx_update_screen(gcm);
      rsx_clear_surface(gcm);
   }
   gcm->vert_idx         = 0;
   gcm->texture_vert_idx = 0;
   gcm->font_vert_idx    = 0;

   return true;
}

static void rsx_set_nonblock_state(void* data, bool toggle,
      bool a, unsigned b)
{
   rsx_t* gcm = (rsx_t*)data;
   if (gcm)
      gcm->vsync = !toggle;
}

static bool rsx_alive(void* data) { return true; }
static bool rsx_focus(void* data) { return true; }
static bool rsx_suppress_screensaver(void* data, bool enable) { return false; }

static void rsx_free(void* data)
{
   rsx_t* gcm = (rsx_t*)data;

   if (!gcm)
      return;

   rsxClearSurface(gcm->context, GCM_CLEAR_Z);
   gcmSetWaitFlip(gcm->context);
#if 0
   /* TODO fix crash on loading core */
   if (gcm->vertices)
      rsxFree(gcm->vertices);
   if (gcm->texture_vertices)
      rsxFree(gcm->texture_vertices);
   for (i = 0; i < RSX_MAX_TEXTURES; i++)
   {
      if (gcm->texture[i].data)
         rsxFree(gcm->texture[i].data);
   }
   if (gcm->menu_texture.data)
     rsxFree(gcm->menu_texture.data);
   for (i = 0; i < RSX_MAX_BUFFERS; i++)
     rsxFree(gcm->buffers[i].ptr);
#if defined(HAVE_MENU_BUFFER)
   for (i = 0; i < RSX_MAX_MENU_BUFFERS; i++)
     rsxFree(gcm->menuBuffers[i].ptr);
#endif
   if (gcm->depth_buffer)
     rsxFree(gcm->depth_buffer);
   if (gcm->fp_buffer)
     rsxFree(gcm->fp_buffer);

   rsxFinish(gcm->context, 1);
   free(gcm->host_addr);
#endif
   free (gcm);
}

static void rsx_set_texture_frame(void* data, const void* frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   rsx_t* gcm              = (rsx_t*)data;
   gcm->menu_texture_alpha = alpha;
   gcm->menu_width         = width;
   gcm->menu_height        = height;
   rsx_load_texture_data(gcm, &gcm->menu_texture, frame, width, height, width * (rgb32 ? 4 : 2),
                         rgb32, true, gcm->smooth ? TEXTURE_FILTER_LINEAR : TEXTURE_FILTER_NEAREST);
}

static void rsx_set_texture_enable(void* data, bool state, bool full_screen)
{
   rsx_t* gcm = (rsx_t*)data;
   if (gcm)
      gcm->menu_frame_enable = state;
}

static void rsx_set_rotation(void* data, unsigned rotation)
{
   rsx_t* gcm               = (rsx_t*)data;
   struct video_ortho ortho = {0, 1, 0, 1, -1, 1};

   if (!gcm)
      return;

   gcm->rotation            = 90 * rotation;
   gcm->should_resize       = true;
   rsx_set_projection(gcm, &ortho, true);
}

static void rsx_set_filtering(void* data, unsigned index, bool smooth, bool ctx_scaling)
{
   rsx_t* gcm = (rsx_t*)data;

   if (gcm)
      gcm->smooth = smooth;
}

static void rsx_set_aspect_ratio(void* data, unsigned aspect_ratio_idx)
{
   rsx_t* gcm         = (rsx_t*)data;

   if (!gcm)
      return;

   gcm->keep_aspect   = true;
   gcm->should_resize = true;
}

static void rsx_apply_state_changes(void* data)
{
   rsx_t* gcm = (rsx_t*)data;
   if (gcm)
      gcm->should_resize = true;
}

static void rsx_viewport_info(void* data, struct video_viewport* vp)
{
   rsx_t* gcm = (rsx_t*)data;
   if (gcm)
      *vp = gcm->vp;
}

#if 0
/* TODO/FIXME - does this function have to be hooked up as a function callback
 * or can it be removed? */
static void rsx_set_osd_msg(void *data,
      video_frame_info_t *video_info,
      const char *msg,
      const struct font_params *params, void *font)
{
   rsx_t* gcm = (rsx_t*)data;
   if (gcm && gcm->msg_rendering_enabled)
      font_driver_render_msg(data, msg, params, font);
}
#endif

static uint32_t rsx_get_flags(void *data) { return 0; }

static const video_poke_interface_t rsx_poke_interface = {
   rsx_get_flags,
   rsx_load_texture,
   rsx_unload_texture,
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   rsx_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   rsx_set_aspect_ratio,
   rsx_apply_state_changes,
   rsx_set_texture_frame,
   rsx_set_texture_enable,
   font_driver_render_msg,
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   NULL, /* get_current_shader */
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
};

static void rsx_get_poke_interface(void* data,
      const video_poke_interface_t** iface) { *iface = &rsx_poke_interface; }

static bool rsx_set_shader(void* data,
      enum rarch_shader_type type, const char* path) { return false; }

#ifdef HAVE_GFX_WIDGETS
static bool rsx_widgets_enabled(void *data)          { return true;  }
#endif

video_driver_t video_gcm =
{
   rsx_init,
   rsx_frame,
   rsx_set_nonblock_state,
   rsx_alive,
   rsx_focus,
   rsx_suppress_screensaver,
   NULL, /* has_windowed */
   rsx_set_shader,
   rsx_free,
   "rsx",
   rsx_set_viewport,
   rsx_set_rotation,
   rsx_viewport_info,
   NULL, /* read_viewport */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   rsx_get_overlay_interface,
#endif
   rsx_get_poke_interface,
   rsx_wrap_type_to_enum,
#ifdef HAVE_GFX_WIDGETS
   rsx_widgets_enabled
#endif
};

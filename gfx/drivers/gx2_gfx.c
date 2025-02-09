/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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

#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include <wiiu/os.h>
#include <wiiu/gx2.h>

#include <encodings/utf.h>
#include <formats/image.h>
#include <file/file_path.h>
#include <string/stdstring.h>

#include "../../driver.h"
#include "../../configuration.h"
#include "../../verbosity.h"
#include "../../retroarch.h"

#ifdef HAVE_REWIND
#include "../../state_manager.h"
#endif

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#ifdef HAVE_MENU
#include "../../menu/menu_driver.h"
#endif
#ifdef HAVE_GFX_WIDGETS
#include "../gfx_widgets.h"
#endif

#include "../common/gx2_defines.h"
#include "../video_shader_parse.h"
#include "../drivers_shader/slang_process.h"
#include "../../wiiu/system/memory.h"
#include "../../wiiu/wiiu_dbg.h"

#include "../font_driver.h"

typedef struct
{
   GX2Texture texture;
   GX2_vec2* ubo_tex;
   const font_renderer_driver_t* font_driver;
   void* font_data;
   struct font_atlas* atlas;
} gx2_font_t;

/* Temporary workaround for GX2 not being able to poll flags during init */
static gfx_ctx_driver_t gx2_fake_context;

static const wiiu_render_mode_t gx2_render_mode_map[] =
{
   {0},                                         /* GX2_TV_SCAN_MODE_NONE  */
   {854,  480,  GX2_TV_RENDER_MODE_WIDE_480P},  /* GX2_TV_SCAN_MODE_576I  */
   {854,  480,  GX2_TV_RENDER_MODE_WIDE_480P},  /* GX2_TV_SCAN_MODE_480I  */
   {854,  480,  GX2_TV_RENDER_MODE_WIDE_480P},  /* GX2_TV_SCAN_MODE_480P  */
   {1280, 720,  GX2_TV_RENDER_MODE_WIDE_720P},  /* GX2_TV_SCAN_MODE_720P  */
   {0},                                         /* GX2_TV_SCAN_MODE_unk   */
   {1920, 1080, GX2_TV_RENDER_MODE_WIDE_1080P}, /* GX2_TV_SCAN_MODE_1080I */
   {1920, 1080, GX2_TV_RENDER_MODE_WIDE_1080P}  /* GX2_TV_SCAN_MODE_1080P */
};

static void gx2_free_shader_preset(wiiu_video_t *wiiu)
{
   int i;

   for (i = 0; i < wiiu->shader_preset->passes; i++)
   {
      gfd_free(wiiu->pass[i].gfd);
      MEM2_free(wiiu->pass[i].vs_ubos[0]);
      MEM2_free(wiiu->pass[i].vs_ubos[1]);
      MEM2_free(wiiu->pass[i].ps_ubos[0]);
      MEM2_free(wiiu->pass[i].ps_ubos[1]);
      if (wiiu->pass[i].mem1)
         MEM1_free(wiiu->pass[i].texture.surface.image);
      else
         MEM2_free(wiiu->pass[i].texture.surface.image);
   }

   memset(wiiu->pass, 0, sizeof(wiiu->pass));

   for (i = 0; i < wiiu->shader_preset->luts; i++)
   {
      MEM2_free(wiiu->luts[i].surface.image);
      wiiu->luts[i].surface.image = NULL;
   }

   free(wiiu->shader_preset);
   wiiu->shader_preset = NULL;
}

static bool gx2_set_shader(void *data,
      enum rarch_shader_type type, const char *path)
{
   unsigned i;
   wiiu_video_t *wiiu  = (wiiu_video_t *)data;

   if (!wiiu)
      return false;

   GX2DrawDone();
   if (wiiu->shader_preset)
      gx2_free_shader_preset(wiiu);

   if (!string_is_empty(path))
   {
      if (type != RARCH_SHADER_SLANG)
      {
         RARCH_WARN("[GX2] Only Slang shaders are supported. Falling back to stock.\n");
         return false;
      }

      wiiu->shader_preset = calloc(1, sizeof(*wiiu->shader_preset));

      if (!video_shader_load_preset_into_shader(path, wiiu->shader_preset))
      {
         free(wiiu->shader_preset);
         wiiu->shader_preset = NULL;
         return false;
      }

      for (i = 0; i < wiiu->shader_preset->passes; i++)
      {
         unsigned j;
         char *ptr;
         char gfdpath[PATH_MAX_LENGTH];
         struct video_shader_pass *pass = &wiiu->shader_preset->pass[i];

         strlcpy(gfdpath, pass->source.path, sizeof(gfdpath));

         if (!(ptr = strrchr(gfdpath, '.')))
            ptr = gfdpath + strlen(gfdpath);

         *ptr++ = '.';
         *ptr++ = 'g';
         *ptr++ = 's';
         *ptr++ = 'h';
         *ptr++ = '\0';

         wiiu->pass[i].gfd = gfd_open(gfdpath);

         if (!wiiu->pass[i].gfd)
         {
            if (wiiu->shader_preset)
               gx2_free_shader_preset(wiiu);
            return false;
         }

         for (j = 0; j < 2 && j < wiiu->pass[i].gfd->vs->uniformBlockCount; j++)
         {
            wiiu->pass[i].vs_ubos[j] = MEM2_alloc(wiiu->pass[i].gfd->vs->uniformBlocks[j].size,
                  GX2_UNIFORM_BLOCK_ALIGNMENT);
            memset(wiiu->pass[i].vs_ubos[j], 0, wiiu->pass[i].gfd->vs->uniformBlocks[j].size);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_UNIFORM_BLOCK, wiiu->pass[i].vs_ubos[j],
                  wiiu->pass[i].gfd->vs->uniformBlocks[j].size);
         }

         for (j = 0; j < 2 && j < wiiu->pass[i].gfd->ps->uniformBlockCount; j++)
         {
            wiiu->pass[i].ps_ubos[j] = MEM2_alloc(wiiu->pass[i].gfd->ps->uniformBlocks[j].size,
                  GX2_UNIFORM_BLOCK_ALIGNMENT);
            memset(wiiu->pass[i].ps_ubos[j], 0, wiiu->pass[i].gfd->ps->uniformBlocks[j].size);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_UNIFORM_BLOCK, wiiu->pass[i].ps_ubos[j],
                  wiiu->pass[i].gfd->ps->uniformBlocks[j].size);
         }
      }

      for (i = 0; i < wiiu->shader_preset->luts; i++)
      {
         struct texture_image image;
         image.pixels               = NULL;
         image.width                = 0;
         image.height               = 0;
         image.supports_rgba        = false;

         if (image_texture_load(&image, wiiu->shader_preset->lut[i].path))
         {
            unsigned j;

            wiiu->luts[i].surface.width       = image.width;
            wiiu->luts[i].surface.height      = image.height;
            wiiu->luts[i].surface.depth       = 1;
            wiiu->luts[i].surface.dim         = GX2_SURFACE_DIM_TEXTURE_2D;
            wiiu->luts[i].surface.tileMode    = GX2_TILE_MODE_LINEAR_ALIGNED;
            wiiu->luts[i].viewNumSlices       = 1;

            wiiu->luts[i].surface.format      = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
            wiiu->luts[i].compMap             = GX2_COMP_SEL(_G, _B, _A, _R);

            GX2CalcSurfaceSizeAndAlignment(&wiiu->luts[i].surface);
            GX2InitTextureRegs(&wiiu->luts[i]);
            wiiu->luts[i].surface.image = MEM2_alloc(wiiu->luts[i].surface.imageSize,
                  wiiu->luts[i].surface.alignment);

            for (j = 0; (j < image.height) && (j < wiiu->luts[i].surface.height); j++)
               memcpy((uint32_t *)wiiu->luts[i].surface.image + (j * wiiu->luts[i].surface.pitch),
                     image.pixels + (j * image.width), image.width * sizeof(image.pixels));

            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, wiiu->luts[i].surface.image,
                  wiiu->luts[i].surface.imageSize);

            image_texture_free(&image);
         }
      }
   }

   return true;
}

/*
 * DISPLAY DRIVER
 */

static void gfx_display_wiiu_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   wiiu_video_t             *wiiu  = (wiiu_video_t*)data;

   if (!wiiu || !draw)
      return;

   if (draw->pipeline_id)
   {
      GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);

      switch(draw->pipeline_id)
      {
         case VIDEO_SHADER_MENU:
            GX2SetShader(&ribbon_shader);
            break;
         case VIDEO_SHADER_MENU_2:
            GX2SetShader(&ribbon_simple_shader);
            break;
         case VIDEO_SHADER_MENU_3:
            GX2SetShader(&snow_simple_shader);
            break;
         case VIDEO_SHADER_MENU_4:
            GX2SetShader(&snow_shader);
            break;
         case VIDEO_SHADER_MENU_5:
            GX2SetShader(&bokeh_shader);
            break;
         case VIDEO_SHADER_MENU_6:
            GX2SetShader(&snowflake_shader);
            break;
         default:
            break;
      }

      switch(draw->pipeline_id)
      {
         case VIDEO_SHADER_MENU:
         case VIDEO_SHADER_MENU_2:
            GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, draw->coords->vertices, 0, 1);
            GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
                  GX2_BLEND_COMBINE_MODE_ADD,
                  GX2_ENABLE,          GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
                  GX2_BLEND_COMBINE_MODE_ADD);
         case VIDEO_SHADER_MENU_3:
         case VIDEO_SHADER_MENU_4:
         case VIDEO_SHADER_MENU_5:
         case VIDEO_SHADER_MENU_6:
            GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);
            break;
      }

   }
   /* TODO come up with a better check for "not all vertexes are the same color" */
   else if (draw->coords->vertex || draw->coords->color[0] != draw->coords->color[12])
   {
      int i;
      if (wiiu->vertex_cache_tex.current + 4 > wiiu->vertex_cache_tex.size)
         return;

      tex_shader_vertex_t* v = wiiu->vertex_cache_tex.v + wiiu->vertex_cache_tex.current;

      GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
      GX2SetShader(&tex_shader);
      GX2SetVertexUniformBlock(tex_shader.vs.uniformBlocks[0].offset,
            tex_shader.vs.uniformBlocks[0].size,
            wiiu->ubo_mvp);
      GX2SetAttribBuffer(0, wiiu->vertex_cache_tex.size * sizeof(*wiiu->vertex_cache_tex.v),
            sizeof(*wiiu->vertex_cache_tex.v), wiiu->vertex_cache_tex.v);

      if (!draw->coords->vertex)
      {
         /* Convert the libretro bottom-up coordinate system to GX2 - low y at
            the top of the screen, large y at the bottom
            The compiler will optimise 90% of this out anyway */
         float y      = -(draw->y + draw->height - video_height);
         /* Remember: this is a triangle strip, not a quad, draw in a Z shape
            Bottom-left, right, top-left, right */
         v[0].pos.x   = (draw->x               ) / video_width;
         v[0].pos.y   = (y       + draw->height) / video_height;
         v[1].pos.x   = (draw->x + draw->width ) / video_width;
         v[1].pos.y   = (y       + draw->height) / video_height;
         v[2].pos.x   = (draw->x               ) / video_width;
         v[2].pos.y   = (y                     ) / video_height;
         v[3].pos.x   = (draw->x + draw->width ) / video_width;
         v[3].pos.y   = (y                     ) / video_height;
      }
      else
      {
         v[0].pos.x   = draw->coords->vertex[0];
         v[0].pos.y   = 1.0 - draw->coords->vertex[1];
         v[1].pos.x   = draw->coords->vertex[2];
         v[1].pos.y   = 1.0 - draw->coords->vertex[3];
         v[2].pos.x   = draw->coords->vertex[4];
         v[2].pos.y   = 1.0 - draw->coords->vertex[5];
         v[3].pos.x   = draw->coords->vertex[6];
         v[3].pos.y   = 1.0 - draw->coords->vertex[7];
      }

      if (!draw->coords->tex_coord)
      {
         v[0].coord.u = 0.0f;
         v[0].coord.v = 1.0f;
         v[1].coord.u = 1.0f;
         v[1].coord.v = 1.0f;
         v[2].coord.u = 0.0f;
         v[2].coord.v = 0.0f;
         v[3].coord.u = 1.0f;
         v[3].coord.v = 0.0f;
      }
      else
      {
         v[0].coord.u = draw->coords->tex_coord[0];
         v[0].coord.v = draw->coords->tex_coord[1];
         v[1].coord.u = draw->coords->tex_coord[2];
         v[1].coord.v = draw->coords->tex_coord[3];
         v[2].coord.u = draw->coords->tex_coord[4];
         v[2].coord.v = draw->coords->tex_coord[5];
         v[3].coord.u = draw->coords->tex_coord[6];
         v[3].coord.v = draw->coords->tex_coord[7];
      }

      for (i = 0; i < 4; i++)
      {
         v[i].color.r = draw->coords->color[(i << 2) + 0];
         v[i].color.g = draw->coords->color[(i << 2) + 1];
         v[i].color.b = draw->coords->color[(i << 2) + 2];
         v[i].color.a = draw->coords->color[(i << 2) + 3];
      }

      if (draw->texture)
         GX2SetPixelTexture((GX2Texture*)draw->texture, tex_shader.ps.samplerVars[0].location);

      GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, 4, wiiu->vertex_cache_tex.current, 1);
      wiiu->vertex_cache_tex.current += 4;
   }
   else
   {
      sprite_vertex_t* v;
      if (wiiu->vertex_cache.current + 1 > wiiu->vertex_cache.size)
         return;

      v                  = wiiu->vertex_cache.v + wiiu->vertex_cache.current;
      v->pos.x           = draw->x;
      v->pos.y           = wiiu->color_buffer.surface.height -
                           draw->y - draw->height;
      v->pos.width       = draw->width;
      v->pos.height      = draw->height;
      v->coord.u         = 0.0f;
      v->coord.v         = 0.0f;
      v->coord.width     = 1.0f;
      v->coord.height    = 1.0f;

      v->color           = COLOR_RGBA(
            0xFF * draw->coords->color[0], 0xFF * draw->coords->color[1],
            0xFF * draw->coords->color[2], 0xFF * draw->coords->color[3]);

      if (draw->texture)
         GX2SetPixelTexture((GX2Texture*)draw->texture, sprite_shader.ps.samplerVars[0].location);

      GX2DrawEx(GX2_PRIMITIVE_MODE_POINTS, 1, wiiu->vertex_cache.current, 1);
      wiiu->vertex_cache.current ++;
      return;
   }

   GX2SetShaderMode(GX2_SHADER_MODE_GEOMETRY_SHADER);
   GX2SetShader(&sprite_shader);
#if 0
   GX2SetGeometryShaderInputRingBuffer(wiiu->input_ring_buffer,
         wiiu->input_ring_buffer_size);
   GX2SetGeometryShaderOutputRingBuffer(wiiu->output_ring_buffer,
         wiiu->output_ring_buffer_size);
#endif
   GX2SetVertexUniformBlock(sprite_shader.vs.uniformBlocks[0].offset,
         sprite_shader.vs.uniformBlocks[0].size,
         wiiu->ubo_vp);
   GX2SetVertexUniformBlock(sprite_shader.vs.uniformBlocks[1].offset,
         sprite_shader.vs.uniformBlocks[1].size,
         wiiu->ubo_tex);
   GX2SetAttribBuffer(0, wiiu->vertex_cache.size
         * sizeof(*wiiu->vertex_cache.v),
         sizeof(*wiiu->vertex_cache.v),
         wiiu->vertex_cache.v);
}

static void gfx_display_wiiu_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data, unsigned video_width, unsigned video_height)
{
   video_coord_array_t *ca        = NULL;
   wiiu_video_t             *wiiu = (wiiu_video_t*)data;

   if (!wiiu || !draw)
      return;

   switch(draw->pipeline_id)
   {
      case VIDEO_SHADER_MENU:
      case VIDEO_SHADER_MENU_2:
         ca = &p_disp->dispca;
         if (!wiiu->menu_shader_vbo)
         {
            wiiu->menu_shader_vbo = MEM2_alloc(ca->coords.vertices * 2 * sizeof(float), GX2_VERTEX_BUFFER_ALIGNMENT);
            memcpy(wiiu->menu_shader_vbo, ca->coords.vertex, ca->coords.vertices * 2 * sizeof(float));
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, wiiu->menu_shader_vbo, ca->coords.vertices * 2 * sizeof(float));
         }

         draw->coords->vertex             = wiiu->menu_shader_vbo;
         draw->coords->vertices           = ca->coords.vertices;
         GX2SetAttribBuffer(0,
               draw->coords->vertices * 2 * sizeof(float),
               2 * sizeof(float), wiiu->menu_shader_vbo);
         GX2SetBlendControl(GX2_RENDER_TARGET_0,
               GX2_BLEND_MODE_SRC_ALPHA,
               GX2_BLEND_MODE_ONE,
               GX2_BLEND_COMBINE_MODE_ADD,
               GX2_DISABLE, 0, 0, 0);

         break;
      case VIDEO_SHADER_MENU_3:
      case VIDEO_SHADER_MENU_4:
      case VIDEO_SHADER_MENU_5:
      case VIDEO_SHADER_MENU_6:
         GX2SetAttribBuffer(0,
               4 * sizeof(*wiiu->v),
               sizeof(*wiiu->v), wiiu->v);
         break;
      default:
         return;
   }

   if (!wiiu->menu_shader_ubo)
   {
      wiiu->menu_shader_ubo = MEM2_alloc(
            sizeof(*wiiu->menu_shader_ubo),
            GX2_UNIFORM_BLOCK_ALIGNMENT);
      matrix_4x4_ortho(wiiu->menu_shader_ubo->mvp, 0, 1, 1, 0, -1, 1);
      wiiu->menu_shader_ubo->OutputSize.width = wiiu->color_buffer.surface.width;
      wiiu->menu_shader_ubo->OutputSize.height = wiiu->color_buffer.surface.height;
      wiiu->menu_shader_ubo->time = 0.0f;
   }
   else
      wiiu->menu_shader_ubo->time += 0.01f;

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_UNIFORM_BLOCK, wiiu->menu_shader_ubo, sizeof(*wiiu->menu_shader_ubo));
   GX2SetVertexUniformBlock(1, sizeof(*wiiu->menu_shader_ubo), wiiu->menu_shader_ubo);
   GX2SetPixelUniformBlock(1, sizeof(*wiiu->menu_shader_ubo), wiiu->menu_shader_ubo);
}

static void gfx_display_wiiu_scissor_begin(
      void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y,
      unsigned width, unsigned height)
{
   GX2SetScissor(MAX(x, 0), MAX(y, 0), MIN(width, video_width), MIN(height, video_height));
}

static void gfx_display_wiiu_scissor_end(
      void *data,
      unsigned video_width,
      unsigned video_height
      )
{
   GX2SetScissor(0, 0, video_width, video_height);
}

gfx_display_ctx_driver_t gfx_display_ctx_wiiu = {
   gfx_display_wiiu_draw,
   gfx_display_wiiu_draw_pipeline,
   NULL,                                     /* blend_begin            */
   NULL,                                     /* blend_end              */
   NULL,                                     /* get_default_mvp        */
   NULL,                                     /* get_default_vertices   */
   NULL,                                     /* get_default_tex_coords */
   FONT_DRIVER_RENDER_WIIU,
   GFX_VIDEO_DRIVER_WIIU,
   "gx2",
   true,
   gfx_display_wiiu_scissor_begin,
   gfx_display_wiiu_scissor_end
};

/*
 * FONT DRIVER
 */

static void* gx2_font_init(void* data, const char* font_path,
      float font_size, bool is_threaded)
{
   uint32_t i;
   gx2_font_t* font = (gx2_font_t*)calloc(1, sizeof(*font));

   if (!font)
      return NULL;

   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
      free(font);
      return NULL;
   }

   font->atlas                       = font->font_driver->get_atlas(font->font_data);
   font->texture.surface.width       = font->atlas->width;
   font->texture.surface.height      = font->atlas->height;
   font->texture.surface.depth       = 1;
   font->texture.surface.dim         = GX2_SURFACE_DIM_TEXTURE_2D;
   font->texture.surface.tileMode    = GX2_TILE_MODE_LINEAR_ALIGNED;
   font->texture.viewNumSlices       = 1;

   font->texture.surface.format      = GX2_SURFACE_FORMAT_UNORM_R8;
   font->texture.compMap             = GX2_COMP_SEL(_1, _1, _1, _R);

   GX2CalcSurfaceSizeAndAlignment(&font->texture.surface);
   GX2InitTextureRegs(&font->texture);
   font->texture.surface.image       = MEM1_alloc(
         font->texture.surface.imageSize,
         font->texture.surface.alignment);

   for (i = 0; (i < font->atlas->height) && (i < font->texture.surface.height); i++)
      memcpy((uint8_t*)font->texture.surface.image
            + (i * font->texture.surface.pitch),
            font->atlas->buffer + (i * font->atlas->width),
            font->atlas->width);

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE,
         font->texture.surface.image,
         font->texture.surface.imageSize);

   font->atlas->dirty    = false;
   font->ubo_tex         = MEM1_alloc(sizeof(*font->ubo_tex), GX2_UNIFORM_BLOCK_ALIGNMENT);
   font->ubo_tex->width  = font->texture.surface.width;
   font->ubo_tex->height = font->texture.surface.height;
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_UNIFORM_BLOCK, font->ubo_tex,
                 sizeof(*font->ubo_tex));

   return font;
}

static void gx2_font_free(void* data, bool is_threaded)
{
   gx2_font_t* font = (gx2_font_t*)data;

   if (!font)
      return;

   if (font->font_driver && font->font_data)
      font->font_driver->free(font->font_data);

   if (font->texture.surface.image)
      MEM1_free(font->texture.surface.image);
   if (font->ubo_tex)
      MEM1_free(font->ubo_tex);
   free(font);
}

static int gx2_font_get_message_width(void* data, const char* msg,
      size_t msg_len, float scale)
{
   int i;
   int delta_x = 0;
   const struct font_glyph* glyph_q = NULL;
   gx2_font_t                *font  = (gx2_font_t*)data;

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

static void gx2_font_render_line(
      wiiu_video_t *wiiu,
      gx2_font_t* font,
      const struct font_glyph* glyph_q,
      const char* msg, size_t msg_len,
      float scale, const unsigned int color, float pos_x,
      float pos_y,
      unsigned width, unsigned height,
      int pre_x,
      unsigned text_align)
{
   int i;
   int count;
   sprite_vertex_t *v;
   int x                            = pre_x;
   int y                            = roundf((1.0 - pos_y) * height);

   switch (text_align)
   {
      case TEXT_ALIGN_RIGHT:
         x -= gx2_font_get_message_width(font, msg, msg_len, scale);
         break;

      case TEXT_ALIGN_CENTER:
         x -= gx2_font_get_message_width(font, msg, msg_len, scale) / 2;
         break;
   }

   v       = wiiu->vertex_cache.v + wiiu->vertex_cache.current;

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
         if (!(glyph  = glyph_q))
            continue;

      v->pos.x        = x + glyph->draw_offset_x * scale;
      v->pos.y        = y + glyph->draw_offset_y * scale;
      v->pos.width    = glyph->width * scale;
      v->pos.height   = glyph->height * scale;

      v->coord.u      = glyph->atlas_offset_x;
      v->coord.v      = glyph->atlas_offset_y;
      v->coord.width  = glyph->width;
      v->coord.height = glyph->height;

      v->color        = color;

      v++;

      x              += glyph->advance_x * scale;
      y              += glyph->advance_y * scale;
   }

   count = v - wiiu->vertex_cache.v - wiiu->vertex_cache.current;

   if (!count)
      return;

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER,
         wiiu->vertex_cache.v + wiiu->vertex_cache.current,
         count * sizeof(wiiu->vertex_cache.v));

   if (font->atlas->dirty)
   {
      for (i = 0; (i < font->atlas->height) && (i < font->texture.surface.height); i++)
         memcpy(font->texture.surface.image
               + (i * font->texture.surface.pitch),
                font->atlas->buffer + (i * font->atlas->width),
                font->atlas->width);

      GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE,
            font->texture.surface.image,
            font->texture.surface.imageSize);
      font->atlas->dirty = false;
   }

   GX2SetPixelTexture(&font->texture,
         sprite_shader.ps.samplerVars[0].location);
   GX2SetVertexUniformBlock(sprite_shader.vs.uniformBlocks[1].offset,
         sprite_shader.vs.uniformBlocks[1].size,
         font->ubo_tex);

   GX2DrawEx(GX2_PRIMITIVE_MODE_POINTS, count, wiiu->vertex_cache.current, 1);

   GX2SetVertexUniformBlock(sprite_shader.vs.uniformBlocks[1].offset,
         sprite_shader.vs.uniformBlocks[1].size,
         wiiu->ubo_tex);

   wiiu->vertex_cache.current = v - wiiu->vertex_cache.v;
}

static void gx2_font_render_message(
      wiiu_video_t *wiiu,
      gx2_font_t* font, const char* msg, float scale,
      const unsigned int color, float pos_x, float pos_y,
      unsigned width, unsigned height, unsigned text_align)
{
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;
   const struct font_glyph* glyph_q       = font->font_driver->get_glyph(font->font_data, '?');
   int x                                  = roundf(pos_x * width);
   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = line_metrics->height * scale / wiiu->vp.height;

   for (;;)
   {
      const char* delim = strchr(msg, '\n');
      size_t msg_len    = delim ? (delim - msg) : strlen(msg);

      /* Draw the line */
      if ((wiiu->vertex_cache.current + (msg_len * 4)
		      <= wiiu->vertex_cache.size))
         gx2_font_render_line(wiiu,
               font,
               glyph_q,
               msg, msg_len,
               scale, color,
               pos_x,
               pos_y - (float)lines * line_height,
               width,
               height,
               x,
               text_align);

      if (!delim)
         break;

      msg += msg_len + 1;
      lines++;
   }
}

static void gx2_font_render_msg(
      void *userdata,
      void* data,
      const char* msg,
      const struct font_params *params)
{
   float x, y, scale, drop_mod, drop_alpha;
   int drop_x, drop_y;
   enum text_alignment text_align;
   unsigned color, r, g, b, alpha;
   wiiu_video_t *wiiu         = (wiiu_video_t*)userdata;
   gx2_font_t *font           = (gx2_font_t*)data;
   unsigned width             = wiiu->vp.full_width;
   unsigned height            = wiiu->vp.full_height;

   if (!font || !wiiu || !msg || !*msg)
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
      color                   = params->color;
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
      color                   = COLOR_RGBA(r, g, b, alpha);

      drop_x                  = -2;
      drop_y                  = -2;
      drop_mod                = 0.3f;
      drop_alpha              = 1.0f;
   }

   if (drop_x || drop_y)
   {
      unsigned r_dark         = r * drop_mod;
      unsigned g_dark         = g * drop_mod;
      unsigned b_dark         = b * drop_mod;
      unsigned alpha_dark     = alpha * drop_alpha;
      unsigned color_dark     = COLOR_RGBA(r_dark, g_dark, b_dark, alpha_dark);
      gx2_font_render_message(wiiu, font, msg, scale, color_dark,
            x + scale * drop_x / width, y +
            scale * drop_y / height, width, height, text_align);
   }

   gx2_font_render_message(wiiu, font, msg, scale,
         color, x, y, width, height, text_align);
}

static const struct font_glyph* gx2_font_get_glyph(void* data, uint32_t code)
{
   gx2_font_t* font = (gx2_font_t*)data;
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static bool gx2_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   gx2_font_t* font = (gx2_font_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t wiiu_font =
{
   gx2_font_init,
   gx2_font_free,
   gx2_font_render_msg,
   "gx2",
   gx2_font_get_glyph,
   NULL,                   /* bind_block */
   NULL,                   /* flush */
   gx2_font_get_message_width,
   gx2_font_get_line_metrics
};

/*
 * VIDEO DRIVER
 */

static void gx2_set_tex_coords(frame_vertex_t *v,
      GX2Texture *texture, float u0, float v0,
      float u1, float v1,
      unsigned rotation)
{
   v[0].coord.u = u0 / texture->surface.width;
   v[0].coord.v = v0 / texture->surface.height;
   v[1].coord.u = u1 / texture->surface.width;
   v[1].coord.v = v0 / texture->surface.height;
   v[2].coord.u = u1 / texture->surface.width;
   v[2].coord.v = v1 / texture->surface.height;
   v[3].coord.u = u0 / texture->surface.width;
   v[3].coord.v = v1 / texture->surface.height;
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, v, 4 * sizeof(*v));
}

static void gx2_set_projection(wiiu_video_t *wiiu)
{
   static math_matrix_4x4 rot     = {
      { 0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    0.0f ,
        0.0f,     0.0f,    0.0f,    1.0f }
   };
   float radians, cosine, sine;
   math_matrix_4x4 proj;

   matrix_4x4_ortho(proj, 0, 1, 1, 0, -1, 1);
   radians                 = wiiu->rotation * M_PI_2;
   cosine                  = cosf(radians);
   sine                    = sinf(radians);
   MAT_ELEM_4X4(rot, 0, 0) = cosine;
   MAT_ELEM_4X4(rot, 0, 1) = -sine;
   MAT_ELEM_4X4(rot, 1, 0) = sine;
   MAT_ELEM_4X4(rot, 1, 1) = cosine;
   matrix_4x4_multiply((*wiiu->ubo_mvp), rot, proj);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_UNIFORM_BLOCK, wiiu->ubo_mvp, sizeof(*wiiu->ubo_mvp));
}

static void gx2_update_viewport(wiiu_video_t *wiiu)
{
   unsigned vp_width               = wiiu->color_buffer.surface.width;
   unsigned vp_height              = wiiu->color_buffer.surface.height;
   settings_t *settings            = config_get_ptr();
   bool video_scale_integer        = settings->bools.video_scale_integer;

   if (video_scale_integer)
   {
      video_viewport_get_scaled_integer(&wiiu->vp,
            vp_width, vp_height,
            video_driver_get_aspect_ratio(), wiiu->keep_aspect, true);
      vp_width  = wiiu->vp.width;
      vp_height = wiiu->vp.height;
   }
   else if (wiiu->keep_aspect)
      video_viewport_get_scaled_aspect(&wiiu->vp, vp_width, vp_height, true);
   else
   {
      wiiu->vp.x      = 0;
      wiiu->vp.y      = 0;
      wiiu->vp.width  = vp_width;
      wiiu->vp.height = vp_height;
   }

   gx2_set_projection(wiiu);
}

static void gx2_set_aspect_ratio(void *data, unsigned aspect_ratio_idx)
{
   wiiu_video_t *wiiu  = (wiiu_video_t *)data;

   if (!wiiu)
      return;

   wiiu->keep_aspect   = true;
   wiiu->should_resize = true;
}

static uint32_t gx2_get_flags(void *data);

static void *gx2_init(const video_info_t *video,
      input_driver_t **input, void **input_data)
{
   unsigned i;
   float refresh_rate              = 60.0f / 1.001f;
   u32 size                        = 0;
   u32 tmp                         = 0;
   void *wiiuinput                 = NULL;
   wiiu_video_t *wiiu              = (wiiu_video_t*)calloc(1, sizeof(*wiiu));
   settings_t *settings            = config_get_ptr();
   const char *input_joypad_driver = settings->arrays.input_joypad_driver;
   bool prefer_drc                 = settings->bools.video_wiiu_prefer_drc;

   if (!wiiu)
      return NULL;

   *input                          = NULL;
   *input_data                     = NULL;

   if (input && input_data)
   {
      wiiuinput                    = input_driver_init_wrap(&input_wiiu, input_joypad_driver);
      *input                       = wiiuinput ? &input_wiiu : NULL;
      *input_data                  = wiiuinput;
   }

   /* video initialize */
   wiiu->cmd_buffer                = MEM2_alloc(0x400000, 0x40);
   u32 init_attributes[]           =
   {
      GX2_INIT_CMD_BUF_BASE, (u32)wiiu->cmd_buffer,
      GX2_INIT_CMD_BUF_POOL_SIZE, 0x400000,
      GX2_INIT_ARGC, 0,
      GX2_INIT_ARGV, 0,
      GX2_INIT_END
   };
   GX2Init(init_attributes);

   wiiu->rgb32                     = video->rgb32;

   /* setup scanbuffers */
   wiiu->render_mode               = gx2_render_mode_map[GX2GetSystemTVScanMode()];
   GX2CalcTVSize(wiiu->render_mode.mode, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
                 GX2_BUFFERING_MODE_DOUBLE, &size, &tmp);

   wiiu->tv_scan_buffer            = MEMBucket_alloc(size, GX2_SCAN_BUFFER_ALIGNMENT);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU, wiiu->tv_scan_buffer, size);
   GX2SetTVBuffer(wiiu->tv_scan_buffer, size, wiiu->render_mode.mode,
                  GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
                  GX2_BUFFERING_MODE_DOUBLE);

   GX2CalcDRCSize(GX2_DRC_RENDER_MODE_SINGLE, GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
                  GX2_BUFFERING_MODE_DOUBLE, &size,
                  &tmp);

   wiiu->drc_scan_buffer           = MEMBucket_alloc(size, GX2_SCAN_BUFFER_ALIGNMENT);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU, wiiu->drc_scan_buffer, size);
   GX2SetDRCBuffer(wiiu->drc_scan_buffer, size, GX2_DRC_RENDER_MODE_SINGLE,
                   GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8,
                   GX2_BUFFERING_MODE_DOUBLE);

   memset(&wiiu->color_buffer, 0, sizeof(GX2ColorBuffer));

   wiiu->color_buffer.surface.dim       = GX2_SURFACE_DIM_TEXTURE_2D;
   if (prefer_drc && (wiiu->render_mode.height != 480))
   {
      wiiu->color_buffer.surface.width  = 1708;
      wiiu->color_buffer.surface.height = 960;
   }
   else
   {
      wiiu->color_buffer.surface.width  = wiiu->render_mode.width;
      wiiu->color_buffer.surface.height = wiiu->render_mode.height;
   }
   wiiu->color_buffer.surface.depth     = 1;
   wiiu->color_buffer.surface.mipLevels = 1;
   wiiu->color_buffer.surface.format    = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
   wiiu->color_buffer.surface.use       = GX2_SURFACE_USE_TEXTURE_COLOR_BUFFER_TV;
   wiiu->color_buffer.viewNumSlices     = 1;

   GX2CalcSurfaceSizeAndAlignment(&wiiu->color_buffer.surface);
   GX2InitColorBufferRegs(&wiiu->color_buffer);

   wiiu->color_buffer.surface.image     = MEM1_alloc(
         wiiu->color_buffer.surface.imageSize,
         wiiu->color_buffer.surface.alignment);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU, wiiu->color_buffer.surface.image,
                 wiiu->color_buffer.surface.imageSize);

   wiiu->ctx_state                      = (GX2ContextState *)MEM2_alloc(
         sizeof(GX2ContextState), GX2_CONTEXT_STATE_ALIGNMENT);
   GX2SetupContextStateEx(wiiu->ctx_state, GX2_TRUE);

   GX2SetContextState(wiiu->ctx_state);
   GX2SetColorBuffer(&wiiu->color_buffer, GX2_RENDER_TARGET_0);
   GX2SetViewport(0.0f, 0.0f, wiiu->color_buffer.surface.width, wiiu->color_buffer.surface.height,
                  0.0f, 1.0f);
   GX2SetScissor(0, 0, wiiu->color_buffer.surface.width, wiiu->color_buffer.surface.height);
   GX2SetDepthOnlyControl(GX2_DISABLE, GX2_DISABLE, GX2_COMPARE_FUNC_ALWAYS);
   GX2SetColorControl(GX2_LOGIC_OP_COPY, 1, GX2_DISABLE, GX2_ENABLE);
   GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
                      GX2_BLEND_COMBINE_MODE_ADD,
                      GX2_ENABLE,          GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA,
                      GX2_BLEND_COMBINE_MODE_ADD);
   GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, GX2_DISABLE, GX2_DISABLE);

   GX2InitShader(&frame_shader);
   GX2InitShader(&tex_shader);
   GX2InitShader(&sprite_shader);
   GX2InitShader(&ribbon_simple_shader);
   GX2InitShader(&ribbon_shader);
   GX2InitShader(&bokeh_shader);
   GX2InitShader(&snow_shader);
   GX2InitShader(&snow_simple_shader);
   GX2InitShader(&snowflake_shader);

   GX2SetShader(&frame_shader);

   wiiu->ubo_vp                       = MEM1_alloc(sizeof(*wiiu->ubo_vp), GX2_UNIFORM_BLOCK_ALIGNMENT);
   wiiu->ubo_vp->width                = wiiu->color_buffer.surface.width;
   wiiu->ubo_vp->height               = wiiu->color_buffer.surface.height;
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_UNIFORM_BLOCK, wiiu->ubo_vp, sizeof(*wiiu->ubo_vp));

   wiiu->ubo_tex                      = MEM1_alloc(sizeof(*wiiu->ubo_tex), GX2_UNIFORM_BLOCK_ALIGNMENT);
   wiiu->ubo_tex->width               = 1.0;
   wiiu->ubo_tex->height              = 1.0;
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_UNIFORM_BLOCK, wiiu->ubo_tex, sizeof(*wiiu->ubo_tex));

   wiiu->ubo_mvp                      = MEM1_alloc(sizeof(*wiiu->ubo_mvp), GX2_UNIFORM_BLOCK_ALIGNMENT);
   gx2_set_projection(wiiu);

   wiiu->input_ring_buffer_size       = GX2CalcGeometryShaderInputRingBufferSize(
                                     sprite_shader.vs.ringItemSize);
   wiiu->output_ring_buffer_size      = GX2CalcGeometryShaderOutputRingBufferSize(
                                      sprite_shader.gs.ringItemSize);
   wiiu->input_ring_buffer            = MEM1_alloc(wiiu->input_ring_buffer_size, 0x1000);
   wiiu->output_ring_buffer           = MEM1_alloc(wiiu->output_ring_buffer_size, 0x1000);

   /* init menu texture */
   memset(&wiiu->menu.texture, 0, sizeof(GX2Texture));
   wiiu->menu.texture.surface.width    = 512;
   wiiu->menu.texture.surface.height   = 512;
   wiiu->menu.texture.surface.depth    = 1;
   wiiu->menu.texture.surface.dim      = GX2_SURFACE_DIM_TEXTURE_2D;
   wiiu->menu.texture.surface.format   = GX2_SURFACE_FORMAT_UNORM_R4_G4_B4_A4;
   wiiu->menu.texture.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
   wiiu->menu.texture.viewNumSlices    = 1;
   /* Presumably an endian thing. RGBA, but swap R and G, then B and A. */
   wiiu->menu.texture.compMap          = GX2_COMP_SEL(_G, _R, _A, _B);
   GX2CalcSurfaceSizeAndAlignment(&wiiu->menu.texture.surface);
   GX2InitTextureRegs(&wiiu->menu.texture);

   wiiu->menu.texture.surface.image    = MEM2_alloc(wiiu->menu.texture.surface.imageSize,
         wiiu->menu.texture.surface.alignment);

   memset(wiiu->menu.texture.surface.image, 0x0, wiiu->menu.texture.surface.imageSize);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, wiiu->menu.texture.surface.image,
                 wiiu->menu.texture.surface.imageSize);

   wiiu->v                             = MEM2_alloc(
         4 * sizeof(*wiiu->v), GX2_VERTEX_BUFFER_ALIGNMENT);

   wiiu->v[0].pos.x                    = 0.0f;
   wiiu->v[0].pos.y                    = 0.0f;
   wiiu->v[1].pos.x                    = 1.0f;
   wiiu->v[1].pos.y                    = 0.0f;
   wiiu->v[2].pos.x                    = 1.0f;
   wiiu->v[2].pos.y                    = 1.0f;
   wiiu->v[3].pos.x                    = 0.0f;
   wiiu->v[3].pos.y                    = 1.0f;
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, wiiu->v, 4 * sizeof(*wiiu->v));

   gx2_set_tex_coords(wiiu->v, &wiiu->texture, 0, 0,
                       wiiu->texture.surface.width, wiiu->texture.surface.height, wiiu->rotation);

   GX2SetAttribBuffer(0, 4 * sizeof(*wiiu->v), sizeof(*wiiu->v), wiiu->v);

   wiiu->menu.v = MEM2_alloc(4 * sizeof(*wiiu->menu.v), GX2_VERTEX_BUFFER_ALIGNMENT);
   wiiu->menu.v->pos.x                 = 0.0f;
   wiiu->menu.v->pos.y                 = 0.0f;
   wiiu->menu.v->pos.width             = wiiu->color_buffer.surface.width;
   wiiu->menu.v->pos.height            = wiiu->color_buffer.surface.height;
   wiiu->menu.v->coord.u               = 0.0f;
   wiiu->menu.v->coord.v               = 0.0f;
   wiiu->menu.v->coord.width           = 1.0f;
   wiiu->menu.v->coord.height          = 1.0f;
   wiiu->menu.v->color                 = 0xFFFFFFFF;
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, wiiu->menu.v, 4 * sizeof(*wiiu->menu.v));

   wiiu->vertex_cache.size             = 0x1000;
   wiiu->vertex_cache.current          = 0;
   wiiu->vertex_cache.v                = MEM2_alloc(wiiu->vertex_cache.size
         * sizeof(*wiiu->vertex_cache.v), GX2_VERTEX_BUFFER_ALIGNMENT);

   wiiu->vertex_cache_tex.size         = 0x1000;
   wiiu->vertex_cache_tex.current      = 0;
   wiiu->vertex_cache_tex.v            = MEM2_alloc(wiiu->vertex_cache_tex.size
         * sizeof(*wiiu->vertex_cache_tex.v), GX2_VERTEX_BUFFER_ALIGNMENT);

   /* Initialize samplers */
   for (i = 0; i < RARCH_WRAP_MAX; i++)
   {
      switch (i)
      {
         case RARCH_WRAP_BORDER:
            GX2InitSampler(&wiiu->sampler_nearest[i],
                  GX2_TEX_CLAMP_MODE_CLAMP_BORDER,
                  GX2_TEX_XY_FILTER_MODE_POINT);
            GX2InitSampler(&wiiu->sampler_linear[i],
                  GX2_TEX_CLAMP_MODE_CLAMP_BORDER,
                  GX2_TEX_XY_FILTER_MODE_LINEAR);
            break;

         case RARCH_WRAP_EDGE:
            GX2InitSampler(&wiiu->sampler_nearest[i],
                  GX2_TEX_CLAMP_MODE_CLAMP,
                  GX2_TEX_XY_FILTER_MODE_POINT);
            GX2InitSampler(&wiiu->sampler_linear[i],
                  GX2_TEX_CLAMP_MODE_CLAMP,
                  GX2_TEX_XY_FILTER_MODE_LINEAR);
            break;

         case RARCH_WRAP_REPEAT:
            GX2InitSampler(&wiiu->sampler_nearest[i],
                  GX2_TEX_CLAMP_MODE_WRAP,
                  GX2_TEX_XY_FILTER_MODE_POINT);
            GX2InitSampler(&wiiu->sampler_linear[i],
                  GX2_TEX_CLAMP_MODE_WRAP,
                  GX2_TEX_XY_FILTER_MODE_LINEAR);
            break;

         case RARCH_WRAP_MIRRORED_REPEAT:
            GX2InitSampler(&wiiu->sampler_nearest[i],
                  GX2_TEX_CLAMP_MODE_MIRROR,
                  GX2_TEX_XY_FILTER_MODE_POINT);
            GX2InitSampler(&wiiu->sampler_linear[i],
                  GX2_TEX_CLAMP_MODE_MIRROR,
                  GX2_TEX_XY_FILTER_MODE_LINEAR);
            break;
      }
   }

   /* set Texture and Sampler */
   GX2SetPixelTexture(&wiiu->texture, frame_shader.ps.samplerVars[0].location);
   GX2SetPixelSampler(&wiiu->sampler_linear[RARCH_WRAP_DEFAULT],
                      frame_shader.ps.samplerVars[0].location);

   /* clear leftover image */
   GX2ClearColor(&wiiu->color_buffer, 0.0f, 0.0f, 0.0f, 1.0f);
   GX2CopyColorBufferToScanBuffer(&wiiu->color_buffer, GX2_SCAN_TARGET_DRC);
   GX2CopyColorBufferToScanBuffer(&wiiu->color_buffer, GX2_SCAN_TARGET_TV);

   GX2SwapScanBuffers();
   GX2Flush();
   GX2WaitForVsync();

   GX2SetTVEnable(GX2_ENABLE);
   GX2SetDRCEnable(GX2_ENABLE);

   wiiu->keep_aspect       = true;
   wiiu->should_resize     = true;
   wiiu->smooth            = video->smooth;
   wiiu->vsync             = video->vsync;
   GX2SetSwapInterval(!!video->vsync);

   wiiu->vp.x              = 0;
   wiiu->vp.y              = 0;

   if (wiiu->render_mode.height != 480 && prefer_drc)
   {
      wiiu->vp.width       = 1708;
      wiiu->vp.height      = 960;
      wiiu->vp.full_width  = 1708;
      wiiu->vp.full_height = 960;
   }
   else
   {
      wiiu->vp.width       = wiiu->render_mode.width;
      wiiu->vp.height      = wiiu->render_mode.height;
      wiiu->vp.full_width  = wiiu->render_mode.width;
      wiiu->vp.full_height = wiiu->render_mode.height;
   }

   video_driver_set_size(wiiu->vp.width, wiiu->vp.height);

   driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, &refresh_rate);

   font_driver_init_osd(wiiu,
         video,
         false,
         video->is_threaded,
         FONT_DRIVER_RENDER_WIIU);

   {
      enum rarch_shader_type type;
      const char *shader_preset   = NULL;

      gx2_fake_context.get_flags = gx2_get_flags;

      video_context_driver_set(&gx2_fake_context);

      shader_preset               = video_shader_get_current_shader_preset();
      type                        = video_shader_parse_type(shader_preset);
      gx2_set_shader(wiiu, type, shader_preset);
   }

   return wiiu;
}

#ifdef HAVE_OVERLAY
static void gx2_overlay_tex_geom(void *data, unsigned image,
                                 float x, float y, float w, float h)
{
   wiiu_video_t            *gx2 = (wiiu_video_t *)data;
   struct gx2_overlay_data *o = NULL;

   if (gx2)
      o = (struct gx2_overlay_data *)&gx2->overlay[image];

   if (!o)
      return;

   o->v.coord.u = x;
   o->v.coord.v = y;
   o->v.coord.width = w;
   o->v.coord.height = h;
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, &o->v, sizeof(o->v));
}

static void gx2_overlay_vertex_geom(void *data, unsigned image,
                                    float x, float y, float w, float h)
{
   wiiu_video_t            *gx2 = (wiiu_video_t *)data;
   struct gx2_overlay_data *o = NULL;

   if (gx2)
      o = (struct gx2_overlay_data *)&gx2->overlay[image];

   if (!o)
      return;

   o->v.pos.x      = x * gx2->color_buffer.surface.width;
   o->v.pos.y      = y * gx2->color_buffer.surface.height;
   o->v.pos.width  = w * gx2->color_buffer.surface.width;
   o->v.pos.height = h * gx2->color_buffer.surface.height;

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, &o->v, sizeof(o->v));
}

static void gx2_free_overlay(wiiu_video_t *gx2)
{
   unsigned i;

   for (i = 0; i < gx2->overlays; i++)
      MEM2_free(gx2->overlay[i].tex.surface.image);

   free(gx2->overlay);
   gx2->overlay = NULL;
   gx2->overlays = 0;

}

static bool gx2_overlay_load(void *data,
                             const void *image_data, unsigned num_images)
{
   unsigned i, j;
   wiiu_video_t *gx2 = (wiiu_video_t *)data;
   const struct texture_image *images = (const struct texture_image *)image_data;

   gx2_free_overlay(gx2);
   gx2->overlay = (struct gx2_overlay_data *)calloc(num_images, sizeof(*gx2->overlay));

   if (!gx2->overlay)
      return false;

   gx2->overlays = num_images;

   for (i = 0; i < num_images; i++)
   {
      struct gx2_overlay_data *o = (struct gx2_overlay_data *)&gx2->overlay[i];

      memset(&o->tex, 0, sizeof(GX2Texture));
      o->tex.surface.width    = images[i].width;
      o->tex.surface.height   = images[i].height;
      o->tex.surface.depth    = 1;
      o->tex.surface.dim      = GX2_SURFACE_DIM_TEXTURE_2D;
      o->tex.surface.format   = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
      o->tex.surface.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
      o->tex.viewNumSlices    = 1;
      o->tex.compMap          = GX2_COMP_SEL(_G, _B, _A, _R);
      GX2CalcSurfaceSizeAndAlignment(&o->tex.surface);
      GX2InitTextureRegs(&o->tex);

      o->tex.surface.image = MEM2_alloc(o->tex.surface.imageSize,
                                        o->tex.surface.alignment);

      for (j = 0; (j < images[i].height) && (j < o->tex.surface.height); j++)
         memcpy((uint32_t *)o->tex.surface.image + (j * o->tex.surface.pitch),
                images[i].pixels + (j * images[i].width), images[i].width * sizeof(images[i].pixels));

      GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE,  o->tex.surface.image,  o->tex.surface.imageSize);

      /* Default. Stretch to whole screen. */
      gx2_overlay_tex_geom(gx2, i, 0, 0, 1, 1);
      gx2_overlay_vertex_geom(gx2, i, 0, 0, 1, 1);
      gx2->overlay[i].alpha_mod = 1.0f;
      gx2->overlay[i].v.color   = 0xFFFFFFFF;

      GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, &o->v, sizeof(o->v));

   }

   return true;
}

static void gx2_overlay_enable(void *data, bool state)
{
   wiiu_video_t *gx2 = (wiiu_video_t *)data;
   gx2->overlay_enable = state;
}

static void gx2_overlay_full_screen(void *data, bool enable)
{
   wiiu_video_t *gx2 = (wiiu_video_t *)data;
   gx2->overlay_full_screen = enable;
}

static void gx2_overlay_set_alpha(void *data, unsigned image, float mod)
{
   wiiu_video_t *gx2 = (wiiu_video_t *)data;

   if (gx2)
   {
      gx2->overlay[image].alpha_mod = mod;
      gx2->overlay[image].v.color = COLOR_RGBA(0xFF, 0xFF, 0xFF, 0xFF * gx2->overlay[image].alpha_mod);
      GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, &gx2->overlay[image].v,
                    sizeof(gx2->overlay[image].v));
   }
}

static void gx2_render_overlay(void *data)
{
   unsigned i;

   wiiu_video_t *gx2 = (wiiu_video_t *)data;

   for (i = 0; i < gx2->overlays; i++)
   {

      GX2SetAttribBuffer(0, sizeof(gx2->overlay[i].v), sizeof(gx2->overlay[i].v), &gx2->overlay[i].v);

      GX2SetPixelTexture(&gx2->overlay[i].tex, sprite_shader.ps.samplerVars[0].location);
      GX2SetPixelSampler(&gx2->sampler_linear[RARCH_WRAP_EDGE], sprite_shader.ps.samplerVars[0].location);

      GX2DrawEx(GX2_PRIMITIVE_MODE_POINTS, 1, 0, 1);

   }

}

static const video_overlay_interface_t gx2_overlay_interface =
{
   gx2_overlay_enable,
   gx2_overlay_load,
   gx2_overlay_tex_geom,
   gx2_overlay_vertex_geom,
   gx2_overlay_full_screen,
   gx2_overlay_set_alpha,
};

static void gx2_get_overlay_interface(void *data,
		const video_overlay_interface_t **iface)
{
   *iface = &gx2_overlay_interface;
}
#endif


static void gx2_free(void *data)
{
   wiiu_video_t *wiiu = (wiiu_video_t *) data;

   if (!wiiu)
      return;

   /* clear leftover image */
   GX2ClearColor(&wiiu->color_buffer, 0.0f, 0.0f, 0.0f, 1.0f);
   GX2CopyColorBufferToScanBuffer(&wiiu->color_buffer, GX2_SCAN_TARGET_DRC);
   GX2CopyColorBufferToScanBuffer(&wiiu->color_buffer, GX2_SCAN_TARGET_TV);

   GX2SwapScanBuffers();
   GX2Flush();
   GX2DrawDone();
   GX2WaitForVsync();
   GX2Shutdown();

   GX2SetTVEnable(GX2_DISABLE);
   GX2SetDRCEnable(GX2_DISABLE);

   GX2DestroyShader(&frame_shader);
   GX2DestroyShader(&tex_shader);
   GX2DestroyShader(&sprite_shader);
   GX2DestroyShader(&ribbon_simple_shader);
   GX2DestroyShader(&ribbon_shader);
   GX2DestroyShader(&bokeh_shader);
   GX2DestroyShader(&snow_shader);
   GX2DestroyShader(&snow_simple_shader);
   GX2DestroyShader(&snowflake_shader);

   if (wiiu->shader_preset)
      gx2_free_shader_preset(wiiu);

#ifdef HAVE_OVERLAY
   gx2_free_overlay(wiiu);
#endif

   MEM2_free(wiiu->ctx_state);
   MEM2_free(wiiu->cmd_buffer);
   MEM2_free(wiiu->texture.surface.image);
   MEM2_free(wiiu->menu.texture.surface.image);
   MEM2_free(wiiu->v);
   MEM2_free(wiiu->menu.v);
   MEM2_free(wiiu->vertex_cache.v);
   MEM2_free(wiiu->vertex_cache_tex.v);
   MEM2_free(wiiu->menu_shader_vbo);
   MEM2_free(wiiu->menu_shader_ubo);

   MEM1_free(wiiu->color_buffer.surface.image);
   MEM1_free(wiiu->ubo_vp);
   MEM1_free(wiiu->ubo_tex);
   MEM1_free(wiiu->ubo_mvp);
   MEM1_free(wiiu->input_ring_buffer);
   MEM1_free(wiiu->output_ring_buffer);

   MEMBucket_free(wiiu->tv_scan_buffer);
   MEMBucket_free(wiiu->drc_scan_buffer);

   free(wiiu);
}

static bool wiiu_init_frame_textures(wiiu_video_t *wiiu, unsigned width, unsigned height)
{
   unsigned i;

   MEM2_free(wiiu->texture.surface.image);

   if (wiiu->shader_preset)
   {
      for (i = 0; i < wiiu->shader_preset->passes; i++)
      {
         if (wiiu->pass[i].mem1)
            MEM1_free(wiiu->pass[i].texture.surface.image);
         else
            MEM2_free(wiiu->pass[i].texture.surface.image);

         wiiu->pass[i].texture.surface.image = NULL;
      }
   }

   /* Initialize frame texture */
   memset(&wiiu->texture, 0, sizeof(GX2Texture));
   wiiu->texture.surface.width       = width;
   wiiu->texture.surface.height      = height;
   wiiu->texture.surface.depth       = 1;
   wiiu->texture.surface.dim         = GX2_SURFACE_DIM_TEXTURE_2D;
   wiiu->texture.surface.tileMode    = GX2_TILE_MODE_LINEAR_ALIGNED;
   wiiu->texture.viewNumSlices       = 1;

   if (wiiu->rgb32)
   {
      wiiu->texture.surface.format   = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
      wiiu->texture.compMap          = GX2_COMP_SEL(_G, _B, _A, _1);
   }
   else
   {
      wiiu->texture.surface.format   = GX2_SURFACE_FORMAT_UNORM_R5_G6_B5;
      wiiu->texture.compMap          = GX2_COMP_SEL(_B, _G, _R, _1);
   }

   GX2CalcSurfaceSizeAndAlignment(&wiiu->texture.surface);
   GX2InitTextureRegs(&wiiu->texture);

   wiiu->texture.surface.image = MEM2_alloc(wiiu->texture.surface.imageSize,
                                 wiiu->texture.surface.alignment);
   memset(wiiu->texture.surface.image, 0x0, wiiu->texture.surface.imageSize);
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, wiiu->texture.surface.image,
                 wiiu->texture.surface.imageSize);

   if (wiiu->shader_preset)
   {
      for (i = 0; i < wiiu->shader_preset->passes; i++)
      {
         struct video_shader_pass *pass = &wiiu->shader_preset->pass[i];

         switch (pass->fbo.type_x)
         {
            case RARCH_SCALE_INPUT:
               width *= pass->fbo.scale_x;
               break;

            case RARCH_SCALE_VIEWPORT:
               width = wiiu->vp.width * pass->fbo.scale_x;
               break;

            case RARCH_SCALE_ABSOLUTE:
               width = pass->fbo.abs_x;
               break;

            default:
               break;
         }

         switch (pass->fbo.type_y)
         {
            case RARCH_SCALE_INPUT:
               height *= pass->fbo.scale_y;
               break;

            case RARCH_SCALE_VIEWPORT:
               height = wiiu->vp.height * pass->fbo.scale_y;
               break;

            case RARCH_SCALE_ABSOLUTE:
               height = pass->fbo.abs_y;
               break;

            default:
               break;
         }

         if (!width)
            width = wiiu->color_buffer.surface.width;

         if (!height)
            height = wiiu->color_buffer.surface.height;

         memset(&wiiu->pass[i].texture, 0, sizeof(wiiu->pass[i].texture));
         wiiu->pass[i].texture.surface.dim         = GX2_SURFACE_DIM_TEXTURE_2D;
         wiiu->pass[i].texture.surface.width       = width;
         wiiu->pass[i].texture.surface.height      = height;
         wiiu->pass[i].texture.surface.depth       = 1;
#if 0
         wiiu->pass[i].texture.surface.mipLevels   = 1;
#endif
         wiiu->pass[i].texture.surface.format      =
              (pass->fbo.flags & FBO_SCALE_FLAG_FP_FBO)
            ? GX2_SURFACE_FORMAT_FLOAT_R32_G32_B32_A32
            : (pass->fbo.flags & FBO_SCALE_FLAG_SRGB_FBO)
            ? GX2_SURFACE_FORMAT_SRGB_R8_G8_B8_A8
            : GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
         wiiu->pass[i].texture.surface.use         = (GX2_SURFACE_USE_TEXTURE
                                                    | GX2_SURFACE_USE_COLOR_BUFFER);
         wiiu->pass[i].texture.viewNumSlices       = 1;
         wiiu->pass[i].texture.compMap             = GX2_COMP_SEL(_R, _G, _B, _A);

         GX2CalcSurfaceSizeAndAlignment(&wiiu->pass[i].texture.surface);
         GX2InitTextureRegs(&wiiu->pass[i].texture);

         if (     (i      != (wiiu->shader_preset->passes - 1))
               || (width  != wiiu->vp.width)
               || (height != wiiu->vp.height))
         {
            wiiu->pass[i].mem1 = true;
            wiiu->pass[i].texture.surface.image = MEM1_alloc(wiiu->pass[i].texture.surface.imageSize,
                                                  wiiu->pass[i].texture.surface.alignment);

            if (!wiiu->pass[i].texture.surface.image)
            {
               /* Failed to allocate Render target memory from MEM1. Trying
                * MEM2... */
               wiiu->pass[i].mem1 = false;
               wiiu->pass[i].texture.surface.image = MEM2_alloc(wiiu->pass[i].texture.surface.imageSize,
                                                     wiiu->pass[i].texture.surface.alignment);

               if (!wiiu->pass[i].texture.surface.image)
               {
                  /* Failed to allocate Render target memory from MEM2. Falling
                   * back to stock... */
                  if (wiiu->shader_preset)
                     gx2_free_shader_preset(wiiu);
                  return false;
               }
            }

            memset(wiiu->pass[i].texture.surface.image, 0x00, wiiu->pass[i].texture.surface.imageSize);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, wiiu->pass[i].texture.surface.image,
                          wiiu->pass[i].texture.surface.imageSize);
         }

         memset(&wiiu->pass[i].color_buffer, 0, sizeof(wiiu->pass[i].color_buffer));
         wiiu->pass[i].color_buffer.surface = wiiu->pass[i].texture.surface;
         GX2InitColorBufferRegs(&wiiu->pass[i].color_buffer);
      }
   }

   return true;
}

static void gx2_update_uniform_block(wiiu_video_t *wiiu,
      int pass, float *ubo, int id,
      int size, int uniformVarCount, GX2UniformVar *uniformVars,
      uint64_t frame_count, int32_t frame_direction, uint32_t rotation, float core_aspect,
      float core_aspect_rot, uint32_t frame_time_delta, uint32_t original_fps)
{
   unsigned i;
   for (i = 0; i < uniformVarCount; i++)
   {
      int k;
      float *dst;
      const char *id = NULL;
      if (uniformVars[i].block != id)
         continue;
      if (!(id = strrchr(uniformVars[i].name, '.')))
         continue;

      id++;

      dst = ubo + uniformVars[i].offset;

      if (string_is_equal(id, "OutputSize"))
      {
         ((GX2_vec4 *)dst)->x = wiiu->pass[pass].color_buffer.surface.width;
         ((GX2_vec4 *)dst)->y = wiiu->pass[pass].color_buffer.surface.height;
         ((GX2_vec4 *)dst)->z = 1.0f / wiiu->pass[pass].color_buffer.surface.width;
         ((GX2_vec4 *)dst)->w = 1.0f / wiiu->pass[pass].color_buffer.surface.height;
         continue;
      }

      if (string_is_equal(id, "FinalViewportSize"))
      {
         ((GX2_vec4 *)dst)->x = wiiu->vp.width;
         ((GX2_vec4 *)dst)->y = wiiu->vp.height;
         ((GX2_vec4 *)dst)->z = 1.0f / wiiu->vp.width;
         ((GX2_vec4 *)dst)->w = 1.0f / wiiu->vp.height;
         continue;
      }

      if (string_is_equal(id, "FrameCount"))
      {
         *dst        = wiiu->shader_preset->pass[pass].frame_count_mod ?
                frame_count % wiiu->shader_preset->pass[pass].frame_count_mod :
                frame_count;
         *(u32 *)dst = __builtin_bswap32(*(u32 *)dst);
         continue;
      }

      if (string_is_equal(id, "FrameDirection"))
      {
         *dst        = frame_direction;
         *(u32 *)dst = __builtin_bswap32(*(u32 *)dst);
         continue;
      }

      if (string_is_equal(id, "FrameTimeDelta"))
      {
         *dst        = frame_time_delta;
         *(u32 *)dst = __builtin_bswap32(*(u32 *)dst);
         continue;
      }

      if (string_is_equal(id, "OriginalFPS"))
      {
         *dst        = original_fps;
         continue;
      }

      if (string_is_equal(id, "Rotation"))
      {
         *dst        = rotation;
         *(u32 *)dst = __builtin_bswap32(*(u32 *)dst);
         continue;
      }

      if (string_is_equal(id, "OriginalAspect"))
      {
         *dst        = core_aspect;
         continue;
      }

      if (string_is_equal(id, "OriginalAspectRotated"))
      {
         *dst        = core_aspect_rot;
         continue;
      }

      if (string_is_equal(id, "OriginalSize"))
      {
         ((GX2_vec4 *)dst)->x = wiiu->texture.surface.width;
         ((GX2_vec4 *)dst)->y = wiiu->texture.surface.height;
         ((GX2_vec4 *)dst)->z = 1.0f / wiiu->texture.surface.width;
         ((GX2_vec4 *)dst)->w = 1.0f / wiiu->texture.surface.height;
         continue;
      }

      if (string_is_equal(id, "SourceSize"))
      {
         GX2Surface *source   = (pass > 0) ? &wiiu->pass[pass - 1].texture.surface : &wiiu->texture.surface;
         ((GX2_vec4 *)dst)->x = source->width;
         ((GX2_vec4 *)dst)->y = source->height;
         ((GX2_vec4 *)dst)->z = 1.0f / source->width;
         ((GX2_vec4 *)dst)->w = 1.0f / source->height;
         continue;
      }

      if (!strncmp(id, "OriginalHistorySize",
               STRLEN_CONST("OriginalHistorySize")))
      {
         GX2Surface *source;
         unsigned index       = strtoul(id + STRLEN_CONST("OriginalHistorySize"),
               NULL, 0);
         if (index > pass)
            index             = 0;

         if (index)
            index             = pass - index;

         source               = (index > 0) ? &wiiu->pass[index - 1].texture.surface : &wiiu->texture.surface;
         ((GX2_vec4 *)dst)->x = source->width;
         ((GX2_vec4 *)dst)->y = source->height;
         ((GX2_vec4 *)dst)->z = 1.0f / source->width;
         ((GX2_vec4 *)dst)->w = 1.0f / source->height;
         continue;
      }

      if ((pass > 0 ) && !strncmp(id, "PassOutputSize",
               STRLEN_CONST("PassOutputSize")))
      {
         GX2Surface *output;
         unsigned index       = strtoul(id + STRLEN_CONST("PassOutputSize"), NULL, 0);
         if (index > pass - 1)
            index             = pass - 1;
         output               = &wiiu->pass[index].texture.surface;
         ((GX2_vec4 *)dst)->x = output->width;
         ((GX2_vec4 *)dst)->y = output->height;
         ((GX2_vec4 *)dst)->z = 1.0f / output->width;
         ((GX2_vec4 *)dst)->w = 1.0f / output->height;
         continue;
      }

      /* feedback not supported yet */
      if (!strncmp(id, "PassFeedbackSize", STRLEN_CONST("PassFeedbackSize")))
      {
         GX2Surface *output;
         unsigned index       = strtoul(id + STRLEN_CONST("PassFeedbackSize"), NULL, 0);
         if (index > wiiu->shader_preset->passes - 1)
            index             = wiiu->shader_preset->passes - 1;
         output               = &wiiu->pass[index].texture.surface;
         ((GX2_vec4 *)dst)->x = output->width;
         ((GX2_vec4 *)dst)->y = output->height;
         ((GX2_vec4 *)dst)->z = 1.0f / output->width;
         ((GX2_vec4 *)dst)->w = 1.0f / output->height;
         continue;
      }

      for (k = 0; k < wiiu->shader_preset->luts; k++)
      {
         size_t lut_id_size = strlen(wiiu->shader_preset->lut[k].id);
         if (    !strncmp(id, wiiu->shader_preset->lut[k].id, lut_id_size)
             && !!strcmp(id + lut_id_size, "Size"))
         {
            GX2Surface *surface  = &wiiu->luts[k].surface;
            ((GX2_vec4 *)dst)->x = surface->width;
            ((GX2_vec4 *)dst)->y = surface->height;
            ((GX2_vec4 *)dst)->z = 1.0f / surface->width;
            ((GX2_vec4 *)dst)->w = 1.0f / surface->height;
         }
      }

      if (string_is_equal(id, "MVP"))
      {
         memcpy(dst, wiiu->ubo_mvp, sizeof(*wiiu->ubo_mvp));
         continue;
      }

      for (k = 0; k < wiiu->shader_preset->num_parameters; k++)
      {
         if (string_is_equal(id, wiiu->shader_preset->parameters[k].id))
         {
            *dst        = wiiu->shader_preset->parameters[k].current;
            *(u32 *)dst = __builtin_bswap32(*(u32 *)dst);
            break;
         }
      }
   }

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_UNIFORM_BLOCK, ubo, size);
}

static bool gx2_frame(void *data, const void *frame,
      unsigned width, unsigned height, uint64_t frame_count,
      unsigned pitch, const char *msg, video_frame_info_t *video_info)
{
   uint32_t i;
   wiiu_video_t *wiiu             = (wiiu_video_t *) data;
#ifdef HAVE_MENU
   bool menu_is_alive = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
#endif
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active            = video_info->widgets_active;
#endif
   struct font_params *osd_params = (struct font_params*)
      &video_info->osd_stat_params;
   bool statistics_show           = video_info->statistics_show;

   if (wiiu->vsync)
   {
      uint32_t swap_count;
      uint32_t flip_count;
      OSTime last_flip;
      OSTime last_vsync;

      GX2GetSwapStatus(&swap_count, &flip_count, &last_flip, &last_vsync);

      if (wiiu->last_vsync >= last_vsync)
      {
         GX2WaitForVsync();
         wiiu->last_vsync = last_vsync + ms_to_ticks(17);
      }
      else
         wiiu->last_vsync = last_vsync;
   }

   GX2WaitForFlip();

   if (!width || !height)
      return true;

   if (wiiu->should_resize)
      gx2_update_viewport(wiiu);

   GX2ClearColor(&wiiu->color_buffer, 0.0f, 0.0f, 0.0f, 1.0f);
   /* Can't call GX2ClearColor after GX2SetContextState for whatever reason */
   GX2SetContextState(wiiu->ctx_state);

   if (frame)
   {
      if (    (width  != wiiu->texture.surface.width)
           || (height != wiiu->texture.surface.height))
         wiiu_init_frame_textures(wiiu, width, height);

      wiiu->width  = width;
      wiiu->height = height;

      if (wiiu->rgb32)
      {
         const uint32_t *src = frame;
         uint32_t       *dst = (uint32_t *)wiiu->texture.surface.image;

         for (i = 0; i < height; i++)
         {
            uint32_t j;

            for (j = 0; j < width; j++)
               dst[j] = src[j];

            dst += wiiu->texture.surface.pitch;
            src += pitch / 4;
         }
      }
      else
      {
         const uint16_t *src = frame;
         uint16_t       *dst = (uint16_t *)wiiu->texture.surface.image;

         for (i = 0; i < height; i++)
         {
            unsigned j;

            for (j = 0; j < width; j++)
               dst[j] = __builtin_bswap16(src[j]);

            dst += wiiu->texture.surface.pitch;
            src += pitch / 2;
         }
      }

      GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, wiiu->texture.surface.image,
                    wiiu->texture.surface.imageSize);

      gx2_set_tex_coords(wiiu->v, &wiiu->texture, 0, 0, width, height, wiiu->rotation);
   }

   GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);

   GX2SetFetchShader(&frame_shader.fs);
   GX2SetAttribBuffer(0, 4 * sizeof(*wiiu->v), sizeof(*wiiu->v), wiiu->v);

   GX2Texture *texture = &wiiu->texture;

   if (wiiu->shader_preset && !wiiu->pass[0].texture.surface.image)
      wiiu_init_frame_textures(wiiu, width, height);

   if (wiiu->shader_preset)
   {
      unsigned i;
#ifdef HAVE_REWIND
      int32_t frame_direction   = state_manager_frame_is_reversed() ? -1 : 1;
#else
      int32_t frame_direction   = 1;
#endif
      uint32_t frame_time_delta = (uint32_t)video_driver_get_frame_time_delta_usec();
      float original_fps        = video_driver_get_original_fps();
      uint32_t rotation         = retroarch_get_rotation();
      float core_aspect         = video_driver_get_core_aspect();
      /* OriginalAspectRotated: return 1/aspect for 90 and 270 rotated content */
      float core_aspect_rot     = core_aspect;
      if (rotation == 1 || rotation == 3)
         core_aspect_rot        = 1 / core_aspect;

      for (i = 0; i < wiiu->shader_preset->passes; i++)
      {
         unsigned j;
         GX2SetVertexShader(wiiu->pass[i].gfd->vs);

         for (j = 0; j < 2 && j < wiiu->pass[i].gfd->vs->uniformBlockCount; j++)
         {
            gx2_update_uniform_block(wiiu, i, wiiu->pass[i].vs_ubos[j], j,
                  wiiu->pass[i].gfd->vs->uniformBlocks[j].size,
                  wiiu->pass[i].gfd->vs->uniformVarCount, wiiu->pass[i].gfd->vs->uniformVars,
                  frame_count, frame_direction, rotation, core_aspect, core_aspect_rot,
                  frame_time_delta, original_fps);

            GX2SetVertexUniformBlock(wiiu->pass[i].gfd->vs->uniformBlocks[j].offset,
                  wiiu->pass[i].gfd->vs->uniformBlocks[j].size, wiiu->pass[i].vs_ubos[j]);
         }

         GX2SetPixelShader(wiiu->pass[i].gfd->ps);

         for (j = 0; j < 2 && j < wiiu->pass[i].gfd->ps->uniformBlockCount; j++)
         {
            gx2_update_uniform_block(wiiu, i, wiiu->pass[i].ps_ubos[j], j,
                  wiiu->pass[i].gfd->ps->uniformBlocks[j].size,
                  wiiu->pass[i].gfd->ps->uniformVarCount, wiiu->pass[i].gfd->ps->uniformVars,
                  frame_count, frame_direction, rotation, core_aspect, core_aspect_rot,
                  frame_time_delta, original_fps);
            GX2SetPixelUniformBlock(wiiu->pass[i].gfd->ps->uniformBlocks[j].offset,
                  wiiu->pass[i].gfd->ps->uniformBlocks[j].size, wiiu->pass[i].ps_ubos[j]);
         }

         for (j = 0; j < wiiu->pass[i].gfd->ps->samplerVarCount; j++)
         {
            int k;
            if (string_is_equal(wiiu->pass[i].gfd->ps->samplerVars[j].name, "Source"))
            {
               GX2SetPixelTexture(texture, wiiu->pass[i].gfd->ps->samplerVars[j].location);
               GX2SetPixelSampler(wiiu->shader_preset->pass[i].filter ?
                     &wiiu->sampler_linear[wiiu->shader_preset->pass[i].wrap] :
                     &wiiu->sampler_nearest[wiiu->shader_preset->pass[i].wrap],
                     wiiu->pass[i].gfd->ps->samplerVars[j].location);
               continue;
            }

            if (string_is_equal(wiiu->pass[i].gfd->ps->samplerVars[j].name, "Original"))
            {
               GX2SetPixelTexture(&wiiu->texture, wiiu->pass[i].gfd->ps->samplerVars[j].location);
               GX2SetPixelSampler(wiiu->shader_preset->pass[0].filter ?
                     &wiiu->sampler_linear[wiiu->shader_preset->pass[0].wrap] :
                     &wiiu->sampler_nearest[wiiu->shader_preset->pass[0].wrap],
                     wiiu->pass[i].gfd->ps->samplerVars[j].location);
               continue;
            }

            if (!strncmp(wiiu->pass[i].gfd->ps->samplerVars[j].name, "OriginalHistory", STRLEN_CONST("OriginalHistory")))
            {
               GX2Texture *source;
               unsigned index     = strtoul(wiiu->pass[i].gfd->ps->samplerVars[j].name + STRLEN_CONST("OriginalHistory"), NULL, 0);
               if (index > i)
                  index           = 0;
               if (index)
                  index           = i - index;

               source             = (index > 0) ? &wiiu->pass[index - 1].texture : &wiiu->texture;
               GX2SetPixelTexture(source, wiiu->pass[i].gfd->ps->samplerVars[j].location);
               GX2SetPixelSampler(wiiu->shader_preset->pass[index].filter ?
                     &wiiu->sampler_linear[wiiu->shader_preset->pass[index].wrap] :
                     &wiiu->sampler_nearest[wiiu->shader_preset->pass[index].wrap],
                     wiiu->pass[i].gfd->ps->samplerVars[j].location);
               continue;
            }

            if ((i > 0) && !strncmp(wiiu->pass[i].gfd->ps->samplerVars[j].name, "PassOutput", STRLEN_CONST("PassOutput")))
            {
               unsigned index = strtoul(wiiu->pass[i].gfd->ps->samplerVars[j].name + STRLEN_CONST("PassOutput"), NULL, 0);
               if (index > i - 1)
                  index       = i - 1;
               GX2SetPixelTexture(&wiiu->pass[index].texture, wiiu->pass[i].gfd->ps->samplerVars[j].location);
               GX2SetPixelSampler(wiiu->shader_preset->pass[index].filter ?
                     &wiiu->sampler_linear[wiiu->shader_preset->pass[index].wrap] :
                     &wiiu->sampler_nearest[wiiu->shader_preset->pass[index].wrap],
                     wiiu->pass[i].gfd->ps->samplerVars[j].location);
               continue;
            }

            /* feedback not supported yet */
            if (!strncmp(wiiu->pass[i].gfd->ps->samplerVars[j].name, "PassFeedback", STRLEN_CONST("PassFeedback")))
            {
               unsigned index = strtoul(wiiu->pass[i].gfd->ps->samplerVars[j].name + STRLEN_CONST("PassFeedback"), NULL, 0);
               if (index > wiiu->shader_preset->passes - 1)
                  index       = wiiu->shader_preset->passes - 1;

               GX2SetPixelTexture(&wiiu->pass[index].texture, wiiu->pass[i].gfd->ps->samplerVars[j].location);
               GX2SetPixelSampler(wiiu->shader_preset->pass[i].filter ?
                     &wiiu->sampler_linear[wiiu->shader_preset->pass[i].wrap] :
                     &wiiu->sampler_nearest[wiiu->shader_preset->pass[i].wrap],
                     wiiu->pass[i].gfd->ps->samplerVars[j].location);
               continue;
            }

            for (k = 0; k < wiiu->shader_preset->luts; k++)
            {
               if (wiiu->luts[k].surface.image
                     && string_is_equal(wiiu->pass[i].gfd->ps->samplerVars[j].name, wiiu->shader_preset->lut[k].id))
               {
                  GX2SetPixelTexture(&wiiu->luts[k], wiiu->pass[i].gfd->ps->samplerVars[j].location);
                  GX2SetPixelSampler(wiiu->shader_preset->lut[k].filter ?
                        &wiiu->sampler_linear[wiiu->shader_preset->lut[k].wrap] :
                        &wiiu->sampler_nearest[wiiu->shader_preset->lut[k].wrap],
                        wiiu->pass[i].gfd->ps->samplerVars[j].location);
               }

            }
         }

         if (wiiu->pass[i].color_buffer.surface.image)
         {
            GX2SetColorBuffer(&wiiu->pass[i].color_buffer, GX2_RENDER_TARGET_0);
            GX2SetViewport(0.0f, 0.0f, wiiu->pass[i].color_buffer.surface.width,
                           wiiu->pass[i].color_buffer.surface.height, 0.0f,
                           1.0f);
            GX2SetScissor(0, 0, wiiu->pass[i].color_buffer.surface.width,
                          wiiu->pass[i].color_buffer.surface.height);
            GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);

            GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, wiiu->pass[i].texture.surface.image,
                          wiiu->pass[i].texture.surface.imageSize);
            texture = &wiiu->pass[i].texture;
         }
         else
         {
            texture = NULL;
            break;
         }
      }

      GX2SetColorBuffer(&wiiu->color_buffer, GX2_RENDER_TARGET_0);
   }

   if (texture)
   {
      GX2SetVertexShader(&frame_shader.vs);
      GX2SetVertexUniformBlock(frame_shader.vs.uniformBlocks[0].offset,
                               frame_shader.vs.uniformBlocks[0].size, wiiu->ubo_mvp);

      GX2SetPixelShader(&frame_shader.ps);
      GX2SetPixelTexture(texture, frame_shader.ps.samplerVars[0].location);
      GX2SetPixelSampler(wiiu->smooth ? &wiiu->sampler_linear[RARCH_WRAP_DEFAULT] :
                         &wiiu->sampler_nearest[RARCH_WRAP_DEFAULT],
                         frame_shader.ps.samplerVars[0].location);
   }

   GX2SetViewport(wiiu->vp.x, wiiu->vp.y, wiiu->vp.width, wiiu->vp.height, 0.0f, 1.0f);
   GX2SetScissor(0, 0, wiiu->color_buffer.surface.width, wiiu->color_buffer.surface.height);
   GX2DrawEx(GX2_PRIMITIVE_MODE_QUADS, 4, 0, 1);

   GX2SetShaderMode(GX2_SHADER_MODE_GEOMETRY_SHADER);
   GX2SetShader(&sprite_shader);
   GX2SetGeometryShaderInputRingBuffer(wiiu->input_ring_buffer, wiiu->input_ring_buffer_size);
   GX2SetGeometryShaderOutputRingBuffer(wiiu->output_ring_buffer, wiiu->output_ring_buffer_size);
   GX2SetVertexUniformBlock(sprite_shader.vs.uniformBlocks[0].offset,
                            sprite_shader.vs.uniformBlocks[0].size,
                            wiiu->ubo_vp);
   GX2SetVertexUniformBlock(sprite_shader.vs.uniformBlocks[1].offset,
                            sprite_shader.vs.uniformBlocks[1].size,
                            wiiu->ubo_tex);
   GX2SetViewport(0.0f, 0.0f, wiiu->color_buffer.surface.width, wiiu->color_buffer.surface.height,
                  0.0f, 1.0f);

#ifdef HAVE_OVERLAY
   if (wiiu->overlay_enable)
      gx2_render_overlay(wiiu);
#endif

#ifdef HAVE_MENU
   if (wiiu->menu.enable)
   {
      GX2SetAttribBuffer(0, 4 * sizeof(*wiiu->menu.v), sizeof(*wiiu->menu.v), wiiu->menu.v);

      GX2SetPixelTexture(&wiiu->menu.texture, sprite_shader.ps.samplerVars[0].location);
      GX2SetPixelSampler(wiiu->smooth ? &wiiu->sampler_linear[RARCH_WRAP_DEFAULT] :
                         &wiiu->sampler_nearest[RARCH_WRAP_DEFAULT],
                         sprite_shader.ps.samplerVars[0].location);

      GX2DrawEx(GX2_PRIMITIVE_MODE_POINTS, 1, 0, 1);
   }
#endif

   wiiu->vertex_cache.current     = 0;
   wiiu->vertex_cache_tex.current = 0;
   GX2SetAttribBuffer(0, wiiu->vertex_cache.size * sizeof(*wiiu->vertex_cache.v),
                      sizeof(*wiiu->vertex_cache.v), wiiu->vertex_cache.v);
   GX2SetPixelSampler(&wiiu->sampler_linear[RARCH_WRAP_EDGE],
                      sprite_shader.ps.samplerVars[0].location);

   wiiu->render_msg_enabled = true;

#ifdef HAVE_MENU
   if (wiiu->menu.enable)
      menu_driver_frame(menu_is_alive, video_info);
   else
#endif
      if (statistics_show)
      {
         if (osd_params)
            font_driver_render_msg(wiiu, video_info->stat_text,
                  osd_params, NULL);
      }

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

   if (msg)
      font_driver_render_msg(wiiu, msg, NULL, NULL);

   wiiu->render_msg_enabled = false;

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER,
                 wiiu->vertex_cache.v,
                 wiiu->vertex_cache.current
                 * sizeof(*wiiu->vertex_cache.v));
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER,
                 wiiu->vertex_cache_tex.v,
                 wiiu->vertex_cache_tex.current
                 * sizeof(*wiiu->vertex_cache_tex.v));

   if (wiiu->menu.enable)
      GX2DrawDone();

   GX2CopyColorBufferToScanBuffer(&wiiu->color_buffer, GX2_SCAN_TARGET_DRC);
   GX2CopyColorBufferToScanBuffer(&wiiu->color_buffer, GX2_SCAN_TARGET_TV);

   GX2SwapScanBuffers();
   GX2Flush();

   return true;
}

static void gx2_set_nonblock_state(void *data, bool toggle,
      bool adaptive_vsync_enabled, unsigned swap_interval)
{
   wiiu_video_t *wiiu = (wiiu_video_t *) data;

   if (!wiiu)
      return;

   wiiu->vsync = !toggle;
   GX2SetSwapInterval(!toggle);  /* do we need this ? */
}

static bool gx2_alive(void *data) { return true; }
static bool gx2_focus(void *data) { return true; }
static bool gx2_suppress_screensaver(void *a, bool b) { return false; }

static struct video_shader *gx2_get_current_shader(void *data)
{
   wiiu_video_t *wiiu = (wiiu_video_t *)data;

   if (!wiiu)
      return NULL;

   return wiiu->shader_preset;
}

static void gx2_set_rotation(void *data,
      unsigned rotation)
{
   wiiu_video_t *wiiu = (wiiu_video_t *) data;

   if (wiiu)
   {
      wiiu->rotation = rotation;
      gx2_set_projection(wiiu);
      wiiu->should_resize = true;
   }
}

static void gx2_viewport_info(void *data,
      struct video_viewport *vp)
{
   wiiu_video_t *wiiu = (wiiu_video_t *) data;

   if (wiiu)
      *vp = wiiu->vp;
}

static uintptr_t gx2_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   uint32_t i;
   GX2Texture *texture          = NULL;
   struct texture_image *image  = (struct texture_image *)data;

   if (!image)
      return 0;

   if (!(texture = (GX2Texture*)calloc(1, sizeof(GX2Texture))))
      return 0;

   texture->surface.width       = image->width;
   texture->surface.height      = image->height;
   texture->surface.depth       = 1;
   texture->surface.dim         = GX2_SURFACE_DIM_TEXTURE_2D;
   texture->surface.tileMode    = GX2_TILE_MODE_LINEAR_ALIGNED;
   texture->viewNumSlices       = 1;

   texture->surface.format      = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
   texture->compMap             = GX2_COMP_SEL(_G, _B, _A, _R);

   GX2CalcSurfaceSizeAndAlignment(&texture->surface);
   GX2InitTextureRegs(texture);
   texture->surface.image       = MEM2_alloc(
         texture->surface.imageSize, texture->surface.alignment);

   for (i = 0; (i < image->height) && (i < texture->surface.height); i++)
      memcpy((uint32_t *)texture->surface.image
            + (i * texture->surface.pitch),
            image->pixels + (i * image->width),
            image->width * sizeof(image->pixels));

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE,
         texture->surface.image,
         texture->surface.imageSize);

   return (uintptr_t)texture;
}

static void gx2_unload_texture(void *data,
      bool threaded, uintptr_t handle)
{
   GX2Texture *texture = (GX2Texture *)handle;

   if (!texture)
      return;

   MEM2_free(texture->surface.image);
   free(texture);
}

static void gx2_set_filtering(void *data,
      unsigned index, bool smooth, bool ctx_scaling)
{
   wiiu_video_t *wiiu = (wiiu_video_t *) data;

   if (wiiu)
      wiiu->smooth = smooth;
}

static void gx2_apply_state_changes(void *data)
{
   wiiu_video_t *wiiu = (wiiu_video_t *)data;

   if (wiiu)
      wiiu->should_resize = true;
}

static void gx2_set_texture_frame(void *data,
      const void *frame, bool rgb32,
      unsigned width, unsigned height, float alpha)
{
   uint32_t i;
   const uint16_t *src = NULL;
   uint16_t *dst       = NULL;
   wiiu_video_t *wiiu  = (wiiu_video_t *) data;

   if (!wiiu)
      return;

   if (!frame || !width || !height)
      return;

   if (width > wiiu->menu.texture.surface.width)
      width = wiiu->menu.texture.surface.width;

   if (height > wiiu->menu.texture.surface.height)
      height = wiiu->menu.texture.surface.height;

   wiiu->menu.width  = width;
   wiiu->menu.height = height;

   src               = frame;
   dst               = (uint16_t *)wiiu->menu.texture.surface.image;

   for (i = 0; i < height; i++)
   {
      memcpy(dst, src, width * sizeof(uint16_t));
      dst += wiiu->menu.texture.surface.pitch;
      src += width;
   }

   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, wiiu->menu.texture.surface.image,
                 wiiu->menu.texture.surface.imageSize);

   wiiu->menu.v->pos.x        = wiiu->vp.x;
   wiiu->menu.v->pos.y        = wiiu->vp.y;
   wiiu->menu.v->pos.width    = wiiu->vp.width;
   wiiu->menu.v->pos.height   = wiiu->vp.height;
   wiiu->menu.v->coord.u      = 0.0f;
   wiiu->menu.v->coord.v      = 0.0f;
   wiiu->menu.v->coord.width  = (float)width / wiiu->menu.texture.surface.width;
   wiiu->menu.v->coord.height = (float)height / wiiu->menu.texture.surface.height;
   GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, wiiu->menu.v, 4 * sizeof(*wiiu->menu.v));

}

static void gx2_set_texture_enable(void *data, bool state, bool full_screen)
{
   wiiu_video_t *wiiu = (wiiu_video_t *) data;
   if (wiiu)
      wiiu->menu.enable = state;
}

static void gx2_set_osd_msg(void *data, const char *msg,
      const struct font_params *params, void *font)
{
   wiiu_video_t *wiiu = (wiiu_video_t *)data;
   if (wiiu && wiiu->render_msg_enabled)
      font_driver_render_msg(wiiu, msg, params, font);
}

static uint32_t gx2_get_flags(void *data)
{
   uint32_t flags = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_SCREENSHOTS_SUPPORTED);
#if defined(HAVE_SLANG) && defined(HAVE_SPIRV_CROSS)
   BIT32_SET(flags, GFX_CTX_FLAGS_SHADERS_SLANG);
#endif

   return flags;
}

static const video_poke_interface_t gx2_poke_interface = {
   gx2_get_flags,
   gx2_load_texture,
   gx2_unload_texture,
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   gx2_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   gx2_set_aspect_ratio,
   gx2_apply_state_changes,
   gx2_set_texture_frame,
   gx2_set_texture_enable,
   gx2_set_osd_msg,
   NULL, /* show_mouse */
   NULL, /* grab_mouse_toggle */
   gx2_get_current_shader,
   NULL, /* get_current_software_framebuffer */
   NULL, /* get_hw_render_interface */
   NULL, /* set_hdr_max_nits */
   NULL, /* set_hdr_paper_white_nits */
   NULL, /* set_hdr_contrast */
   NULL  /* set_hdr_expand_gamut */
};

static void gx2_get_poke_interface(void *data,
      const video_poke_interface_t **iface) { *iface = &gx2_poke_interface; }

#ifdef HAVE_GFX_WIDGETS
static bool gx2_widgets_enabled(void *data) { return true; }
#endif

video_driver_t video_wiiu =
{
   gx2_init,
   gx2_frame,
   gx2_set_nonblock_state,
   gx2_alive,
   gx2_focus,
   gx2_suppress_screensaver,
   NULL, /* has_windowed */
   gx2_set_shader,
   gx2_free,
   "gx2",
   NULL, /* set_viewport */
   gx2_set_rotation,
   gx2_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   gx2_get_overlay_interface,
#endif
   gx2_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   gx2_widgets_enabled
#endif
};

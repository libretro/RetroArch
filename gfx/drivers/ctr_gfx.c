/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2014-2017 - Ali Bouhlel
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
#include <stdint.h>
#include <string.h>
#include <malloc.h>
#include <math.h>

#include <3ds.h>

#include <retro_inline.h>
#include <encodings/utf.h>
#include <retro_math.h>
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

#include "../font_driver.h"
#include "../../ctr/gpu_old.h"
#include "ctr_gu.h"

#include "../../configuration.h"
#include "../../command.h"
#include "../../driver.h"

#include "../../retroarch.h"
#include "../../runloop.h"
#include "../../verbosity.h"

#include "../common/ctr_defines.h"
#ifndef HAVE_THREADS
#include "../../tasks/tasks_internal.h"
#endif

#ifdef HAVE_BOTTOM_SCREEN
#include "../../ctr/ctr-bottom/ctr_bottom.h"
#include "../../ctr/ctr-bottom/ctr_bottom_gfx.h"
#include "../../ctr/ctr-bottom/ctr_bottom_kbd.h"
#include "../../ctr/ctr-bottom/ctr_bottom_states.h"
#include "../../ctr/ctr-bottom/ctr_bottom_debug.h"
#endif

#include "../../input/common/ctr_common.h"
/*
struct ctr_bottom_texture_data
{
   uintptr_t texture;
   ctr_vertex_t* frame_coords;
   ctr_scale_vector_t scale_vector;
};
*/
typedef struct
{
   ctr_texture_t texture;
   ctr_scale_vector_t scale_vector_top;
   ctr_scale_vector_t scale_vector_bottom;
   const font_renderer_driver_t* font_driver;
   void* font_data;
} ctr_font_t;


/* An annoyance...
 * Have to keep track of bottom screen enable state
 * externally, otherwise cannot detect current state
 * when reinitialising... */

//static bool ctr_bottom_screen_enabled  = true;
//static int fade_count                  = 256;

/*
 * FORWARD DECLARATIONS
 */
 
#ifdef HAVE_OVERLAY
static void ctr_render_overlay(ctr_video_t *ctr);
#endif
static void ctr_bottom_set_screen_enable(bool enabled, bool idle);

/*
 * DISPLAY DRIVER 
 */

static void gfx_display_ctr_draw(gfx_display_ctx_draw_t *draw,
      void *data, unsigned video_width, unsigned video_height)
{
   ctr_scale_vector_t scale_vector;
   int colorR, colorG, colorB, colorA;
   ctr_scale_vector_t *vec          = NULL;
   ctr_vertex_t *v                  = NULL;
   struct ctr_texture *texture      = NULL;
   const float *color               = NULL;
   ctr_video_t             *ctr     = (ctr_video_t*)data;

   if (!ctr || !draw)
      return;

   texture            = (struct ctr_texture*)draw->texture;
   color              = draw->coords->color;

   if (!texture)
      return;

   vec                = &scale_vector;
   CTR_SET_SCALE_VECTOR(
         vec,
         CTR_TOP_FRAMEBUFFER_WIDTH,
         CTR_TOP_FRAMEBUFFER_HEIGHT,
         texture->width,
         texture->height);
   GPUCMD_AddWrite(GPUREG_GSH_BOOLUNIFORM, 0);
   ctrGuSetVertexShaderFloatUniform(0, (float*)&scale_vector, 1);

   if ((ctr->vertex_cache.size - (ctr->vertex_cache.current 
               - ctr->vertex_cache.buffer)) < 1)
      ctr->vertex_cache.current = ctr->vertex_cache.buffer;

   v     = ctr->vertex_cache.current++;

   v->x0 = draw->x;
   v->y0 = 240 - draw->height - draw->y;
   v->x1 = v->x0 + draw->width;
   v->y1 = v->y0 + draw->height;
   v->u0 = 0;
   v->v0 = 0;
   v->u1 = texture->active_width;
   v->v1 = texture->active_height;

   ctrGuSetAttributeBuffers(2,
         VIRT_TO_PHYS(v),
         CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 0 |
         CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 4,
         sizeof(ctr_vertex_t));

   color  = draw->coords->color;
   colorR = (int)((*color++)*255.f);
   colorG = (int)((*color++)*255.f);
   colorB = (int)((*color++)*255.f);
   colorA = (int)((*color++)*255.f);

   GPU_SetTexEnv(0,
         GPU_TEVSOURCES(GPU_TEXTURE0, GPU_CONSTANT, 0),
         GPU_TEVSOURCES(GPU_TEXTURE0, GPU_CONSTANT, 0),
         0,
         0,
         GPU_MODULATE, GPU_MODULATE,
         COLOR_ABGR(colorR,colorG,colorB,colorA)
         );

#if 0
   GPU_SetTexEnv(0,
         GPU_TEVSOURCES(GPU_CONSTANT, GPU_CONSTANT, 0),
         GPU_TEVSOURCES(GPU_CONSTANT, GPU_CONSTANT, 0),
         0,
         GPU_TEVOPERANDS(GPU_TEVOP_RGB_SRC_COLOR, GPU_TEVOP_RGB_SRC_COLOR, 0),
         GPU_REPLACE, GPU_REPLACE,
         0x3FFFFFFF);
#endif

   ctrGuSetTexture(GPU_TEXUNIT0,
         VIRT_TO_PHYS(texture->data),
         texture->width,
         texture->height,
           GPU_TEXTURE_MAG_FILTER(GPU_LINEAR)  
         | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR) 
         | GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE)
         | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
         GPU_RGBA8);

   GPU_SetViewport(NULL,
         VIRT_TO_PHYS(ctr->drawbuffers.top.left),
         0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
         ctr->video_mode == CTR_VIDEO_MODE_2D_800X240 
         ? CTR_TOP_FRAMEBUFFER_WIDTH * 2 
         : CTR_TOP_FRAMEBUFFER_WIDTH);

   GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);

   if (ctr->video_mode == CTR_VIDEO_MODE_3D)
   {
      GPU_SetViewport(NULL,
            VIRT_TO_PHYS(ctr->drawbuffers.top.right),
            0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
            CTR_TOP_FRAMEBUFFER_WIDTH);
      GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);
   }

   GPU_SetTexEnv(0, GPU_TEXTURE0, GPU_TEXTURE0, 0, 0, GPU_REPLACE, GPU_REPLACE, 0);
}

gfx_display_ctx_driver_t gfx_display_ctx_ctr = {
   gfx_display_ctr_draw,
   NULL,                                     /* draw_pipeline          */
   NULL,                                     /* blend_begin            */
   NULL,                                     /* blend_end              */
   NULL,                                     /* get_default_mvp        */
   NULL,                                     /* get_default_vertices   */
   NULL,                                     /* get_default_tex_coords */
   FONT_DRIVER_RENDER_CTR,
   GFX_VIDEO_DRIVER_CTR,
   "ctr",
   true,
   NULL,
   NULL
};

/*
 * FONT DRIVER 
 */

static void* ctr_font_init(void* data, const char* font_path,
      float font_size, bool is_threaded)
{
   unsigned int i, j;
   ctr_scale_vector_t *vec_top    = NULL;
   ctr_scale_vector_t *vec_bottom = NULL;
   const uint8_t*     src         = NULL;
   uint8_t* tmp                   = NULL;
   const struct font_atlas* atlas = NULL;
   ctr_font_t* font               = (ctr_font_t*)calloc(1, sizeof(*font));
   ctr_video_t* ctr               = (ctr_video_t*)data;

   if (!font)
      return NULL;

   font_size                      = 10;
   if (!font_renderer_create_default(
            &font->font_driver,
            &font->font_data, font_path, font_size))
   {
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

   vec_top    = &font->scale_vector_top;
   vec_bottom = &font->scale_vector_bottom;

   CTR_SET_SCALE_VECTOR(
         vec_top,
         CTR_TOP_FRAMEBUFFER_WIDTH, 
         CTR_TOP_FRAMEBUFFER_HEIGHT,
         font->texture.width,
         font->texture.height);

   CTR_SET_SCALE_VECTOR(
         vec_bottom,
         CTR_BOTTOM_FRAMEBUFFER_WIDTH, 
         CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
         font->texture.width,
         font->texture.height);

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
      size_t msg_len, float scale)
{
   size_t i;
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
      ctr_font_t* font, const char* msg, size_t msg_len,
      float scale, const unsigned int color, float pos_x,
      float pos_y,
      unsigned width, unsigned height, unsigned text_align)
{
   unsigned int i;
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
   float line_height;
   struct font_line_metrics *line_metrics = NULL;
   int lines                              = 0;

   font->font_driver->get_line_metrics(font->font_data, &line_metrics);
   line_height = (float)line_metrics->height * scale / (float)height;

   for (;;)
   {
      const char* delim = strchr(msg, '\n');
      size_t msg_len    = delim ? (size_t)(delim - msg) : strlen(msg);

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
   int drop_x, drop_y;
   unsigned color, r, g, b, alpha;
   enum text_alignment text_align;
   float x, y, scale, drop_mod, drop_alpha;
   ctr_font_t *font           = (ctr_font_t*)data;
   ctr_video_t *ctr           = (ctr_video_t*)userdata;
   unsigned width             = ctr->render_font_bottom ?
      CTR_BOTTOM_FRAMEBUFFER_WIDTH : CTR_TOP_FRAMEBUFFER_WIDTH;
   unsigned height            = ctr->render_font_bottom ?
      CTR_BOTTOM_FRAMEBUFFER_HEIGHT : CTR_TOP_FRAMEBUFFER_HEIGHT;

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
   if (font && font->font_driver)
      return font->font_driver->get_glyph((void*)font->font_driver, code);
   return NULL;
}

static bool ctr_font_get_line_metrics(void* data, struct font_line_metrics **metrics)
{
   ctr_font_t* font = (ctr_font_t*)data;
   if (font && font->font_driver && font->font_data)
   {
      font->font_driver->get_line_metrics(font->font_data, metrics);
      return true;
   }
   return false;
}

font_renderer_t ctr_font =
{
   ctr_font_init,
   ctr_font_free,
   ctr_font_render_msg,
   "ctr",
   ctr_font_get_glyph,
   NULL,                         /* bind_block */
   NULL,                         /* flush_block */
   ctr_font_get_message_width,
   ctr_font_get_line_metrics
};

/*
 * VIDEO DRIVER 
 */

static INLINE void ctr_check_3D_slider(ctr_video_t *ctr, ctr_video_mode_enum video_mode)
{
   float slider_val = *(float*)0x1FF81080;

   if (slider_val == 0.0f)
   {
      ctr->video_mode = CTR_VIDEO_MODE_2D;
      ctr->enable_3d = false;
      return;
   }

   switch (video_mode)
   {
      case CTR_VIDEO_MODE_3D:
         {
            s16 offset = slider_val * 10.0f;

            ctr->video_mode = CTR_VIDEO_MODE_3D;

            ctr->frame_coords[1] = ctr->frame_coords[0];
            ctr->frame_coords[2] = ctr->frame_coords[0];

            ctr->frame_coords[1].x0 -= offset;
            ctr->frame_coords[1].x1 -= offset;
            ctr->frame_coords[2].x0 += offset;
            ctr->frame_coords[2].x1 += offset;

            GSPGPU_FlushDataCache(ctr->frame_coords, 3 * sizeof(ctr_vertex_t));

            if (ctr->supports_parallax_disable)
               ctr_set_parallax_layer(true);
            ctr->enable_3d = true;
         }
         break;
      case CTR_VIDEO_MODE_2D_400X240:
      case CTR_VIDEO_MODE_2D_800X240:
         if (ctr->supports_parallax_disable)
         {
            ctr->video_mode = video_mode;
            ctr_set_parallax_layer(false);
            ctr->enable_3d = true;
         }
         else
         {
            ctr->video_mode = CTR_VIDEO_MODE_2D;
            ctr->enable_3d = false;
         }
         break;
      case CTR_VIDEO_MODE_2D:
      default:
         ctr->video_mode = CTR_VIDEO_MODE_2D;
         if (ctr->supports_parallax_disable)
            ctr_set_parallax_layer(false);
         ctr->enable_3d = false;
         break;
   }
}

#ifdef HAVE_OVERLAY
static void ctr_free_overlay(ctr_video_t *ctr)
{
   unsigned int i;

   for (i = 0; i < ctr->overlays; i++)
   {
      linearFree(ctr->overlay[i].frame_coords);
      linearFree(ctr->overlay[i].frame_coords_bottom);
      linearFree(ctr->overlay[i].texture.data);
   }

   free(ctr->overlay);
   ctr->overlay  = NULL;
   ctr->overlays = 0;
}
#endif

static INLINE void ctr_set_screen_geom(ctr_video_t *ctr, bool dual)
{
int width, height;

   if (width > ctr->texture_width)
      width = ctr->texture_width;
   if (height > ctr->texture_height)
      height = ctr->texture_height;

#ifdef HAVE_BOTTOM_SCREEN
      if (dual)
      {
         if (ctr->rotation == 0) /* 0° */
         {
            ctr->frame_coords->u0 = 0;
            ctr->frame_coords->v0 = 0;
            ctr->frame_coords->u1 = width;
            ctr->frame_coords->v1 = height / 2;
			
            if (video_ctr_dual_offset_y != 0 )
            {
               float tmp_percent = ( ((float)ctr->frame_coords->y1 - (float)ctr->frame_coords->y0) / (float)ctr->vp.height );
               ctr->frame_coords->v1 = roundf(tmp_percent * height);
            }
         }
         else if (ctr->rotation == 1) /* 90° */
         {
            ctr->frame_coords->u0 = 0;
            ctr->frame_coords->v0 = 0;
            ctr->frame_coords->u1 = width / 2;
            ctr->frame_coords->v1 = height;

            if (video_ctr_dual_offset_y != 0 )
            {
               float tmp_percent = ( ((float)ctr->frame_coords->y1 - (float)ctr->frame_coords->y0) / (float)ctr->vp.height );
               ctr->frame_coords->u1 = roundf(tmp_percent * height);
            }
         }
         else if (ctr->rotation == 2) /* 180° */
         {
            ctr->frame_coords->u0 = 0;
            ctr->frame_coords->v0 = height / 2;
            ctr->frame_coords->u1 = width;
            ctr->frame_coords->v1 = height;

            if (video_ctr_dual_offset_y != 0 )
            {
               float tmp_percent = ( ((float)ctr->frame_coords->y1 - (float)ctr->frame_coords->y0) / (float)ctr->vp.height );
               ctr->frame_coords->v1 = roundf(tmp_percent * height);
            }
         }
         else if (ctr->rotation == 3) /* 270° */
         {
            ctr->frame_coords->u0 = width / 2;
            ctr->frame_coords->v0 = 0;
            ctr->frame_coords->u1 = width;
            ctr->frame_coords->v1 = height;

            if (video_ctr_dual_offset_y != 0 )
            {
               float tmp_percent = ( ((float)ctr->frame_coords->y1 - (float)ctr->frame_coords->y0) / (float)ctr->vp.width );
               ctr->frame_coords->u1 = roundf(tmp_percent * width);
            }
         }
      }
      else
#endif
      {
         ctr->frame_coords->u1 = width;
         ctr->frame_coords->v1 = height;
      }

}

static INLINE void ctr_set_screen_coords(ctr_video_t *ctr, bool dual)
{
   unsigned offset = 0; //float?
   unsigned width = ctr->vp.width; // float?
   unsigned height = ctr->vp.height; // float?

   if (dual)
   {
//      if ((ctr->rotation == 1) || (ctr->rotation == 3)) /* 90°, 270° */
//      {
//         offset = (CTR_BOTTOM_FRAMEBUFFER_HEIGHT - (ctr->vp.width / 2));
//         width /= 2;
//      }
//      else
//      {
         offset = (CTR_BOTTOM_FRAMEBUFFER_HEIGHT - (ctr->vp.height / 2));
         height /= 2;
//      }
   }

   if (ctr->rotation == 0) /* 0°  rotation OK */
   {
      ctr->frame_coords->x0 = ctr->vp.x;
      ctr->frame_coords->y0 = dual? offset:ctr->vp.y;
      ctr->frame_coords->x1 = ctr->vp.x + ctr->vp.width;
      ctr->frame_coords->y1 = ctr->frame_coords->y0 + height;
   }
   else if (ctr->rotation == 1) /* 90°  rotation OK */
   {
      ctr->frame_coords->x0 = ctr->vp.x;
      ctr->frame_coords->y0 = dual? offset:ctr->vp.y;
      ctr->frame_coords->x1 = ctr->vp.x + ctr->vp.width;
      ctr->frame_coords->y1 = ctr->frame_coords->y0 + height;
   }
   else if (ctr->rotation == 2) /* 180°  rotation */
{
    ctr->frame_coords->x0 = ctr->vp.x + ctr->vp.width;
    ctr->frame_coords->y0 = ctr->vp.y + height;
    ctr->frame_coords->x1 = ctr->vp.x;
    ctr->frame_coords->y1 = dual ? offset : ctr->vp.y;
}
else if (ctr->rotation == 3) /* 270° rotation */
{
    ctr->frame_coords->x0 = ctr->vp.x + ctr->vp.width;
    ctr->frame_coords->y0 = ctr->vp.y + height;
    ctr->frame_coords->x1 = ctr->vp.x;
    ctr->frame_coords->y1 = dual ? offset : ctr->vp.y;
}

#ifdef HAVE_BOTTOM_SCREEN /* TODO: only set when bottom enabled */
   if (ctr->rotation == 0) /* 0° rotation OK offset OK*/
   {
      ctr->frame_coords_bottom->x0 = ctr->vp.x;
      ctr->frame_coords_bottom->y0 = dual? 0:ctr->vp.y;
      ctr->frame_coords_bottom->x1 = ctr->vp.x + ctr->vp.width;
      ctr->frame_coords_bottom->y1 = ctr->frame_coords_bottom->y0 + height;
   }
   else if (ctr->rotation == 1)/* 90° rotation OK*/
   {
      ctr->frame_coords_bottom->x0 = ctr->vp.x;
      ctr->frame_coords_bottom->y0 = dual? 0:ctr->vp.y;
      ctr->frame_coords_bottom->x1 = ctr->vp.x + ctr->vp.width;
      ctr->frame_coords_bottom->y1 = ctr->frame_coords_bottom->y0 + height;
   }
   else if (ctr->rotation == 2) /* 180° rotation OK*/
   {
      ctr->frame_coords_bottom->x1 = ctr->vp.x;
      ctr->frame_coords_bottom->y1 = dual? 0:ctr->vp.y;
      ctr->frame_coords_bottom->x0 = ctr->vp.x + ctr->vp.width;
      ctr->frame_coords_bottom->y0 = ctr->frame_coords_bottom->y1 + height;
   }
   else if (ctr->rotation == 3) /* 270° rotation OK*/
   {
      ctr->frame_coords_bottom->x1 = ctr->vp.x;
      ctr->frame_coords_bottom->y1 = dual? 0:ctr->vp.y;
      ctr->frame_coords_bottom->x0 = ctr->vp.x + ctr->vp.width;
      ctr->frame_coords_bottom->y0 = ctr->frame_coords_bottom->y1 + height;
   }

   if (ctr->vp.full_width == CTR_BOTTOM_FRAMEBUFFER_WIDTH)
   {
      ctr->frame_coords->x0 += 40; // required to align frame to center of topscreen
      ctr->frame_coords->x1 += 40; // required to align frame to center of topscreen
   }
#endif
}

static void ctr_update_viewport(
      ctr_video_t* ctr,
      settings_t *settings,
      int custom_vp_x,
      int custom_vp_y,
      unsigned custom_vp_width,
      unsigned custom_vp_height
      )
{
   int x                            = 0;
   int y                            = 0;
   float width                      = ctr->vp.full_width;
   float height                     = ctr->vp.full_height;
   float desired_aspect             = video_driver_get_aspect_ratio();
   bool video_scale_integer         = settings->bools.video_scale_integer;
   unsigned aspect_ratio_idx        = settings->uints.video_aspect_ratio_idx;
#ifdef HAVE_BOTTOM_SCREEN
   unsigned ctr_bottom_display_mode = settings->uints.ctr_bottom_display_mode;
   unsigned video_ctr_render_target = settings->uints.video_ctr_render_target;
   unsigned video_ctr_dual_deadzone = settings->uints.video_ctr_dual_deadzone;
   int video_ctr_dual_offset_x      = settings->ints.video_ctr_dual_offset_x;
   int video_ctr_dual_offset_y      = settings->ints.video_ctr_dual_offset_y;
#endif

   if (video_scale_integer)
   {
      /* TODO: does CTR use top-left or bottom-left coordinates? assuming top-left. */
      video_viewport_get_scaled_integer(&ctr->vp, ctr->vp.full_width,
          ctr->vp.full_height, desired_aspect, ctr->keep_aspect,
          true);
   }
   else if (ctr->keep_aspect)
   {
#if defined(HAVE_MENU)
      if (aspect_ratio_idx == ASPECT_RATIO_CUSTOM)
      {
         x      = custom_vp_x;
         y      = custom_vp_y;
         width  = custom_vp_width;
         height = custom_vp_height;
      }
      else
#endif
      {
         float delta;
         float device_aspect  = ((float)ctr->vp.full_width) / ctr->vp.full_height;

         if (fabsf(device_aspect - desired_aspect) < 0.0001f)
         {
            /* If the aspect ratios of screen and desired aspect
             * ratio are sufficiently equal (floating point stuff),
             * assume they are actually equal.
             */
         }
         else if (device_aspect > desired_aspect)
         {
            delta    = (desired_aspect / device_aspect - 1.0f)
               / 2.0f + 0.5f;
            x        = (int)roundf(ctr->vp.full_width * (0.5f - delta));
            width    = (unsigned)roundf(2.0f * width * delta);
         }
         else
         {
            delta    = (device_aspect / desired_aspect - 1.0f)
               / 2.0f + 0.5f;
            y        = (int)roundf(height * (0.5f - delta));
            height   = (unsigned)roundf(2.0f * height * delta);
         }
      }
      ctr->vp.x      = x;
      ctr->vp.y      = y;
      ctr->vp.width  = width;
      ctr->vp.height = height;
   }

   else
   {
      ctr->vp.x      = (CTR_TOP_FRAMEBUFFER_WIDTH - width) / 2;
      ctr->vp.y      = (ctr->vp.full_height - height) / 2;
      ctr->vp.width  = width;
      ctr->vp.height = height;
   }
   
      video_viewport_get_scaled_aspect(&ctr->vp, width, height, true);

#ifdef HAVE_BOTTOM_SCREEN
   if (ctr_bottom_display_mode == CTR_BOTTOM_MODE_RETROARCH)
   {
      ctr_set_screen_coords(ctr,(video_ctr_render_target == CTR_VIDEO_TARGET_DUAL)? true:false);

	  if (video_ctr_dual_offset_x != 0)
	  {
         ctr->frame_coords->x0 += video_ctr_dual_offset_x;
         ctr->frame_coords->x1 += video_ctr_dual_offset_x;
         ctr->frame_coords_bottom->x0 += video_ctr_dual_offset_x;
         ctr->frame_coords_bottom->x1 += video_ctr_dual_offset_x;
	  }
  	  if (video_ctr_dual_offset_y != 0)
	  {
         ctr->frame_coords->y0 += video_ctr_dual_offset_y;
//         if (ctr->frame_coords->y1 > height)
//            ctr->frame_coords->y0 += video_ctr_dual_offset_y;
//         ctr->frame_coords->y1 += video_ctr_dual_offset_y;
//         ctr->frame_coords_bottom->y0 += video_ctr_dual_offset_y;
         ctr->frame_coords_bottom->y1 += video_ctr_dual_offset_y;

//         if (ctr->frame_coords_bottom->y1 > height)
//            ctr->frame_coords_bottom->y0 += video_ctr_dual_offset_y;
         
      ctr_set_screen_geom(ctr,(video_ctr_render_target == CTR_VIDEO_TARGET_DUAL)? true:false); //HUUTMP DISABLED

	  }
   }
   else
   {
      ctr_set_screen_coords(ctr, false);
   }
#else
   ctr_set_screen_coords(ctr, false);
#endif
//   ctr->should_resize = false;
}

#ifdef HAVE_BOTTOM_SCREEN
static void ctr_bottom_state_thumbnail_geom(ctr_video_t *ctr)
{
   float scale;
   unsigned width, height;
   int x_offset, y_offset;
   ctr_texture_t *texture            = NULL;
   struct ctr_bottom_texture_data *o = NULL;
   const int target_width            = 120;
   const int target_height           = 90;

   if (ctr)
      o = &ctr->bottom_textures[CTR_BOTTOM_TEXTURE_THUMBNAIL];

   if (!o)
      return;

   if (!(texture = (ctr_texture_t *) o->texture))
      return;

   scale = (float)target_width / texture->active_width;
   if (target_width > texture->active_width * scale)
      scale = (float)(target_width + 1) / texture->active_width;

   o->frame_coords->u0 = 0;
   o->frame_coords->v0 = 0;
   o->frame_coords->u1 = texture->active_width;
   o->frame_coords->v1 = texture->active_height;

   x_offset            = 184;
   y_offset            = 46 + 
      (target_height - texture->active_height * scale) / 2;

   o->frame_coords->x0 = x_offset;
   o->frame_coords->y0 = y_offset;
   o->frame_coords->x1 =   o->frame_coords->x0 
                         + texture->active_width * scale;
   o->frame_coords->y1 =   o->frame_coords->y0 
                         + texture->active_height * scale;

   CTR_SET_SCALE_VECTOR(&o->scale_vector,
         CTR_BOTTOM_FRAMEBUFFER_WIDTH,
         CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
         texture->width,
         texture->height);
}



static bool ctr_bottom_free_menu_texture(ctr_video_t *ctr, unsigned id)
{
   struct ctr_bottom_texture_data *o = &ctr->bottom_textures[id];
   ctr_texture_t *texture            = (ctr_texture_t *) o->texture;

   if (texture)
   {
      linearFree(texture->data);
      linearFree(o->frame_coords);
      o->texture = 0;
   }
}


static bool ctr_bottom_load_menu_texture(ctr_video_t *ctr, unsigned id)
{
   settings_t *settings = config_get_ptr();
   bool lcd_bottom      = settings->bools.ctr_bottom_lcd_enable;
   char *dir_assets     = NULL;

   char state_path[PATH_MAX_LENGTH];

   switch (id)
   {
      case CTR_BOTTOM_TEXTURE_THUMBNAIL:
         if (ctr_bottom_state_gfx.reload_texture)
//   if (!(ctr_bottom_state_savestates.state_slot == config_slot))
         {
            ctr_texture_t *texture            = NULL;
            struct ctr_bottom_texture_data *o = NULL;

            save_state_to_file(ctr);

            ctr_bottom_state_gfx.reload_texture = !ctr_bottom_state_gfx.reload_texture;
//      ctr_bottom_state_savestates.state_slot        = config_slot;
            o       = &ctr->bottom_textures[CTR_BOTTOM_TEXTURE_THUMBNAIL];
            texture = (ctr_texture_t *)o->texture;


            if (texture)
            {
               linearFree(texture->data);
               linearFree(o->frame_coords);
               o->texture = 0;
            }

            if (ctr_update_state_date_from_file(ctr))
            {
               if (gfx_display_reset_textures_list(
                     ctr_bottom_state_gfx.texture_name,
                     ctr_bottom_state_gfx.texture_path,
                     &o->texture,
                     TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
               {
                  o->frame_coords = linearAlloc(sizeof(ctr_vertex_t));
                  ctr_bottom_state_thumbnail_geom(ctr);
                  ctr->render_state_from_png_file = true;
               }
            }

            ctr->refresh_bottom_menu = true;
         }
         return true;

      case CTR_BOTTOM_TEXTURE_GFX:
         dir_assets = settings->paths.directory_ctr_bottom_assets;

         ctr_bottom_free_menu_texture(ctr,CTR_BOTTOM_TEXTURE_GFX);

         if (gfx_display_reset_textures_list(
               ctr_bottom_gfx[ctr_bottom_state_gfx.gfx_id].path,
               dir_assets,
               &ctr->bottom_textures[CTR_BOTTOM_TEXTURE_GFX].texture,
               TEXTURE_FILTER_MIPMAP_LINEAR,
               NULL, NULL))
         {

         struct ctr_bottom_texture_data *o = &ctr->bottom_textures[CTR_BOTTOM_TEXTURE_GFX];
         o->frame_coords = linearAlloc(sizeof(ctr_vertex_t));
         ctr_texture_t *texture = (ctr_texture_t *) o->texture;

         o->frame_coords->u0 = 0;
         o->frame_coords->v0 = 0;
         o->frame_coords->u1 = texture->width;
         o->frame_coords->v1 = texture->height;

         o->frame_coords->x0 = 0;
         o->frame_coords->y0 = 0;
         o->frame_coords->x1 = o->frame_coords->x0 + o->frame_coords->u1;
         o->frame_coords->y1 = o->frame_coords->y0 + o->frame_coords->v1;

         CTR_SET_SCALE_VECTOR(&o->scale_vector,
               CTR_BOTTOM_FRAMEBUFFER_WIDTH,
               CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
               texture->width,
               texture->height);

         }
         return true;
   }
   return false;
}

static bool ctr_bottom_load_kbd_texture(ctr_video_t *ctr)
{
   unsigned i;
   const char *dir_assets = NULL;
   settings_t *settings   = config_get_ptr();
   dir_assets             = settings->paths.directory_ctr_bottom_assets;

   for (i = 0; i < CTR_TEXTURE_KBD_KEY_TAB + 1; i++)
   {
      if (gfx_display_reset_textures_list(
            ctr_bottom_kbd_gfx[i].path, dir_assets,
            &ctr->bottom_kbd_textures[i].texture,
            TEXTURE_FILTER_MIPMAP_LINEAR, NULL, NULL))
      {
      struct ctr_bottom_texture_data *o = &ctr->bottom_kbd_textures[i];
      o->frame_coords = linearAlloc(sizeof(ctr_vertex_t));

      ctr_texture_t *texture = (ctr_texture_t *) o->texture;

      o->frame_coords->u0 = 0;
      o->frame_coords->v0 = 0;
//      o->frame_coords->u1 = (i < 1) ? texture->width : texture->active_width;
//      o->frame_coords->v1 = (i < 1) ? texture->height : texture->active_height;
      o->frame_coords->u1 = texture->active_width;
      o->frame_coords->v1 = texture->active_height;

      o->frame_coords->x0 = ctr_bottom_kbd_gfx[i].x;
      o->frame_coords->y0 = ctr_bottom_kbd_gfx[i].y;
      o->frame_coords->x1 = o->frame_coords->x0 + o->frame_coords->u1;
      o->frame_coords->y1 = o->frame_coords->y0 + o->frame_coords->v1;

      CTR_SET_SCALE_VECTOR(&o->scale_vector,
            CTR_BOTTOM_FRAMEBUFFER_WIDTH,
            CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
            texture->width,
            texture->height);
      }
   }
   return true;
}
#endif

static void font_driver_render_msg_bottom(ctr_video_t *ctr,
      const char *msg, const void *_params)
{
   ctr->render_font_bottom = true;
   font_driver_render_msg(ctr, msg, _params, NULL);
   ctr->render_font_bottom = false;
}


static void ctr_bottom_render_screen_thumbnail(ctr_video_t *ctr)
{
   struct font_params params = { 0, };
   settings_t *settings      = config_get_ptr();
   bool font_enable          = settings->bools.ctr_bottom_font_enable;
   int font_color_red        = settings->ints.ctr_bottom_font_color_red;
   int font_color_green      = settings->ints.ctr_bottom_font_color_green;
   int font_color_blue       = settings->ints.ctr_bottom_font_color_blue;
   int font_color_opacity    = settings->ints.ctr_bottom_font_color_opacity;

   if (!ctr)
      return;

   struct ctr_bottom_texture_data *o = NULL;
   ctr_texture_t *texture            = NULL;

   params.text_align = TEXT_ALIGN_CENTER;
   params.color      = COLOR_ABGR(font_color_opacity,
                                  font_color_blue,
                                  font_color_green,
                                  font_color_red);
   params.scale      = 1.6f;

   /* draw state thumbnail */
   if (ctr_bottom_state_savestates.state_data_exist)
   {
      o       = (struct ctr_bottom_texture_data*)
                      &ctr->bottom_textures[CTR_BOTTOM_TEXTURE_THUMBNAIL];
      texture = (ctr_texture_t *) o->texture;

      if (texture)
      {
         GPU_TEXCOLOR colorType = GPU_RGBA8;
         if (!ctr->render_state_from_png_file && !ctr->rgb32)
            colorType = GPU_RGB565;

         ctrGuSetTexture(GPU_TEXUNIT0,
               VIRT_TO_PHYS(texture->data),
               texture->width,
               texture->height,
               GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) |
               GPU_TEXTURE_MIN_FILTER(GPU_LINEAR) |
               GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) |
               GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
               colorType);

         GPUCMD_AddWrite(GPUREG_GSH_BOOLUNIFORM, 0);

         ctrGuSetVertexShaderFloatUniform(0,
               (float*)&o->scale_vector, 1);

         ctrGuSetAttributeBuffersAddress(
               VIRT_TO_PHYS(o->frame_coords));

         GPU_SetViewport(NULL,
               VIRT_TO_PHYS(ctr->drawbuffers.bottom),
               0,
               0,
               CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
               CTR_BOTTOM_FRAMEBUFFER_WIDTH);

         GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);
      }
      else
      {
         params.x = 0.266f;
         params.y = 0.64f;
         font_driver_render_msg_bottom(ctr, 
               msg_hash_to_str(MSG_3DS_BOTTOM_MENU_NO_STATE_THUMBNAIL),
               &params);
      }
   }
   else
   {
      params.x = 0.266f;
      params.y = 0.64f;
      font_driver_render_msg_bottom(ctr, 
            msg_hash_to_str(MSG_3DS_BOTTOM_MENU_NO_STATE_DATA),
            &params);
   }
}

static void ctr_bottom_render_screen_text(ctr_video_t *ctr)
{
   struct font_params params = { 0, };
   settings_t *settings      = config_get_ptr();
   int config_slot           = settings->ints.state_slot;
   bool font_enable          = settings->bools.ctr_bottom_font_enable;
   int font_color_red        = settings->ints.ctr_bottom_font_color_red;
   int font_color_green      = settings->ints.ctr_bottom_font_color_green;
   int font_color_blue       = settings->ints.ctr_bottom_font_color_blue;
   int font_color_opacity    = settings->ints.ctr_bottom_font_color_opacity;
   float font_scale          = settings->floats.ctr_bottom_font_scale;
   uint32_t flags            = runloop_get_flags();

   if (!ctr)
      return;

   switch (ctr_bottom_state.mode)
   {
      case MODE_SAVESTATE:
      {
         if (flags & RUNLOOP_FLAG_CORE_RUNNING)
         {
            struct ctr_bottom_texture_data *o = NULL;
            ctr_texture_t *texture            = NULL;

            params.text_align                 = TEXT_ALIGN_CENTER;
            params.color                      = COLOR_ABGR(font_color_opacity,
                                                           font_color_blue,
                                                           font_color_green,
                                                           font_color_red);
            params.scale                      = font_scale;

            if (font_enable)
            {
               /* draw resume game */
               params.x = -0.275f;
               params.y = 0.78f;

               font_driver_render_msg_bottom(ctr, 
                     msg_hash_to_str(MSG_3DS_BOTTOM_MENU_RESET),
                     &params);

               /* draw create restore point */
               params.x = -0.178f;
               params.y = 0.40f;

               font_driver_render_msg_bottom(ctr, 
                     msg_hash_to_str(MSG_3DS_BOTTOM_MENU_SAVE_STATE),
                     &params);

               /* draw load restore point */
               params.x = 0.266f;
               params.y = 0.33f;
               font_driver_render_msg_bottom(ctr, 
               msg_hash_to_str(MSG_3DS_BOTTOM_MENU_LOAD_STATE),
                     &params);
            }
            /* draw date */
            params.x = 0.266f;
            params.y = 0.87f;
            font_driver_render_msg_bottom(ctr, ctr_bottom_state_savestates.state_date,
                  &params);

            /* draw state slot */
            char str_slot[4];
            sprintf(str_slot, "%i", config_slot);

            params.scale = 1.00f;
            params.x     = -0.055f;
            params.y     = 0.785f;
            font_driver_render_msg_bottom(ctr, str_slot,
                  &params);
         }
         break;
      }
      case MODE_MOUSE:
      {
         char str_multiplier[10];

         sprintf(str_multiplier, "%dx", ctr_bottom_state_mouse.mouse_multiplier);

         params.text_align = TEXT_ALIGN_CENTER;
         params.color      = COLOR_ABGR(font_color_opacity,
                                        font_color_blue,
                                        font_color_green,
                                        font_color_red);
         params.scale      = 1.0f;
         params.x          = 0.39f;
         params.y          = 0.255f;

         font_driver_render_msg_bottom(ctr, str_multiplier,
               &params);

         break;
      }

      case MODE_TODO:
      {
		  /*
//         char str_fps_top[20];
         char str_fps_bottom[20];
         static uint64_t currentTick,lastTick;
         uint32_t diff;
         static float fps_bottom  = 0.0;
         static int frames = 0;

         frames++;
         currentTick = svcGetSystemTick();
         diff        = currentTick - lastTick;

         if(diff > CTR_CPU_TICKS_PER_SECOND)
         {
            fps_bottom = (float)frames * ((float) CTR_CPU_TICKS_PER_SECOND / (float) diff);
            lastTick = currentTick;
            frames = 0;
         }


//         sprintf(str_fps_top, "   Top FPS: %.2f", ctr->fps_top);
         sprintf(str_fps_bottom, "Bottom FPS: %.2f", fps_bottom);

         params.text_align = TEXT_ALIGN_LEFT;
         params.color      = COLOR_ABGR(255, 255, 255, 255);
         params.scale = 2.0f;
         params.x     = 0.17f; // width

//         params.y     = 0.60f; // height
//         font_driver_render_msg_bottom(ctr, str_fps_top,
//            &params);

         params.y     = 0.50f; //height
         font_driver_render_msg_bottom(ctr, str_fps_bottom,
            &params);
*/
         break;
      }
   break;
   }
}


static void ctr_render_sensor_cursor(ctr_video_t *ctr)
{
   struct font_params params         = { 0, };
   settings_t *settings              = config_get_ptr();
   input_driver_state_t *ctr_input   = input_state_get_ptr();
   const ctr_input_t *ctr_input_data = ctr_input->current_data;
   bool input_ctr_lightgun_abs       = settings->bools.input_ctr_lightgun_abs;

   ctr->msg_rendering_enabled = true;

   params.text_align = TEXT_ALIGN_CENTER;
   params.color      = COLOR_ABGR(255, 255, 255, 255);
   params.scale      = 1.6f;

   if(input_ctr_lightgun_abs)
   {
      params.x       =
            ((((float)ctr_input_data->mouse_state.abs_x+32767.0f)/65534.0f)*1.0f)-0.5f;
      params.y       =
            1.0f-((((float)ctr_input_data->mouse_state.abs_y+32767.0f)/65534.0f)*1.0f);
   }
   else
   {
      params.x       =
            (((float)ctr_input_data->mouse_state.pos_x / 320.0f)*1.0f)-0.5f;
      params.y       =
            1.0f-(((float)ctr_input_data->mouse_state.pos_y / 240.0f)*1.0f);
   }
   font_driver_render_msg(ctr, "+",
         &params, NULL);
		 
   ctr->msg_rendering_enabled = false;
}





#ifdef HAVE_BOTTOM_SCREEN

static void ctr_bottom_render_kbd_mod(ctr_video_t *ctr, void *texture_data, bool modifier)
{
   struct ctr_bottom_texture_data *o = texture_data;

   if (!ctr || !o)
      return;

   ctr_texture_t *kbd_texture = (ctr_texture_t *) o->texture;

   ctrGuSetTexture(GPU_TEXUNIT0, VIRT_TO_PHYS(kbd_texture->data),
         kbd_texture->width, kbd_texture->height,
         GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR) |
         GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
         GPU_RGBA8);

   GPUCMD_AddWrite(GPUREG_GSH_BOOLUNIFORM, 0);
   ctrGuSetVertexShaderFloatUniform(0, (float*)&o->scale_vector, 1);
   if (!modifier)
   {
      if ( ctr_bottom_kbd_lut[ctr_bottom_state_kbd.isPressed].gfx == 0  ||  ctr_bottom_kbd_lut[ctr_bottom_state_kbd.isPressed].gfx == 1 )
      {
         o->frame_coords->x0 = ctr_bottom_kbd_lut[ctr_bottom_state_kbd.isPressed].x0;
         o->frame_coords->y0 = ctr_bottom_kbd_lut[ctr_bottom_state_kbd.isPressed].y0;
         o->frame_coords->x1 = o->frame_coords->x0 + o->frame_coords->u1;
         o->frame_coords->y1 = o->frame_coords->y0 + o->frame_coords->v1;
      }
   }
   ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(o->frame_coords));

   GPU_SetViewport(NULL,
         VIRT_TO_PHYS(ctr->drawbuffers.bottom),
         0, 0, CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
         CTR_BOTTOM_FRAMEBUFFER_WIDTH);
   GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);
}


static void ctr_bottom_render_screen_menu(ctr_video_t *ctr)
{
   settings_t *settings = config_get_ptr();
   bool debug_bottom    = settings->bools.ctr_bottom_debug_enable;

   if (!ctr)
      return;

// render background	  
   struct ctr_bottom_texture_data *o = &ctr->bottom_textures[CTR_BOTTOM_TEXTURE_GFX];

   if (!o)
      return;

   ctr_texture_t *texture = (ctr_texture_t *) o->texture;

   if (!texture)
      return;

   ctrGuSetTexture(GPU_TEXUNIT0, VIRT_TO_PHYS(texture->data),
         texture->width, texture->height,
         GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR) |
         GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
         GPU_RGBA8);

   GPUCMD_AddWrite(GPUREG_GSH_BOOLUNIFORM, 0);
   ctrGuSetVertexShaderFloatUniform(0, (float*)&o->scale_vector, 1);
   ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(o->frame_coords));

   GPU_SetViewport(NULL,
         VIRT_TO_PHYS(ctr->drawbuffers.bottom),
         0, 0, CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
         CTR_BOTTOM_FRAMEBUFFER_WIDTH);
   GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);
// end render background

/* render pressed keyboard buttons */
   if ( ctr_bottom_state.mode == MODE_KBD )
   {
      if ( ctr_bottom_state_kbd.isPressed > 0 )
      {
         ctr_bottom_render_kbd_mod(ctr, &ctr->bottom_kbd_textures[ctr_bottom_kbd_lut[ctr_bottom_state_kbd.isPressed].gfx], false);
      }

      if (ctr_bottom_state_kbd.isShift)
      {
         ctr_bottom_render_kbd_mod(ctr, &ctr->bottom_kbd_textures[5], true);
      }
      if (ctr_bottom_state_kbd.isCaps)
      {
         ctr_bottom_render_kbd_mod(ctr, &ctr->bottom_kbd_textures[6], true);
      }
      if (ctr_bottom_state_kbd.isAlt)
      {
         ctr_bottom_render_kbd_mod(ctr, &ctr->bottom_kbd_textures[2], true);
      }
      if (ctr_bottom_state_kbd.isCtrl)
      {
         ctr_bottom_render_kbd_mod(ctr, &ctr->bottom_kbd_textures[3], true);
      }
   }
/* end render pressed keyboard buttons */

// ctr_bottom_render_screen_debug_kbd(ctr);

}
#endif// HAVE_BOTTOM_SCREEN

/* graphic function originates from here:
 * https://github.com/smealum/3ds_hb_menu/blob/master/source/gfx.c
 */
/*
void ctr_fade_bottom_screen(gfxScreen_t screen, gfx3dSide_t side, u32 f)
{
   int i;
   u16 fbWidth, fbHeight;
   u8* fbAdr = gfxGetFramebuffer(screen, side, &fbWidth, &fbHeight);

   for(i = 0; i < fbWidth * fbHeight / 2; i++)
   {
      *fbAdr = (*fbAdr * f) >> 8;
      fbAdr++;
      *fbAdr = (*fbAdr * f) >> 8;
      fbAdr++;
      *fbAdr = (*fbAdr * f) >> 8;
      fbAdr++;
      *fbAdr = (*fbAdr * f) >> 8;
      fbAdr++;
      *fbAdr = (*fbAdr * f) >> 8;
      fbAdr++;
      *fbAdr = (*fbAdr * f) >> 8;
      fbAdr++;
   }
}


static void ctr_set_bottom_screen_idle(ctr_video_t * ctr)
{
   u64 elapsed_tick;
   if (ctr->bottom_menu == CTR_BOTTOM_MENU_SELECT)
      return;

   elapsed_tick = svcGetSystemTick() - ctr->idle_timestamp;

   if ( elapsed_tick > 2000000000 )
   {
      if (!ctr->bottom_is_fading)
	  {
         ctr->bottom_is_fading    = true;
         ctr->refresh_bottom_menu = true;
         return;
      }

      if (fade_count > 0)
      {
         fade_count--;
         ctr_fade_bottom_screen(GFX_BOTTOM, GFX_LEFT, fade_count);

         if (fade_count <= 128)
         {
            ctr->bottom_is_idle    = true;
            ctr->bottom_is_fading  = false;
            ctr->bottom_check_idle = false;
            fade_count             = 256;
            ctr_set_bottom_screen_enable(false,true);
            return;
         }
      }
   }
}
*/


static void ctr_bottom_state_thumbnail(void* data)
{
   uint32_t state_tmp   = 0;
   ctr_video_t *ctr     = (ctr_video_t*)data;
   settings_t *settings = config_get_ptr();
   uint32_t runloop_flags       = runloop_get_flags();
   char screenshot_full_path[PATH_MAX_LENGTH];
   video_driver_state_t *video_st = video_state_get_ptr();


   if (ctr_bottom_state.task_save)
   {

//if (!(runloop_flags & RUNLOOP_FLAG_PAUSED)) {
//	command_event(CMD_EVENT_PAUSE, NULL);
//} else {

      ctr_bottom_state.task_save = false;

      struct ctr_bottom_texture_data *o =
            &ctr->bottom_textures[CTR_BOTTOM_TEXTURE_THUMBNAIL];
      ctr_texture_t            *texture = 
      (ctr_texture_t *) o->texture;

      if (texture)
         linearFree(texture->data);
      else
      {
         o->texture      = (uintptr_t)
               calloc(1, sizeof(ctr_texture_t));
         o->frame_coords = linearAlloc(sizeof(ctr_vertex_t));
         texture         = (ctr_texture_t *)o->texture;
      }

      texture->width         = ctr->texture_width;
      texture->height        = ctr->texture_width;
      texture->active_width  = ctr->frame_coords->u1;
      texture->active_height = ctr->frame_coords->v1;

      texture->data          = linearAlloc(
            ctr->texture_width * ctr->texture_height * 
            (ctr->rgb32? 4:2));

      memcpy(texture->data, ctr->texture_swizzled, 
            ctr->texture_width * ctr->texture_height * 
            (ctr->rgb32? 4:2));

      ctr_bottom_state_thumbnail_geom(ctr);

      ctr_bottom_state_savestates.state_data_exist = true;
      ctr->render_state_from_png_file = false;

      ctr_update_state_date(ctr);


      if (settings->bools.savestate_thumbnail_enable)
      {
         sprintf(screenshot_full_path, "%s/%s",
               ctr_bottom_state_gfx.texture_name,
               ctr_bottom_state_gfx.texture_path);

                  take_screenshot(NULL,
                        screenshot_full_path,
                        true,
                        video_st->frame_cache_data && (video_st->frame_cache_data == RETRO_HW_FRAME_BUFFER_VALID),
                        true,
                        false /*use_thread*/);

      }
//	  command_event(CMD_EVENT_UNPAUSE, NULL);
//   }
   }
}

static void ctr_bottom_set_screen_enable(bool enabled, bool idle)
{
   Handle lcd_handle;
   u8 not_2DS;

   CFGU_GetModelNintendo2DS(&not_2DS);
   if (not_2DS && srvGetServiceHandle(&lcd_handle, "gsp::Lcd") >= 0)
   {
      u32 *cmdbuf = getThreadCommandBuffer();
      cmdbuf[0]   = enabled? 0x00110040:  0x00120040;
      cmdbuf[1]   = 2;
      svcSendSyncRequest(lcd_handle);
      svcCloseHandle(lcd_handle);
   }

//   if (!idle)
//      ctr_bottom_screen_enabled = enabled;
}

static void ctr_lcd_aptHook(APT_HookType hook, void* param)
{
   ctr_video_t *ctr  = (ctr_video_t*)param;

   if (!ctr)
      return;

   if (hook == APTHOOK_ONRESTORE)
   {
      GPUCMD_SetBufferOffset(0);
      shaderProgramUse(&ctr->shader);

      GPU_SetViewport(NULL,
                      VIRT_TO_PHYS(ctr->drawbuffers.top.left),
                      0,
                      0,
                      CTR_TOP_FRAMEBUFFER_HEIGHT,
                      CTR_TOP_FRAMEBUFFER_WIDTH);

      GPU_DepthMap(-1.0f, 0.0f);
      GPU_SetFaceCulling(GPU_CULL_NONE);
      GPU_SetStencilTest(false, GPU_ALWAYS, 0x00, 0xFF, 0x00);
      GPU_SetStencilOp(GPU_STENCIL_KEEP,
            GPU_STENCIL_KEEP,
            GPU_STENCIL_KEEP);
      GPU_SetBlendingColor(0, 0, 0, 0);
      GPU_SetDepthTestAndWriteMask(false, GPU_ALWAYS, GPU_WRITE_COLOR);
      GPUCMD_AddMaskedWrite(GPUREG_EARLYDEPTH_TEST1, 0x1, 0);
      GPUCMD_AddWrite(GPUREG_EARLYDEPTH_TEST2, 0);
      GPU_SetAlphaBlending(
            GPU_BLEND_ADD,
            GPU_BLEND_ADD,
            GPU_SRC_ALPHA,
            GPU_ONE_MINUS_SRC_ALPHA,
            GPU_SRC_ALPHA,
            GPU_ONE_MINUS_SRC_ALPHA);
      GPU_SetAlphaTest(false, GPU_ALWAYS, 0x00);
      GPU_SetTextureEnable(GPU_TEXUNIT0);
      GPU_SetTexEnv(0, GPU_TEXTURE0, GPU_TEXTURE0, 0, 0, GPU_REPLACE, GPU_REPLACE, 0);
      GPU_SetTexEnv(1, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
      GPU_SetTexEnv(2, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
      GPU_SetTexEnv(3, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
      GPU_SetTexEnv(4, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
      GPU_SetTexEnv(5, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
      ctrGuSetAttributeBuffers(2,
            VIRT_TO_PHYS(ctr->menu.frame_coords),
              CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 0
            | CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 4,
            sizeof(ctr_vertex_t));
      GPU_Finalize();
      ctrGuFlushAndRun(true);
      gspWaitForEvent(GSPGPU_EVENT_P3D, false);
      ctr->p3d_event_pending = false;
   }

   switch (hook)
   {
      case APTHOOK_ONSUSPEND:
         if (ctr->video_mode == CTR_VIDEO_MODE_2D_400X240)
         {
            memcpy(gfxTopRightFramebuffers[ctr->current_buffer_top],
               gfxTopLeftFramebuffers[ctr->current_buffer_top],
               400 * 240 * 3);
            GSPGPU_FlushDataCache(
                  gfxTopRightFramebuffers[
                  ctr->current_buffer_top], 400 * 240 * 3);
         }
         if (ctr->supports_parallax_disable)
            ctr_set_parallax_layer(*(float*)0x1FF81080 != 0.0);
         ctr_bottom_set_screen_enable(true, false);
         save_state_to_file(ctr);
         break;  
      case APTHOOK_ONRESTORE:
      case APTHOOK_ONWAKEUP:
//         ctr_bottom_set_screen_enable(false, ctr->bottom_is_idle);
         save_state_to_file(ctr);
         break;
      default:
         break;
   }

#ifdef HAVE_MENU
   if (menu_state_get_ptr()->flags & MENU_ST_FLAG_ALIVE)
      return;
#endif

   switch (hook)
   {
      case APTHOOK_ONSUSPEND:
      case APTHOOK_ONSLEEP:
         command_event(CMD_EVENT_AUDIO_STOP, NULL);
         break;
      case APTHOOK_ONRESTORE:
      case APTHOOK_ONWAKEUP:
         command_event(CMD_EVENT_AUDIO_START, NULL);
         break;
      case APTHOOK_ONEXIT:
      case APTHOOK_COUNT:
         break;
   }
}

static void ctr_vsync_hook(ctr_video_t* ctr)
{
   ctr->vsync_event_pending = false;
}

#ifndef HAVE_THREADS
static bool ctr_tasks_finder(retro_task_t *task,void *userdata)
{
   return task;
}

task_finder_data_t ctr_tasks_finder_data = {ctr_tasks_finder, NULL};
#endif


static void* ctr_init(const video_info_t* video,
      input_driver_t** input, void** input_data)
{
   size_t i;
   float refresh_rate;
   u8 device_model                  = 0xFF;
   void* ctrinput                   = NULL;
   settings_t *settings             = config_get_ptr();
   bool lcd_bottom                  = settings->bools.ctr_bottom_lcd_enable;
   bool console_bottom              = settings->bools.ctr_bottom_console_enable;
   bool speedup_enable              = settings->bools.ctr_n3ds_speedup_enable;
   ctr_video_t* ctr                 = (ctr_video_t*)linearAlloc(sizeof(ctr_video_t));
#ifdef HAVE_BOTTOM_SCREEN
   unsigned ctr_bottom_display_mode = settings->uints.ctr_bottom_display_mode;
   unsigned video_ctr_render_target = settings->uints.video_ctr_render_target;
#endif

   if (!ctr)
      return NULL;

   memset(ctr, 0, sizeof(ctr_video_t));

	  
#ifdef HAVE_BOTTOM_SCREEN
   switch (ctr_bottom_display_mode)
   {
      case CTR_BOTTOM_MODE_RETROARCH:
		 ctr->custom_framebuffer_width = true;
         ctr->vp.width          = CTR_BOTTOM_FRAMEBUFFER_WIDTH;
         ctr->vp.full_width     = CTR_BOTTOM_FRAMEBUFFER_WIDTH;
         if (video_ctr_render_target == CTR_VIDEO_TARGET_DUAL)
         {
            ctr->vp.height      = CTR_TOP_FRAMEBUFFER_HEIGHT + CTR_BOTTOM_FRAMEBUFFER_HEIGHT;
            ctr->vp.full_height = CTR_TOP_FRAMEBUFFER_HEIGHT + CTR_BOTTOM_FRAMEBUFFER_HEIGHT;
         }
         else
         {
            ctr->vp.height      = CTR_TOP_FRAMEBUFFER_HEIGHT;
            ctr->vp.full_height = CTR_TOP_FRAMEBUFFER_HEIGHT;
         }
         break;

	  default:
	     ctr->custom_framebuffer_width = false;
         ctr->vp.width          = CTR_TOP_FRAMEBUFFER_WIDTH;
         ctr->vp.full_width     = CTR_TOP_FRAMEBUFFER_WIDTH;
         ctr->vp.height         = CTR_TOP_FRAMEBUFFER_HEIGHT;
         ctr->vp.full_height    = CTR_TOP_FRAMEBUFFER_HEIGHT;
         break;
   }
#endif


   ctr->vp.x                = 0;
   ctr->vp.y                = 0;

   video_driver_set_size(ctr->vp.width, ctr->vp.height);

   ctr->drawbuffers.top.left = vramAlloc(CTR_TOP_FRAMEBUFFER_WIDTH * CTR_TOP_FRAMEBUFFER_HEIGHT * 2 * sizeof(uint32_t));
   ctr->drawbuffers.top.right = (void*)((uint32_t*)ctr->drawbuffers.top.left + CTR_TOP_FRAMEBUFFER_WIDTH * CTR_TOP_FRAMEBUFFER_HEIGHT);
#ifdef HAVE_BOTTOM_SCREEN
   ctr->drawbuffers.bottom = vramAlloc(CTR_BOTTOM_FRAMEBUFFER_WIDTH * CTR_BOTTOM_FRAMEBUFFER_HEIGHT * 2 * sizeof(uint32_t));
#endif
   ctr->display_list_size = 0x4000;
   ctr->display_list = linearAlloc(ctr->display_list_size * sizeof(uint32_t));
   GPU_Reset(NULL, ctr->display_list, ctr->display_list_size);

   ctr->vertex_cache.size = 0x1000;
   ctr->vertex_cache.buffer = linearAlloc(ctr->vertex_cache.size * sizeof(ctr_vertex_t));
   ctr->vertex_cache.current = ctr->vertex_cache.buffer;


#ifdef HAVE_BOTTOM_SCREEN

   ctr->bottom_textures = (struct ctr_bottom_texture_data *)calloc(2,
      sizeof(*ctr->bottom_textures));
	  
   ctr->bottom_kbd_textures  = (struct ctr_bottom_texture_data *)calloc(CTR_TEXTURE_KBD_KEY_TAB + 1,
      sizeof(*ctr->bottom_kbd_textures));


//   ctr->init_bottom_menu = false;
//   ctr->state_data_exist = false;
   ctr->render_font_bottom = false;
   ctr->refresh_bottom_menu = true;
   ctr->render_state_from_png_file = false;
   ctr->bottom_menu = CTR_BOTTOM_MENU_NOT_AVAILABLE;
   ctr->prev_bottom_menu = CTR_BOTTOM_MENU_NOT_AVAILABLE;
//   ctr->bottom_check_idle = false;
//   ctr->bottom_is_idle = false;
//   ctr->bottom_is_fading = false;
//   ctr->idle_timestamp = 0;
//   ctr->state_slot = settings->ints.state_slot;

//   snprintf(ctr->state_date, sizeof(ctr->state_date), "%s", "00/00/0000");
//   ctr->state_date[CTR_STATE_DATE_SIZE - 1] = '\0';

#endif // HAVE_BOTTOM_SCREEN

   ctr->rgb32 = video->rgb32;
   ctr->texture_width = video->input_scale * RARCH_SCALE_BASE;
   ctr->texture_height = video->input_scale * RARCH_SCALE_BASE;
   ctr->texture_linear =
         linearMemAlign(ctr->texture_width * ctr->texture_height * (ctr->rgb32? 4:2), 128);
   ctr->texture_swizzled =
         linearMemAlign(ctr->texture_width * ctr->texture_height * (ctr->rgb32? 4:2), 128);

   ctr->frame_coords = linearAlloc(3 * sizeof(ctr_vertex_t));
   ctr->frame_coords->x0 = 0;
   ctr->frame_coords->y0 = 0;
   if (ctr_bottom_display_mode == CTR_BOTTOM_MODE_RETROARCH)
      ctr->frame_coords->x1 = CTR_BOTTOM_FRAMEBUFFER_WIDTH;
   else
      ctr->frame_coords->x1 = CTR_TOP_FRAMEBUFFER_WIDTH;

   ctr->frame_coords->y1 = CTR_TOP_FRAMEBUFFER_HEIGHT;
   ctr->frame_coords->u0 = 0;
   ctr->frame_coords->v0 = 0;
   if (ctr->custom_framebuffer_width)
      ctr->frame_coords->u1 = CTR_BOTTOM_FRAMEBUFFER_WIDTH;
   else
      ctr->frame_coords->u1 = CTR_TOP_FRAMEBUFFER_WIDTH;
   ctr->frame_coords->v1 = CTR_TOP_FRAMEBUFFER_HEIGHT;
   GSPGPU_FlushDataCache(ctr->frame_coords, sizeof(ctr_vertex_t));




   ctr->menu.texture_width = 512;
   ctr->menu.texture_height = 512;
   ctr->menu.texture_linear =
         linearMemAlign(ctr->menu.texture_width * ctr->menu.texture_height * sizeof(uint16_t), 128);
   ctr->menu.texture_swizzled =
         linearMemAlign(ctr->menu.texture_width * ctr->menu.texture_height * sizeof(uint16_t), 128);

   ctr->menu.frame_coords = linearAlloc(sizeof(ctr_vertex_t));

   ctr->menu.frame_coords->x0 = 40;
   ctr->menu.frame_coords->y0 = 0;
   ctr->menu.frame_coords->x1 = CTR_TOP_FRAMEBUFFER_WIDTH - 40;
   ctr->menu.frame_coords->y1 = CTR_TOP_FRAMEBUFFER_HEIGHT;
   ctr->menu.frame_coords->u0 = 0;
   ctr->menu.frame_coords->v0 = 0;
   ctr->menu.frame_coords->u1 = CTR_TOP_FRAMEBUFFER_WIDTH - 80;
   ctr->menu.frame_coords->v1 = CTR_TOP_FRAMEBUFFER_HEIGHT;

   GSPGPU_FlushDataCache(ctr->menu.frame_coords, sizeof(ctr_vertex_t));

//--------------
ctr->frame_coords_bottom = linearAlloc(sizeof(ctr_vertex_t));
   ctr->frame_coords_bottom->x0 = 0; //width, top left
   ctr->frame_coords_bottom->y0 = 0;  //hieght?
   ctr->frame_coords_bottom->x1 = CTR_BOTTOM_FRAMEBUFFER_WIDTH;
   ctr->frame_coords_bottom->y1 = CTR_BOTTOM_FRAMEBUFFER_HEIGHT;
   ctr->frame_coords_bottom->u0 = 0;
   ctr->frame_coords_bottom->v0 = 0;
   ctr->frame_coords_bottom->u1 = CTR_BOTTOM_FRAMEBUFFER_WIDTH;
   ctr->frame_coords_bottom->v1 = CTR_BOTTOM_FRAMEBUFFER_HEIGHT;

   GSPGPU_FlushDataCache(ctr->frame_coords_bottom, sizeof(ctr_vertex_t));

ctr->menu.frame_coords_bottom = linearAlloc(sizeof(ctr_vertex_t));
   ctr->menu.frame_coords_bottom->x0 = 0;
   ctr->menu.frame_coords_bottom->y0 = 0;
   ctr->menu.frame_coords_bottom->x1 = CTR_BOTTOM_FRAMEBUFFER_WIDTH;
   ctr->menu.frame_coords_bottom->y1 = CTR_BOTTOM_FRAMEBUFFER_HEIGHT;
   ctr->menu.frame_coords_bottom->u0 = 0;
   ctr->menu.frame_coords_bottom->v0 = 0;
   ctr->menu.frame_coords_bottom->u1 = CTR_BOTTOM_FRAMEBUFFER_WIDTH;
   ctr->menu.frame_coords_bottom->v1 = CTR_BOTTOM_FRAMEBUFFER_HEIGHT;

   GSPGPU_FlushDataCache(ctr->menu.frame_coords_bottom, sizeof(ctr_vertex_t));
//----------------
      if (video_ctr_render_target == CTR_VIDEO_TARGET_DUAL )
      {
   CTR_SET_SCALE_VECTOR(&ctr->scale_vector,
                        CTR_TOP_FRAMEBUFFER_WIDTH, CTR_TOP_FRAMEBUFFER_HEIGHT,
                        ctr->texture_width, ctr->texture_height);
	  }
	  else
	  {
		     CTR_SET_SCALE_VECTOR(&ctr->scale_vector,
                        CTR_TOP_FRAMEBUFFER_WIDTH, CTR_TOP_FRAMEBUFFER_HEIGHT,
                        ctr->texture_width, ctr->texture_height);
	  }
   CTR_SET_SCALE_VECTOR(&ctr->menu.scale_vector,
                        CTR_TOP_FRAMEBUFFER_WIDTH, CTR_TOP_FRAMEBUFFER_HEIGHT,
                        ctr->menu.texture_width, ctr->menu.texture_height);
      if (video_ctr_render_target == CTR_VIDEO_TARGET_DUAL )
      {
   CTR_SET_SCALE_VECTOR(&ctr->scale_vector_bottom,
                        CTR_BOTTOM_FRAMEBUFFER_WIDTH, CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
                        ctr->texture_width, ctr->texture_height);
							  }
	  else
	  {
		     CTR_SET_SCALE_VECTOR(&ctr->scale_vector_bottom,
                        CTR_BOTTOM_FRAMEBUFFER_WIDTH, CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
                        ctr->texture_width, ctr->texture_height);
	  }
   CTR_SET_SCALE_VECTOR(&ctr->menu.scale_vector_bottom,
                        CTR_BOTTOM_FRAMEBUFFER_WIDTH, CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
                        ctr->menu.texture_width, ctr->menu.texture_height);

   memset(ctr->texture_linear, 0x00, ctr->texture_width * ctr->texture_height * (ctr->rgb32? 4:2));
#if 0
   memset(ctr->menu.texture_swizzled , 0x00, ctr->menu.texture_width * ctr->menu.texture_height * 2);
#endif

   ctr->dvlb = DVLB_ParseFile((u32*)ctr_sprite_shbin, ctr_sprite_shbin_size);
   ctrGuSetVshGsh(&ctr->shader, ctr->dvlb, 2, 2);
   shaderProgramUse(&ctr->shader);

   GPU_SetViewport(NULL,
                   VIRT_TO_PHYS(ctr->drawbuffers.top.left),
                   0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT, CTR_TOP_FRAMEBUFFER_WIDTH);

   GPU_DepthMap(-1.0f, 0.0f);
   GPU_SetFaceCulling(GPU_CULL_NONE);
   GPU_SetStencilTest(false, GPU_ALWAYS, 0x00, 0xFF, 0x00);
   GPU_SetStencilOp(GPU_STENCIL_KEEP, GPU_STENCIL_KEEP, GPU_STENCIL_KEEP);
   GPU_SetBlendingColor(0, 0, 0, 0);
#if 0
   GPU_SetDepthTestAndWriteMask(true, GPU_GREATER, GPU_WRITE_ALL);
#endif
   GPU_SetDepthTestAndWriteMask(false, GPU_ALWAYS, GPU_WRITE_COLOR);
#if 0
   GPU_SetDepthTestAndWriteMask(true, GPU_ALWAYS, GPU_WRITE_ALL);
#endif

   GPUCMD_AddMaskedWrite(GPUREG_EARLYDEPTH_TEST1, 0x1, 0);
   GPUCMD_AddWrite(GPUREG_EARLYDEPTH_TEST2, 0);

   GPU_SetAlphaBlending(GPU_BLEND_ADD, GPU_BLEND_ADD,
                        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA,
                        GPU_SRC_ALPHA, GPU_ONE_MINUS_SRC_ALPHA);
   GPU_SetAlphaTest(false, GPU_ALWAYS, 0x00);

   GPU_SetTextureEnable(GPU_TEXUNIT0);

   GPU_SetTexEnv(0, GPU_TEXTURE0, GPU_TEXTURE0, 0, 0, GPU_REPLACE, GPU_REPLACE, 0);
   GPU_SetTexEnv(1, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
   GPU_SetTexEnv(2, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
   GPU_SetTexEnv(3, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
   GPU_SetTexEnv(4, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
   GPU_SetTexEnv(5, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);

   ctrGuSetAttributeBuffers(2,
                            VIRT_TO_PHYS(ctr->menu.frame_coords),
                            CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 0 |
                            CTRGU_ATTRIBFMT(GPU_SHORT, 4) << 4,
                            sizeof(ctr_vertex_t));
   GPU_Finalize();
   ctrGuFlushAndRun(true);

   ctr->p3d_event_pending = true;
   ctr->ppf_event_pending = false;

   if (input && input_data)
   {
      ctrinput             = input_driver_init_wrap(&input_ctr, settings->arrays.input_joypad_driver);
      *input               = ctrinput ? &input_ctr : NULL;
      *input_data          = ctrinput;
   }

   ctr->keep_aspect   = true;
   ctr->should_resize = true;
   ctr->smooth        = video->smooth;
   ctr->vsync         = video->vsync;
   ctr->current_buffer_top = 0;
   ctr->current_buffer_bottom = 0;

   /* Only O3DS and O3DSXL support running in 'dual-framebuffer'
    * mode with the parallax barrier disabled
    * (i.e. these are the only platforms that can use
    * CTR_VIDEO_MODE_2D_400X240 and CTR_VIDEO_MODE_2D_800X240) */
   CFGU_GetSystemModel(&device_model); /* (0 = O3DS, 1 = O3DSXL, 2 = N3DS, 3 = 2DS, 4 = N3DSXL, 5 = N2DSXL) */
   ctr->supports_parallax_disable = (device_model == 0) || (device_model == 1);

   refresh_rate = (32730.0 * 8192.0) / 4481134.0;

   driver_ctl(RARCH_DRIVER_CTL_SET_REFRESH_RATE, &refresh_rate);
   aptHook(&ctr->lcd_aptHook, ctr_lcd_aptHook, ctr);

   font_driver_init_osd(ctr, video,
         false,
         video->is_threaded,
         FONT_DRIVER_RENDER_CTR);

   ctr->msg_rendering_enabled     = true;
   ctr->menu_texture_frame_enable = false;
   ctr->menu_texture_enable       = false;

   /* Set bottom screen enable state, if required */
//   if (lcd_bottom != ctr_bottom_screen_enabled)
//      ctr_set_bottom_screen_enable(true, false);

   gspSetEventCallback(GSPGPU_EVENT_VBlank0,
         (ThreadFunc)ctr_vsync_hook, ctr, false);
		 
   osSetSpeedupEnable(speedup_enable);

   return ctr;
}

#if 0
#define CTR_INSPECT_MEMORY_USAGE
#endif

bool gfxIsInit = false;
unsigned prevmode = 0;

static bool ctr_frame(void* data, const void* frame,
      unsigned width, unsigned height,
      uint64_t frame_count,
      unsigned pitch, const char* msg, video_frame_info_t *video_info)
{
   static uint64_t current_tick, last_tick;
   extern GSPGPU_FramebufferInfo topFramebufferInfo, bottomFramebufferInfo;
   extern u8* gfxSharedMemory;
   extern u8 gfxThreadID;
   uint32_t diff;
   ctr_video_t       *ctr           = (ctr_video_t*)data;
   static float        fps          = 0.0;
   static int total_frames          = 0;
   static int       frames          = 0;
   settings_t    *settings          = config_get_ptr();
   unsigned disp_mode               = settings->uints.video_ctr_display_mode;
   bool statistics_show             = video_info->statistics_show;
   const char *stat_text            = video_info->stat_text;
   float video_refresh_rate         = video_info->refresh_rate;
   struct font_params *osd_params   = (struct font_params*)
      &video_info->osd_stat_params;
   int custom_vp_x                  = video_info->custom_vp_x;
   int custom_vp_y                  = video_info->custom_vp_y;
   unsigned custom_vp_width         = video_info->custom_vp_width;
   unsigned custom_vp_height        = video_info->custom_vp_height;
#ifdef HAVE_MENU
   bool menu_is_alive               = (video_info->menu_st_flags & MENU_ST_FLAG_ALIVE) ? true : false;
   bool overlay_behind_menu         = video_info->overlay_behind_menu;
#endif
#ifdef HAVE_GFX_WIDGETS
   bool widgets_active              = video_info->widgets_active;
#endif

#ifdef HAVE_BOTTOM_SCREEN
   unsigned ctr_bottom_display_mode = settings->uints.ctr_bottom_display_mode;
   unsigned video_ctr_render_target = settings->uints.video_ctr_render_target;
//   int config_slot                  = settings->ints.state_slot;
   bool lcd_bottom                  = false;
   bool console_bottom              = settings->bools.ctr_bottom_console_enable;
   bool debug_bottom                = settings->bools.ctr_bottom_debug_enable;
   
   unsigned video_ctr_dual_deadzone = settings->uints.video_ctr_dual_deadzone;
   int video_ctr_dual_offset_x      = settings->ints.video_ctr_dual_offset_x;
   int video_ctr_dual_offset_y      = settings->ints.video_ctr_dual_offset_y;

   bool input_ctr_sensors_cursor    = settings->bools.input_ctr_sensors_cursor;

#endif
   uint32_t flags                 = runloop_get_flags();

   if (!width || !height || !settings)
   {
      gspWaitForEvent(GSPGPU_EVENT_VBlank0, true);
      return true;
   }

#ifdef HAVE_BOTTOM_SCREEN
   switch (ctr_bottom_display_mode)
   {
      case CTR_BOTTOM_MODE_DISABLED:
         ctr_bottom_set_screen_enable(false,false);
		 ctr->refresh_bottom_menu = false;
		 
         break;
      case CTR_BOTTOM_MODE_CONTROL:
	  
         if (!lcd_bottom && !ctr_bottom_state_gfx.bottom_idle)
         {
			 lcd_bottom = true;
             ctr_bottom_set_screen_enable(true, false);
         }


         if (!gfxIsInit)
         {
            if (ctr_bottom_load_menu_texture(ctr,CTR_BOTTOM_TEXTURE_GFX) &&
                  ctr_bottom_load_menu_texture(ctr,CTR_BOTTOM_TEXTURE_THUMBNAIL) &&
                  ctr_bottom_load_kbd_texture(ctr))
            {
		   	 gfxIsInit = true;
            //ctr_bottom_state_gfx.isInit = true;
            ctr->refresh_bottom_menu = true;
			
//            if (retroarch_ctl(RARCH_CTL_CORE_IS_RUNNING, NULL))
//               ctr_update_state_date_from_file(ctr);
			
//            ctr_bottom_set_screen_enable(true,false);
            }
         }

         switch (ctr_bottom_state.mode)
		 {
			 case MODE_SAVESTATE:
                if (ctr_bottom_state.task_save)
                {
                   ctr_bottom_state_thumbnail(ctr);
				}
//                if (ctr_bottom_state_savestates.state_slot != config_slot ||
//				      !(prevmode==ctr_bottom_display_mode))
                if (!(prevmode==ctr_bottom_display_mode) ||
				ctr_bottom_state_gfx.reload_texture)
                {
//                   if (!(flags & RUNLOOP_FLAG_CORE_RUNNING));
                      ctr_bottom_load_menu_texture(ctr,CTR_BOTTOM_TEXTURE_THUMBNAIL);
					  
                }
                break;

			 case MODE_TODO:
                //ctr_refresh_bottom(true);
				ctr->refresh_bottom_menu = true;
                break;

			 default:
			    break;
		 }

         if (ctr_refresh_bottom(false) || !(prevmode==ctr_bottom_display_mode))
         {
            ctr_bottom_load_menu_texture(ctr,CTR_BOTTOM_TEXTURE_GFX);
            ctr->refresh_bottom_menu = true;
         }

         break;
      case CTR_BOTTOM_MODE_OVERLAY:
      case CTR_BOTTOM_MODE_RETROARCH:
         if (!lcd_bottom)
         {
			 lcd_bottom = true;
             ctr_bottom_set_screen_enable(true, false);
         }

         ctr->refresh_bottom_menu = true;
         break;
      case CTR_BOTTOM_MODE_CONSOLE:
         break;
   }

   if (!(prevmode==ctr_bottom_display_mode))
   {
      prevmode = ctr_bottom_display_mode;
      ctr->refresh_bottom_menu = true;
   }

   if (debug_bottom)
      ctr->refresh_bottom_menu = true;
#endif

   if (ctr->p3d_event_pending)
   {
      gspWaitForEvent(GSPGPU_EVENT_P3D, false);
      ctr->p3d_event_pending = false;
   }
   if (ctr->ppf_event_pending)
   {
      gspWaitForEvent(GSPGPU_EVENT_PPF, false);
      ctr->ppf_event_pending = false;
   }
#ifndef HAVE_THREADS
   if (task_queue_find(&ctr_tasks_finder_data))
   {
#if 0
      ctr->vsync_event_pending = true;
#endif
      while (ctr->vsync_event_pending)
      {
         task_queue_check();
         svcSleepThread(0);
      }
   }
#endif
   if (ctr->vsync)
   {
      /* If we are running at the display refresh rate,
       * then all is well - just wait on the *current* VBlank0
       * event and carry on.
       * 
       * If we are running at below the display refresh rate,
       * then we have problems: frame updates will happen
       * entirely out of sync with VBlank0 events. To elaborate,
       * we'll wait for a VBlank0 here, but it will already have
       * happened partway through the previous frame. So it's:
       * 'oh good - let's render the current frame', but the next
       * VBlank0 will occur in less time than it takes to draw the
       * current frame, resulting in 'overlap' and screen tearing.
       * 
       * This seems to be a consequence of using the GPU directly.
       * Other 3DS homebrew typically uses the ctrulib function
       * gfxSwapBuffers(), which ensures an immediate buffer
       * swap every time, and no tearing. We can't do this:
       * instead, we use a variant of the ctrulib function
       * gfxSwapBuffersGpu(), which seems to send a notification,
       * and the swap happens when it happens...
       * 
       * I don't know how to fix this 'properly' (probably needs
       * some low level rewriting, maybe switching to an implementation
       * based on citro3d), but I can at least implement a hack/workaround
       * that allows 50Hz content to be run without tearing. This involves
       * the following:
       * 
       * If content frame rate is more than 10% lower than the 3DS
       * display refresh rate, don't wait on the *current* VBlank0
       * event (because it is 'tainted'), but instead wait on the
       * *next* VBlank0 event (which will ensure we have enough time
       * to write/flush the display buffers).
       * 
       * This fixes screen tearing, but it has a significant impact on
       * performance...
       * */
      bool next_event                      = false;
      video_driver_state_t *video_st       = video_state_get_ptr();
      struct retro_system_av_info *av_info = &video_st->av_info;
      if (av_info)
         next_event = av_info->timing.fps < video_refresh_rate * 0.9f;
      gspWaitForEvent(GSPGPU_EVENT_VBlank0, next_event);
   }

   ctr->vsync_event_pending = true;

   /* Internal counters/statistics
    * > This is only required if the bottom screen is enabled */
/*
   if (ctr_bottom_state.mode == MODE_TODO)
   {
      frames++;
      current_tick = svcGetSystemTick();
      diff         = current_tick - last_tick;
      if (diff > CTR_CPU_TICKS_PER_SECOND)
      {
         fps       = (float)frames * ((float) CTR_CPU_TICKS_PER_SECOND / (float) diff);
         last_tick = current_tick;
         frames    = 0;
      }
      
	  ctr->fps_top = fps;
   }
*/
/*
#ifdef CTR_INSPECT_MEMORY_USAGE
      uint32_t ctr_get_stack_usage(void);
      void ctr_linear_get_stats(void);
      extern u32 __linear_heap_size;
      extern u32 __heap_size;

      MemInfo mem_info;
      PageInfo page_info;
      u32 query_addr = 0x08000000;
      printf(PRINTFPOS(0,0));
      while (query_addr < 0x40000000)
      {
         svcQueryMemory(&mem_info, &page_info, query_addr);
         printf("0x%08X --> 0x%08X (0x%08X) \n", mem_info.base_addr, mem_info.base_addr + mem_info.size, mem_info.size);
         query_addr = mem_info.base_addr + mem_info.size;
         if (query_addr == 0x1F000000)
            query_addr = 0x30000000;
      }

#if 0
      static u32* dummy_pointer;
      if (total_frames == 500)
         dummy_pointer = malloc(0x2000000);
      if (total_frames == 1000)
         free(dummy_pointer);
#endif

      printf("========================================");
      printf("0x%08X 0x%08X 0x%08X\n", __heap_size, gpuCmdBufOffset, (__linear_heap_size - linearSpaceFree()));
      printf("fps: %8.4f frames: %i (%X)\n", fps, total_frames++, (__linear_heap_size - linearSpaceFree()));
      printf("========================================");
      u32 app_memory = *((u32*)0x1FF80040);
      u64 mem_used;
      svcGetSystemInfo(&mem_used, 0, 1);
      printf("total mem : 0x%08X          \n", app_memory);
      printf("used: 0x%08X free: 0x%08X      \n", (u32)mem_used, app_memory - (u32)mem_used);
      static u32 stack_usage = 0;
      extern u32 __stack_bottom;
      if (!(total_frames & 0x3F))
         stack_usage = ctr_get_stack_usage();
      printf("stack total:0x%08X used: 0x%08X\n", 0x10000000 - __stack_bottom, stack_usage);

      printf("========================================");
      ctr_linear_get_stats();
      printf("========================================");
#else
      printf(PRINTFPOS(29,0)"fps: %8.4f frames: %i\r", fps, total_frames++);
#endif
      fflush(stdout);
   }
*/
   if (ctr->should_resize)
      ctr_update_viewport(ctr, settings,
            custom_vp_x,
            custom_vp_y,
            custom_vp_width,
            custom_vp_height
            );
#ifdef HAVE_BOTTOM_SCREEN
   if (ctr->refresh_bottom_menu)
      ctrGuSetMemoryFill(true,
            (u32*)ctr->drawbuffers.top.left,
            0x00000000,
            (u32*)ctr->drawbuffers.top.left + 2 * CTR_TOP_FRAMEBUFFER_WIDTH * CTR_TOP_FRAMEBUFFER_HEIGHT,
            0x201,
            (u32*)ctr->drawbuffers.bottom,
            0x00000000,
            (u32*)ctr->drawbuffers.bottom + 2 * CTR_BOTTOM_FRAMEBUFFER_WIDTH * CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
            0x201);
   else
      ctrGuSetMemoryFill(true,
            (u32*)ctr->drawbuffers.top.left,
            0x00000000,
            (u32*)ctr->drawbuffers.top.left + 2 * CTR_TOP_FRAMEBUFFER_WIDTH * CTR_TOP_FRAMEBUFFER_HEIGHT,
            0x201,
            NULL,
            0x00000000,
            0,
            0x201);
//   }
#endif


   GPUCMD_SetBufferOffset(0);

   if (width > ctr->texture_width)
      width = ctr->texture_width;
   if (height > ctr->texture_height)
      height = ctr->texture_height;

   if (frame)
   {
      if ( ((((u32)(frame)) >= 0x14000000 && ((u32)(frame)) < 0x40000000)) /* Frame in linear memory */
         && !((u32)frame & 0x7F)                                           /* 128-byte aligned */
         && !(pitch & 0xF)                                                 /* 16-byte aligned */
         &&  (pitch > 0x40))
      {
         /* Can copy the buffer directly with the GPU */
#if 0
         GSPGPU_FlushDataCache(frame, pitch * height);
#endif
         ctrGuSetCommandList_First(true,(void*)frame, pitch * height, 0, 0, 0, 0);
         ctrGuCopyImage(true, frame, pitch / (ctr->rgb32? 4: 2), height, ctr->rgb32 ? CTRGU_RGBA8 : CTRGU_RGB565, false,
               ctr->texture_swizzled, ctr->texture_width,                ctr->rgb32 ? CTRGU_RGBA8 : CTRGU_RGB565,  true);
      }
      else
      {
         unsigned int i;
         uint8_t       *dst = (uint8_t*)ctr->texture_linear;
         const uint8_t *src = frame;

         for (i = 0; i < height; i++)
         {
            memcpy(dst, src, width    * (ctr->rgb32? 4: 2));
            dst += ctr->texture_width * (ctr->rgb32? 4: 2);
            src += pitch;
         }
         GSPGPU_FlushDataCache(ctr->texture_linear,
                               ctr->texture_width * ctr->texture_height * (ctr->rgb32? 4: 2));

         ctrGuCopyImage(false, ctr->texture_linear, ctr->texture_width, ctr->texture_height, ctr->rgb32 ? CTRGU_RGBA8: CTRGU_RGB565, false,
                        ctr->texture_swizzled, ctr->texture_width, ctr->rgb32 ? CTRGU_RGBA8: CTRGU_RGB565,  true);

      }

// render game to top screen

#ifdef HAVE_BOTTOM_SCREEN
   if (ctr->should_resize)
   {
      if (video_ctr_render_target == CTR_VIDEO_TARGET_DUAL )
      {
         if (ctr->rotation == 0) /* offset resize texture OK */
         {
            ctr->frame_coords->u0 = 0;
            ctr->frame_coords->v0 = 0;
            ctr->frame_coords->u1 = width;
            ctr->frame_coords->v1 = height / 2;

            if (video_ctr_dual_offset_y != 0 )
            {
               float tmp_percent = ( ((float)ctr->frame_coords->y1 - (float)ctr->frame_coords->y0) / (float)ctr->vp.height );
               ctr->frame_coords->v1 = roundf(tmp_percent * height);
            }

         }
         else if (ctr->rotation == 1) /* offset resize texture OK */
         {
            ctr->frame_coords->u0 = 0;
            ctr->frame_coords->v0 = 0;
            ctr->frame_coords->u1 = width / 2;
            ctr->frame_coords->v1 = height;

            if (video_ctr_dual_offset_y != 0 )
            {
               float tmp_percent = ( ((float)ctr->frame_coords->y1 - (float)ctr->frame_coords->y0) / (float)ctr->vp.height );
               ctr->frame_coords->u1 = roundf(tmp_percent * width);
            }

         }
         else if (ctr->rotation == 2)
         {
            ctr->frame_coords->u0 = 0;
            ctr->frame_coords->v0 = height / 2;
            ctr->frame_coords->u1 = width;
            ctr->frame_coords->v1 = height;

            if (video_ctr_dual_offset_y != 0 )
            {
               float tmp_percent = ( ((float)ctr->frame_coords->y0 - (float)ctr->frame_coords->y1) / (float)ctr->vp.height );
               ctr->frame_coords->u0 = roundf(tmp_percent * height);
            }


         }
         else if (ctr->rotation == 3)
         {
            ctr->frame_coords->u0 = width / 2;
            ctr->frame_coords->v0 = 0;
            ctr->frame_coords->u1 = width;
            ctr->frame_coords->v1 = height;
/*
            if (video_ctr_dual_offset_y != 0 )
            {
               float tmp_percent = ( ((float)ctr->frame_coords->y1 - (float)ctr->frame_coords->y0) / (float)ctr->vp.width );
               ctr->frame_coords->u1 = roundf(tmp_percent * width);
            }
*/
         }
      }
      else
#endif
      {
         ctr->frame_coords->u1 = width;
         ctr->frame_coords->v1 = height;
      }
	  ctr->should_resize = false;
   }
//	  ctr_set_screen_geom(ctr,true);
	  
//	  ctr_set_screen_geom(ctr,(video_ctr_render_target == CTR_VIDEO_TARGET_DUAL)? true:false); // TMPHUU

      GSPGPU_FlushDataCache(ctr->frame_coords, sizeof(ctr_vertex_t));
   }


   if ((ctr->rotation == 1) || (ctr->rotation == 3))
      GPUCMD_AddWrite(GPUREG_GSH_BOOLUNIFORM, ctr->rotation & 1); // try disable shader rotation manipulation?

   ctrGuSetVertexShaderFloatUniform(0, (float*)&ctr->scale_vector, 1);
   ctrGuSetTexture(GPU_TEXUNIT0, VIRT_TO_PHYS(ctr->texture_swizzled), ctr->texture_width, ctr->texture_height,
                  (ctr->smooth? GPU_TEXTURE_MAG_FILTER(GPU_LINEAR)  | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR)
                              : GPU_TEXTURE_MAG_FILTER(GPU_NEAREST) | GPU_TEXTURE_MIN_FILTER(GPU_NEAREST)) |
                  GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
                  ctr->rgb32 ? GPU_RGBA8: GPU_RGB565);

   ctr_check_3D_slider(ctr, (ctr_video_mode_enum)disp_mode);

   /* ARGB --> RGBA  */
   if (ctr->rgb32)
   {
      GPU_SetTexEnv(0,
                    GPU_TEVSOURCES(GPU_TEXTURE0, GPU_CONSTANT, 0),
                    GPU_CONSTANT,
                    GPU_TEVOPERANDS(GPU_TEVOP_RGB_SRC_G, 0, 0),
                    0,
                    GPU_MODULATE,
                    GPU_REPLACE,
                    0xFF0000FF);
      GPU_SetTexEnv(1,
                    GPU_TEVSOURCES(GPU_TEXTURE0, GPU_CONSTANT, GPU_PREVIOUS),
                    GPU_PREVIOUS,
                    GPU_TEVOPERANDS(GPU_TEVOP_RGB_SRC_B, 0, 0),
                    0,
                    GPU_MULTIPLY_ADD,
                    GPU_REPLACE,
                    0x00FF00);
      GPU_SetTexEnv(2,
                    GPU_TEVSOURCES(GPU_TEXTURE0, GPU_CONSTANT, GPU_PREVIOUS),
                    GPU_PREVIOUS,
                    GPU_TEVOPERANDS(GPU_TEVOP_RGB_SRC_ALPHA, 0, 0),
                    0,
                    GPU_MULTIPLY_ADD,
                    GPU_REPLACE,
                    0xFF0000);
   }
		  if (video_ctr_render_target != CTR_VIDEO_TARGET_BOTTOM )
		  {	
   GPU_SetViewport(NULL,
                   VIRT_TO_PHYS(ctr->drawbuffers.top.left),
                   0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
                   ctr->video_mode == CTR_VIDEO_MODE_2D_800X240 ? CTR_TOP_FRAMEBUFFER_WIDTH * 2 : CTR_TOP_FRAMEBUFFER_WIDTH);


   if (ctr->video_mode == CTR_VIDEO_MODE_3D)
   {
      if (ctr->menu_texture_enable)
      {
         ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(&ctr->frame_coords[1]));
         GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);
         ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(&ctr->frame_coords[2]));
      }
      else
      {
         ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(ctr->frame_coords));
         GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);
      }
      GPU_SetViewport(NULL,
                      VIRT_TO_PHYS(ctr->drawbuffers.top.right),
                      0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
                      CTR_TOP_FRAMEBUFFER_WIDTH);
   }
   else
      ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(ctr->frame_coords));

   GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);
		  }
#ifdef HAVE_BOTTOM_SCREEN
// render game to bottom screen

   if (ctr_bottom_display_mode == CTR_BOTTOM_MODE_RETROARCH)
   {
      if (video_ctr_render_target == CTR_VIDEO_TARGET_DUAL )
      {
         if (ctr->rotation == 0) /* 0 */
         {
            ctr->frame_coords_bottom->u0 = 0;
            ctr->frame_coords_bottom->v0 = height / 2;
            ctr->frame_coords_bottom->u1 = width;
            ctr->frame_coords_bottom->v1 = height;
            if (video_ctr_dual_offset_y != 0 )
            {
            float tmp_percent = ( ((float)ctr->frame_coords_bottom->y1 - (float)ctr->frame_coords_bottom->y0) / (float)ctr->vp.height );
            ctr->frame_coords_bottom->v0 = roundf((float)height - (tmp_percent * height));
			}
         }
         else if (ctr->rotation == 1) /* 90 */
         {
            ctr->frame_coords_bottom->u0 = width / 2;
            ctr->frame_coords_bottom->v0 = 0;
            ctr->frame_coords_bottom->u1 = width;
            ctr->frame_coords_bottom->v1 = height;
            if (video_ctr_dual_offset_y != 0 )
            {
            float tmp_percent = ( ((float)ctr->frame_coords_bottom->y1 - (float)ctr->frame_coords_bottom->y0) / (float)ctr->vp.height );
            ctr->frame_coords_bottom->u0 = roundf((float)height - (tmp_percent * height));
			}
         }
         else if (ctr->rotation == 2) /* 180 */
         {
            ctr->frame_coords_bottom->u0 = 0;
            ctr->frame_coords_bottom->v0 = 0;
            ctr->frame_coords_bottom->u1 = width;
            ctr->frame_coords_bottom->v1 = height / 2;
            if (video_ctr_dual_offset_y != 0 )
            {
            float tmp_percent = ( ((float)ctr->frame_coords_bottom->y1 - (float)ctr->frame_coords_bottom->y0) / (float)ctr->vp.height );
            ctr->frame_coords_bottom->v1 = roundf(tmp_percent * height);
			}
         }
         else if (ctr->rotation == 3) /* 270 */
         {
            ctr->frame_coords_bottom->u0 = 0;
            ctr->frame_coords_bottom->v0 = 0;
            ctr->frame_coords_bottom->u1 = width / 2;
            ctr->frame_coords_bottom->v1 = height;
            if (video_ctr_dual_offset_y != 0 )
            {
            float tmp_percent = ( ((float)ctr->frame_coords_bottom->y1 - (float)ctr->frame_coords_bottom->y0) / (float)ctr->vp.width );
            ctr->frame_coords_bottom->v1 = roundf((float)height - (tmp_percent * height));
			}
         }
         if (video_ctr_dual_offset_x != 0 )
         {
//            ctr->frame_coords_bottom->u0 += video_ctr_dual_offset_x;
//            ctr->frame_coords_bottom->u1 += video_ctr_dual_offset_x;
         }
         if (video_ctr_dual_offset_y != 0 )
         {
//            if (ctr->vp.y != 0 )
//            {
//               ctr->frame_coords_bottom->v0 -= roundf((float)video_ctr_dual_offset_y * ((float)ctr->vp.y/((float)height/2)));

//             float tmp_percent = ( ((float)ctr->frame_coords_bottom->y1 - (float)ctr->frame_coords_bottom->y0) / (float)ctr->vp.height );
//             ctr->frame_coords_bottom->v0 = roundf((float)height - (tmp_percent * height));

//            }
//            else
//            {
//               ctr->frame_coords_bottom->v0 -= video_ctr_dual_offset_y; // vp.y / ctr->frame_coords->v1

			   
//            }
//            if (ctr->frame_coords_bottom->v0 < 0 )
//               ctr->frame_coords_bottom->v1 -= video_ctr_dual_offset_y;
         }
      }
      else
      {
         ctr->frame_coords_bottom->u0 = 0;
         ctr->frame_coords_bottom->v0 = 0;
         ctr->frame_coords_bottom->u1 = width;
         ctr->frame_coords_bottom->v1 = height;
      }

      GSPGPU_FlushDataCache(ctr->frame_coords_bottom, sizeof(ctr_vertex_t));

      ctrGuSetVertexShaderFloatUniform(0, (float*)&ctr->scale_vector_bottom, 1);
      ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(ctr->frame_coords_bottom));

      GPU_SetViewport(NULL,
            VIRT_TO_PHYS(ctr->drawbuffers.bottom),
            0, 0, CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
            CTR_BOTTOM_FRAMEBUFFER_WIDTH);
      GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);
   }
#endif // HUUTMP

   /* restore */
   if (ctr->rgb32)
   {
      GPU_SetTexEnv(0, GPU_TEXTURE0, GPU_TEXTURE0, 0, 0, GPU_REPLACE, GPU_REPLACE, 0);
      GPU_SetTexEnv(1, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
      GPU_SetTexEnv(2, GPU_PREVIOUS, GPU_PREVIOUS, 0, 0, 0, 0, 0);
   }

#ifdef HAVE_OVERLAY
   if (ctr->overlay_enabled && overlay_behind_menu)
      ctr_render_overlay(ctr);
#endif

   if(input_ctr_sensors_cursor)
      ctr_render_sensor_cursor(ctr);

#ifdef HAVE_MENU
   if (ctr->menu_texture_enable)
   {
      if (ctr->menu_texture_frame_enable)
      {

         ctrGuSetTexture(GPU_TEXUNIT0, VIRT_TO_PHYS(ctr->menu.texture_swizzled), ctr->menu.texture_width, ctr->menu.texture_height,
                        GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR) |
                        GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
                        GPU_RGBA4);

         GPUCMD_AddWrite(GPUREG_GSH_BOOLUNIFORM, 0);
		  if (video_ctr_render_target != CTR_VIDEO_TARGET_BOTTOM )
		  {		 
         ctrGuSetVertexShaderFloatUniform(0, (float*)&ctr->menu.scale_vector, 1);
         ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(ctr->menu.frame_coords));

         GPU_SetViewport(NULL,
                         VIRT_TO_PHYS(ctr->drawbuffers.top.left),
                         0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
                         ctr->video_mode == CTR_VIDEO_MODE_2D_800X240 ? CTR_TOP_FRAMEBUFFER_WIDTH * 2 : CTR_TOP_FRAMEBUFFER_WIDTH);
         GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);

         if (ctr->video_mode == CTR_VIDEO_MODE_3D)
         {
            GPU_SetViewport(NULL,
                            VIRT_TO_PHYS(ctr->drawbuffers.top.right),
                            0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
                            CTR_TOP_FRAMEBUFFER_WIDTH);
            GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);
         }
		  }
#ifdef HAVE_BOTTOM_SCREEN
// render menu to bottom screen
         if (ctr_bottom_display_mode == CTR_BOTTOM_MODE_RETROARCH &&
		       video_ctr_render_target != CTR_VIDEO_TARGET_DUAL)
         {
            ctrGuSetVertexShaderFloatUniform(0, (float*)&ctr->menu.scale_vector_bottom, 1);
            ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(ctr->menu.frame_coords_bottom));

            GPU_SetViewport(NULL,
                  VIRT_TO_PHYS(ctr->drawbuffers.bottom),
                  0, 0, CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
                  CTR_BOTTOM_FRAMEBUFFER_WIDTH);
            GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);
         }
#endif
      }

      ctr->msg_rendering_enabled = true;

      menu_driver_frame(menu_is_alive, video_info);

      ctr->msg_rendering_enabled = false;
   }
   else if (statistics_show)
   {
      if (osd_params)
      {
         font_driver_render_msg(ctr, stat_text,
               (const struct font_params*)osd_params, NULL);
      }
   }
#endif

#ifdef HAVE_OVERLAY
   if (ctr->overlay_enabled && !overlay_behind_menu)
      ctr_render_overlay(ctr);
#endif

#ifdef HAVE_GFX_WIDGETS
   if (widgets_active)
      gfx_widgets_frame(video_info);
#endif

#ifdef HAVE_BOTTOM_SCREEN
   if (ctr_bottom_display_mode == CTR_BOTTOM_MODE_CONTROL)
   {
      if (ctr->refresh_bottom_menu)
      {
         if (ctr_bottom_state.mode == MODE_SAVESTATE)
         {
            ctr_bottom_render_screen_thumbnail(ctr);
         }
         ctr_bottom_render_screen_menu(ctr);
         ctr_bottom_render_screen_text(ctr);
      }
   }
   
   if (debug_bottom)
      ctr_bottom_render_screen_debug(ctr);

#endif

   if (msg)
      font_driver_render_msg(ctr, msg, NULL, NULL);

   GPU_FinishDrawing();
   GPU_Finalize();
   ctrGuFlushAndRun(true);

   ctrGuDisplayTransfer(true, ctr->drawbuffers.top.left,
                        240,
                        ctr->video_mode == CTR_VIDEO_MODE_2D_800X240 ? 800 : 400,
                        CTRGU_RGBA8,
                        gfxTopLeftFramebuffers[ctr->current_buffer_top], 240, CTRGU_RGB8, CTRGU_MULTISAMPLE_NONE);

   if ((ctr->video_mode == CTR_VIDEO_MODE_2D_400X240) || (ctr->video_mode == CTR_VIDEO_MODE_3D))
      ctrGuDisplayTransfer(
            true,
            ctr->drawbuffers.top.right,
            240,
            400,
            CTRGU_RGBA8,
            gfxTopRightFramebuffers[ctr->current_buffer_top],
            240,
            CTRGU_RGB8,
            CTRGU_MULTISAMPLE_NONE);

#ifdef HAVE_BOTTOM_SCREEN
   if (ctr->refresh_bottom_menu)
      ctrGuDisplayTransfer(
            true,
            ctr->drawbuffers.bottom,
            240,
            320,
            CTRGU_RGBA8,
            gfxBottomFramebuffers[ctr->current_buffer_bottom],
            240,
            CTRGU_RGB8,
            CTRGU_MULTISAMPLE_NONE);
#endif


   /* Swap buffers : */

#ifdef USE_CTRULIB_2
   u32 *buf0, *buf1, *bottom;
   u32 stride;

   buf0 = (u32*)gfxTopLeftFramebuffers[ctr->current_buffer_top];

   if (ctr->video_mode == CTR_VIDEO_MODE_2D_800X240)
   {
      buf1 = (u32*)(gfxTopLeftFramebuffers[ctr->current_buffer_top] + 240 * 3);
      stride = 240 * 3 * 2;
   }
   else
   {
      if (ctr->enable_3d)
         buf1 = (u32*)gfxTopRightFramebuffers[ctr->current_buffer_top];
      else
         buf1 = buf0;

      stride = 240 * 3;
   }

   u8 bit5 = (ctr->enable_3d != 0);

   gspPresentBuffer(GFX_TOP, ctr->current_buffer_top, buf0, buf1,
                    stride, (1<<8)|((1^bit5)<<6)|((bit5)<<5)|GSP_BGR8_OES);

#ifdef HAVE_BOTTOM_SCREEN
   if (ctr->refresh_bottom_menu)
   {
      bottom = (u32*)gfxBottomFramebuffers[ctr->current_buffer_bottom];
      stride = 240 * 3;
      gspPresentBuffer(GFX_BOTTOM, ctr->current_buffer_bottom, bottom, bottom,
            stride, GSP_BGR8_OES);
   }
//   else if (ctr->bottom_is_fading)
//   {
//      gfxScreenSwapBuffers(GFX_BOTTOM,false);
//   }
#endif

#else
   topFramebufferInfo.
      active_framebuf           = ctr->current_buffer_top;
   topFramebufferInfo.
      framebuf0_vaddr           = (u32*)gfxTopLeftFramebuffers[ctr->current_buffer_top];

   if (ctr->video_mode == CTR_VIDEO_MODE_2D_800X240)
   {
      topFramebufferInfo.
         framebuf1_vaddr        = (u32*)(gfxTopLeftFramebuffers[ctr->current_buffer_top] + 240 * 3);
      topFramebufferInfo.
         framebuf_widthbytesize = 240 * 3 * 2;
   }
   else
   {
      if (ctr->enable_3d)
         topFramebufferInfo.
            framebuf1_vaddr     = (u32*)gfxTopRightFramebuffers[ctr->current_buffer_top];
      else
         topFramebufferInfo.
            framebuf1_vaddr     = topFramebufferInfo.framebuf0_vaddr;

      topFramebufferInfo.
         framebuf_widthbytesize = 240 * 3;
   }

   u8 bit5                      = (ctr->enable_3d != 0);
   topFramebufferInfo.format    = (1<<8)|((1^bit5)<<6)|((bit5)<<5)|GSP_BGR8_OES;
   topFramebufferInfo.
      framebuf_dispselect       = ctr->current_buffer_top;
   topFramebufferInfo.unk       = 0x00000000;

   u8* framebufferInfoHeader    = gfxSharedMemory + 0x200 + gfxThreadID * 0x80;
	GSPGPU_FramebufferInfo*
      framebufferInfo           = (GSPGPU_FramebufferInfo*)&framebufferInfoHeader[0x4];
	framebufferInfoHeader[0x0]  ^= 1;
	framebufferInfo[framebufferInfoHeader[0x0]] = topFramebufferInfo;
	framebufferInfoHeader[0x1]   = 1;

#ifdef HAVE_BOTTOM_SCREEN
   if (ctr->refresh_bottom_menu)
   {
      bottomFramebufferInfo.
         active_framebuf           = ctr->current_buffer_bottom;
      bottomFramebufferInfo.
         framebuf0_vaddr           = (u32*)gfxBottomFramebuffers[
         ctr->current_buffer_bottom];

      bottomFramebufferInfo.
         framebuf1_vaddr           = (u32*)(gfxBottomFramebuffers[
               ctr->current_buffer_bottom] + 240 * 3);
      bottomFramebufferInfo.
         framebuf_widthbytesize    = 240 * 3;

      bottomFramebufferInfo.format = GSP_BGR8_OES;
      bottomFramebufferInfo.
         framebuf_dispselect       = ctr->current_buffer_bottom;
      bottomFramebufferInfo.unk    = 0x00000000;

      u8* framebufferInfoHeader2   = gfxSharedMemory + 0x200 + gfxThreadID * 0x80 + 0x40;
      GSPGPU_FramebufferInfo*
         framebufferInfo2          =
         (GSPGPU_FramebufferInfo*)&framebufferInfoHeader2[0x4];
      framebufferInfoHeader2[0x0] ^= 1;
      framebufferInfo2[framebufferInfoHeader2[0x0]] = bottomFramebufferInfo;
      framebufferInfoHeader2[0x1]  = 1;
   }
}
#endif
#endif
   ctr->current_buffer_top        ^= 1;
#ifdef HAVE_BOTTOM_SCREEN
   if (ctr->refresh_bottom_menu)
      ctr->current_buffer_bottom  ^= 1;
#endif
   ctr->p3d_event_pending          = true;
   ctr->ppf_event_pending          = true;
   ctr->refresh_bottom_menu        = false;

   return true;
}

static void ctr_set_nonblock_state(void* data, bool toggle,
      bool a, unsigned b)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      ctr->vsync = !toggle;
}

static bool ctr_alive(void* data)
{
   (void)data;
   return true;
}

static bool ctr_focus(void* data)
{
   (void)data;
   return true;
}

static bool ctr_suppress_screensaver(void* data, bool enable)
{
   (void)data;
   (void)enable;
   return false;
}

static void ctr_free(void* data)
{
   unsigned i;
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (!ctr)
      return;

   aptUnhook(&ctr->lcd_aptHook);
   gspSetEventCallback(GSPGPU_EVENT_VBlank0, NULL, NULL, true);
   shaderProgramFree(&ctr->shader);
   DVLB_Free(ctr->dvlb);
   vramFree(ctr->drawbuffers.top.left);
   vramFree(ctr->drawbuffers.bottom);

   for (i = 0; i < (CTR_TEXTURE_KBD_KEY_TAB + 1); i++)
   {
      struct ctr_bottom_texture_data *o = &ctr->bottom_kbd_textures[i];
      ctr_texture_t *texture = (ctr_texture_t *) o->texture;
      if (texture)
      {
         linearFree(texture->data);
         linearFree(o->frame_coords);
         o->texture = 0;
      }
   }

   for (i = 0; i < CTR_BOTTOM_TEXTURE_THUMBNAIL + 1; i++)
   {
      struct ctr_bottom_texture_data *o = &ctr->bottom_textures[i];
      ctr_texture_t *texture = (ctr_texture_t *) o->texture;
      if (texture)
      {
         linearFree(texture->data);
         linearFree(o->frame_coords);
         o->texture = 0;
      }
   }

   free(ctr->bottom_textures);
   free(ctr->bottom_kbd_textures);
   linearFree(ctr->display_list);
   linearFree(ctr->texture_linear);
   linearFree(ctr->texture_swizzled);
   linearFree(ctr->frame_coords);
   linearFree(ctr->menu.texture_linear);
   linearFree(ctr->menu.texture_swizzled);
   linearFree(ctr->menu.frame_coords);
   linearFree(ctr->frame_coords_bottom);
   linearFree(ctr->menu.frame_coords_bottom);
   linearFree(ctr->vertex_cache.buffer);
   linearFree(ctr);
#if 0
   gfxExit();
#endif
}
static void ctr_set_texture_frame(void* data, const void* frame, bool rgb32,
                                  unsigned width, unsigned height, float alpha)
{
   unsigned int i;
   uint16_t *dst;
   const uint16_t *src;
   ctr_video_t *ctr = (ctr_video_t*)data;
   int line_width   = width;

   if (!ctr || !frame)
      return;

   if (line_width > ctr->menu.texture_width)
      line_width = ctr->menu.texture_width;

   if (height > (unsigned)ctr->menu.texture_height)
      height = (unsigned)ctr->menu.texture_height;

   src = frame;
   dst = (uint16_t*)ctr->menu.texture_linear;
   for (i = 0; i < height; i++)
   {
      memcpy(dst, src, line_width * sizeof(uint16_t));
      dst += ctr->menu.texture_width;
      src += width;
   }

   ctr->menu.frame_coords->x0 = (CTR_TOP_FRAMEBUFFER_WIDTH - width) / 2;
   ctr->menu.frame_coords->y0 = (CTR_TOP_FRAMEBUFFER_HEIGHT - height) / 2;
   ctr->menu.frame_coords->x1 = ctr->menu.frame_coords->x0 + width;
   ctr->menu.frame_coords->y1 = ctr->menu.frame_coords->y0 + height;
   ctr->menu.frame_coords->u0 = 0;
   ctr->menu.frame_coords->v0 = 0;
   ctr->menu.frame_coords->u1 = width;
   ctr->menu.frame_coords->v1 = height;
   GSPGPU_FlushDataCache(ctr->menu.frame_coords, sizeof(ctr_vertex_t));
   ctr->menu_texture_frame_enable = true;
   GSPGPU_FlushDataCache(ctr->menu.texture_linear,
                         ctr->menu.texture_width * ctr->menu.texture_height * sizeof(uint16_t));

   ctrGuCopyImage(false, ctr->menu.texture_linear, ctr->menu.texture_width, ctr->menu.texture_height, CTRGU_RGBA4444,false,
                  ctr->menu.texture_swizzled, ctr->menu.texture_width, CTRGU_RGBA4444,  true);
#ifdef HAVE_BOTTOM_SCREEN
   ctr->menu.frame_coords_bottom->x0 = (CTR_BOTTOM_FRAMEBUFFER_WIDTH - width) / 2;
   ctr->menu.frame_coords_bottom->y0 = (CTR_BOTTOM_FRAMEBUFFER_HEIGHT - height) / 2;
   ctr->menu.frame_coords_bottom->x1 = ctr->menu.frame_coords_bottom->x0 + width;
   ctr->menu.frame_coords_bottom->y1 = ctr->menu.frame_coords_bottom->y0 + height;
   ctr->menu.frame_coords_bottom->u0 = 0;
   ctr->menu.frame_coords_bottom->v0 = 0;
   ctr->menu.frame_coords_bottom->u1 = width;
   ctr->menu.frame_coords_bottom->v1 = height;
#endif
}

static void ctr_set_texture_enable(void* data, bool state, bool full_screen)
{
   ctr_video_t* ctr = (ctr_video_t*)data;
   if (ctr)
      ctr->menu_texture_enable = state;
}

static void ctr_set_rotation(void* data, unsigned rotation)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (!ctr)
      return;

   ctr->rotation      = rotation;
   ctr->should_resize = true;
}
static void ctr_set_filtering(void* data, unsigned index, bool smooth, bool ctx_scaling)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      ctr->smooth = smooth;
}

static void ctr_set_aspect_ratio(void* data, unsigned aspect_ratio_idx)
{
   ctr_video_t *ctr = (ctr_video_t*)data;

   if (!ctr)
      return;

   ctr->keep_aspect   = true;
   ctr->should_resize = true;
}

static void ctr_apply_state_changes(void* data)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      ctr->should_resize = true;
}

static void ctr_viewport_info(void* data, struct video_viewport* vp)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr)
      *vp = ctr->vp;
}

static uintptr_t ctr_load_texture(void *video_data, void *data,
      bool threaded, enum texture_filter_type filter_type)
{
   void* tmpdata;
   ctr_texture_t *texture      = NULL;
   ctr_video_t            *ctr = (ctr_video_t*)video_data;
   struct texture_image *image = (struct texture_image*)data;
   u32 size                    = image->width
      * image->height * sizeof(uint32_t);

   if ((u64)size * 3 > linearSpaceFree())
      return 0;

//   if (!ctr || !image || image->width > 2048 || image->height > 2048)
   if (!ctr || !image || image->width > 1024 || image->height > 1024)

      return 0;

   texture                     = (ctr_texture_t*)
      calloc(1, sizeof(ctr_texture_t));

   texture->width              = next_pow2(image->width);
   texture->height             = next_pow2(image->height);
   texture->active_width       = image->width;
   texture->active_height      = image->height;
   texture->data               = linearAlloc(
         texture->width * texture->height * sizeof(uint32_t));
   texture->type               = filter_type;

   if (!texture->data)
   {
      free(texture);
      return 0;
   }

   if ((image->width <= 32) || (image->height <= 32))
   {
      unsigned int i, j;
      uint32_t* src = (uint32_t*)image->pixels;

      for (j = 0; j < image->height; j++)
         for (i = 0; i < image->width; i++)
         {
            ((uint32_t*)texture->data)[ctrgu_swizzle_coords(i, j,
               texture->width)] =
                    ((*src << 8)  & 0xFF000000) 
                  | ((*src << 8)  & 0x00FF0000)
                  | ((*src << 8)  & 0x0000FF00)
                  | ((*src >> 24) & 0x000000FF);
            src++;
         }
      GSPGPU_FlushDataCache(texture->data, texture->width 
            * texture->height * sizeof(uint32_t));
   }
   else
   {
      unsigned int i;
      uint32_t *src = NULL;
      uint32_t *dst = NULL;

      tmpdata       = linearAlloc(image->width 
            * image->height * sizeof(uint32_t));
      if (!tmpdata)
      {
         free(texture->data);
         free(texture);
         return 0;
      }

      src = (uint32_t*)image->pixels;
      dst = (uint32_t*)tmpdata;

      for (i = 0; i < image->width * image->height; i++)
      {
         *dst = 
              ((*src << 8)  & 0xFF000000) 
            | ((*src << 8)  & 0x00FF0000)
            | ((*src << 8)  & 0x0000FF00)
            | ((*src >> 24) & 0x000000FF);
         dst++;
         src++;
      }

      GSPGPU_FlushDataCache(tmpdata,
            image->width * image->height * sizeof(uint32_t));
      ctrGuCopyImage(true, tmpdata,
            image->width, image->height, CTRGU_RGBA8, false,
            texture->data, texture->width, CTRGU_RGBA8,  true);
#if 0
      gspWaitForEvent(GSPGPU_EVENT_PPF, false);
      ctr->ppf_event_pending = false;
#else
      ctr->ppf_event_pending = true;
#endif
      linearFree(tmpdata);
   }

   return (uintptr_t)texture;
}

static void ctr_unload_texture(void *data, bool threaded,
      uintptr_t handle)
{
   struct ctr_texture *texture   = (struct ctr_texture*)handle;

   if (!texture)
      return;

   if (texture->data)
   {
      if (((u32)texture->data & 0xFF000000) == 0x1F000000)
         vramFree(texture->data);
      else
         linearFree(texture->data);
   }
   free(texture);
}

#ifdef HAVE_OVERLAY
static void ctr_overlay_tex_geom(void *data,
            unsigned image, float x, float y, float w, float h)
{
   ctr_video_t           *ctr = (ctr_video_t *)data;
   struct ctr_overlay_data *o = NULL;

   if (!ctr)
      return;

   if (!(o = (struct ctr_overlay_data *)&ctr->overlay[image]))
      return;

   o->frame_coords->u0 = x*o->texture.width;
   o->frame_coords->v0 = y*o->texture.height;
   o->frame_coords->u1 = w*o->texture.width;
   o->frame_coords->v1 = h*o->texture.height;
   GSPGPU_FlushDataCache(o->frame_coords, sizeof(ctr_vertex_t));
#ifdef HAVE_BOTTOM_SCREEN
   o->frame_coords_bottom->u0 = x*o->texture.width + 40;
   o->frame_coords_bottom->v0 = (y*o->texture.height) + CTR_TOP_FRAMEBUFFER_HEIGHT;
   o->frame_coords_bottom->u1 = w*o->texture.width - 40;
   o->frame_coords_bottom->v1 = (h*o->texture.height);
   GSPGPU_FlushDataCache(o->frame_coords_bottom, sizeof(ctr_vertex_t));
#endif
//   printf("tex_geom[%i]: %i, %i, %i, %i\n", image, o->frame_coords->u0,o->frame_coords->v0,o->frame_coords->u1,o->frame_coords->v1);


}

static void ctr_overlay_vertex_geom(void *data,
            unsigned image, float x, float y, float w, float h)
{
   ctr_video_t           *ctr = (ctr_video_t *)data;
   struct ctr_overlay_data *o = NULL;

   if (!ctr)
      return;

   if (!(o = (struct ctr_overlay_data *)&ctr->overlay[image]))
      return;

   o->frame_coords->x0 = x * CTR_TOP_FRAMEBUFFER_WIDTH;
   o->frame_coords->y0 = y * CTR_TOP_FRAMEBUFFER_HEIGHT;
   o->frame_coords->x1 = w * (o->frame_coords->x0 + o->texture.width);
   o->frame_coords->y1 = h * (o->frame_coords->y0 + o->texture.height);
   GSPGPU_FlushDataCache(o->frame_coords, sizeof(ctr_vertex_t));
#ifdef HAVE_BOTTOM_SCREEN
   o->frame_coords_bottom->x0 = x * CTR_BOTTOM_FRAMEBUFFER_WIDTH;
   o->frame_coords_bottom->y0 = y * CTR_BOTTOM_FRAMEBUFFER_HEIGHT;
   o->frame_coords_bottom->x1 = w * (o->frame_coords->x0 + o->texture.width);
   o->frame_coords_bottom->y1 = h * (o->frame_coords->y0 + (o->texture.height - CTR_TOP_FRAMEBUFFER_HEIGHT ));
   GSPGPU_FlushDataCache(o->frame_coords_bottom, sizeof(ctr_vertex_t));
#endif
//   printf("vertex_geom[%i]: %i, %i, %i, %i\n", image, o->frame_coords->x0,o->frame_coords->y0,o->frame_coords->x1,o->frame_coords->y1);

}

static bool ctr_overlay_load(void *data,
            const void *image_data, unsigned num_images)
{
   unsigned int i, j;
   void *tmpdata;
   ctr_texture_t       *texture = NULL;
   ctr_video_t             *ctr = (ctr_video_t *)data;
   struct texture_image *images = (struct texture_image *)image_data;

   if (!ctr)
      return false;

   ctr_free_overlay(ctr);

   ctr->overlay  = (struct ctr_overlay_data *)calloc(num_images, sizeof(*ctr->overlay));
   ctr->overlays = num_images;

   for (i = 0; i < num_images; i++)
   {
      uint32_t *src = NULL;
      uint32_t *dst = NULL;
      struct ctr_overlay_data *o = (struct ctr_overlay_data *)&ctr->overlay[i];

      o->frame_coords = linearAlloc(sizeof(ctr_vertex_t));
#ifdef HAVE_BOTTOM_SCREEN
	  o->frame_coords_bottom = linearAlloc(sizeof(ctr_vertex_t));
#endif
      if (!ctr || !images || images[i].width > 1024 || images[i].height > 1024)
         return 0;

      memset(&o->texture, 0, sizeof(ctr_texture_t));

      o->texture.width              = next_pow2(images[i].width);
      o->texture.height             = next_pow2(images[i].height);
      o->texture.data               = linearAlloc(
            o->texture.width * o->texture.height * sizeof(uint32_t));

      if (!o->texture.data)
      {
         free(o);
         return 0;
      }


      tmpdata = linearAlloc(images[i].width
            * images[i].height * sizeof(uint32_t));
      if (!tmpdata)
      {
         linearFree(o->texture.data);
         free(o);
         return 0;
      }

      src = (uint32_t*)images[i].pixels;
      dst = (uint32_t*)tmpdata;

      for (j = 0; j < images[i].width * images[i].height; j++)
      {
         *dst = 
              ((*src << 8)  & 0xFF000000) 
            | ((*src << 8)  & 0x00FF0000)
            | ((*src << 8)  & 0x0000FF00)
            | ((*src >> 24) & 0x000000FF);
         dst++;
         src++;
      }

      GSPGPU_FlushDataCache(tmpdata,
            images[i].width * images[i].height * sizeof(uint32_t));
      ctrGuCopyImage(true, tmpdata,
            images[i].width, images[i].height, CTRGU_RGBA8, false,
            o->texture.data, o->texture.width, CTRGU_RGBA8,  true);

      ctr->ppf_event_pending = true;

      linearFree(tmpdata);

      ctr_overlay_tex_geom(ctr, i, 0, 0, 1, 1);
      ctr_overlay_vertex_geom(ctr, i, 0, 0, 1, 1);

      CTR_SET_SCALE_VECTOR(&o->scale_vector,
                       CTR_TOP_FRAMEBUFFER_WIDTH, CTR_TOP_FRAMEBUFFER_HEIGHT,
                       o->texture.width, o->texture.height);

      ctr->overlay[i].alpha_mod = 1.0f;
   }

   return true;
}

static void ctr_overlay_enable(void *data, bool state)
{
   ctr_video_t *ctr = (ctr_video_t *)data;

   if (ctr)
      ctr->overlay_enabled = state;
}

static void ctr_overlay_full_screen(void *data, bool enable)
{
   ctr_video_t *ctr = (ctr_video_t *)data;
   ctr->overlay_full_screen = enable;
}

static void ctr_overlay_set_alpha(void *data, unsigned image, float mod){ }

static void ctr_render_overlay(ctr_video_t *ctr)
{
   unsigned int i;
   settings_t             *settings = config_get_ptr();
   unsigned ctr_bottom_display_mode = settings->uints.ctr_bottom_display_mode;

   for (i = 0; i < ctr->overlays; i++)
   {
      ctrGuSetTexture(GPU_TEXUNIT0, VIRT_TO_PHYS(ctr->overlay[i].texture.data), ctr->overlay[i].texture.width, ctr->overlay[i].texture.height,
                      GPU_TEXTURE_MAG_FILTER(GPU_LINEAR) | GPU_TEXTURE_MIN_FILTER(GPU_LINEAR) |
                      GPU_TEXTURE_WRAP_S(GPU_CLAMP_TO_EDGE) | GPU_TEXTURE_WRAP_T(GPU_CLAMP_TO_EDGE),
                      GPU_RGBA8);

      GPUCMD_AddWrite(GPUREG_GSH_BOOLUNIFORM, 0);
      ctrGuSetVertexShaderFloatUniform(0, (float*)&ctr->overlay[i].scale_vector, 1);
      ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(ctr->overlay[i].frame_coords));


      GPU_SetViewport(NULL,
            VIRT_TO_PHYS(ctr->drawbuffers.top.left),
            0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
            ctr->video_mode == CTR_VIDEO_MODE_2D_800X240 ? CTR_TOP_FRAMEBUFFER_WIDTH * 2 : CTR_TOP_FRAMEBUFFER_WIDTH);
      GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);

      if (ctr->video_mode == CTR_VIDEO_MODE_3D)
      {
         GPU_SetViewport(NULL,
               VIRT_TO_PHYS(ctr->drawbuffers.top.right),
               0, 0, CTR_TOP_FRAMEBUFFER_HEIGHT,
               CTR_TOP_FRAMEBUFFER_WIDTH);
         GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);
      }
#ifdef HAVE_BOTTOM_SCREEN
      if (ctr_bottom_display_mode == CTR_BOTTOM_MODE_OVERLAY || ctr_bottom_display_mode == CTR_BOTTOM_MODE_RETROARCH)
      {
         ctrGuSetVertexShaderFloatUniform(0, (float*)&ctr->overlay[i].scale_vector, 1);
         ctrGuSetAttributeBuffersAddress(VIRT_TO_PHYS(ctr->overlay[i].frame_coords_bottom));


         GPU_SetViewport(NULL,
               VIRT_TO_PHYS(ctr->drawbuffers.bottom),
               CTR_TOP_FRAMEBUFFER_HEIGHT, 0, CTR_BOTTOM_FRAMEBUFFER_HEIGHT,
               CTR_BOTTOM_FRAMEBUFFER_WIDTH);
         GPU_DrawArray(GPU_GEOMETRY_PRIM, 0, 1);
      }
#endif
   }
}

static const video_overlay_interface_t ctr_overlay = {
   ctr_overlay_enable,
   ctr_overlay_load,
   ctr_overlay_tex_geom,
   ctr_overlay_vertex_geom,
   ctr_overlay_full_screen,
   ctr_overlay_set_alpha,
};

void ctr_overlay_interface(void *data, const video_overlay_interface_t **iface)
{
   ctr_video_t *ctr = (ctr_video_t *)data;

   if (ctr)
      *iface = &ctr_overlay;
}
#endif

static void ctr_set_osd_msg(void *data, const char *msg,
      const struct font_params *params, void *font)
{
   ctr_video_t* ctr = (ctr_video_t*)data;

   if (ctr && ctr->msg_rendering_enabled)
      font_driver_render_msg(data, msg, params, font);
}

static uint32_t ctr_get_flags(void *data)
{
   uint32_t             flags   = 0;

   BIT32_SET(flags, GFX_CTX_FLAGS_OVERLAY_BEHIND_MENU_SUPPORTED);

   return flags;
}

static const video_poke_interface_t ctr_poke_interface = {
   ctr_get_flags,
   ctr_load_texture,
   ctr_unload_texture,
   NULL, /* set_video_mode */
   NULL, /* get_refresh_rate */
   ctr_set_filtering,
   NULL, /* get_video_output_size */
   NULL, /* get_video_output_prev */
   NULL, /* get_video_output_next */
   NULL, /* get_current_framebuffer */
   NULL, /* get_proc_address */
   ctr_set_aspect_ratio,
   ctr_apply_state_changes,
   ctr_set_texture_frame,
   ctr_set_texture_enable,
   font_driver_render_msg, /* ctr_set_osd_msg, */
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

static void ctr_get_poke_interface(void* data,
                                   const video_poke_interface_t** iface)
{
   *iface = &ctr_poke_interface;
}

#ifdef HAVE_GFX_WIDGETS
static bool ctr_widgets_enabled(void *data) { return true; }
#endif
static bool ctr_set_shader(void* data,
      enum rarch_shader_type type, const char* path) { return false; }

video_driver_t video_ctr =
{
   ctr_init,
   ctr_frame,
   ctr_set_nonblock_state,
   ctr_alive,
   ctr_focus,
   ctr_suppress_screensaver,
   NULL, /* has_windowed */
   ctr_set_shader,
   ctr_free,
   "ctr",
   NULL, /* set_viewport */
   ctr_set_rotation,
   ctr_viewport_info,
   NULL, /* read_viewport  */
   NULL, /* read_frame_raw */
#ifdef HAVE_OVERLAY
   ctr_overlay_interface,
#endif
   ctr_get_poke_interface,
   NULL, /* wrap_type_to_enum */
#ifdef HAVE_GFX_WIDGETS
   ctr_widgets_enabled
#endif
};

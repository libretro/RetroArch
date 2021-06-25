/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2018      - Stuart Carnie
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

#include <retro_miscellaneous.h>

#ifdef HAVE_CONFIG_H
#include "../../config.h"
#endif

#include "../gfx_display.h"

#include "../font_driver.h"
#include "../../retroarch.h"
#import "../common/metal_common.h"

static const float *gfx_display_metal_get_default_vertices(void)
{
   return [MenuDisplay defaultVertices];
}

static const float *gfx_display_metal_get_default_tex_coords(void)
{
   return [MenuDisplay defaultTexCoords];
}

static void *gfx_display_metal_get_default_mvp(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return NULL;

   return (void *)&md.viewportMVP->projectionMatrix;
}

static void gfx_display_metal_blend_begin(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;

   md.display.blend = YES;
}

static void gfx_display_metal_blend_end(void *data)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;

   md.display.blend = NO;
}

static void gfx_display_metal_draw(gfx_display_ctx_draw_t *draw,
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md || !draw)
      return;

   [md.display draw:draw];
}

static void gfx_display_metal_draw_pipeline(
      gfx_display_ctx_draw_t *draw,
      gfx_display_t *p_disp,
      void *data,
      unsigned video_width,
      unsigned video_height)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md || !draw)
      return;

   [md.display drawPipeline:draw];
}

static void gfx_display_metal_scissor_begin(
      void *data,
      unsigned video_width,
      unsigned video_height,
      int x, int y, unsigned width, unsigned height)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;

   MTLScissorRect r = {.x = (NSUInteger)x, .y = (NSUInteger)y, .width = width, .height = height};
   [md.display setScissorRect:r];
}

static void gfx_display_metal_scissor_end(void *data,
      unsigned video_width,
      unsigned video_height)
{
   MetalDriver *md = (__bridge MetalDriver *)data;
   if (!md)
      return;

   [md.display clearScissorRect];
}

static bool gfx_display_metal_font_init_first(
   void **font_handle, void *video_data,
   const char *font_path, float font_size,
   bool is_threaded)
{
   font_data_t **handle = (font_data_t **)font_handle;
   *handle = font_driver_init_first(video_data,
         font_path, font_size, true,
         is_threaded,
         FONT_DRIVER_RENDER_METAL_API);

   if (*handle)
      return true;

   return false;
}

gfx_display_ctx_driver_t gfx_display_ctx_metal = {
   .draw                   = gfx_display_metal_draw,
   .draw_pipeline          = gfx_display_metal_draw_pipeline,
   .blend_begin            = gfx_display_metal_blend_begin,
   .blend_end              = gfx_display_metal_blend_end,
   .get_default_mvp        = gfx_display_metal_get_default_mvp,
   .get_default_vertices   = gfx_display_metal_get_default_vertices,
   .get_default_tex_coords = gfx_display_metal_get_default_tex_coords,
   .font_init_first        = gfx_display_metal_font_init_first,
   .type                   = GFX_VIDEO_DRIVER_METAL,
   .ident                  = "gfx_display_metal",
   .handles_transform      = NO,
   .scissor_begin          = gfx_display_metal_scissor_begin,
   .scissor_end            = gfx_display_metal_scissor_end
};

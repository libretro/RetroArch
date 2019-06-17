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

#include "../menu_driver.h"

#include "../../gfx/font_driver.h"
#include "../../retroarch.h"
#import "../../gfx/common/metal_common.h"

static const float *menu_display_metal_get_default_vertices(void)
{
   return [MenuDisplay defaultVertices];
}

static const float *menu_display_metal_get_default_tex_coords(void)
{
   return [MenuDisplay defaultTexCoords];
}

static void *menu_display_metal_get_default_mvp(video_frame_info_t *video_info)
{
   MetalDriver *md = (__bridge MetalDriver *)video_info->userdata;
   if (!md)
      return NULL;

   return (void *)&md.viewportMVP->projectionMatrix;
}

static void menu_display_metal_blend_begin(video_frame_info_t *video_info)
{
   MetalDriver *md = (__bridge MetalDriver *)video_info->userdata;
   if (!md)
      return;

   md.display.blend = YES;
}

static void menu_display_metal_blend_end(video_frame_info_t *video_info)
{
   MetalDriver *md = (__bridge MetalDriver *)video_info->userdata;
   if (!md)
      return;

   md.display.blend = NO;
}

static void menu_display_metal_draw(menu_display_ctx_draw_t *draw,
                                    video_frame_info_t *video_info)
{
   MetalDriver *md = (__bridge MetalDriver *)video_info->userdata;
   if (!md || !draw)
      return;

   [md.display draw:draw video:video_info];
}

static void menu_display_metal_draw_pipeline(menu_display_ctx_draw_t *draw, video_frame_info_t *video_info)
{
   MetalDriver *md = (__bridge MetalDriver *)video_info->userdata;
   if (!md || !draw)
      return;

   [md.display drawPipeline:draw video:video_info];
}

static void menu_display_metal_viewport(menu_display_ctx_draw_t *draw,
                                        video_frame_info_t *video_info)
{
}

static void menu_display_metal_scissor_begin(video_frame_info_t *video_info, int x, int y, unsigned width, unsigned height)
{
   MetalDriver *md = (__bridge MetalDriver *)video_info->userdata;
   if (!md)
      return;

   MTLScissorRect r = {.x = (NSUInteger)x, .y = (NSUInteger)y, .width = width, .height = height};
   [md.display setScissorRect:r];
}

static void menu_display_metal_scissor_end(video_frame_info_t *video_info)
{
   MetalDriver *md = (__bridge MetalDriver *)video_info->userdata;
   if (!md)
      return;

   [md.display clearScissorRect];
}

static void menu_display_metal_restore_clear_color(void)
{
   /* nothing to do */
}

static void menu_display_metal_clear_color(menu_display_ctx_clearcolor_t *clearcolor,
                                           video_frame_info_t *video_info)
{
   MetalDriver *md = (__bridge MetalDriver *)video_info->userdata;
   if (!md)
      return;

   md.display.clearColor = MTLClearColorMake(clearcolor->r, clearcolor->g, clearcolor->b, clearcolor->a);
}

static bool menu_display_metal_font_init_first(
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

menu_display_ctx_driver_t menu_display_ctx_metal = {
   .draw                   = menu_display_metal_draw,
   .draw_pipeline          = menu_display_metal_draw_pipeline,
   .viewport               = menu_display_metal_viewport,
   .blend_begin            = menu_display_metal_blend_begin,
   .blend_end              = menu_display_metal_blend_end,
   .restore_clear_color    = menu_display_metal_restore_clear_color,
   .clear_color            = menu_display_metal_clear_color,
   .get_default_mvp        = menu_display_metal_get_default_mvp,
   .get_default_vertices   = menu_display_metal_get_default_vertices,
   .get_default_tex_coords = menu_display_metal_get_default_tex_coords,
   .font_init_first        = menu_display_metal_font_init_first,
   .type                   = MENU_VIDEO_DRIVER_METAL,
   .ident                  = "menu_display_metal",
   .handles_transform      = NO,
   .scissor_begin          = menu_display_metal_scissor_begin,
   .scissor_end            = menu_display_metal_scissor_end
};

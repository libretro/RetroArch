/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2016-2017 - Hans-Kristian Arntzen
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

#include <string.h>

#include <encodings/utf.h>
#include <compat/strl.h>

#include "../common/metal_common.h"

#include "../font_driver.h"

typedef struct {
   int stride;
   void * mapped;
} metal_texture_t;

typedef struct
{
   const font_renderer_driver_t *font_driver;
   void *font_data;
   metal_texture_t texture;
   struct font_atlas *atlas;
} font_ctx_t;

@interface MetalRaster: NSObject {
   font_ctx_t *_font;
}

@property (readwrite) MetalDriver *metal;
@property (readwrite) font_ctx_t *font;
@property (readwrite) bool needsUpdate;

- (instancetype)initWithDriver:(MetalDriver *)metal fontPath:(const char *)font_path fontSize:(unsigned)font_size;

@end

@implementation MetalRaster

- (instancetype)initWithDriver:(MetalDriver *)metal fontPath:(const char *)font_path fontSize:(unsigned)font_size {
   if (self = [super init])
   {
      if (metal == nil)
         return nil;

      _metal = metal;
      _font = (font_ctx_t *)calloc(1, sizeof(font_ctx_t));
      if (!font_renderer_create_default((const void**)&_font->font_driver,
                                        &_font->font_data, font_path, font_size))
      {
         RARCH_WARN("Couldn't initialize font renderer.\n");
         return nil;
      }

      _font->atlas = _font->font_driver->get_atlas(_font->font_data);

      //   font->texture = vulkan_create_texture(font->vk, NULL,
      //         font->atlas->width, font->atlas->height, VK_FORMAT_R8_UNORM, font->atlas->buffer,
      //         NULL /*&swizzle*/, VULKAN_TEXTURE_STAGING);
      //
      //   vulkan_map_persistent_texture(
      //         font->vk->context->device, &font->texture);
      //
      //   font->texture_optimal = vulkan_create_texture(font->vk, NULL,
      //         font->atlas->width, font->atlas->height, VK_FORMAT_R8_UNORM, NULL,
      //         NULL /*&swizzle*/, VULKAN_TEXTURE_DYNAMIC);
      //
      _needsUpdate = true;
   }
   return self;
}

- (void)dealloc {
   if (_font) {
      if (_font->font_driver && _font->font_data) {
         _font->font_driver->free(_font->font_data);
         _font->font_data = NULL;
         _font->font_driver = NULL;
      }

      free(_font);
      _font = nil;
   }
}

@end



static void metal_raster_font_free_font(void *data, bool is_threaded);

static void *metal_raster_font_init_font(void *data,
      const char *font_path, float font_size,
      bool is_threaded)
{
   MetalRaster *r = [[MetalRaster alloc] initWithDriver:(__bridge MetalDriver *)data fontPath:font_path fontSize:font_size];

   if (!r)
      return NULL;

   return (__bridge_retained void *)r;
}

static void metal_raster_font_free_font(void *data, bool is_threaded)
{
   MetalRaster * r = (__bridge_transfer MetalRaster *)data;
   r = nil;
}

static INLINE void metal_raster_font_update_glyph(MetalRaster *r, const struct font_glyph *glyph)
{
   font_ctx_t * font = r.font;

   if(font->atlas->dirty)
   {
      unsigned row;
      for (row = glyph->atlas_offset_y; row < (glyph->atlas_offset_y + glyph->height); row++)
      {
         uint8_t *src = font->atlas->buffer + row * font->atlas->width + glyph->atlas_offset_x;
         uint8_t *dst = (uint8_t*)font->texture.mapped + row * font->texture.stride + glyph->atlas_offset_x;
         memcpy(dst, src, glyph->width);
      }

      font->atlas->dirty = false;
      r.needsUpdate = true;
   }
}

static int metal_get_message_width(void *data, const char *msg,
      unsigned msg_len, float scale)
{
   MetalRaster * r = (__bridge MetalRaster *)data;
   font_ctx_t *font = r.font;

   unsigned i;
   int delta_x = 0;

   if (!font)
      return 0;

   for (i = 0; i < msg_len; i++)
   {
      const struct font_glyph *glyph =
         font->font_driver->get_glyph(font->font_data, (uint8_t)msg[i]);
      if (!glyph) /* Do something smarter here ... */
         glyph = font->font_driver->get_glyph(font->font_data, '?');


      if (glyph)
      {
         metal_raster_font_update_glyph(r, glyph);
         delta_x += glyph->advance_x;
      }
   }

   return delta_x * scale;
}

static void metal_raster_font_render_line(
      MetalRaster *r, const char *msg, unsigned msg_len,
      float scale, const float color[4], float pos_x,
      float pos_y, unsigned text_align)
{

}

static void metal_raster_font_render_message(
      MetalRaster *r, const char *msg, float scale,
      const float color[4], float pos_x, float pos_y,
      unsigned text_align)
{
   font_ctx_t *font = r.font;

   int lines = 0;
   float line_height;

   if (!msg || !*msg || !r.metal)
      return;

   /* If the font height is not supported just draw as usual */
   if (!font->font_driver->get_line_height)
   {
      if (r.metal)
         metal_raster_font_render_line(r, msg, (unsigned)strlen(msg),
               scale, color, pos_x, pos_y, text_align);
      return;
   }

   line_height = (float) font->font_driver->get_line_height(font->font_data) *
                     scale / r.metal.viewport->height;

   for (;;)
   {
      const char *delim = strchr(msg, '\n');

      /* Draw the line */
      if (delim)
      {
         unsigned msg_len = (unsigned)(delim - msg);
         if (r.metal)
            metal_raster_font_render_line(r, msg, msg_len,
                  scale, color, pos_x, pos_y - (float)lines * line_height,
                  text_align);
         msg += msg_len + 1;
         lines++;
      }
      else
      {
         unsigned msg_len = (unsigned)strlen(msg);
         if (r.metal)
            metal_raster_font_render_line(r, msg, msg_len,
                  scale, color, pos_x, pos_y - (float)lines * line_height,
                  text_align);
         break;
      }
   }
}

static void metal_raster_font_render_msg(
      video_frame_info_t *video_info,
      void *data, const char *msg,
      const struct font_params *params)
{
   MetalRaster *r = (__bridge MetalRaster *)data;
   
   if (!r || !msg || !*msg)
      return;
   
   
}

static const struct font_glyph *metal_raster_font_get_glyph(
      void *data, uint32_t code)
{
   const struct font_glyph* glyph;
   MetalRaster * r = (__bridge MetalRaster *)data;
   font_ctx_t *font = r.font;

   if (!font || !font->font_driver)
      return NULL;
   
   if (!font->font_driver->ident)
       return NULL;

   glyph = font->font_driver->get_glyph((void*)font->font_driver, code);

   if(glyph)
      metal_raster_font_update_glyph(r, glyph);

   return glyph;
}

static void metal_raster_font_flush_block(unsigned width, unsigned height,
      void *data, video_frame_info_t *video_info)
{
   (void)data;
}

static void metal_raster_font_bind_block(void *data, void *userdata)
{
   (void)data;
}

font_renderer_t metal_raster_font = {
   metal_raster_font_init_font,
   metal_raster_font_free_font,
   metal_raster_font_render_msg,
   "Metal raster",
   metal_raster_font_get_glyph,
   metal_raster_font_bind_block,
   metal_raster_font_flush_block,
   metal_get_message_width
};

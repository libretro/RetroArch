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

#ifndef __FONT_DRIVER_H__
#define __FONT_DRIVER_H__

#include <stdint.h>

#include <boolean.h>
#include <retro_common_api.h>
#include <retro_inline.h>

#include "../retroarch.h"

#include "video_defines.h"

RETRO_BEGIN_DECLS

typedef struct font_renderer
{
   void *(*init)(void *data, const char *font_path,
         float font_size, bool is_threaded);
   void (*free)(void *data, bool is_threaded);
   void (*render_msg)(void *userdata,
         void *data, const char *msg,
         const struct font_params *params);
   const char *ident;

   const struct font_glyph *(*get_glyph)(void *data, uint32_t code);
   void (*bind_block)(void *data, void *block);
   void (*flush)(unsigned width, unsigned height, void *data);

   int (*get_message_width)(void *data, const char *msg, size_t msg_len, float scale);
   bool (*get_line_metrics)(void* data, struct font_line_metrics **metrics);
} font_renderer_t;

/* NOTE: All functions are required to be implemented for font_renderer_driver */

typedef struct font_renderer_driver
{
   void *(*init)(const char *font_path, float font_size);

   struct font_atlas *(*get_atlas)(void *data);

   /* Returns NULL if no glyph for this code is found. */
   const struct font_glyph *(*get_glyph)(void *data, uint32_t code);

   void (*free)(void *data);

   const char *(*get_default_font)(void);

   const char *ident;

   void (*get_line_metrics)(void* data, struct font_line_metrics **metrics);
} font_renderer_driver_t;

typedef struct
{
   const font_renderer_t *renderer;
   void *renderer_data;
   float size;
} font_data_t;

/* This structure holds all objects + metadata
 * corresponding to a particular font */
typedef struct
{
   font_data_t *font;
   video_font_raster_block_t raster_block; /* ptr alignment */
   unsigned glyph_width;
   unsigned wideglyph_width;
   int line_height;
   int line_ascender;
   int line_centre_offset;
} font_data_impl_t;

void font_driver_bind_block(void *font_data, void *block);

static INLINE void font_bind(font_data_impl_t *font_data)
{
   font_driver_bind_block(font_data->font, &font_data->raster_block);
   font_data->raster_block.carr.coords.vertices = 0;
}

static INLINE void font_unbind(font_data_impl_t *font_data)
{
   font_driver_bind_block(font_data->font, NULL);
}

/* font_path can be NULL for default font. */
int font_renderer_create_default(
      const font_renderer_driver_t **drv,
      void **handle,
      const char *font_path, unsigned font_size);

void font_driver_render_msg(void *data,
      const char *msg, const struct font_params *params, void *font_data);

int font_driver_get_message_width(void *font_data, const char *msg, size_t len, float scale);

void font_driver_free(font_data_t *font);

void font_flush(
      unsigned video_width,
      unsigned video_height,
      font_data_impl_t *font_data);

font_data_t *font_driver_init_first(
      void *video_data,
      const char *font_path,
      float font_size,
      bool threading_hint,
      bool is_threaded,
      enum font_driver_render_api api);

void font_driver_init_osd(
      void *video_data,
      const video_info_t *video_info,
      bool threading_hint,
      bool is_threaded,
      enum font_driver_render_api api);

void font_driver_free_osd(void);

int font_driver_get_line_height(font_data_t *font, float scale);
int font_driver_get_line_ascender(font_data_t *font, float scale);
int font_driver_get_line_descender(font_data_t *font, float scale);
int font_driver_get_line_centre_offset(font_data_t *font, float scale);

extern font_renderer_t gl2_raster_font;
extern font_renderer_t gl3_raster_font;
extern font_renderer_t gl1_raster_font;
extern font_renderer_t d3d9x_win32_font;
extern font_renderer_t ps2_font;
extern font_renderer_t vita2d_vita_font;
extern font_renderer_t ctr_font;
extern font_renderer_t wiiu_font;
extern font_renderer_t vulkan_raster_font;
extern font_renderer_t metal_raster_font;
extern font_renderer_t d3d10_font;
extern font_renderer_t d3d11_font;
extern font_renderer_t d3d12_font;
extern font_renderer_t caca_font;
extern font_renderer_t gdi_font;
extern font_renderer_t vga_font;
extern font_renderer_t sixel_font;
extern font_renderer_t switch_font;
extern font_renderer_t rsx_font;

extern font_renderer_driver_t stb_font_renderer;
extern font_renderer_driver_t stb_unicode_font_renderer;
extern font_renderer_driver_t freetype_font_renderer;
extern font_renderer_driver_t coretext_font_renderer;
extern font_renderer_driver_t bitmap_font_renderer;

RETRO_END_DECLS

#endif

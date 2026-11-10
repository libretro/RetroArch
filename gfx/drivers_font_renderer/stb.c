/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2015-2014 - Hans-Kristian Arntzen
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

#include <ctype.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include <stdint.h>

#include <string/stdstring.h>
#include <retro_miscellaneous.h>

#ifdef WIIU
#include <wiiu/os.h>
#endif

#include <gfx/font/rtt.h>

#include "../font_driver.h"

#include "../bitmapfont.h"

typedef struct
{
   uint8_t *font_data;
   size_t font_data_size;
   rtt_scratch_t scratch;                /* ptr alignment */
   rtt_font_t info;                       /* ptr alignment */
   float scale_factor;
   struct font_line_metrics line_metrics; /* float alignment */
   /* The cell every glyph is drawn into, packed. */
   unsigned cell_dims;
   /* Scale of the built-in 5x10 glyphs; 0 for a TrueType face. */
   unsigned builtin_scale;
} stb_face_t;

static void font_rasterizer_stb_free(void *data)
{
   stb_face_t *self = (stb_face_t*)data;
   if (!self)
      return;
   /* The bytes are borrowed - from font_driver.c, or on WiiU from the
    * OS shared font - and never freed here. */
   rtt_scratch_free(&self->scratch);
   free(self);
}

/* Built-in fallback: the 5x10 bitmap in bitmap.h, scaled to the
 * requested size. Used when no TrueType font can be found, so that
 * on-screen text still appears on a system with no fonts installed and
 * no assets downloaded. */
static bool font_rasterizer_stb_init_builtin(stb_face_t *self,
      float font_size)
{
   unsigned scale;

   if (!(scale = (unsigned)roundf(font_size / FONT_HEIGHT)))
      scale = 1;

   self->builtin_scale          = scale;
   self->cell_dims              = VIDEO_SCALE_PACK(FONT_WIDTH * scale,
         FONT_HEIGHT * scale);
   self->line_metrics.ascender  = (float)FONT_HEIGHT_BASELINE_OFFSET * scale;
   self->line_metrics.descender = (float)(FONT_HEIGHT
         - FONT_HEIGHT_BASELINE_OFFSET) * scale;
   self->line_metrics.height    = (float)FONT_HEIGHT_STRIDE * scale;
   return true;
}

static void *font_rasterizer_stb_init(uint8_t *font_data,
      size_t font_data_len, unsigned face_index, float font_size)
{
   int ascent, descent, line_gap, max_glyph_size;
   stb_face_t *self = (stb_face_t*)calloc(1, sizeof(*self));

   /* stb_truetype has no collection support, so the face index is
    * not applicable here. */
   (void)face_index;

   if (!self || font_size < 1.0f)
      goto error;

#ifdef WIIU
   if (!font_data)
   {
      uint32_t size = 0;
      /* OS-owned shared memory: borrowed, not owned. */
      if (!OSGetSharedData(SHARED_FONT_DEFAULT, 0,
               (void**)&self->font_data, &size))
         goto error;
      self->font_data_size = size;
   }
   else
#endif
   if (!font_data || !font_data_len)
   {
      /* Nothing was loaded for us - either no candidate existed or it
       * could not be read - so fall back to the built-in glyphs. */
      if (!font_rasterizer_stb_init_builtin(self, font_size))
         goto error;
      return self;
   }
   else
   {
      /* Borrowed: rtt_font_t keeps a pointer into the buffer, which
       * font_driver.c keeps alive for as long as this face. */
      self->font_data      = font_data;
      self->font_data_size = font_data_len;
   }

   {
      int font_offs = rtt_font_offset_for_index(self->font_data,
            self->font_data_size, 0);
      if (font_offs < 0 || !rtt_init(&self->info, self->font_data,
               self->font_data_size, font_offs))
         goto error;
   }

   rtt_vmetrics(&self->info, &ascent, &descent, &line_gap);

   /* The size is pixels to the em */
   if ((self->scale_factor = rtt_scale_for_em_to_pixels(&self->info,
               font_size)) <= 0.0f)
      goto error;

   /* Ascender, descender and line_gap values always
    * end up ~0.5 pixels too small when scaled...
    * > Add a manual correction factor */
   self->line_metrics.ascender  = 0.5f + (float)ascent * self->scale_factor;
   self->line_metrics.descender = 0.5f + ((float)(-descent) * self->scale_factor);
   self->line_metrics.height    = 0.5f + (float)(ascent - descent + line_gap) * self->scale_factor;

   /* Every glyph cell is the em on a side, bounded so the atlas built
    * from it stays within texture limits. */
   max_glyph_size = (int)font_size;
   if (max_glyph_size > 127)
      max_glyph_size = 127;
   self->cell_dims = VIDEO_SCALE_PACK(max_glyph_size, max_glyph_size);

   return self;

error:
   free(self);
   return NULL;
}

static unsigned font_rasterizer_stb_glyph_index(void *data, uint32_t code)
{
   stb_face_t *self = (stb_face_t*)data;
   if (self->builtin_scale)
      return (code < 256) ? code + 1 : 0;
   return (unsigned)rtt_find_glyph(&self->info, code);
}

static bool font_rasterizer_stb_render_builtin(stb_face_t *self,
      unsigned gi, uint8_t *dst, unsigned pitch, unsigned cell_dims,
      enum font_atlas_format fmt, struct font_glyph *glyph)
{
   unsigned x, y, xo, yo;
   unsigned scale  = self->builtin_scale;
   unsigned cell_w = VIDEO_SCALE_W(cell_dims);
   unsigned cell_h = VIDEO_SCALE_H(cell_dims);
   unsigned i;
   bool     fmt16  = (fmt == FONT_ATLAS_FORMAT_A16);
   size_t   esz    = fmt16 ? sizeof(uint16_t) : sizeof(uint8_t);

   /* No glyph: the caller has nothing to draw, as before */
   if (!gi)
      return false;
   i = gi - 1;

   for (y = 0; y < cell_h; y++)
      memset(dst + (size_t)y * pitch * esz, 0, (size_t)cell_w * esz);

   for (y = 0; y < FONT_HEIGHT; y++)
   {
      for (x = 0; x < FONT_WIDTH; x++)
      {
         unsigned px = x + y * FONT_WIDTH;
         bool     on = (bitmap_bin[FONT_OFFSET(i) + (px >> 3)]
               & (1 << (px & 7))) != 0;

         if (!on)
            continue;

         for (yo = 0; yo < scale; yo++)
         {
            size_t row = (size_t)(y * scale + yo) * pitch;
            for (xo = 0; xo < scale; xo++)
            {
               size_t at = row + x * scale + xo;
               if (fmt16)
                  ((uint16_t*)(void*)dst)[at] = 0xFFFF;
               else
                  dst[at]                    = 0xFF;
            }
         }
      }
   }

   glyph->dims          = VIDEO_SCALE_PACK(FONT_WIDTH * scale,
         FONT_HEIGHT * scale);
   glyph->draw_offset_x = 0;
   glyph->draw_offset_y = 1 - FONT_HEIGHT_BASELINE_OFFSET * (int)scale;
   glyph->advance_x     = FONT_WIDTH_STRIDE * scale;
   glyph->advance_y     = 0;
   return true;
}

static bool font_rasterizer_stb_render_glyph(void *data, uint32_t code,
      unsigned gi, uint8_t *dst, unsigned pitch, unsigned cell_dims,
      enum font_atlas_format fmt, struct font_glyph *glyph)
{
   unsigned cell_w, cell_h;
   int x0                    = 0;
   int y1                    = 0;
   int advance_width         = 0;
   int left_side_bearing     = 0;
   float glyph_advance_x     = 0.0f;
   float glyph_draw_offset_y = 0.0f;
   stb_face_t *self          = (stb_face_t*)data;

   (void)code;

   if (self->builtin_scale)
      return font_rasterizer_stb_render_builtin(self, gi, dst, pitch,
            cell_dims, fmt, glyph);

   cell_w = VIDEO_SCALE_W(cell_dims);
   cell_h = VIDEO_SCALE_H(cell_dims);

   rtt_glyph_hmetrics(&self->info, (int)gi, &advance_width,
         &left_side_bearing);

   if (rtt_glyph_box(&self->info, (int)gi, &x0, NULL, NULL, &y1))
      rtt_render_glyph(&self->info, &self->scratch, dst, cell_w, cell_h,
            pitch, self->scale_factor, self->scale_factor, (int)gi, x0, y1,
            fmt == FONT_ATLAS_FORMAT_A16);
   else
   {
      /* The glyph is empty; clear its cell */
      int row;
      size_t esz = (fmt == FONT_ATLAS_FORMAT_A16)
            ? sizeof(uint16_t) : sizeof(uint8_t);
      for (row = 0; row < (int)cell_h; row++)
         memset(dst + (size_t)row * pitch * esz, 0, (size_t)cell_w * esz);
   }

   glyph->dims           = cell_dims;

   /* advance_x must always be rounded to the
    * *nearest* integer */
   glyph_advance_x       = (float)advance_width * self->scale_factor;
   glyph->advance_x      = (int)((glyph_advance_x > 0.0f)
         ? (glyph_advance_x + 0.5f)
         : (glyph_advance_x - 0.5f));
   /* advance_y is always zero */
   glyph->advance_y      = 0;

   /* draw_offset_x must always be rounded *down*
    * to the nearest integer */
   glyph->draw_offset_x  = (int)((float)x0 * self->scale_factor);

   /* draw_offset_y must always be rounded *up*
    * to the nearest integer */
   glyph_draw_offset_y   = (float)(-y1) * self->scale_factor;
   glyph->draw_offset_y  = (int)((glyph_draw_offset_y < 0.0f)
         ? floor((double)glyph_draw_offset_y)
         : ceil((double)glyph_draw_offset_y));
   return true;
}

static unsigned font_rasterizer_stb_cell_dims(void *data)
{
   return ((stb_face_t*)data)->cell_dims;
}

static void font_rasterizer_stb_get_line_metrics(void *data,
      struct font_line_metrics *metrics)
{
   *metrics = ((stb_face_t*)data)->line_metrics;
}

static const char * const *font_renderer_stb_get_default_fonts(
      const char *requested, unsigned *face_index)
{
#ifdef WIIU
   /* The shared system font, fetched in init(); no file to open. */
   static const char * const wiiu_paths[] = { "", NULL };
   (void)requested;
   (void)face_index;
   return wiiu_paths;
#else
   static const char * const paths[] = {
#if defined(_WIN32) && !defined(__WINRT__)
      "C:\\Windows\\Fonts\\consola.ttf",
      "C:\\Windows\\Fonts\\verdana.ttf",
#elif defined(__APPLE__)
      "/Library/Fonts/Microsoft/Candara.ttf",
      "/Library/Fonts/Verdana.ttf",
      "/Library/Fonts/Tahoma.ttf",
      "/Library/Fonts/Andale Mono.ttf",
      "/Library/Fonts/Courier New.ttf",
#elif defined(__ANDROID_API__)
      "/system/fonts/DroidSansMono.ttf",
      "/system/fonts/CutiveMono.ttf",
      "/system/fonts/DroidSans.ttf",
#elif defined(VITA)
      "vs0:data/external/font/pvf/c041056ts.ttf",
      "vs0:data/external/font/pvf/d013013ds.ttf",
      "vs0:data/external/font/pvf/e046323ms.ttf",
      "vs0:data/external/font/pvf/e046323ts.ttf",
      "vs0:data/external/font/pvf/k006004ds.ttf",
      "vs0:data/external/font/pvf/n023055ms.ttf",
      "vs0:data/external/font/pvf/n023055ts.ttf",
#elif defined(ORBIS)
      "/preinst/common/font/c041056ts.ttf",
      "/preinst/common/font/d013013ds.ttf",
      "/preinst/common/font/e046323ms.ttf",
      "/preinst/common/font/e046323ts.ttf",
      "/preinst/common/font/k006004ds.ttf",
      "/preinst/common/font/n023055ms.ttf",
      "/preinst/common/font/n023055ts.ttf",
#elif !defined(__WINRT__)
      "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
      "/usr/share/fonts/TTF/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf",
      "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSansMono.ttf",
      "/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf",
      "osd-font.ttf",
#endif
      "",              /* built-in glyphs, no file needed */
      NULL
   };

   (void)face_index;

   /* An explicit request is the caller's choice and this renderer has
    * no reason to second-guess it. Answering with candidates here
    * replaced the menu font with the first system font on the list. */
   if (requested && *requested)
      return NULL;

   /* The empty entry is the fallback: no file, built-in glyphs. */
   return paths;
#endif
}

const font_rasterizer_t stb_font_rasterizer = {
   font_rasterizer_stb_init,
   font_rasterizer_stb_free,
   font_rasterizer_stb_glyph_index,
   font_rasterizer_stb_render_glyph,
   font_rasterizer_stb_cell_dims,
   font_rasterizer_stb_get_line_metrics,
   font_renderer_stb_get_default_fonts,
   "font_renderer_stb",
   true                        /* borrows_font_data */
};

/*
Copyright (c) 2012, Broadcom Europe Ltd
All rights reserved.

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions are met:
    * Redistributions of source code must retain the above copyright
      notice, this list of conditions and the following disclaimer.
    * Redistributions in binary form must reproduce the above copyright
      notice, this list of conditions and the following disclaimer in the
      documentation and/or other materials provided with the distribution.
    * Neither the name of the copyright holder nor the
      names of its contributors may be used to endorse or promote products
      derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY
DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

// Font handling for graphicsx

/** @file font.c
  *
  * Fairly primitive font handling, just enough to emulate the old API.
  *
  * Hinting and Font Size
  *
  * The old API does not create fonts explicitly, it just renders them
  * as needed. That works fine for unhinted fonts, but for hinted fonts we
  * care about font size.
  *
  * Since we now *can* do hinted fonts, we should do. Regenerating the
  * fonts each time becomes quite slow, so we maintain a cache of fonts.
  *
  * For the typical applications which use graphics_x this is fine, but
  * won't work well if lots of fonts sizes are used.
  *
  * Unicode
  *
  * This API doesn't support unicode at all at present, nor UTF-8.
  */

#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <sys/types.h>

#include "graphics_x_private.h"
#include "vgft.h"

#define VMCS_INSTALL_PREFIX ""

/** The one and only (default) font we support for now.
  */
static struct
{
   const char *file;
   void *mem;
   size_t len;
} default_font = { "Vera.ttf" };

/** An entry in our list of fonts
  */
typedef struct gx_font_cache_entry_t
{
   struct gx_font_cache_entry_t *next;
   VGFT_FONT_T font;
   uint32_t ptsize;                    /** size in points, 26.6 */
} gx_font_cache_entry_t;

static char fname[128];
static int inited;
static gx_font_cache_entry_t *fonts;

static VGFT_FONT_T *find_font(const char *text, uint32_t text_size);

VCOS_STATUS_T gx_priv_font_init(const char *font_dir)
{
   VCOS_STATUS_T ret;
   size_t len;
   int rc;
   if (vgft_init())
   {
      ret = VCOS_ENOMEM;
      goto fail_init;
   }

   int fd = -1;
   // search for the font
   sprintf(fname, "%s/%s", font_dir, default_font.file);
   fd = open(fname, O_RDONLY);

   if (fd < 0)
   {
      GX_ERROR("Could not open font file '%s'", default_font.file);
      ret = VCOS_ENOENT;
      goto fail_open;
   }

   len = lseek(fd, 0, SEEK_END);
   lseek(fd, 0, SEEK_SET);

   default_font.mem = vcos_malloc(len, default_font.file);
   if (!default_font.mem)
   {
      GX_ERROR("No memory for font %s", fname);
      ret = VCOS_ENOMEM;
      goto fail_mem;
   }

   rc = read(fd, default_font.mem, len);
   if (rc != len)
   {
      GX_ERROR("Could not read font %s", fname);
      ret = VCOS_EINVAL;
      goto fail_rd;
   }
   default_font.len = len;
   close(fd);

   GX_TRACE("Opened font file '%s'", fname);

   inited = 1;
   return VCOS_SUCCESS;

fail_rd:
   vcos_free(default_font.mem);
fail_mem:
   if (fd >= 0) close(fd);
fail_open:
   vgft_term();
fail_init:
   return ret;
}

void gx_priv_font_term(void)
{
   gx_font_cache_flush();
   vgft_term();
   vcos_free(default_font.mem);
}

/** Render text.
  *
  * FIXME: Not at all optimal - re-renders each time.
  * FIXME: Not UTF-8 aware
  * FIXME: better caching
  */
VCOS_STATUS_T gx_priv_render_text( GX_DISPLAY_T *disp,
                                   GRAPHICS_RESOURCE_HANDLE res,
                                   int32_t x,
                                   int32_t y,
                                   uint32_t width,
                                   uint32_t height,
                                   uint32_t fg_colour,
                                   uint32_t bg_colour,
                                   const char *text,
                                   uint32_t text_length,
                                   uint32_t text_size )
{
   VGfloat vg_colour[4];
   VGFT_FONT_T *font;
   VGPaint fg;
   GX_CLIENT_STATE_T save;
   VCOS_STATUS_T status = VCOS_SUCCESS;
   int clip = 1;

   vcos_demand(inited); // has gx_font_init() been called?

   gx_priv_save(&save, res);

   if (width == GRAPHICS_RESOURCE_WIDTH &&
       height == GRAPHICS_RESOURCE_HEIGHT)
   {
      clip = 0;
   }

   width = (width == GRAPHICS_RESOURCE_WIDTH) ? res->width : width;
   height = (height == GRAPHICS_RESOURCE_HEIGHT) ? res->height : height;
   font = find_font(text, text_size);
   if (!font)
   {
      status = VCOS_ENOMEM;
      goto finish;
   }

   // setup the clipping rectangle
   if (clip)
   {
      VGint coords[] = {x,y,width,height};
      vgSeti(VG_SCISSORING, VG_TRUE);
      vgSetiv(VG_SCISSOR_RECTS, 4, coords);
   }

   // setup the background colour if needed
   if (bg_colour != GRAPHICS_TRANSPARENT_COLOUR)
   {
      int err;
      VGfloat rendered_w, rendered_h;
      VGfloat vg_bg_colour[4];

      // setup the background colour...
      gx_priv_colour_to_paint(bg_colour, vg_bg_colour);
      vgSetfv(VG_CLEAR_COLOR, 4, vg_bg_colour);

      // fill in a rectangle...
      vgft_get_text_extents(font, text, text_length, (VGfloat)x, (VGfloat)y, &rendered_w, &rendered_h);

      if ( ( 0 < (VGint)rendered_w ) && ( 0 < (VGint)rendered_h ) )
      {
         // Have to compensate for the messed up y position of multiline text.
         VGfloat offset = vgft_first_line_y_offset(font);
         int32_t bottom = y + offset - rendered_h;

         vgClear(x, bottom, (VGint)rendered_w, (VGint)rendered_h);
         err = vgGetError();
         if (err)
         {
            GX_LOG("Error %d clearing bg text %d %d %g %g",
                   err, x, y, rendered_w, rendered_h);
            vcos_assert(0);
         } // if
      } // if
   } // if
   // setup the foreground colour
   fg = vgCreatePaint();
   if (!fg)
   {
      status = VCOS_ENOMEM;
      goto finish;
   }

   // draw the foreground text
   vgSetParameteri(fg, VG_PAINT_TYPE, VG_PAINT_TYPE_COLOR);
   gx_priv_colour_to_paint(fg_colour, vg_colour);
   vgSetParameterfv(fg, VG_PAINT_COLOR, 4, vg_colour);
   vgSetPaint(fg, VG_FILL_PATH);

   vgft_font_draw(font, (VGfloat)x, (VGfloat)y, text, text_length, VG_FILL_PATH);

   vgDestroyPaint(fg);

   vcos_assert(vgGetError() == 0);
   vgSeti(VG_SCISSORING, VG_FALSE);

finish:
   gx_priv_restore(&save);

   return status;
}


/** Find a font in our cache, or create a new entry in the cache.
  *
  * Very primitive at present.
  */
static VGFT_FONT_T *find_font(const char *text, uint32_t text_size)
{
   int ptsize, dpi_x = 0, dpi_y = 0;
   VCOS_STATUS_T status;
   gx_font_cache_entry_t *font;

   ptsize = text_size << 6; // freetype takes size in points, in 26.6 format.

   for (font = fonts; font; font = font->next)
   {
      if (font->ptsize == ptsize)
         return &font->font;
   }

   font = vcos_malloc(sizeof(*font), "font");
   if (!font)
      return NULL;

   font->ptsize = ptsize;

   status = vgft_font_init(&font->font);
   if (status != VCOS_SUCCESS)
   {
      vcos_free(font);
      return NULL;
   }

   // load the font
   status = vgft_font_load_mem(&font->font, default_font.mem, default_font.len);
   if (status != VCOS_SUCCESS)
   {
      GX_LOG("Could not load font from memory: %d", status);
      vgft_font_term(&font->font);
      vcos_free(font);
      return NULL;
   }

   status = vgft_font_convert_glyphs(&font->font, ptsize, dpi_x, dpi_y);
   if (status != VCOS_SUCCESS)
   {
      GX_LOG("Could not convert font '%s' at size %d", fname, ptsize);
      vgft_font_term(&font->font);
      vcos_free(font);
      return NULL;
   }

   font->next = fonts;
   fonts = font;

   return &font->font;
}

void gx_font_cache_flush(void)
{
   while (fonts != NULL)
   {
      struct gx_font_cache_entry_t *next = fonts->next;
      vgft_font_term(&fonts->font);
      vcos_free(fonts);
      fonts = next;
   }
}

int32_t graphics_resource_text_dimensions_ext(GRAPHICS_RESOURCE_HANDLE res,
                                              const char *text,
                                              const uint32_t text_length,
                                              uint32_t *width,
                                              uint32_t *height,
                                              const uint32_t text_size )
{
   GX_CLIENT_STATE_T save;
   VGfloat w, h;
   int ret = -1;

   gx_priv_save(&save, res);

   VGFT_FONT_T *font = find_font(text, text_size);
   if (!font)
      goto finish;


   vgft_get_text_extents(font, text, text_length, 0.0, 0.0, &w, &h);
   *width = w;
   *height = h;
   ret = 0;

finish:
   gx_priv_restore(&save);
   return ret;
}


/*  SSNES - A Super Nintendo Entertainment System (SNES) Emulator frontend for libsnes.
 *  Copyright (C) 2010-2011 - Hans-Kristian Arntzen
 *
 *  Some code herein may be based on code found in BSNES.
 * 
 *  SSNES is free software: you can redistribute it and/or modify it under the terms
 *  of the GNU General Public License as published by the Free Software Found-
 *  ation, either version 3 of the License, or (at your option) any later version.
 *
 *  SSNES is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY;
 *  without even the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
 *  PURPOSE.  See the GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along with SSNES.
 *  If not, see <http://www.gnu.org/licenses/>.
 */

#include "fonts.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>
#include <assert.h>

#include <ft2build.h>
#include FT_FREETYPE_H

struct font_renderer
{
   FT_Library lib;
   FT_Face face;
};

font_renderer_t *font_renderer_new(const char *font_path, unsigned font_size)
{
   (void)font_size;
   font_renderer_t *handle = calloc(1, sizeof(*handle));
   if (!handle)
      goto error;

   FT_Error err = FT_Init_FreeType(&handle->lib);
   if (err)
      goto error;

   err = FT_New_Face(handle->lib, font_path, 0, &handle->face);
   if (err)
      goto error;

   err = FT_Set_Pixel_Sizes(handle->face, 0, font_size);
   if (err)
      goto error;

   return handle;

error:
   free(handle);
   if (handle->face)
      FT_Done_Face(handle->face);
   if (handle->lib)
      FT_Done_FreeType(handle->lib);
   return NULL;
}

void font_renderer_msg(font_renderer_t *handle, const char *msg, struct font_output_list *output) 
{
   output->head = NULL;

   FT_GlyphSlot slot = handle->face->glyph;
   struct font_output *cur = NULL;
   size_t len = strlen(msg);
   int off_x = 0, off_y = 0;

   for (size_t i = 0; i < len; i++)
   {
      FT_Error err = FT_Load_Char(handle->face, msg[i], FT_LOAD_RENDER);

      if (!err)
      {
         struct font_output *tmp = calloc(1, sizeof(*tmp));
         assert(tmp);

         //fprintf(stderr, "Char: %c, off_x: %d, off_y: %d, bmp_left: %d, bmp_top: %d\n", msg[i], off_x, off_y, slot->bitmap_left, slot->bitmap_top);

         tmp->output = malloc(slot->bitmap.pitch * slot->bitmap.rows);
         assert(tmp->output);
         memcpy(tmp->output, slot->bitmap.buffer, slot->bitmap.pitch * slot->bitmap.rows);

         tmp->width = slot->bitmap.width;
         tmp->height = slot->bitmap.rows;
         tmp->pitch = slot->bitmap.pitch;
         tmp->off_x = off_x + slot->bitmap_left;
         tmp->off_y = off_y + slot->bitmap_top - slot->bitmap.rows;
         tmp->next = NULL;

         if (i == 0)
            output->head = tmp;
         else
            cur->next = tmp;

         cur = tmp;
      }

      off_x += slot->advance.x >> 6;
      off_y += slot->advance.y >> 6;
   }
}

void font_renderer_free_output(struct font_output_list *output)
{
   struct font_output *itr = output->head;
   struct font_output *tmp = NULL;
   while (itr != NULL)
   {
      free(itr->output);
      tmp = itr;
      itr = itr->next;
      free(tmp);
   }
   output->head = NULL;
}

void font_renderer_free(font_renderer_t *handle)
{
   if (handle->face)
      FT_Done_Face(handle->face);
   if (handle->lib)
      FT_Done_FreeType(handle->lib);
}

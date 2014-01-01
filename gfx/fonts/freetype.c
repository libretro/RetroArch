/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
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

#include "fonts.h"
#include "../../file.h"
#include <string.h>
#include <stddef.h>
#include <stdlib.h>

#include <ft2build.h>
#include FT_FREETYPE_H

struct font_renderer
{
   FT_Library lib;
   FT_Face face;
};

static void ft_renderer_free(void *data)
{
   font_renderer_t *handle = (font_renderer_t*)data;
   if (!handle)
      return;

   if (handle->face)
      FT_Done_Face(handle->face);
   if (handle->lib)
      FT_Done_FreeType(handle->lib);
   free(handle);
}

static void *ft_renderer_init(const char *font_path, float font_size)
{
   (void)font_size;
   FT_Error err;
   font_renderer_t *handle = (font_renderer_t*)calloc(1, sizeof(*handle));
   if (!handle)
      goto error;

   err = FT_Init_FreeType(&handle->lib);
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
   ft_renderer_free(handle);
   return NULL;
}

static void ft_renderer_msg(void *data, const char *msg, struct font_output_list *output) 
{
   size_t i;
   font_renderer_t *handle = (font_renderer_t*)data;
   output->head = NULL;

   FT_GlyphSlot slot = handle->face->glyph;
   struct font_output *cur = NULL;
   size_t len = strlen(msg);
   int off_x = 0, off_y = 0;

   for (i = 0; i < len; i++)
   {
      FT_Error err = FT_Load_Char(handle->face, msg[i], FT_LOAD_RENDER);

      if (!err)
      {
         struct font_output *tmp = (struct font_output*)calloc(1, sizeof(*tmp));
         if (!tmp)
            break;

         tmp->output = (uint8_t*)malloc(slot->bitmap.pitch * slot->bitmap.rows);
         if (!tmp->output)
         {
            free(tmp);
            break;
         }

         memcpy(tmp->output, slot->bitmap.buffer, slot->bitmap.pitch * slot->bitmap.rows);

         tmp->width = slot->bitmap.width;
         tmp->height = slot->bitmap.rows;
         tmp->pitch = slot->bitmap.pitch;
         tmp->advance_x = slot->advance.x >> 6;
         tmp->advance_y = slot->advance.y >> 6;
         tmp->char_off_x = slot->bitmap_left;
         tmp->char_off_y = slot->bitmap_top - slot->bitmap.rows;
         tmp->off_x = off_x + tmp->char_off_x;
         tmp->off_y = off_y + tmp->char_off_y;
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

static void ft_renderer_free_output(void *data, struct font_output_list *output)
{
   (void)data;

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

// Not the cleanest way to do things for sure, but should hopefully work ... :)

static const char *font_paths[] = {
#if defined(_WIN32)
   "C:\\Windows\\Fonts\\consola.ttf",
   "C:\\Windows\\Fonts\\verdana.ttf",
#elif defined(__APPLE__)
   "/Library/Fonts/Microsoft/Candara.ttf",
   "/Library/Fonts/Verdana.ttf",
   "/Library/Fonts/Tahoma.ttf",
#else
   "/usr/share/fonts/TTF/DejaVuSansMono.ttf",
   "/usr/share/fonts/TTF/DejaVuSans.ttf",
   "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSansMono.ttf",
   "/usr/share/fonts/truetype/ttf-dejavu/DejaVuSans.ttf",
#endif
   "osd-font.ttf", // Magic font to search for, useful for distribution.
};

// Highly OS/platform dependent.
static const char *ft_renderer_get_default_font(void)
{
   size_t i;
   for (i = 0; i < ARRAY_SIZE(font_paths); i++)
   {
      if (path_file_exists(font_paths[i]))
         return font_paths[i];
   }

   return NULL;
}

const font_renderer_driver_t ft_font_renderer = {
   ft_renderer_init,
   ft_renderer_msg,
   ft_renderer_free_output,
   ft_renderer_free,
   ft_renderer_get_default_font,
   "freetype",
};


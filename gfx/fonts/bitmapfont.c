/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2013 - Hans-Kristian Arntzen
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
#include "bitmap.h"
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include "../../msvc/msvc_compat.h"
#include "../../boolean.h"

struct font_renderer
{
   unsigned scale_factor;
   uint8_t *bitmap_chars[256];
   uint8_t *bitmap_alloc;
};

static void char_to_texture(font_renderer_t *handle, uint8_t letter)
{
   unsigned y, x, xo, yo;
   handle->bitmap_chars[letter] = &handle->bitmap_alloc[letter * FONT_WIDTH * FONT_HEIGHT * handle->scale_factor * handle->scale_factor];
   for (y = 0; y < FONT_HEIGHT; y++)
   {
      for (x = 0; x < FONT_WIDTH; x++)
      {
         uint8_t rem = 1 << ((x + y * FONT_WIDTH) & 7);
         unsigned offset = (x + y * FONT_WIDTH) >> 3;
         uint8_t col = (bitmap_bin[FONT_OFFSET(letter) + offset] & rem) ? 0xFF : 0;

         for (xo = 0; xo < handle->scale_factor; xo++)
            for (yo = 0; yo < handle->scale_factor; yo++)
               handle->bitmap_chars[letter][x * handle->scale_factor + xo + (y * handle->scale_factor + yo) * FONT_WIDTH * handle->scale_factor] = col;
      }
   }
}


static void *font_renderer_init(const char *font_path, float font_size)
{
   unsigned i;
   font_renderer_t *handle = (font_renderer_t*)calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   handle->scale_factor = (unsigned)roundf(font_size / FONT_HEIGHT);
   if (!handle->scale_factor)
      handle->scale_factor = 1;

   handle->bitmap_alloc = (uint8_t*)malloc(FONT_WIDTH * FONT_HEIGHT * handle->scale_factor * handle->scale_factor * 256);

   if (!handle->bitmap_alloc)
   {
      free(handle);
      return NULL;
   }

   for (i = 0; i < 256; i++)
      char_to_texture(handle, i);

   return handle;
}

static void font_renderer_msg(void *data, const char *msg, struct font_output_list *output) 
{
   size_t i;
   font_renderer_t *handle = (font_renderer_t*)data;
   output->head = NULL;

   struct font_output *cur = NULL;
   size_t len = strlen(msg);
   int off_x = 0;

   for (i = 0; i < len; i++)
   {
      struct font_output *tmp = (struct font_output*)calloc(1, sizeof(*tmp));
      if (!tmp)
         break;

      tmp->output = handle->bitmap_chars[(unsigned) msg[i]];
      tmp->width = FONT_WIDTH * handle->scale_factor;
      tmp->height = FONT_HEIGHT * handle->scale_factor;
      tmp->pitch = tmp->width;
      tmp->advance_x = tmp->width;
      tmp->advance_y = tmp->height;
      tmp->char_off_x = 0;
      tmp->char_off_y = tmp->height;
      tmp->off_x = off_x;
      tmp->off_y = 0;
      tmp->next = NULL;

      if (i == 0)
         output->head = tmp;
      else
         cur->next = tmp;

      cur = tmp;

      off_x += FONT_WIDTH_STRIDE * handle->scale_factor;
   }
}

static void font_renderer_free_output(void *data, struct font_output_list *output)
{
   (void)data;
   struct font_output *itr = output->head;
   struct font_output *tmp = NULL;
   while (itr != NULL)
   {
      tmp = itr;
      itr = itr->next;
      free(tmp);
   }
   output->head = NULL;
}

static void font_renderer_free(void *data)
{
   font_renderer_t *handle = (font_renderer_t*)data;
   free(handle->bitmap_alloc);
   free(handle);
}

static const char *font_renderer_get_default_font(void)
{
   return "";
}

const font_renderer_driver_t bitmap_font_renderer = {
   font_renderer_init,
   font_renderer_msg,
   font_renderer_free_output,
   font_renderer_free,
   font_renderer_get_default_font,
   "bitmap",
};

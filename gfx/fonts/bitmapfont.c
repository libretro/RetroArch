/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
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
#include <stdbool.h>

#include "../../general.h"

static uint8_t bitmap_chars[256][50];

struct font_renderer
{
   unsigned scale_factor;
};

static void char_to_texture(uint8_t letter, uint8_t *buffer)
{
   for (unsigned j = 0; j < FONT_HEIGHT; j++)
   {
      for (unsigned i = 0; i < FONT_WIDTH; i++)
      {
         uint8_t rem = 1 << ((i + j * FONT_WIDTH) & 7);
         unsigned offset = (i + j * FONT_WIDTH) >> 3;
         bool col = (bitmap_bin[FONT_OFFSET(letter) + offset] & rem);

         buffer[i + j * FONT_WIDTH] = col ? 0xFF : 0;
      }
   }
}


static void *font_renderer_init(const char *font_path, unsigned font_size)
{
   font_renderer_t *handle = (font_renderer_t*)calloc(1, sizeof(*handle));
   if (!handle)
      return NULL;

   handle->scale_factor = font_size / FONT_HEIGHT;
   if (!handle->scale_factor)
      handle->scale_factor = 1;
   RARCH_LOG("scale_factor: %d\n", handle->scale_factor);

   for (unsigned i = 0; i < 256; i++)
      char_to_texture(i, bitmap_chars[i]);

   return handle;
}

static void font_renderer_msg(void *data, const char *msg, struct font_output_list *output) 
{
   font_renderer_t *handle = (font_renderer_t*)data;
   output->head = NULL;

   struct font_output *cur = NULL;
   size_t len = strlen(msg);
   int off_x = 0;

   for (size_t i = 0; i < len; i++)
   {
      struct font_output *tmp = (struct font_output*)calloc(1, sizeof(*tmp));
      if (!tmp)
         break;

      tmp->output = (uint8_t*)malloc(FONT_WIDTH * FONT_HEIGHT * handle->scale_factor * handle->scale_factor);
      if (!tmp->output)
      {
         free(tmp);
         break;
      }

      unsigned msg_char = msg[i];
      for (unsigned x = 0; x < FONT_WIDTH; x++)
         for (unsigned y = 0; y < FONT_HEIGHT; y++)
            for (unsigned xo = 0; xo < handle->scale_factor; xo++)
               for (unsigned yo = 0; yo < handle->scale_factor; yo++)
                  tmp->output[x * handle->scale_factor + xo + (y * handle->scale_factor + yo) * FONT_WIDTH * handle->scale_factor] = bitmap_chars[msg_char][x + y * FONT_WIDTH];

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
      free(itr->output);
      tmp = itr;
      itr = itr->next;
      free(tmp);
   }
   output->head = NULL;
}

static void font_renderer_free(void *data)
{
   font_renderer_t *handle = (font_renderer_t*)data;
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


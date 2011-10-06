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


#ifndef __SSNES_FONTS_H
#define __SSNES_FONTS_H

#include <stdint.h>

typedef struct font_renderer font_renderer_t;

struct font_output
{
   uint8_t *output; // 8-bit intensity.
   unsigned width, height, pitch;
   int off_x, off_y;
   struct font_output *next; // linked list.
};

struct font_output_list
{
   struct font_output *head;
};

font_renderer_t *font_renderer_new(const char *font_path, unsigned font_size);
void font_renderer_msg(font_renderer_t *handle, const char *msg,
      struct font_output_list *output);

void font_renderer_free_output(struct font_output_list *list);
void font_renderer_free(font_renderer_t *handle);

const char *font_renderer_get_default_font(void);

#endif

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


#ifndef __RARCH_FONTS_H
#define __RARCH_FONTS_H

#include <stdint.h>
#include "../../boolean.h"

typedef struct font_renderer font_renderer_t;

struct font_output
{
   uint8_t *output; // 8-bit alpha.
   unsigned width, height, pitch;
   unsigned color;
   unsigned scaling_factor;
   int off_x, off_y;
   int advance_x, advance_y, char_off_x, char_off_y; // for advanced font rendering
   struct font_output *next; // linked list.
};

struct font_output_list
{
   struct font_output *head;
};

typedef struct font_renderer_driver
{
   void *(*init)(const char *font_path, unsigned font_size);
   void (*render_msg)(void *data, const char *msg, struct font_output_list *output);
   void (*free_output)(void *data, struct font_output_list *list);
   void (*free)(void *data);
   const char *(*get_default_font)(void);
   const char *ident;
} font_renderer_driver_t;

extern const font_renderer_driver_t ft_font_renderer;
extern const font_renderer_driver_t bitmap_font_renderer;

bool font_renderer_create_default(const font_renderer_driver_t **driver, void **handle);

#endif


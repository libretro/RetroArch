/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2014 - Daniel De Matteis
 *  Copyright (C) 2014      - Jean-André Santoni
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

#ifndef _MENU_DISP_LAKKA_H
#define _MENU_DISP_LAKKA_H

#include "../../../gfx/gl_common.h"
#include "../../../gfx/fonts/fonts.h"

#define THEME "monochrome" // flatui or monochrome themes are available

#ifndef LAKKA_DELAY
#define LAKKA_DELAY 0.02
#endif

extern int depth;
extern int num_categories;
extern float all_categories_x;
extern int menu_active_category;
extern float global_alpha;
extern float global_scale;
extern float arrow_alpha;
extern float hspacing;
extern float vspacing;
extern float c_active_zoom;
extern float c_passive_zoom;
extern float i_active_zoom;
extern float i_passive_zoom;
extern float above_subitem_offset;
extern float above_item_offset;
extern float active_item_factor;
extern float under_item_offset;

typedef struct
{
   char  name[256];
   GLuint icon;
   float  alpha;
   float  zoom;
   float  y;
   rarch_setting_t *setting;
} menu_subitem_t;

typedef struct
{
   char  name[256];
   char  rom[PATH_MAX];
   float  alpha;
   float  zoom;
   float  y;
   int    active_subitem;
   int num_subitems;
   menu_subitem_t *subitems;
} menu_item_t;

typedef struct
{
   char  name[256];
   char  libretro[PATH_MAX];
   GLuint icon;
   GLuint item_icon;
   float  alpha;
   float  zoom;
   int    active_item;
   int    num_items;
   menu_item_t *items;
} menu_category_t;

extern menu_category_t *categories;

#endif /* MENU_DISP_LAKKA_H */

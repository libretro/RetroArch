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

extern int depth;
extern int num_categories;
extern int menu_active_category;

typedef struct
{
   char  name[256];
   GLuint icon;
   float  alpha;
   float  zoom;
   float  y;
   struct font_output_list out;
} menu_subitem;

typedef struct
{
   char  name[256];
   char  rom[256];
   GLuint icon;
   float  alpha;
   float  zoom;
   float  y;
   int    active_subitem;
   int num_subitems;
   menu_subitem *subitems;
   struct font_output_list out;
} menu_item;

typedef struct
{
   char  name[256];
   char  libretro[256];
   GLuint icon;
   float  alpha;
   float  zoom;
   int    active_item;
   int    num_items;
   menu_item *items;
   struct font_output_list out;
} menu_category;

extern menu_category *categories;

void lakka_switch_items(void);
void lakka_switch_subitems(void);
void lakka_open_submenu(void);
void lakka_close_submenu(void);
void lakka_switch_categories(void);

#endif /* MENU_DISP_LAKKA_H */

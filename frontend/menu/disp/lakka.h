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

#define DELAY 0.02
#define HSPACING 300
#define VSPACING 75
#define C_ACTIVE_ZOOM 1.0
#define C_PASSIVE_ZOOM 0.5
#define I_ACTIVE_ZOOM 0.75
#define I_PASSIVE_ZOOM 0.35

extern int depth;
extern int num_categories;
extern float all_categories_x;
extern int menu_active_category;
extern float global_alpha;

typedef struct
{
   char  name[256];
   GLuint icon;
   float  alpha;
   float  zoom;
   float  y;
   struct font_output_list out;
} menu_subitem_t;

typedef struct
{
   char  name[256];
   char  rom[256];
   float  alpha;
   float  zoom;
   float  y;
   int    active_subitem;
   int num_subitems;
   menu_subitem_t *subitems;
   struct font_output_list out;
} menu_item_t;

typedef struct
{
   char  name[256];
   char  libretro[256];
   GLuint icon;
   GLuint item_icon;
   float  alpha;
   float  zoom;
   int    active_item;
   int    num_items;
   menu_item_t *items;
   struct font_output_list out;
} menu_category_t;

typedef float (*easingFunc)(float, float, float, float);
typedef void (*tweenCallback) (void);

typedef struct
{
   int    alive;
   float  duration;
   float  running_since;
   float  initial_value;
   float  target_value;
   float* subject;
   easingFunc easing;
   tweenCallback callback;
} tween_t;

extern menu_category_t *categories;

void add_tween(float duration, float target_value, float* subject, easingFunc easing, tweenCallback callback);
float inOutQuad(float t, float b, float c, float d);

#endif /* MENU_DISP_LAKKA_H */

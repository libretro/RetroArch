/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2012 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2012 - Daniel De Matteis
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

#ifndef _RMENU_STACK_H_
#define _RMENU_STACK_H_

typedef struct
{
   char title[32];
   unsigned char enum_id;
   unsigned char selected;
   unsigned char page;
   unsigned char first_setting;
   unsigned char max_settings;
   unsigned char category_id;
   int (*iterate)(void *data, uint64_t input);
   void (*browser_draw)(void *data);
} menu;

#endif

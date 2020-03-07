/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  copyright (c) 2011-2015 - Daniel De Matteis
 *  copyright (c) 2016-2019 - Brad Parker
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

#ifndef __VGA_COMMON_H
#define __VGA_COMMON_H

#define VGA_WIDTH 320
#define VGA_HEIGHT 200

typedef struct vga
{
   bool color;
   bool vga_rgb32;

   unsigned vga_menu_width;
   unsigned vga_menu_height;
   unsigned vga_menu_pitch;
   unsigned vga_menu_bits;
   unsigned vga_video_width;
   unsigned vga_video_height;
   unsigned vga_video_pitch;
   unsigned vga_video_bits;

   unsigned char *vga_menu_frame;
   unsigned char *vga_frame;
} vga_t;

#endif

/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2016 - Daniel De Matteis
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

#ifndef _FILE_PATH_SPECIAL_H
#define _FILE_PATH_SPECIAL_H

#include <stdint.h>
#include <stddef.h>

#include <boolean.h>

enum application_special_type
{
   APPLICATION_SPECIAL_NONE = 0,
   APPLICATION_SPECIAL_DIRECTORY_AUTOCONFIG,
   APPLICATION_SPECIAL_DIRECTORY_CONFIG,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_FONT,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_MATERIALUI_ICONS,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_BG,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_ICONS,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_XMB_FONT,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_ZARCH,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_ZARCH_FONT,
   APPLICATION_SPECIAL_DIRECTORY_ASSETS_ZARCH_ICONS
};

bool fill_pathname_application_data(char *s, size_t len);

void fill_pathname_application_special(char *s, size_t len, enum application_special_type type);

#endif

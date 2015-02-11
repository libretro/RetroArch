/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2014-2015 - Jean-André Santoni
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

#ifndef _MENU_TEXTURE_MANAGER_H
#define _MENU_TEXTURE_MANAGER_H

enum texture_backend_type
{
   TEXTURE_BACKEND_DEFAULT = 0,
   TEXTURE_BACKEND_OPENGL,
};

enum texture_filter_type
{
   TEXTURE_FILTER_DEFAULT = 0,
   TEXTURE_FILTER_MIPMAP,
};

#ifdef __cplusplus
extern "C" {
#endif

unsigned menu_texture_load(const char *path,
      enum texture_backend_type type,
      enum texture_filter_type  filter_type);

#ifdef __cplusplus
}
#endif

#endif

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

#ifndef __RARCH_IOS_DIRENT_LIST_H
#define __RARCH_IOS_DIRENT_LIST_H

#include <dirent.h>

struct dirent_list
{
   size_t count;
   struct dirent* entries;
};

struct dirent_list* build_dirent_list(const char* path);
void free_dirent_list(struct dirent_list* list);

const struct dirent* get_dirent_at_index(struct dirent_list* list, unsigned index);
unsigned get_dirent_list_count(struct dirent_list* list);

#endif
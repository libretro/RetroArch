/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2011-2015 - Daniel De Matteis
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

#ifndef _MENU_DATABASE_H
#define _MENU_DATABASE_H

#include <stddef.h>
#include <file/file_list.h>
#ifdef HAVE_LIBRETRODB
#include "../libretrodb/libretrodb.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif

int menu_database_populate_list(file_list_t *list, const char *path);

#ifdef __cplusplus
}
#endif

#endif

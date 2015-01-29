/*  RetroArch - A frontend for libretro.
 *  Copyright (C) 2010-2014 - Hans-Kristian Arntzen
 *  Copyright (C) 2011-2015 - Daniel De Matteis
 *  Copyright (C) 2013-2015 - Jason Fetters
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

#ifndef XMB_ENTRY_H_
#define XMB_ENTRY_H_

#include <file/config_file.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef struct
{
   char *path;
   config_file_t *data;
   char *display_name;
   char *core_name;
   char *icon_name;
   char *content_icon_name;
   char *content_subdirectory;
   void *userdata;
} xmb_entry_t;

typedef struct
{
   xmb_entry_t *list;
   size_t count;
} xmb_entry_list_t;

xmb_entry_list_t *xmb_entry_list_new(const char *xmb_entrie_path);
void xmb_entry_list_free(xmb_entry_list_t *list);

#ifdef __cplusplus
}
#endif

#endif /* XMB_ENTRY_H_ */

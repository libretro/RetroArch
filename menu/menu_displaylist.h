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

#ifndef _MENU_DISPLAYLIST_H
#define _MENU_DISPLAYLIST_H

#include <stdint.h>
#include <retro_miscellaneous.h>
#include "menu_list.h"

#ifdef __cplusplus
extern "C" {
#endif

enum
{
   DISPLAYLIST_NONE = 0,
   DISPLAYLIST_MAIN_MENU,
   DISPLAYLIST_SETTINGS,
   DISPLAYLIST_CORES,
   DISPLAYLIST_CORES_DETECTED,
   DISPLAYLIST_PERFCOUNTER_SELECTION,
   DISPLAYLIST_PERFCOUNTERS_CORE,
   DISPLAYLIST_PERFCOUNTERS_FRONTEND,
   DISPLAYLIST_SHADER_PASS,
   DISPLAYLIST_SHADER_PRESET,
   DISPLAYLIST_DATABASES,
   DISPLAYLIST_DATABASE_CURSORS,
};

typedef struct menu_displaylist_info
{
   file_list_t *list;
   file_list_t *menu_list;
   char path[PATH_MAX_LENGTH];
   char label[PATH_MAX_LENGTH];
   char exts[PATH_MAX_LENGTH];
   unsigned type;
   unsigned type_default;
   unsigned flags;
} menu_displaylist_info_t;

int menu_displaylist_deferred_push(menu_displaylist_info_t *info);

int menu_displaylist_push_list(menu_displaylist_info_t *info, unsigned type);

int menu_displaylist_push(file_list_t *list, file_list_t *menu_list);

#ifdef __cplusplus
}
#endif

#endif
